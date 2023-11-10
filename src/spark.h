#ifndef FIRO_LIBSPARK_SPARK_H
#define FIRO_LIBSPARK_SPARK_H
#include "../bitcoin/script.h"
#include "../bitcoin/amount.h"
#include "../src/primitives.h"
#include "../src/coin.h"
#include "../src/mint_transaction.h"
#include "../src/spend_transaction.h"
#include <list>

std::pair<CAmount, std::vector<CSparkMintMeta>> SelectSparkCoins(CAmount required, bool subtractFeeFromAmount, std::list<CSparkMintMeta> coins, std::size_t mintNum);

bool GetCoinsToSpend(
        CAmount required,
        std::vector<CSparkMintMeta>& coinsToSpend_out,
        std::list<CSparkMintMeta> coins,
        int64_t& changeToMint);


void getSparkSpendScripts(const spark::FullViewKey& fullViewKey,
                          const spark::SpendKey& spendKey,
                          const std::vector<spark::InputCoinData>& inputs,
                          const std::unordered_map<uint64_t, spark::CoverSetData> cover_set_data,
                          const std::map<uint64_t, uint256>& idAndBlockHashes,
                          CAmount fee,
                          uint64_t transparentOut,
                          const std::vector<spark::OutputCoinData>& privOutputs,
                          std::vector<uint8_t>& inputScript, std::vector<std::vector<unsigned char>>& outputScripts);


void ParseSparkMintTransaction(const std::vector<CScript>& scripts, spark::MintTransaction& mintTransaction);
void ParseSparkMintCoin(const CScript& script, spark::Coin& txCoin);

#define EXPORT_DART __attribute__((visibility("default"))) __attribute__((used))
#ifdef __cplusplus
#define EXPORT_DART extern "C" __attribute__((visibility("default"))) __attribute__((used))
#endif
#ifdef _WIN32
#define EXPORT_DART __declspec(dllexport)
#endif

EXPORT_DART
const char *createFullViewKey(const char* keyData, int index);

EXPORT_DART
const char* createIncomingViewKey(const char* keyData, int index);

EXPORT_DART
const char* getAddress(const char* keyDataHex, int index, int diversifier, int isTestNet);

EXPORT_DART
const char* identifyCoin(const char* coinDataHex, const char* keyDataHex, int index, int diversifier);

EXPORT_DART
const char* createSparkMintRecipients(const char* keyDataHex, int index, int diversifier, int isTestNet, const char* outputsJson);

EXPORT_DART
const char* createSparkSpendTransaction(const char* keyDataHex, int index, int diversifier, int isTestNet, const char* recipientsJson, const char* privateRecipientsJson, const char* coinsJson, const char* coverSetDataJson, const char* idAndBlockHashesJson, const char* txHashSigHex);

char const hexArray[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
                           'e', 'f'};

unsigned char *hexToBytes(const char *str);

std::vector<unsigned char> hexToBytes(unsigned char* hexstr);

std::vector<unsigned char> hexToBytes(std::string hexstr);

const char *bytesToHex(const unsigned char *bytes, int size);

const char *bytesToHex(const char *bytes, int size);

const char *bytesToHex(std::vector<unsigned char> bytes, int size);

std::string bytesToHex(std::vector<unsigned char> bytes);

spark::SpendKey createSpendKeyFromData(const char* keyDataHex, int index);

spark::Coin createCoinFromData(const char* coinDataHex, spark::Address address);

uint256 createTxHashSigFromData(const char* txHashSigHex);

std::vector<std::pair<CAmount, bool>> createRecipientsFromJson(const char* recipientsJson);

std::vector<std::pair<spark::OutputCoinData, bool>> createPrivateRecipientsFromJson(const char* privateRecipientsJson);

std::list<CSparkMintMeta> createCoinsFromJson(const char* coinsJson);

std::vector<spark::MintedCoinData> createMintedCoinDataFromJson(const char* outputsJson, spark::Address address);

std::unordered_map<uint64_t, spark::CoverSetData> createCoverSetDataFromJson(const char* coverSetDataJson);

std::map<uint64_t, uint256> createIdAndBlockHashesFromJson(const char* idAndBlockHashesJson);

std::string serializeFullViewKey(spark::FullViewKey fullViewKey);

std::string serializeIncomingViewKey(spark::IncomingViewKey incomingViewKey);

std::string serializeIdentifiedCoinData(spark::IdentifiedCoinData identifiedCoinData);

std::string serializeSpendTransaction(std::vector<uint8_t> serializedSpend, std::vector<std::vector<unsigned char>> outputScripts);

std::string serializeRecipients(std::vector<CRecipient> recipients);

std::vector<std::pair<CAmount, bool>> parseRecipients(std::string json);

std::vector<std::pair<spark::OutputCoinData, bool>> parsePrivateRecipients(std::string json);

std::list<CSparkMintMeta> parseCoins(std::string json);

std::vector<spark::MintedCoinData> parseMintedCoinData(std::string json, spark::Address address);

std::unordered_map<uint64_t, spark::CoverSetData> parseCoverSetData(std::string json);

std::map<uint64_t, uint256> parseIdAndBlockHashes(std::string json);

#endif //FIRO_LIBSPARK_SPARK_H
