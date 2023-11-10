#ifndef ORG_FIRO_SPARK_DART_INTERFACE_H
#define ORG_FIRO_SPARK_DART_INTERFACE_H

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

#endif //ORG_FIRO_SPARK_DART_INTERFACE_H
