#include <json-c/json.h>
#include <stdio.h>
#include <string.h>

#include <libpay/tlv.h>
#include <feig/emvco_tags.h>
#include <feig/feig_tags.h>
#include <feig/feig_e2ee_tags.h>
#include <feig/feig_trace_tags.h>
#include <feig/emvco_ep.h>

#include "include/tlvhelper.h"
#include "include/emvconfig.h"
#include "include/logutil.h"
#include "include/commonutil.h"

struct tlv *parseEmvConfigFile()
{
    logInfo("Going to read emv config json file");
    char *buffer;
    long length;
    FILE *file = fopen("config/emv_config.json", "r");
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    logData("File length : %d", length);
    fseek(file, 0, SEEK_SET);
    buffer = (char *)malloc(length);
    if (buffer)
    {
        fread(buffer, 1, length, file);
    }
    fclose(file);

    json_object *jObject = json_tokener_parse(buffer);

    EMV_CONFIG emvConfig;
    strcpy(emvConfig.traceEnabled, getString(jObject, "traceEnabled"));

    emvConfig = loadEncrypts(jObject, emvConfig);
    emvConfig.emv = loadKeys(jObject, emvConfig.emv);
    emvConfig.emv.onlineTags = loadOnlineTags(jObject);
    emvConfig.emv.onlineTagsMasterCard = loadOnlineTagsMasterCard(jObject);
    emvConfig.emv.onlineTagsVisa = loadOnlineTagsVisa(jObject);
    emvConfig.emv = loadCombinations(jObject, emvConfig.emv);

    json_object_put(jObject); // Clear the json memory
    free(buffer);

    logInfo("Config file read and structure updated successfully.");
    // printEmvConfig(emvConfig);

    generateTlv(emvConfig);

    struct tlv *tlvConfig = generatePaymentConfig(emvConfig);
    return tlvConfig;
}

void generateTlv(EMV_CONFIG emvConfig)
{
    struct tlv *tlvConfig = generatePaymentConfig(emvConfig);
    void *data = NULL;
    size_t dataLen = 0;
    serializeTlv(tlvConfig, &data, &dataLen);
    logHexDataNoSpace("Value: ", data, dataLen);
}

struct tlv *generatePaymentConfig(EMV_CONFIG emvConfig)
{
    int traceLen = strlen(emvConfig.traceEnabled) / 2;
    uint8_t trace[traceLen];
    hexToByte(emvConfig.traceEnabled, trace);

    struct tlv *tlvTraceData = tlv_new(FEIG_ID_TRACE_TAG, traceLen, trace);
    void *dataTD = NULL;
    size_t dataTDLen = 0;
    serializeTlv(tlvTraceData, &dataTD, &dataTDLen);

    struct tlv *tlv_emvcoConfig = generateEmvCoConfig(emvConfig);
    void *dataEC = NULL;
    size_t dataECLen = 0;
    serializeTlv(tlv_emvcoConfig, &dataEC, &dataECLen);

    struct tlv *tlv;
    struct tlv *tlvConfig = NULL;
    struct tlv *tlv_tail;

    tlv = tlv_new(FEIG_ID_EMVCO, dataECLen, dataEC);
    tlvConfig = tlv;
    tlv_tail = tlv;

    tlv = tlv_new(FEIG_ID_TRACE_CONFIGURATION, dataTDLen, dataTD);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    void *data = NULL;
    size_t dataLen = 0;
    serializeTlv(tlvConfig, &data, &dataLen);
    struct tlv *tlv_paymentConfig;
    tlv_paymentConfig = tlv_new(FEIG_ID_PAYMENT_SYSTEM_CONFIGURATION, dataLen, data);

    return tlv_paymentConfig;
}

struct tlv *generateEmvCoConfig(EMV_CONFIG emvConfig)
{
    struct tlv *tlv;
    struct tlv *tlv_tail = NULL;
    struct tlv *tlv_emvCoConfig = NULL;

    for (int i = 0; i < emvConfig.emv.keysLen; i++)
    {

        struct tlv *tlv_key = generateKeyTlv(emvConfig.emv.keyList[i]);
        void *data = NULL;
        size_t dataLen = 0;
        serializeTlv(tlv_key, &data, &dataLen);

        tlv = tlv_new(FEIG_ID_CAPK, dataLen, data);

        if (i == 0)
        {
            tlv_emvCoConfig = tlv;
            tlv_tail = tlv;
        }
        else
        {
            tlv_tail = tlv_insert_after(tlv_tail, tlv);
        }
    }

    struct tlv *tlv_combinations = generateAllCombination(emvConfig.emv);
    void *dataEP = NULL;
    size_t dataEPLen = 0;
    serializeTlv(tlv_combinations, &dataEP, &dataEPLen);

    tlv = tlv_new(FEIG_ID_EP, dataEPLen, dataEP);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    struct tlv *tlv_onlineTags = generateOnlineTags(emvConfig.emv.onlineTags);
    void *dataOT = NULL;
    size_t dataOTLen = 0;
    serializeTlv(tlv_onlineTags, &dataOT, &dataOTLen);

    tlv = tlv_new(FEIG_ID_DETL_SET, dataOTLen, dataOT);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    struct tlv *tlv_onlineTagsMc = generateOnlineTags(emvConfig.emv.onlineTagsMasterCard);
    void *dataOTMC = NULL;
    size_t dataOTMCLen = 0;
    serializeTlv(tlv_onlineTagsMc, &dataOTMC, &dataOTMCLen);

    tlv = tlv_new(FEIG_ID_DETL_SET, dataOTMCLen, dataOTMC);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    struct tlv *tlv_onlineTagsVisa = generateOnlineTags(emvConfig.emv.onlineTagsVisa);
    void *dataOTVisa = NULL;
    size_t dataOTVisaLen = 0;
    serializeTlv(tlv_onlineTagsVisa, &dataOTVisa, &dataOTVisaLen);

    tlv = tlv_new(FEIG_ID_DETL_SET, dataOTVisaLen, dataOTVisa);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    return tlv_emvCoConfig;
}

struct tlv *generateAllCombination(EMV emv)
{
    struct tlv *tlv_tail;
    struct tlv *tlv_allCombinations = NULL;

    for (int i = 0; i < emv.combinationsLen; i++)
    {
        struct tlv *tlv_comb = generateCombination(*emv.combinations[i]);
        if (i == 0)
        {
            tlv_allCombinations = tlv_comb;
            tlv_tail = tlv_comb;
        }
        else
        {
            tlv_tail = tlv_insert_after(tlv_tail, tlv_comb);
        }
    }

    return tlv_allCombinations;
}

struct tlv *generateCombination(COMBINATIONS combination)
{
    struct tlv *tlv_entryPoint = generateEntryPoint(combination.entryPoint);
    struct tlv *tlv_kernelConfig = generateKernelConfig(combination.kernelConfig);

    struct tlv *tlv;
    struct tlv *tlv_tail;
    struct tlv *tlv_combSet = NULL;

    void *dataEP = NULL;
    size_t dataLenEP = 0;
    serializeTlv(tlv_entryPoint, &dataEP, &dataLenEP);

    tlv = tlv_new(FEIG_ID_EP_DATA, dataLenEP, dataEP);
    tlv_combSet = tlv;
    tlv_tail = tlv;

    void *dataKC = NULL;
    size_t dataLenKC = 0;
    serializeTlv(tlv_kernelConfig, &dataKC, &dataLenKC);

    tlv = tlv_new(FEIG_ID_KERNEL_CONFIG, dataLenKC, dataKC);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int aidLen = strlen(combination.aid) / 2;
    uint8_t aid[aidLen];
    hexToByte(combination.aid, aid);

    tlv = tlv_new(EMV_ID_APPLICATION_IDENTIFIER_TERMINAL, aidLen, aid);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int appSelLen = strlen(combination.partial) / 2;
    uint8_t appSel[appSelLen];
    hexToByte(combination.partial, appSel);

    tlv = tlv_new(FEIG_ID_CT_APPLICATION_SELECTION_INDICATOR, appSelLen, appSel);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int kernelIdLen = strlen(combination.kernel) / 2;
    uint8_t kernelId[kernelIdLen];
    hexToByte(combination.kernel, kernelId);

    tlv = tlv_new(FEIG_ID_EP_KERNEL_ID, kernelIdLen, kernelId);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int txnTypeLen = strlen(combination.txnType) / 2;
    uint8_t txnType[txnTypeLen];
    hexToByte(combination.txnType, txnType);

    tlv = tlv_new(EMV_ID_TRANSACTION_TYPE, txnTypeLen, txnType);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    struct tlv *tlv_entryConfig = NULL;

    void *dataCS = NULL;
    size_t dataLenCS = 0;
    serializeTlv(tlv_combSet, &dataCS, &dataLenCS);

    tlv_entryConfig = tlv_new(FEIG_ID_EP_COMBINATION, dataLenCS, dataCS);

    return tlv_entryConfig;
}

struct tlv *generateOnlineTags(ONLINE_TAGS onlineTags)
{
    struct tlv *tlv;
    struct tlv *tlv_tail;
    struct tlv *tlv_onlineTags = NULL;

    int kernelIdLen = strlen(onlineTags.kernelId) / 2;
    uint8_t kernelId[kernelIdLen];
    hexToByte(onlineTags.kernelId, kernelId);

    tlv = tlv_new(FEIG_ID_EP_KERNEL_ID, kernelIdLen, kernelId);
    tlv_onlineTags = tlv;
    tlv_tail = tlv;

    int tagsLen = strlen(onlineTags.tags) / 2;
    uint8_t tags[tagsLen];
    hexToByte(onlineTags.tags, tags);

    tlv = tlv_new(FEIG_ID_DETL_EMV, tagsLen, tags);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    return tlv_onlineTags;
}

struct tlv *generateKernelConfig(KERNEL_CONFIG kernelConfig)
{
    struct tlv *tlv;
    struct tlv *tlv_tail;
    struct tlv *tlv_kernelConfig = NULL;

    int appVerLen = strlen(kernelConfig.appVersion) / 2;
    uint8_t appVer[appVerLen];
    hexToByte(kernelConfig.appVersion, appVer);

    tlv = tlv_new(EMV_ID_APPLICATION_VERSION_NUMBER_READER, appVerLen, appVer);
    tlv_kernelConfig = tlv;
    tlv_tail = tlv;

    int tacDenialLen = strlen(kernelConfig.tacDenial) / 2;
    uint8_t tacDenial[tacDenialLen];
    hexToByte(kernelConfig.tacDenial, tacDenial);

    tlv = tlv_new(FEIG_C_ID_TAC_DENIAL, tacDenialLen, tacDenial);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int tacOnlineLen = strlen(kernelConfig.tacOnline) / 2;
    uint8_t tacOnline[tacOnlineLen];
    hexToByte(kernelConfig.tacOnline, tacOnline);

    tlv = tlv_new(FEIG_C_ID_TAC_ONLINE, tacOnlineLen, tacOnline);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int tacDefaultLen = strlen(kernelConfig.tacDefault) / 2;
    uint8_t tacDefault[tacDefaultLen];
    hexToByte(kernelConfig.tacDefault, tacDefault);

    tlv = tlv_new(FEIG_C_ID_TAC_DEFAULT, tacDefaultLen, tacDefault);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int countryCodeLen = strlen(kernelConfig.countryCode) / 2;
    uint8_t countryCode[countryCodeLen];
    hexToByte(kernelConfig.countryCode, countryCode);

    tlv = tlv_new(EMV_ID_TERMINAL_COUNTRY_CODE, countryCodeLen, countryCode);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    if (strlen(kernelConfig.termCap) > 0)
    {
        int terminalCapLen = strlen(kernelConfig.termCap) / 2;
        uint8_t terminalCap[terminalCapLen];
        hexToByte(kernelConfig.termCap, terminalCap);

        tlv = tlv_new(EMV_ID_TERMINAL_CAPABILITIES, terminalCapLen, terminalCap);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    int addTermCapLen = strlen(kernelConfig.addTermCap) / 2;
    uint8_t addTermCap[addTermCapLen];
    hexToByte(kernelConfig.addTermCap, addTermCap);

    tlv = tlv_new(EMV_ID_ADDITIONAL_TERMINAL_CAPABILITIES, addTermCapLen, addTermCap);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int mccLen = strlen(kernelConfig.mcc) / 2;
    uint8_t mcc[mccLen];
    hexToByte(kernelConfig.mcc, mcc);

    tlv = tlv_new(EMV_ID_MERCHANT_CATEGORY_CODE, mccLen, mcc);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int termTypeLen = strlen(kernelConfig.terminalType) / 2;
    uint8_t termType[termTypeLen];
    hexToByte(kernelConfig.terminalType, termType);

    tlv = tlv_new(EMV_ID_TERMINAL_TYPE, termTypeLen, termType);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int termIdLen = strlen(kernelConfig.terminalId) / 2;
    uint8_t termId[termIdLen];
    hexToByte(kernelConfig.terminalId, termId);

    tlv = tlv_new(EMV_ID_TERMINAL_IDENTIFICATION, termIdLen, termId);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    if (strlen(kernelConfig.serviceDataFormat) > 0)
    {
        int sdfLen = strlen(kernelConfig.serviceDataFormat) / 2;
        uint8_t sdf[sdfLen];
        hexToByte(kernelConfig.serviceDataFormat, sdf);

        tlv = tlv_new(C13_ID_SERVICE_DATA_FORMAT, sdfLen, sdf);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    if (strlen(kernelConfig.addTermCapExt) > 0)
    {
        int addTermCapExLen = strlen(kernelConfig.addTermCapExt) / 2;
        uint8_t addTermCapEx[addTermCapExLen];
        hexToByte(kernelConfig.addTermCapExt, addTermCapEx);

        tlv = tlv_new(C13_ID_ADDITIONAL_TERMINAL_CAPABILITIES_EXTENSION, addTermCapExLen, addTermCapEx);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    int floorLimitLen = strlen(kernelConfig.terminalFloorLimit) / 2;
    uint8_t floorLimit[floorLimitLen];
    hexToByte(kernelConfig.terminalFloorLimit, floorLimit);

    tlv = tlv_new(EMV_ID_TERMINAL_FLOOR_LIMIT, floorLimitLen, floorLimit);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int appUnBlockLen = strlen(kernelConfig.appUnblockSupport) / 2;
    uint8_t appUnBlock[appUnBlockLen];
    hexToByte(kernelConfig.appUnblockSupport, appUnBlock);

    tlv = tlv_new(FEIG_C13_ID_APPLICATION_UNBLOCK_SUPPORT, appUnBlockLen, appUnBlock);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int tdolLen = strlen(kernelConfig.tdol) / 2;
    uint8_t tdol[tdolLen];
    hexToByte(kernelConfig.tdol, tdol);

    tlv = tlv_new(FEIG_C_ID_DEFAULT_TDOL, tdolLen, tdol);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    if (strlen(kernelConfig.serviceQualifier) > 0)
    {
        int sqLen = strlen(kernelConfig.serviceQualifier) / 2;
        uint8_t sq[sqLen];
        hexToByte(kernelConfig.serviceQualifier, sq);

        tlv = tlv_new(FEIG_C13_ID_TERMINAL_SERVICE_QUALIFIER, sqLen, sq);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    if (strlen(kernelConfig.trmData) > 0)
    {
        int trmDataLen = strlen(kernelConfig.trmData) / 2;
        uint8_t trmData[trmDataLen];
        hexToByte(kernelConfig.trmData, trmData);

        tlv = tlv_new(EMV_ID_TERMINAL_RISK_MANAGEMENT_DATA, trmDataLen, trmData);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    if (strlen(kernelConfig.tcCategoryCode) > 0)
    {
        int tcCatLen = strlen(kernelConfig.tcCategoryCode) / 2;
        uint8_t tcCat[tcCatLen];
        hexToByte(kernelConfig.tcCategoryCode, tcCat);

        tlv = tlv_new(C2_ID_TRANSACTION_CATEGORY_CODE, tcCatLen, tcCat);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    if (strlen(kernelConfig.kernelConfiguration) > 0)
    {
        int kConfigLen = strlen(kernelConfig.kernelConfiguration) / 2;
        uint8_t kConfig[kConfigLen];
        hexToByte(kernelConfig.kernelConfiguration, kConfig);

        tlv = tlv_new(C2_ID_KERNEL_CONFIGURATION, kConfigLen, kConfig);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    if (strlen(kernelConfig.cvmCapCVM) > 0)
    {
        int cLen = strlen(kernelConfig.cvmCapCVM) / 2;
        uint8_t cData[cLen];
        hexToByte(kernelConfig.cvmCapCVM, cData);

        tlv = tlv_new(C2_ID_CVM_CAPABILITY_CVM_REQUIRED, cLen, cData);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    if (strlen(kernelConfig.cvmCapNoCVM) > 0)
    {
        int cLen = strlen(kernelConfig.cvmCapNoCVM) / 2;
        uint8_t cData[cLen];
        hexToByte(kernelConfig.cvmCapNoCVM, cData);

        tlv = tlv_new(C2_ID_CVM_CAPABILITY_NO_CVM_REQUIRED, cLen, cData);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    if (strlen(kernelConfig.secCap) > 0)
    {
        int cLen = strlen(kernelConfig.secCap) / 2;
        uint8_t cData[cLen];
        hexToByte(kernelConfig.secCap, cData);

        tlv = tlv_new(C2_ID_SECURITY_CAPABILITY, cLen, cData);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    return tlv_kernelConfig;
}

struct tlv *generateEntryPoint(ENTRYPOINT entryPoint)
{
    struct tlv *tlv;
    struct tlv *tlv_tail;
    struct tlv *tlv_entryPoint = NULL;

    int cvmReqLimitLen = strlen(entryPoint.cvmRequireLimit) / 2;
    uint8_t cvmReqLimit[cvmReqLimitLen];
    hexToByte(entryPoint.cvmRequireLimit, cvmReqLimit);

    tlv = tlv_new(FEIG_ID_EP_READER_CVM_REQUIRED_LIMIT, cvmReqLimitLen, cvmReqLimit);
    tlv_entryPoint = tlv;
    tlv_tail = tlv;

    int clessTxnLimitLen = strlen(entryPoint.clessTxnLimit) / 2;
    uint8_t clessTxnLimit[clessTxnLimitLen];
    hexToByte(entryPoint.clessTxnLimit, clessTxnLimit);

    tlv = tlv_new(FEIG_ID_EP_READER_CONTACTLESS_TRANSACTION_LIMIT, clessTxnLimitLen, clessTxnLimit);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    int floorLimitLen = strlen(entryPoint.termFloorLimit) / 2;
    uint8_t floorLimit[floorLimitLen];
    hexToByte(entryPoint.termFloorLimit, floorLimit);

    tlv = tlv_new(EMV_ID_TERMINAL_FLOOR_LIMIT, floorLimitLen, floorLimit);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    if (strlen(entryPoint.zeroAmountAllowed) > 1)
    {
        int zeroAmountLen = strlen(entryPoint.zeroAmountAllowed) / 2;
        uint8_t zeroAmount[zeroAmountLen];
        hexToByte(entryPoint.zeroAmountAllowed, zeroAmount);

        tlv = tlv_new(FEIG_ID_EP_ZERO_AMOUNT_ALLOWED, zeroAmountLen, zeroAmount);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    if (strlen(entryPoint.zeroAmountOfflineAllowed) > 1)
    {
        int zeroOfflineLen = strlen(entryPoint.zeroAmountOfflineAllowed) / 2;
        uint8_t zeroOffline[zeroOfflineLen];
        hexToByte(entryPoint.zeroAmountOfflineAllowed, zeroOffline);

        tlv = tlv_new(FEIG_ID_EP_ZERO_AMOUNT_FOR_OFFLINE_ALLOWED, zeroOfflineLen, zeroOffline);
        tlv_tail = tlv_insert_after(tlv_tail, tlv);
    }

    return tlv_entryPoint;
}

struct tlv *generateKeyTlv(KEYS *key)
{
    struct tlv *tlv;
    struct tlv *tlv_tail;
    struct tlv *tlv_key = NULL;

    uint8_t keyIndex[1];
    hexToByte(key->keyIndex, keyIndex);

    int modulusLen = strlen(key->modulus) / 2;
    uint8_t modulus[modulusLen];
    hexToByte(key->modulus, modulus);

    int ridLen = strlen(key->aid) / 2;
    uint8_t rid[ridLen];
    hexToByte(key->aid, rid);

    int expLen = strlen(key->exponent) / 2;
    uint8_t exponent[expLen];
    hexToByte(key->exponent, exponent);

    tlv = tlv_new(EMV_ID_CA_PUBLIC_KEY_INDEX_TERMINAL, 1, keyIndex);
    tlv_key = tlv;
    tlv_tail = tlv;

    tlv = tlv_new(FEIG_ID_CAPK_MODULUS, modulusLen, modulus);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    tlv = tlv_new(FEIG_ID_CAPK_RID, ridLen, rid);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    tlv = tlv_new(FEIG_ID_CAPK_EXPONENT, expLen, exponent);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    return tlv_key;
}

const char *getString(json_object *jObject, const char *name)
{
    const char *data = json_object_get_string(json_object_object_get(jObject, name));
    if (data == NULL)
        return "\0";

    return data;
}

EMV loadCombinations(json_object *jObject, EMV emv)
{
    json_object *emvObject = json_object_object_get(jObject, "emv");
    json_object *combinationsObject = json_object_object_get(emvObject, "combinations");

    int combinationsLen = json_object_array_length(combinationsObject);
    logData("Length of Combinations : %d", combinationsLen);
    emv.combinationsLen = combinationsLen;
    emv.combinations = malloc(combinationsLen * sizeof(COMBINATIONS));

    json_object *combObject;
    for (int i = 0; i < combinationsLen; i++)
    {
        combObject = json_object_array_get_idx(combinationsObject, i);
        emv.combinations[i] = malloc(sizeof(COMBINATIONS));
        emv.combinations[i]->entryPoint = loadEntryPoint(combObject);

        strcpy(emv.combinations[i]->aid, getString(combObject, "aid"));
        strcpy(emv.combinations[i]->partial, getString(combObject, "partial"));
        strcpy(emv.combinations[i]->kernel, getString(combObject, "kernel"));
        strcpy(emv.combinations[i]->txnType, getString(combObject, "txnType"));

        emv.combinations[i]->kernelConfig = loadKernelConfig(combObject);
    }

    return emv;
}

KERNEL_CONFIG loadKernelConfig(json_object *combObject)
{
    KERNEL_CONFIG kernelConfig;
    json_object *kernelConfigObject = json_object_object_get(combObject, "kernelConfig");

    strcpy(kernelConfig.addTermCap, getString(kernelConfigObject, "addTermCap"));
    strcpy(kernelConfig.addTermCapExt, getString(kernelConfigObject, "addTermCapExt"));
    strcpy(kernelConfig.appUnblockSupport, getString(kernelConfigObject, "appUnblockSupport"));
    strcpy(kernelConfig.appVersion, getString(kernelConfigObject, "appVersion"));
    strcpy(kernelConfig.countryCode, getString(kernelConfigObject, "countryCode"));
    strcpy(kernelConfig.mcc, getString(kernelConfigObject, "mcc"));
    strcpy(kernelConfig.serviceDataFormat, getString(kernelConfigObject, "serviceDataFormat"));
    strcpy(kernelConfig.serviceQualifier, getString(kernelConfigObject, "serviceQualifier"));
    strcpy(kernelConfig.tacDefault, getString(kernelConfigObject, "tacDefault"));
    strcpy(kernelConfig.tacDenial, getString(kernelConfigObject, "tacDenial"));
    strcpy(kernelConfig.tacOnline, getString(kernelConfigObject, "tacOnline"));
    strcpy(kernelConfig.tdol, getString(kernelConfigObject, "tdol"));
    strcpy(kernelConfig.termCap, getString(kernelConfigObject, "termCap"));
    strcpy(kernelConfig.terminalFloorLimit, getString(kernelConfigObject, "terminalFloorLimit"));
    strcpy(kernelConfig.terminalId, getString(kernelConfigObject, "terminalId"));
    strcpy(kernelConfig.terminalType, getString(kernelConfigObject, "terminalType"));
    strcpy(kernelConfig.trmData, getString(kernelConfigObject, "trmData"));
    strcpy(kernelConfig.tcCategoryCode, getString(kernelConfigObject, "tcCategoryCode"));
    strcpy(kernelConfig.kernelConfiguration, getString(kernelConfigObject, "kernelConfiguration"));
    strcpy(kernelConfig.cvmCapCVM, getString(kernelConfigObject, "cvmCapCVM"));
    strcpy(kernelConfig.cvmCapNoCVM, getString(kernelConfigObject, "cvmCapNoCVM"));
    strcpy(kernelConfig.secCap, getString(kernelConfigObject, "secCap"));

    return kernelConfig;
}

ENTRYPOINT loadEntryPoint(json_object *combObject)
{
    ENTRYPOINT entryPoint;
    json_object *entryPointObject = json_object_object_get(combObject, "entryPoint");

    strcpy(entryPoint.clessTxnLimit, getString(entryPointObject, "clessTxnLimit"));
    strcpy(entryPoint.cvmRequireLimit, getString(entryPointObject, "cvmRequireLimit"));
    strcpy(entryPoint.termFloorLimit, getString(entryPointObject, "termFloorLimit"));
    strcpy(entryPoint.zeroAmountAllowed, getString(entryPointObject, "zeroAmountAllowed"));
    strcpy(entryPoint.zeroAmountOfflineAllowed, getString(entryPointObject, "zeroAmountOfflineAllowed"));

    return entryPoint;
}

ONLINE_TAGS loadOnlineTags(json_object *jObject)
{
    json_object *emvObject = json_object_object_get(jObject, "emv");
    json_object *onlineTagsObject = json_object_object_get(emvObject, "onlineTags");

    ONLINE_TAGS onlineTags;
    strcpy(onlineTags.kernelId, getString(onlineTagsObject, "kernelId"));
    strcpy(onlineTags.tags, getString(onlineTagsObject, "tags"));

    return onlineTags;
}

ONLINE_TAGS loadOnlineTagsMasterCard(json_object *jObject)
{
    json_object *emvObject = json_object_object_get(jObject, "emv");
    json_object *onlineTagsObject = json_object_object_get(emvObject, "onlineTagsMasterCard");

    ONLINE_TAGS onlineTags;
    strcpy(onlineTags.kernelId, getString(onlineTagsObject, "kernelId"));
    strcpy(onlineTags.tags, getString(onlineTagsObject, "tags"));

    return onlineTags;
}

ONLINE_TAGS loadOnlineTagsVisa(json_object *jObject)
{
    json_object *emvObject = json_object_object_get(jObject, "emv");
    json_object *onlineTagsObject = json_object_object_get(emvObject, "onlineTagsVisa");

    ONLINE_TAGS onlineTags;
    strcpy(onlineTags.kernelId, getString(onlineTagsObject, "kernelId"));
    strcpy(onlineTags.tags, getString(onlineTagsObject, "tags"));

    return onlineTags;
}

EMV loadKeys(json_object *jObject, EMV emv)
{
    json_object *emvObject = json_object_object_get(jObject, "emv");
    json_object *keys = json_object_object_get(emvObject, "keys");

    int keysLen = json_object_array_length(keys);
    logData("Length of keys : %d", keysLen);
    emv.keysLen = keysLen;
    emv.keyList = malloc(keysLen * sizeof(KEYS));

    json_object *keyObject;
    for (int i = 0; i < keysLen; i++)
    {
        keyObject = json_object_array_get_idx(keys, i);
        emv.keyList[i] = malloc(sizeof(KEYS));

        strcpy(emv.keyList[i]->aid, getString(keyObject, "aid"));
        strcpy(emv.keyList[i]->keyIndex, getString(keyObject, "keyIndex"));
        strcpy(emv.keyList[i]->modulus, getString(keyObject, "modulus"));
        strcpy(emv.keyList[i]->exponent, getString(keyObject, "exponent"));
    }

    return emv;
}

EMV_CONFIG loadEncrypts(json_object *jObject, EMV_CONFIG emvConfig)
{
    json_object *encrypts = json_object_object_get(jObject, "encrypts");
    int encryptsLen = json_object_array_length(encrypts);
    logData("Length of encrypts : %d", encryptsLen);
    emvConfig.encryptLen = encryptsLen;
    emvConfig.encryptList = malloc(encryptsLen * sizeof(ENCRYPT));

    json_object *encyObject;
    for (int i = 0; i < encryptsLen; i++)
    {
        encyObject = json_object_array_get_idx(encrypts, i);
        emvConfig.encryptList[i] = malloc(sizeof(ENCRYPT));
        strcpy(emvConfig.encryptList[i]->name, getString(encyObject, "name"));
        strcpy(emvConfig.encryptList[i]->key, getString(encyObject, "key"));
        strcpy(emvConfig.encryptList[i]->type, getString(encyObject, "type"));
        strcpy(emvConfig.encryptList[i]->tags, getString(encyObject, "tags"));
    }

    return emvConfig;
}

void printEmvConfig(EMV_CONFIG emvConfig)
{
    logData("===================================================");
    logData("EMV Data");
    logData("Trace Enabled : %s", emvConfig.traceEnabled);
    logData("Total Encrypts : %d", emvConfig.encryptLen);
    for (int i = 0; i < emvConfig.encryptLen; i++)
    {
        logData("");
        logData("Item : %d", (i + 1));
        logData("\tName \t: %s", emvConfig.encryptList[i]->name);
        logData("\tKey \t: %s", emvConfig.encryptList[i]->key);
        logData("\tType \t: %s", emvConfig.encryptList[i]->type);
        logData("\tTags \t: %s", emvConfig.encryptList[i]->tags);
    }

    logData("");
    logData("Total Keys : %d", emvConfig.emv.keysLen);

    for (int i = 0; i < emvConfig.emv.keysLen; i++)
    {
        logData("");
        logData("Item : %d", (i + 1));
        logData("\tAid \t: %s", emvConfig.emv.keyList[i]->aid);
        logData("\tKey Index \t: %s", emvConfig.emv.keyList[i]->keyIndex);
        logData("\tExponent \t: %s", emvConfig.emv.keyList[i]->exponent);
        logData("\tModulus \t: %s", emvConfig.emv.keyList[i]->modulus);
    }

    logData("");
    logData("Online Tags");
    logData("\tKernel Id \t: %s", emvConfig.emv.onlineTags.kernelId);
    logData("\tTags \t\t: %s", emvConfig.emv.onlineTags.tags);

    logData("");
    logData("Combinations");

    for (int i = 0; i < emvConfig.emv.combinationsLen; i++)
    {
        COMBINATIONS *combinations = emvConfig.emv.combinations[i];
        logData("");
        logData("Item : %d", (i + 1));
        logData("Entry Point");
        logData("\tContactless Txn Limit \t: %s", combinations->entryPoint.clessTxnLimit);
        logData("\tCVM Require Limit \t: %s", combinations->entryPoint.cvmRequireLimit);
        logData("\tTerminal Floor Limit \t: %s", combinations->entryPoint.termFloorLimit);

        logData("");
        logData("\tAID \t\t\t: %s", combinations->aid);
        logData("\tPartial \t\t: %s", combinations->partial);
        logData("\tKernel \t\t\t: %s", combinations->kernel);
        logData("\tTxnType \t\t: %s", combinations->txnType);

        logData("");
        logData("Kernel Config");
        logData("\tAdd Term Cap \t\t:%s", combinations->kernelConfig.addTermCap);
        logData("\tAdd Term Cap Extn \t:%s", combinations->kernelConfig.addTermCapExt);
        logData("\tApp Unblock Supp \t:%s", combinations->kernelConfig.appUnblockSupport);
        logData("\tApp Version \t\t:%s", combinations->kernelConfig.appVersion);
        logData("\tCountry Code \t\t:%s", combinations->kernelConfig.countryCode);
        logData("\tMCC \t\t\t:%s", combinations->kernelConfig.mcc);
        logData("\tService Data Format \t:%s", combinations->kernelConfig.serviceDataFormat);
        logData("\tService Qualifier \t:%s", combinations->kernelConfig.serviceQualifier);
        logData("\tTac Default \t\t:%s", combinations->kernelConfig.tacDefault);
        logData("\tTac Denial \t\t:%s", combinations->kernelConfig.tacDenial);
        logData("\tTan Online \t\t:%s", combinations->kernelConfig.tacOnline);
        logData("\tTDOL \t\t\t:%s", combinations->kernelConfig.tdol);
        logData("\tTerminal Cap \t\t:%s", combinations->kernelConfig.termCap);
        logData("\tTerminal Floor Limit \t:%s", combinations->kernelConfig.terminalFloorLimit);
        logData("\tTerminal Id \t\t:%s", combinations->kernelConfig.terminalId);
        logData("\tTerminal Type \t\t:%s", combinations->kernelConfig.terminalType);
    }

    logData("===================================================");
}
