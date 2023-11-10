#ifndef ORG_FIRO_SPARK_UTILS_H
#define ORG_FIRO_SPARK_UTILS_H

#include "../include/spark.h"

char const hexArray[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
						   'e', 'f'};

unsigned char *hexToBytes(const char *str);

const char *bytesToHex(const unsigned char *bytes, int size);

const char *bytesToHex(const char *bytes, int size);

const char *bytesToHex(std::vector<unsigned char> bytes, int size);

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

#endif //ORG_FIRO_SPARK_UTILS_H
