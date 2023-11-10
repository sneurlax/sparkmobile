#include "utils.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#include "../json/single_include/nlohmann/json.hpp"

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
