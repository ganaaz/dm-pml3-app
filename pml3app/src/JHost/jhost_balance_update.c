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

char *generateBalanceUpdateRequest(struct transactionData trxData)
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
    logData("Balance update request timestamp : %s", timeData);

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

/**
 * Common parsing for Balance update, service creation, money add
 **/
HostResponse parseHostResponseData(const char *data)
{
    HostResponse hostResponse;
    hostResponse.status = -1;
    memset(hostResponse.authorizationCode, 0, sizeof(hostResponse.authorizationCode));
    memset(hostResponse.bankResultCode, 0, sizeof(hostResponse.bankResultCode));
    memset(hostResponse.iccData, 0, sizeof(hostResponse.iccData));
    memset(hostResponse.invoiceNumber, 0, sizeof(hostResponse.invoiceNumber));
    memset(hostResponse.resultStatus, 0, sizeof(hostResponse.resultStatus));
    memset(hostResponse.retrievalReferenceNumber, 0, sizeof(hostResponse.retrievalReferenceNumber));
    memset(hostResponse.hostResponseTimeStamp, 0, sizeof(hostResponse.hostResponseTimeStamp));
    memset(hostResponse.acquirementId, 0, sizeof(hostResponse.acquirementId));

    json_object *jObject = json_tokener_parse(data);
    if (jObject == NULL)
    {
        goto done;
    }

    json_object *jHead = json_object_object_get(jObject, "head");
    if (jHead == NULL)
    {
        goto done;
    }

    if (json_object_object_get(jHead, "responseTimestamp") != NULL)
    {
        strcpy(hostResponse.hostResponseTimeStamp,
               (char *)json_object_get_string(json_object_object_get(jHead, "responseTimestamp")));
    }

    json_object *jBody = json_object_object_get(jObject, "body");
    if (jBody == NULL)
    {
        goto done;
    }

    if (json_object_object_get(jBody, "resultStatus") != NULL)
    {
        strcpy(hostResponse.resultStatus,
               (char *)json_object_get_string(json_object_object_get(jBody, "resultStatus")));
    }

    if (json_object_object_get(jBody, "resultCodeId") != NULL)
    {
        strcpy(hostResponse.resultCodeId,
               (char *)json_object_get_string(json_object_object_get(jBody, "resultCodeId")));
    }

    if (json_object_object_get(jBody, "resultMsg") != NULL)
    {
        strcpy(hostResponse.resultMsg,
               (char *)json_object_get_string(json_object_object_get(jBody, "resultMsg")));
    }

    if (json_object_object_get(jBody, "bankResultCode") != NULL)
    {
        strcpy(hostResponse.bankResultCode,
               (char *)json_object_get_string(json_object_object_get(jBody, "bankResultCode")));
    }

    if (json_object_object_get(jBody, "invoiceNumber") != NULL)
    {
        strcpy(hostResponse.invoiceNumber,
               (char *)json_object_get_string(json_object_object_get(jBody, "invoiceNumber")));
    }

    if (json_object_object_get(jBody, "retrievalReferenceNumber") != NULL)
    {
        strcpy(hostResponse.retrievalReferenceNumber,
               (char *)json_object_get_string(json_object_object_get(jBody, "retrievalReferenceNumber")));
    }

    if (json_object_object_get(jBody, "authorizationCode") != NULL)
    {
        strcpy(hostResponse.authorizationCode,
               (char *)json_object_get_string(json_object_object_get(jBody, "authorizationCode")));
    }

    if (json_object_object_get(jBody, "acquirementId") != NULL)
    {
        strcpy(hostResponse.acquirementId,
               (char *)json_object_get_string(json_object_object_get(jBody, "acquirementId")));
    }

    if (json_object_object_get(jBody, "iccData") != NULL)
    {
        strcpy(hostResponse.iccData,
               (char *)json_object_get_string(json_object_object_get(jBody, "iccData")));
    }

    hostResponse.status = 0;

done:
    json_object_put(jObject); // Clear the json memory
    return hostResponse;
}
