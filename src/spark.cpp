#include "../include/spark.h"
#include "spark.h"
//#include "../bitcoin/amount.h"
//#include <iostream>

#define SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION     (10000 * COIN)


spark::SpendKey createSpendKey(const SpendKeyData& data) {
    std::string nCountStr = std::to_string(data.getIndex());
    CHash256 hasher;
    std::string prefix = "r_generation";
    hasher.Write(reinterpret_cast<const unsigned char*>(prefix.c_str()), prefix.size());
    hasher.Write(data.getKeyData(), data.size());
    hasher.Write(reinterpret_cast<const unsigned char*>(nCountStr.c_str()), nCountStr.size());
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(hash);

    secp_primitives::Scalar r;
    r.memberFromSeed(hash);
    spark::SpendKey key(spark::Params::get_default(), r);
    return key;
}

spark::FullViewKey createFullViewKey(const spark::SpendKey& spendKey) {
    return spark::FullViewKey(spendKey);
}

spark::IncomingViewKey createIncomingViewKey(const spark::FullViewKey& fullViewKey) {
    return spark::IncomingViewKey(fullViewKey);
}

template <typename Iterator>
static uint64_t CalculateSparkCoinsBalance(Iterator begin, Iterator end)
{
    uint64_t balance = 0;
    for (auto start = begin; start != end; ++start) {
        balance += start->v;
    }
    return balance;
}

std::vector<CRecipient> createSparkMintRecipients(
                        const std::vector<spark::MintedCoinData>& outputs,
                        const std::vector<unsigned char>& serial_context,
                        bool generate)
{
    const spark::Params* params = spark::Params::get_default();

    spark::MintTransaction sparkMint(params, outputs, serial_context, generate);

    if (generate && !sparkMint.verify()) {
        throw std::runtime_error("Unable to validate spark mint.");
    }

    std::vector<CDataStream> serializedCoins = sparkMint.getMintedCoinsSerialized();

    if (outputs.size() != serializedCoins.size())
        throw std::runtime_error("Spark mint output number should be equal to required number.");

    std::vector<CRecipient> results;
    results.reserve(outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        CScript script;
        script << OP_SPARKMINT;
        script.insert(script.end(), serializedCoins[i].begin(), serializedCoins[i].end());
        CRecipient recipient = {script, CAmount(outputs[i].v), false};
        results.emplace_back(recipient);
    }
    return results;
}

bool GetCoinsToSpend(
        CAmount required,
        std::vector<CSparkMintMeta>& coinsToSpend_out,
        std::list<CSparkMintMeta> coins,
        int64_t& changeToMint)
{
    CAmount availableBalance = CalculateSparkCoinsBalance(coins.begin(), coins.end());

    if (required > availableBalance) {
        throw std::runtime_error("Insufficient funds !");
    }

    // sort by biggest amount. if it is same amount we will prefer the older block
    auto comparer = [](const CSparkMintMeta& a, const CSparkMintMeta& b) -> bool {
        return a.v != b.v ? a.v > b.v : a.nHeight < b.nHeight;
    };
    coins.sort(comparer);

    CAmount spend_val(0);

    std::list<CSparkMintMeta> coinsToSpend;
    while (spend_val < required) {
        if (coins.empty())
            break;

        CSparkMintMeta choosen;
        CAmount need = required - spend_val;

        auto itr = coins.begin();
        if (need >= itr->v) {
            choosen = *itr;
            coins.erase(itr);
        } else {
            for (auto coinIt = coins.rbegin(); coinIt != coins.rend(); coinIt++) {
                auto nextItr = coinIt;
                nextItr++;

                if (coinIt->v >= need && (nextItr == coins.rend() || nextItr->v != coinIt->v)) {
                    choosen = *coinIt;
                    coins.erase(std::next(coinIt).base());
                    break;
                }
            }
        }

        spend_val += choosen.v;
        coinsToSpend.push_back(choosen);
    }

    // sort by group id ay ascending order. it is mandatory for creting proper joinsplit
    auto idComparer = [](const CSparkMintMeta& a, const CSparkMintMeta& b) -> bool {
        return a.nId < b.nId;
    };
    coinsToSpend.sort(idComparer);

    changeToMint = spend_val - required;
    coinsToSpend_out.insert(coinsToSpend_out.begin(), coinsToSpend.begin(), coinsToSpend.end());

    return true;
}

std::pair<CAmount, std::vector<CSparkMintMeta>> SelectSparkCoins(
        CAmount required,
        bool subtractFeeFromAmount,
        std::list<CSparkMintMeta> coins,
        std::size_t mintNum) {
    CFeeRate fRate;

    CAmount fee;
    unsigned size;
    int64_t changeToMint = 0; // this value can be negative, that means we need to spend remaining part of required value with another transaction (nMaxInputPerTransaction exceeded)

    std::vector<CSparkMintMeta> spendCoins;
    for (fee = fRate.GetFeePerK();;) {
        CAmount currentRequired = required;

        if (!subtractFeeFromAmount)
            currentRequired += fee;
        spendCoins.clear();
        if (!GetCoinsToSpend(currentRequired, spendCoins, coins, changeToMint)) {
            throw std::invalid_argument("Unable to select cons for spend");
        }

        // 924 is constant part, mainly Schnorr and Range proofs, 2535 is for each grootle proof/aux data
        // 213 for each private output, 144 other parts of tx,
        size = 924 + 2535 * (spendCoins.size()) + 213 * mintNum + 144; //TODO (levon) take in account also utxoNum
        CAmount feeNeeded = size;

        if (fee >= feeNeeded) {
            break;
        }

        fee = feeNeeded;

        if (subtractFeeFromAmount)
            break;
    }

    if (changeToMint < 0)
        throw std::invalid_argument("Unable to select cons for spend");

    return std::make_pair(fee, spendCoins);

}


bool getIndex(const spark::Coin& coin, const std::vector<spark::Coin>& anonymity_set, size_t& index) {
    for (std::size_t j = 0; j < anonymity_set.size(); ++j) {
        if (anonymity_set[j] == coin){
            index = j;
            return true;
        }
    }
    return false;
}

void createSparkSpendTransaction(
        const spark::SpendKey& spendKey,
        const spark::FullViewKey& fullViewKey,
        const spark::IncomingViewKey& incomingViewKey,
        const std::vector<std::pair<CAmount, bool>>& recipients,
        const std::vector<std::pair<spark::OutputCoinData, bool>>& privateRecipients,
        std::list<CSparkMintMeta> coins,
        const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data_all,
        const std::map<uint64_t, uint256>& idAndBlockHashes_all,
        const uint256& txHashSig,
        CAmount &fee,
        std::vector<uint8_t>& serializedSpend,
        std::vector<std::vector<unsigned char>>& outputScripts) {

    if (recipients.empty() && privateRecipients.empty()) {
        throw std::runtime_error("Either recipients or newMints has to be nonempty.");
    }

    if (privateRecipients.size() >= SPARK_OUT_LIMIT_PER_TX - 1)
        throw std::runtime_error("Spark shielded output limit exceeded.");

    // calculate total value to spend
    CAmount vOut = 0;
    CAmount mintVOut = 0;
    unsigned recipientsToSubtractFee = 0;

    for (size_t i = 0; i < recipients.size(); i++) {
        auto& recipient = recipients[i];

        if (!MoneyRange(recipient.first)) {
            throw std::runtime_error("Recipient has invalid amount");
        }

        vOut += recipient.first;

        if (recipient.second) {
            recipientsToSubtractFee++;
        }
    }

    for (const auto& privRecipient : privateRecipients) {
        mintVOut += privRecipient.first.v;
        if (privRecipient.second) {
            recipientsToSubtractFee++;
        }
    }

    if (vOut > SPARK_VALUE_SPEND_LIMIT_PER_TRANSACTION)
        throw std::runtime_error("Spend to transparent address limit exceeded (10,000 Firo per transaction).");

    std::pair<CAmount, std::vector<CSparkMintMeta>> estimated =
            SelectSparkCoins(vOut + mintVOut, recipientsToSubtractFee, coins, privateRecipients.size());

    auto recipients_ = recipients;
    auto privateRecipients_ = privateRecipients;
    {
        bool remainderSubtracted = false;
        fee = estimated.first;
        for (size_t i = 0; i < recipients_.size(); i++) {
            auto &recipient = recipients_[i];

            if (recipient.second) {
                // Subtract fee equally from each selected recipient.
                recipient.first -= fee / recipientsToSubtractFee;

                if (!remainderSubtracted) {
                    // First receiver pays the remainder not divisible by output count.
                    recipient.first -= fee % recipientsToSubtractFee;
                    remainderSubtracted = true;
                }
            }
        }

        for (size_t i = 0; i < privateRecipients_.size(); i++) {
            auto &privateRecipient = privateRecipients_[i];

            if (privateRecipient.second) {
                // Subtract fee equally from each selected recipient.
                privateRecipient.first.v -= fee / recipientsToSubtractFee;

                if (!remainderSubtracted) {
                    // First receiver pays the remainder not divisible by output count.
                    privateRecipient.first.v -= fee % recipientsToSubtractFee;
                    remainderSubtracted = true;
                }
            }
        }

    }

    const spark::Params* params = spark::Params::get_default();
    if (spendKey == spark::SpendKey(params))
        throw std::runtime_error("Invalid pend key.");

    CAmount spendInCurrentTx = 0;
    for (auto& spendCoin : estimated.second)
        spendInCurrentTx += spendCoin.v;
    spendInCurrentTx -= fee;

    uint64_t transparentOut = 0;
    // fill outputs
    for (size_t i = 0; i < recipients_.size(); i++) {
        auto& recipient = recipients_[i];
        if (recipient.first == 0)
            continue;

        transparentOut += recipient.first;
    }

    spendInCurrentTx -= transparentOut;
    std::vector<spark::OutputCoinData> privOutputs;
    // fill outputs
    for (size_t i = 0; i < privateRecipients_.size(); i++) {
        auto& recipient = privateRecipients_[i];
        if (recipient.first.v == 0)
            continue;

        CAmount recipientAmount = recipient.first.v;
        spendInCurrentTx -= recipientAmount;
        spark::OutputCoinData output = recipient.first;
        output.v = recipientAmount;
        privOutputs.push_back(output);
    }

    if (spendInCurrentTx < 0)
        throw std::invalid_argument("Unable to create spend transaction.");

    if (!privOutputs.size() || spendInCurrentTx > 0) {
        spark::OutputCoinData output;
        output.address = spark::Address(incomingViewKey, SPARK_CHANGE_D);
        output.memo = "";
        if (spendInCurrentTx > 0)
            output.v = spendInCurrentTx;
        else
            output.v = 0;
        privOutputs.push_back(output);
    }

    // clear vExtraPayload to calculate metadata hash correctly
    serializedSpend.clear();

    // We will write this into cover set representation, with anonymity set hash
    uint256 sig = txHashSig;

    std::vector<spark::InputCoinData> inputs;
    std::map<uint64_t, uint256> idAndBlockHashes;
    std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data;
    for (auto& coin : estimated.second) {
        uint64_t groupId = coin.nId;
        if (cover_set_data.count(groupId) == 0) {
            if (!(cover_set_data_all.count(groupId) > 0 && idAndBlockHashes.count(groupId) > 0 ))
                throw std::runtime_error("No such coin in set in input data");
            cover_set_data[groupId] = cover_set_data_all.at(groupId);
            idAndBlockHashes[groupId] = idAndBlockHashes.at(groupId);
        }

        spark::InputCoinData inputCoinData;
        inputCoinData.cover_set_id = groupId;
        std::size_t index = 0;
        if (!getIndex(coin.coin, cover_set_data[groupId].cover_set, index))
            throw std::runtime_error("No such coin in set");
        inputCoinData.index = index;
        inputCoinData.v = coin.v;
        inputCoinData.k = coin.k;

        spark::IdentifiedCoinData identifiedCoinData;
        identifiedCoinData.i = coin.i;
        identifiedCoinData.d = coin.d;
        identifiedCoinData.v = coin.v;
        identifiedCoinData.k = coin.k;
        identifiedCoinData.memo = coin.memo;
        spark::RecoveredCoinData recoveredCoinData = coin.coin.recover(fullViewKey, identifiedCoinData);

        inputCoinData.T = recoveredCoinData.T;
        inputCoinData.s = recoveredCoinData.s;
        inputs.push_back(inputCoinData);
    }

    spark::SpendTransaction spendTransaction(params, fullViewKey, spendKey, inputs, cover_set_data, fee, transparentOut, privOutputs);
    spendTransaction.setBlockHashes(idAndBlockHashes);
    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    serialized << spendTransaction;
    serializedSpend.assign(serialized.begin(), serialized.end());

    outputScripts.clear();
    const std::vector<spark::Coin>& outCoins = spendTransaction.getOutCoins();
    for (auto& outCoin : outCoins) {
        // construct spend script
        CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
        serialized << outCoin;
        std::vector<unsigned char> script;
        script.push_back((unsigned char)OP_SPARKSMINT);
        script.insert(script.end(), serialized.begin(), serialized.end());
        outputScripts.emplace_back(script);
    }

}

spark::Address getAddress(const spark::IncomingViewKey& incomingViewKey, const uint64_t diversifier)
{
    return spark::Address(incomingViewKey, diversifier);
}

spark::Coin getCoinFromMeta(const CSparkMintMeta& meta, const spark::IncomingViewKey& incomingViewKey)
{
    const auto* params = spark::Params::get_default();
    spark::Address address(incomingViewKey, meta.i);
    return spark::Coin(params, meta.type, meta.k, address, meta.v, meta.memo, meta.serial_context);
}

spark::IdentifiedCoinData identifyCoin(spark::Coin coin, const spark::IncomingViewKey& incoming_view_key)
{
    return coin.identify(incoming_view_key);
}

void getSparkSpendScripts(const spark::FullViewKey& fullViewKey,
                          const spark::SpendKey& spendKey,
                          const std::vector<spark::InputCoinData>& inputs,
                          const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data,
                          const std::map<uint64_t, uint256>& idAndBlockHashes,
                          CAmount fee,
                          uint64_t transparentOut,
                          const std::vector<spark::OutputCoinData>& privOutputs,
                          std::vector<uint8_t>& inputScript, std::vector<std::vector<unsigned char>>& outputScripts)
{
    inputScript.clear();
    outputScripts.clear();
    const auto* params = spark::Params::get_default();
    spark::SpendTransaction spendTransaction(params, fullViewKey, spendKey, inputs, cover_set_data, fee, transparentOut, privOutputs);
    spendTransaction.setBlockHashes(idAndBlockHashes);
    CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
    serialized << spendTransaction;

    inputScript = std::vector<uint8_t>(serialized.begin(), serialized.end());

    const std::vector<spark::Coin>& outCoins = spendTransaction.getOutCoins();
    for (auto& outCoin : outCoins) {
        // construct spend script
        CDataStream serialized(SER_NETWORK, PROTOCOL_VERSION);
        serialized << outCoin;
        std::vector<unsigned char> script;
        script.insert(script.end(), serialized.begin(), serialized.end());
        outputScripts.emplace_back(script);
    }
}

void ParseSparkMintTransaction(const std::vector<CScript>& scripts, spark::MintTransaction& mintTransaction)
{
    std::vector<CDataStream> serializedCoins;
    for (const auto& script : scripts) {
        if (!script.IsSparkMint())
            throw std::invalid_argument("Script is not a Spark mint");

        std::vector<unsigned char> serialized(script.begin() + 1, script.end());
        size_t size = spark::Coin::memoryRequired() + 8; // 8 is the size of uint64_t
        if (serialized.size() < size) {
            throw std::invalid_argument("Script is not a valid Spark mint");
        }

        CDataStream stream(
                std::vector<unsigned char>(serialized.begin(), serialized.end()),
                SER_NETWORK,
                PROTOCOL_VERSION
        );

        serializedCoins.push_back(stream);
    }
    try {
        mintTransaction.setMintTransaction(serializedCoins);
    } catch (...) {
        throw std::invalid_argument("Unable to deserialize Spark mint transaction");
    }
}

void ParseSparkMintCoin(const CScript& script, spark::Coin& txCoin)
{
    if (!script.IsSparkMint() && !script.IsSparkSMint())
        throw std::invalid_argument("Script is not a Spark mint");

    if (script.size() < 213) {
        throw std::invalid_argument("Script is not a valid Spark Mint");
    }

    std::vector<unsigned char> serialized(script.begin() + 1, script.end());
    CDataStream stream(
            std::vector<unsigned char>(serialized.begin(), serialized.end()),
            SER_NETWORK,
            PROTOCOL_VERSION
    );

    try {
        stream >> txCoin;
    } catch (...) {
        throw std::invalid_argument("Unable to deserialize Spark mint");
    }
}

CSparkMintMeta getMetadata(const spark::Coin& coin, const spark::IncomingViewKey& incoming_view_key) {
    CSparkMintMeta meta;
    spark::IdentifiedCoinData identifiedCoinData;
    try {
        identifiedCoinData = identifyCoin(coin, incoming_view_key);
    } catch (...) {
        return meta;
    }

    meta.isUsed = false;
    meta.v = identifiedCoinData.v;
    meta.memo = identifiedCoinData.memo;
    meta.d = identifiedCoinData.d;
    meta.i = identifiedCoinData.i;
    meta.k = identifiedCoinData.k;
    meta.serial_context = {};

    return meta;
}

spark::InputCoinData getInputData(spark::Coin coin, const spark::FullViewKey& full_view_key, const spark::IncomingViewKey& incoming_view_key)
{
    spark::InputCoinData inputCoinData;
    spark::IdentifiedCoinData identifiedCoinData;
    try {
        identifiedCoinData = identifyCoin(coin, incoming_view_key);
    } catch (...) {
        return inputCoinData;
    }

    spark::RecoveredCoinData recoveredCoinData = coin.recover(full_view_key, identifiedCoinData);
    inputCoinData.T = recoveredCoinData.T;
    inputCoinData.s = recoveredCoinData.s;
    inputCoinData.k = identifiedCoinData.k;
    inputCoinData.v = identifiedCoinData.v;

    return inputCoinData;
}

spark::InputCoinData getInputData(std::pair<spark::Coin, CSparkMintMeta> coin, const spark::FullViewKey& full_view_key)
{
    spark::IdentifiedCoinData identifiedCoinData;
    identifiedCoinData.k = coin.second.k;
    identifiedCoinData.v = coin.second.v;
    identifiedCoinData.d = coin.second.d;
    identifiedCoinData.memo = coin.second.memo;
    identifiedCoinData.i = coin.second.i;

    spark::RecoveredCoinData recoveredCoinData = coin.first.recover(full_view_key, identifiedCoinData);
    spark::InputCoinData inputCoinData;
    inputCoinData.T = recoveredCoinData.T;
    inputCoinData.s = recoveredCoinData.s;
    inputCoinData.k = identifiedCoinData.k;
    inputCoinData.v = identifiedCoinData.v;

    return inputCoinData;
}

#define EXPORT_DART extern "C" __attribute__((visibility("default"))) __attribute__((used))
#ifdef _WIN32
    #define EXPORT_DART extern "C" __declspec(dllexport)
#endif

/*
 * FFI-friendly wrapper for spark::createSpendKeyFromData.
 */
EXPORT_DART
const char* createFullViewKey(const char* keyData, int index) {
    try {
        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
        spark::FullViewKey fullViewKey(spendKey);

        // Serialize the FullViewKey.
        std::string serializedKey = serializeFullViewKey(fullViewKey);

        // Cast the string to an FFI-friendly char*.
        char* result = new char[serializedKey.size() + 1];
        std::copy(serializedKey.begin(), serializedKey.end(), result);
        result[serializedKey.size()] = '\0'; // Null-terminate the C string.

        return result;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

/*
 * FFI-friendly wrapper for spark::createSpendKeyFromData.
 */
EXPORT_DART
const char* createIncomingViewKey(const char* keyData, int index) {
    try {
        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);

        // Serialize the FullViewKey.
        std::string serializedKey = serializeIncomingViewKey(incomingViewKey);

        // Cast the string to an FFI-friendly char*.
        char* result = new char[serializedKey.size() + 1];
        std::copy(serializedKey.begin(), serializedKey.end(), result);
        result[serializedKey.size()] = '\0'; // Null-terminate the C string.

        return result;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

/*
 * FFI-friendly wrapper for spark::getAddress.
 */
EXPORT_DART
const char* getAddress(const char* keyDataHex, int index, int diversifier, int isTestNet) {
    try {
        // Use the hex string directly to create the SpendKey.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);

        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);
        spark::Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

        // Encode the Address object into a string using the appropriate network.
        std::string encodedAddress = address.encode(isTestNet ? spark::ADDRESS_NETWORK_TESTNET : spark::ADDRESS_NETWORK_MAINNET);

        // Allocate memory for the C-style string.
        char* cstr = new char[encodedAddress.length() + 1];
        std::strcpy(cstr, encodedAddress.c_str());

        return cstr;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}

/*
 * FFI-friendly wrapper for spark::identifyCoin.
 */
EXPORT_DART
const char* identifyCoin(const char* coinDataHex, const char* keyDataHex, int index, int diversifier) {
    try {
        // Use the hex string directly to create the SpendKey.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);

        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);
        spark::Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

        // Use the hex string directly to create the Coin.
        spark::Coin coin = createCoinFromData(coinDataHex, address);

        // Identify the Coin.
        spark::IdentifiedCoinData identifiedCoinData = identifyCoin(coin, incomingViewKey);

        // Serialize the IdentifiedCoinData.
        std::string serializedData = serializeIdentifiedCoinData(identifiedCoinData);

        // Cast the string to an FFI-friendly char*.
        char* result = new char[serializedData.size() + 1];
        std::copy(serializedData.begin(), serializedData.end(), result);
        result[serializedData.size()] = '\0'; // Null-terminate the C string.

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}

/*
 * FFI-friendly wrapper for spark::createSparkMintRecipients.
 */
EXPORT_DART
const char* createSparkMintRecipients(const char* keyDataHex, int index, int diversifier, int isTestNet, const char* outputsJson) {
    try {
        // Use the hex string directly to create the SpendKey.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);

        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);
        spark::Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

        // Use the hex string directly to create the Coin.
        std::vector<spark::MintedCoinData> outputs = createMintedCoinDataFromJson(outputsJson, address);

        // Create the recipients.
        std::vector<CRecipient> recipients = createSparkMintRecipients(outputs, {}, false);

        // Serialize the recipients.
        std::string serializedData = serializeRecipients(recipients);

        // Cast the string to an FFI-friendly char*.
        char* result = new char[serializedData.size() + 1];
        std::copy(serializedData.begin(), serializedData.end(), result);
        result[serializedData.size()] = '\0'; // Null-terminate the C string.

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}

/*
 * FFI-friendly wrapper for spark::createSparkSpendTransaction.
 */
EXPORT_DART
const char* createSparkSpendTransaction(const char* keyDataHex, int index, int diversifier, int isTestNet, const char* recipientsJson, const char* privateRecipientsJson, const char* coinsJson, const char* coverSetDataJson, const char* idAndBlockHashesJson, const char* txHashSigHex) {
    try {
        // Use the hex string directly to create the SpendKey.
        spark::SpendKey spendKey = createSpendKeyFromData(keyDataHex, index);

        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);
        spark::Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

        // Use the hex string directly to create the Coin.
        std::vector<std::pair<CAmount, bool>> recipients = createRecipientsFromJson(recipientsJson);
        std::vector<std::pair<spark::OutputCoinData, bool>> privateRecipients = createPrivateRecipientsFromJson(privateRecipientsJson);
        std::list<CSparkMintMeta> coins = createCoinsFromJson(coinsJson);
        std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data = createCoverSetDataFromJson(coverSetDataJson);
        std::map<uint64_t, uint256> idAndBlockHashes = createIdAndBlockHashesFromJson(idAndBlockHashesJson);
        uint256 txHashSig = createTxHashSigFromData(txHashSigHex);

        // Create the transaction.
        CAmount fee;
        std::vector<uint8_t> serializedSpend;
        std::vector<std::vector<unsigned char>> outputScripts;
        createSparkSpendTransaction(spendKey, fullViewKey, incomingViewKey, recipients, privateRecipients, coins, cover_set_data, idAndBlockHashes, txHashSig, fee, serializedSpend, outputScripts);

        // Serialize the transaction.
        std::string serializedData = serializeSpendTransaction(serializedSpend, outputScripts);

        // Cast the string to an FFI-friendly char*.
        char*
        result = new char[serializedData.size() + 1];
        std::copy(serializedData.begin(), serializedData.end(), result);
        result[serializedData.size()] = '\0'; // Null-terminate the C string.

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    }
}

/*
 * Utility function for converting a hex string to a vector of bytes.
 */
unsigned char *hexToBytes(const char *hexstr) {
    size_t length = strlen(hexstr) / 2;
    auto *chrs = (unsigned char *) malloc((length + 1) * sizeof(unsigned char));
    for (size_t i = 0, j = 0; j < length; i += 2, j++) {
        chrs[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i + 1] % 32 + 9) % 25;
    }
    chrs[length] = '\0';
    return chrs;
}

/*
 * Utility function for converting a hex string to a vector of bytes.
 */
std::vector<unsigned char> hexToBytes(std::string hexstr) {
    size_t length = hexstr.length() / 2;
    std::vector<unsigned char> bytes(length);
    for (size_t i = 0, j = 0; j < length; i += 2, j++) {
        bytes[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i + 1] % 32 + 9) % 25;
    }
    return bytes;
}

/*
 * Utility function for converting a hex string to a vector of bytes.
 */
std::vector<unsigned char> hexToBytes(unsigned char* hexstr) {
    size_t length = strlen((const char*) hexstr) / 2;
    std::vector<unsigned char> bytes(length);
    for (size_t i = 0, j = 0; j < length; i += 2, j++) {
        bytes[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i + 1] % 32 + 9) % 25;
    }
    return bytes;
}

/*
 * Utility function for converting a hex string to a vector of bytes.
 */
std::vector<unsigned char> hexToBytes(const char* hexstr) {
    size_t length = strlen(hexstr) / 2;
    std::vector<unsigned char> bytes(length);
    for (size_t i = 0, j = 0; j < length; i += 2, j++) {
        bytes[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i + 1] % 32 + 9) % 25;
    }
    return bytes;
}

/*
 * Utility function for converting unsigned bytes to a hex string.
 */
const char *bytesToHex(const unsigned char *bytes, int size) {
    std::string str;
    for (int i = 0; i < size; ++i) {
        const unsigned char ch = bytes[i];
        str.append(&hexArray[(ch & 0xF0) >> 4], 1);
        str.append(&hexArray[ch & 0xF], 1);
    }
    char *new_str = new char[std::strlen(str.c_str()) + 1];
    std::strcpy(new_str, str.c_str());
    return new_str;
}

/*
 * Utility function for converting bytes to a hex string.
 */
const char *bytesToHex(const char *bytes, int size) {
    std::string str;
    for (int i = 0; i < size; ++i) {
        const unsigned char ch = (const unsigned char) bytes[i];
        str.append(&hexArray[(ch & 0xF0) >> 4], 1);
        str.append(&hexArray[ch & 0xF], 1);
    }
    char *new_str = new char[std::strlen(str.c_str()) + 1];
    std::strcpy(new_str, str.c_str());
    return new_str;
}

/*
 * Utility function for converting a bytes vector to a hex string.
 */
const char *bytesToHex(std::vector<unsigned char> bytes, int size) {
    std::string str;
    for (int i = 0; i < size; ++i) {
        const unsigned char ch = bytes[i];
        str.append(&hexArray[(ch & 0xF0) >> 4], 1);
        str.append(&hexArray[ch & 0xF], 1);
    }
    char *new_str = new char[std::strlen(str.c_str()) + 1];
    std::strcpy(new_str, str.c_str());
    return new_str;
}

/*
 * Utility function for converting a bytes vector to a hex string.
 */
std::string bytesToHex(std::vector<unsigned char> bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < bytes.size(); ++i) {
        ss << std::setw(2) << (int) bytes[i];
    }
    return ss.str();
}

/*
 * Utility function for creating a SpendKey from a hex string.
 */
spark::SpendKey createSpendKeyFromData(const char* keyDataHex, int index) {
    // Convert the hex string to a vector of bytes.
    std::vector<unsigned char> keyData = hexToBytes(keyDataHex);

    // Create the SpendKey.
    spark::SpendKeyData data(keyData);
    data.setIndex(index);
    return createSpendKey(data);
}

/*
 * Utility function for creating a Coin from a hex string.
 */
spark::Coin createCoinFromData(const char* coinDataHex, spark::Address address) {
    // Convert the hex string to a vector of bytes.
    std::vector<unsigned char> coinData = hexToBytes(coinDataHex);

    // Create the Coin.
    spark::Coin coin(coinData, address);
    return coin;
}

/*
 * Utility function for creating a TxHashSig from a hex string.
 */
uint256 createTxHashSigFromData(const char* txHashSigHex) {
    // Convert the hex string to a vector of bytes.
    std::vector<unsigned char> txHashSigData = hexToBytes(txHashSigHex);

    // Create the TxHashSig.
    uint256 txHashSig;
    std::copy(txHashSigData.begin(), txHashSigData.end(), txHashSig.begin());
    return txHashSig;
}

#include "../json/single_include/nlohmann/json.hpp"

/*
 * Utility function for creating a vector of Recipients from a JSON string.
 */
std::vector<std::pair<CAmount, bool>> createRecipientsFromJson(const char* recipientsJson) {
    // Parse the JSON string.
    std::string json(recipientsJson);
    std::vector<std::pair<CAmount, bool>> recipients = parseRecipients(json);

    return recipients;
}

/*
 * Utility function for creating a vector of PrivateRecipients from a JSON string.
 */
std::vector<std::pair<spark::OutputCoinData, bool>> createPrivateRecipientsFromJson(const char* privateRecipientsJson) {
    // Parse the JSON string.
    std::string json(privateRecipientsJson);
    std::vector<std::pair<spark::OutputCoinData, bool>> privateRecipients = parsePrivateRecipients(json);

    return privateRecipients;
}

/*
 * Utility function for creating a list of Coins from a JSON string.
 */
std::list<CSparkMintMeta> createCoinsFromJson(const char* coinsJson) {
    // Parse the JSON string.
    std::string json(coinsJson);
    std::list<CSparkMintMeta> coins = parseCoins(json);

    return coins;
}

/*
 * Utility function for creating a vector of MintedCoinData from a JSON string.
 */
std::vector<spark::MintedCoinData> createMintedCoinDataFromJson(const char* outputsJson, spark::Address address) {
    // Parse the JSON string.
    std::string json(outputsJson);
    std::vector<spark::MintedCoinData> outputs = parseMintedCoinData(json, address);

    return outputs;
}

/*
 * Utility function for creating a map of CoverSetData from a JSON string.
 */
std::unordered_map<uint64_t, spark::CoverSetData> createCoverSetDataFromJson(const char* coverSetDataJson) {
    // Parse the JSON string.
    std::string json(coverSetDataJson);
    std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data = parseCoverSetData(json);

    return cover_set_data;
}

/*
 * Utility function for creating a map of IdAndBlockHashes from a JSON string.
 */
std::map<uint64_t, uint256> createIdAndBlockHashesFromJson(const char* idAndBlockHashesJson) {
    // Parse the JSON string.
    std::string json(idAndBlockHashesJson);
    std::map<uint64_t, uint256> id_and_block_hashes = parseIdAndBlockHashes(json);

    return id_and_block_hashes;
}

/*
 * Utility function for serializing a FullViewKey.
 */
std::string serializeFullViewKey(spark::FullViewKey fullViewKey) {
    // Serialize the FullViewKey.
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << fullViewKey;

    // Convert the serialized FullViewKey to a hex string.
    std::string serializedKey = bytesToHex(std::vector<unsigned char>(stream.begin(), stream.end()));

    return serializedKey;
}

/*
 * Utility function for serializing an IncomingViewKey.
 */
std::string serializeIncomingViewKey(spark::IncomingViewKey incomingViewKey) {
    // Serialize the IncomingViewKey.
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << incomingViewKey;

    // Convert the serialized IncomingViewKey to a hex string.
    std::string serializedKey = bytesToHex(std::vector<unsigned char>(stream.begin(), stream.end()));

    return serializedKey;
}

/*
 * Utility function for serializing an IdentifiedCoinData.
 */
std::string serializeIdentifiedCoinData(spark::IdentifiedCoinData identifiedCoinData) {
    // Serialize the IdentifiedCoinData.
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << identifiedCoinData;

    // Convert the serialized IdentifiedCoinData to a hex string.
    std::string serializedData = bytesToHex(std::vector<unsigned char>(stream.begin(), stream.end()));

    return serializedData;
}

/*
 * Utility function for serializing a SpendTransaction.
 */
std::string serializeSpendTransaction(std::vector<uint8_t> serializedSpend, std::vector<std::vector<unsigned char>> outputScripts) {
    // Serialize the SpendTransaction.
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << serializedSpend;

    // Convert the serialized SpendTransaction to a hex string.
    std::string serializedData = bytesToHex(std::vector<unsigned char>(stream.begin(), stream.end()));

    // Serialize the output scripts.
    for (auto& outputScript : outputScripts) {
        CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
        stream << outputScript;

        // Convert the serialized output script to a hex string.
        std::string serializedOutputScript = bytesToHex(std::vector<unsigned char>(stream.begin(), stream.end()));

        // Append the serialized output script to the SpendTransaction.
        serializedData += serializedOutputScript;
    }

    return serializedData;
}

/*
 * Utility function for serializing a vector of Recipients.
 */
std::string serializeRecipients(std::vector<CRecipient> recipients) {
    // Serialize the Recipients.
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << recipients;

    // Convert the serialized Recipients to a hex string.
    std::string serializedData = bytesToHex(std::vector<unsigned char>(stream.begin(), stream.end()));

    return serializedData;
}

/*
 * Utility function for parsing a JSON string into a vector of Recipients.
 */
std::vector<std::pair<CAmount, bool>> parseRecipients(std::string json) {
    // Parse the JSON string.
    std::vector<std::pair<CAmount, bool>> recipients;
    try {
        nlohmann::json j = nlohmann::json::parse(json);

        // Iterate over the JSON array.
        for (auto& recipient : j) {
            // Parse the recipient.
            CAmount amount = recipient["amount"];
            bool subtractFee = recipient["subtractFee"];

            // Add the recipient to the vector.
            recipients.emplace_back(amount, subtractFee);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return recipients;
}

/*
 * Utility function for parsing a JSON string into a vector of PrivateRecipients.
 */
std::vector<std::pair<spark::OutputCoinData, bool>> parsePrivateRecipients(std::string json) {
    // Parse the JSON string.
    std::vector<std::pair<spark::OutputCoinData, bool>> privateRecipients;
    try {
        nlohmann::json j = nlohmann::json::parse(json);

        // Iterate over the JSON array.
        for (auto& privateRecipient : j) {
            // Parse the private recipient.
            spark::OutputCoinData outputCoinData;
            outputCoinData.v = privateRecipient["amount"];
            outputCoinData.address = spark::Address(privateRecipient["address"]);
            outputCoinData.memo = privateRecipient["memo"];
            bool subtractFee = privateRecipient["subtractFee"];

            // Add the private recipient to the vector.
            privateRecipients.emplace_back(outputCoinData, subtractFee);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return privateRecipients;
}

/*
 * Utility function for parsing a JSON string into a list of Coins.
 */
std::list<CSparkMintMeta> parseCoins(std::string json) {
    // Parse the JSON string.
    std::list<CSparkMintMeta> coins;
    try {
        nlohmann::json j = nlohmann::json::parse(json);

        // Iterate over the JSON array.
        for (auto& coin : j) {
            // Parse the coin.
            CSparkMintMeta meta;
            meta.isUsed = false;
            meta.v = coin["amount"];
            meta.memo = coin["memo"];
            meta.d = coin["diversifier"];
            meta.i = coin["index"];
            meta.k = coin["k"];
            meta.serial_context = {};

            // Add the coin to the list.
            coins.push_back(meta);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return coins;
}

/*
 * Utility function for parsing a JSON string into a vector of MintedCoinData.
 */
std::vector<spark::MintedCoinData> parseMintedCoinData(std::string json, spark::Address address) {
    // Parse the JSON string.
    std::vector<spark::MintedCoinData> outputs;
    try {
        nlohmann::json j = nlohmann::json::parse(json);

        // Iterate over the JSON array.
        for (auto& output : j) {
            // Parse the output.
            spark::MintedCoinData outputCoinData;
            outputCoinData.v = output["amount"];
            outputCoinData.address = address;
            outputCoinData.memo = output["memo"];

            // Add the output to the vector.
            outputs.push_back(outputCoinData);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return outputs;
}

/*
 * Utility function for parsing a JSON string into a map of CoverSetData.
 */
std::unordered_map<uint64_t, spark::CoverSetData> parseCoverSetData(std::string json) {
    // Parse the JSON string.
    std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data;
    try {
        nlohmann::json j = nlohmann::json::parse(json);

        // Iterate over the JSON object.
        for (auto& coverSetData : j.items()) {
            // Parse the cover set data.
            uint64_t groupId = coverSetData.key();
            std::vector<spark::Coin> cover_set;
            for (auto& coin : coverSetData.value()["cover_set"]) {
                spark::Coin c = createCoinFromData(coin, spark::Address(coverSetData.value()["incoming_view_key"], coverSetData.value()["diversifier"]));
                cover_set.push_back(c);
            }
            spark::CoverSetData data;
            data.cover_set = cover_set;
            data.incoming_view_key = coverSetData.value()["incoming_view_key"];
            data.diversifier = coverSetData.value()["diversifier"];

            // Add the cover set data to the map.
            cover_set_data[groupId] = data;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return cover_set_data;
}

/*
 * Utility function for parsing a JSON string into a map of IdAndBlockHashes.
 */
std::map<uint64_t, uint256> parseIdAndBlockHashes(std::string json) {
    // Parse the JSON string.
    std::map<uint64_t, uint256> id_and_block_hashes;
    try {
        nlohmann::json j = nlohmann::json::parse(json);

        // Iterate over the JSON object.
        for (auto& idAndBlockHash : j.items()) {
            // Parse the id and block hash.
            uint64_t groupId = idAndBlockHash.key();
            uint256 blockHash = createTxHashSigFromData(idAndBlockHash.value());

            // Add the id and block hash to the map.
            id_and_block_hashes[groupId] = blockHash;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return id_and_block_hashes;
}
