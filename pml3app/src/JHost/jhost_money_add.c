#include <json-c/json.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "jhost_interface.h"
#include "jhostutil.h"
#include "../include/logutil.h"
#include "../include/dboperations.h"
#include "../include/config.h"
#include "../include/commonutil.h"

extern struct applicationConfig appConfig;

/**
 * Generate Money add request
 **/
char *generateMoneyAddRequest(struct transactionData trxData)
{
    // Top level object
    json_object *jobj = json_object_new_object();

    // balance update header
    json_object *reqHeaderObj = json_object_new_object();
    json_object_object_add(reqHeaderObj, "dataKsn", json_object_new_string(trxData.ksn));
    json_object_object_add(reqHeaderObj, "version", json_object_new_string(appConfig.hostVersion));
    json_object_object_add(reqHeaderObj, "clientId", json_object_new_string(appConfig.clientId));
    json_object_object_add(reqHeaderObj, "mac", json_object_new_string(trxData.mac));
    json_object_object_add(reqHeaderObj, "macKsn", json_object_new_string(trxData.macKsn));
    json_object_object_add(reqHeaderObj, "isP2PEDevice", json_object_new_boolean(true));
    json_object_object_add(jobj, "head", reqHeaderObj);

    // balance update body
    json_object *reqBodyObj = json_object_new_object();
    char timeData[20];
    populateCurrentTime(timeData);
    logData("Generate money add request timestamp : %s", timeData);

    json_object_object_add(reqBodyObj, "processingCode", json_object_new_string(trxData.processingCode));
    json_object_object_add(reqBodyObj, "amount", json_object_new_string(trxData.sAmount));
    json_object_object_add(reqBodyObj, "stan", json_object_new_string(trxData.stan));
    json_object_object_add(reqBodyObj, "expiryDate", json_object_new_string(trxData.expDateEnc));
    json_object_object_add(reqBodyObj, "time", json_object_new_string(trxData.time));
    json_object_object_add(reqBodyObj, "date", json_object_new_string(trxData.date));
    json_object_object_add(reqBodyObj, "year", json_object_new_string(trxData.year));
    json_object_object_add(reqBodyObj, "invoiceNumber", json_object_new_string(trxData.stan));

    json_object *txnExtObj = json_object_new_object();
    json_object_object_add(txnExtObj, "orderId", json_object_new_string(trxData.orderId));
    json_object_object_add(txnExtObj, "txnType", json_object_new_string(trxData.moneyAddTrxType));
    json_object_object_add(txnExtObj, "gateNumber", json_object_new_string(appConfig.deviceCode));
    json_object_object_add(txnExtObj, "stationId", json_object_new_string(appConfig.stationId));
    json_object_object_add(txnExtObj, "stationName", json_object_new_string(appConfig.stationName));
    json_object_object_add(txnExtObj, "operatorOrderId", json_object_new_string(trxData.orderId));

    // Add extended info to body
    json_object_object_add(reqBodyObj, "extendInfo", txnExtObj);

    json_object_object_add(reqBodyObj, "posConditionCode", json_object_new_string("02"));
    json_object_object_add(reqBodyObj, "posEntryMode", json_object_new_string("071"));
    json_object_object_add(reqBodyObj, "encryptedTrack2", json_object_new_string(trxData.track2Enc));
    json_object_object_add(reqBodyObj, "iccData", json_object_new_string(trxData.iccData));

    json_object_object_add(reqBodyObj, "tid", json_object_new_string(appConfig.terminalId));
    json_object_object_add(reqBodyObj, "mid", json_object_new_string(appConfig.merchantId));

    // add body to main
    json_object_object_add(jobj, "body", reqBodyObj);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}
