#include "utils.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

/// Utility function to generate an address from keyData, index, and a diversifier.
const char* getAddressFromData(const char* keyData, int index, const uint64_t diversifier) {
	try {
		spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
		spark::FullViewKey fullViewKey(spendKey);
		spark::IncomingViewKey incomingViewKey(fullViewKey);
		spark::Address address(incomingViewKey, diversifier);

		// Encode the Address object into a string.
		std::string encodedAddress = address.encode(spark::ADDRESS_NETWORK_TESTNET);

		// Allocate memory for the C-style string and return it.
		char* result = new char[encodedAddress.size() + 1];
		std::copy(encodedAddress.begin(), encodedAddress.end(), result);
		result[encodedAddress.size()] = '\0'; // Null-terminate the C string.

		return result;
	} catch (const std::exception& e) {
		return nullptr;
	}
}

/// Utility function to generate SpendKey from keyData and index.
spark::SpendKey createSpendKeyFromData(const char *keyData, int index) {
    try {
        // Convert the keyData from hex string to binary
        unsigned char* key_data_bin = hex2bin(keyData);

        const SpendKeyData *data = new SpendKeyData(key_data_bin, index);

        return createSpendKey(*data);
    } catch (const std::exception& e) {
        // We can't return here, so just throw the exception again.
        throw e;
    }
}

unsigned char *hex2bin(const char *hexstr) {
	size_t length = strlen(hexstr) / 2;
	auto *chrs = (unsigned char *) malloc((length + 1) * sizeof(unsigned char));
	for (size_t i = 0, j = 0; j < length; i += 2, j++) {
		chrs[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i + 1] % 32 + 9) % 25;
	}
	chrs[length] = '\0';
	return chrs;
}

const char *bin2hex(const unsigned char *bytes, int size) {
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

const char *bin2hex(const char *bytes, int size) {
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

const char *bin2hex(std::vector<unsigned char> bytes, int size) {
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
