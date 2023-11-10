#include "../include/spark.h"
#include "utils.h"

#include <cstring>
#include <iostream> // Just for printing.

#define EXPORT_DART extern "C" __attribute__((visibility("default"))) __attribute__((used))
#ifdef _WIN32
    #define EXPORT_DART extern "C" __declspec(dllexport)
#endif

using namespace spark;

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
