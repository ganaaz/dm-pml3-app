#ifndef EMVCONFIG_H_
#define EMVCONFIG_H_

#define FEIG_ID_TRACE_TAG "\xDF\xFE\xF0\x01"

typedef struct ENCRYPT_STRUCT
{
    char type[10];
    char key[10];
    char name[255];
    char tags[50];

} ENCRYPT;

typedef struct KEYS_STRUCT
{
    char keyIndex[03];
    char modulus[1024 * 2];
    char exponent[03];
    char aid[128];
} KEYS;

typedef struct ONLINE_TAGS_STRUCT
{
    char tags[512];
    char kernelId[3];
} ONLINE_TAGS;

typedef struct ENTRYPOINT_STRUCT
{
    char cvmRequireLimit[13];
    char clessTxnLimit[13];
    char termFloorLimit[9];
    char zeroAmountAllowed[3];
    char zeroAmountOfflineAllowed[3];
} ENTRYPOINT;

typedef struct KERNEL_CONFIG_STRUCT
{
    char terminalType[3];
    char serviceQualifier[50];
    char terminalId[17];
    char terminalFloorLimit[9];
    char appVersion[5];
    char tacDenial[11];
    char tacOnline[11];
    char tacDefault[11];
    char countryCode[5];
    char termCap[7];
    char addTermCap[11];
    char mcc[97];
    char serviceDataFormat[129];
    char addTermCapExt[11];
    char appUnblockSupport[3];
    char tdol[129];
    char trmData[17];
    char tcCategoryCode[3];
    char kernelConfiguration[3];
    char cvmCapCVM[3];
    char cvmCapNoCVM[3];
    char secCap[3];

} KERNEL_CONFIG;

typedef struct COMBINATIONS_STRUCT
{
    ENTRYPOINT entryPoint;
    char aid[128];
    char partial[3];
    char kernel[3];
    char txnType[3];
    KERNEL_CONFIG kernelConfig;

} COMBINATIONS;

typedef struct EMV_STRUCT
{
    KEYS **keyList;
    int keysLen;
    ONLINE_TAGS onlineTags;
    ONLINE_TAGS onlineTagsMasterCard;
    ONLINE_TAGS onlineTagsVisa;
    COMBINATIONS **combinations;
    int combinationsLen;

} EMV;

typedef struct EMV_CONFIG_STRUCT
{
    char traceEnabled[3];
    ENCRYPT **encryptList;
    int encryptLen;
    EMV emv;

} EMV_CONFIG;

const char *getString(json_object *jObject, const char *name);

struct tlv *parseEmvConfigFile();

EMV_CONFIG loadEncrypts(json_object *jObject, EMV_CONFIG emvConfig);

EMV loadKeys(json_object *jObject, EMV emv);

ONLINE_TAGS loadOnlineTags(json_object *jObject);

ONLINE_TAGS loadOnlineTagsMasterCard(json_object *jObject);

ONLINE_TAGS loadOnlineTagsVisa(json_object *jObject);

EMV loadCombinations(json_object *jObject, EMV emv);

ENTRYPOINT loadEntryPoint(json_object *combObject);

KERNEL_CONFIG loadKernelConfig(json_object *combObject);

void printEmvConfig(EMV_CONFIG emvConfig);

void generateTlv(EMV_CONFIG emvConfig);

struct tlv *generateEmvCoConfig(EMV_CONFIG emvConfig);

struct tlv *generateKeyTlv(KEYS *key);

struct tlv *generateEntryPoint(ENTRYPOINT entryPoint);

struct tlv *generateKernelConfig(KERNEL_CONFIG kernelConfig);

struct tlv *generateCombination(COMBINATIONS combination);

struct tlv *generateAllCombination(EMV emv);

struct tlv *generateOnlineTags(ONLINE_TAGS onlineTags);

struct tlv *generatePaymentConfig(EMV_CONFIG emvConfig);

#endif