#include <json-c/json.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "jhost_interface.h"
#include "jhostutil.h"
#include "../include/dboperations.h"
#include "../include/config.h"
#include "../include/commonutil.h"
#include "../include/logutil.h"

extern struct applicationConfig appConfig;

char *generateOfflineSaleRequest(TransactionTable trxDataList[], int start, int trxCount)
{
    // Top level object
    json_object *jobj = json_object_new_object();

    // offlineSaleRequestHeader
    json_object *reqHeaderObj = json_object_new_object();
    json_object_object_add(reqHeaderObj, "clientId", json_object_new_string(appConfig.clientId));
    json_object_object_add(reqHeaderObj, "version", json_object_new_string(appConfig.hostVersion));
    json_object_object_add(jobj, "offlineSaleRequestHeader", reqHeaderObj);

    // offlineSaleRequestBody
    json_object *reqBodyObj = json_object_new_object();
    char timeData[20];
    populateCurrentTime(timeData);
    json_object_object_add(reqBodyObj, "requestTimestamp", json_object_new_string(timeData));
    json_object_object_add(reqBodyObj, "tid", json_object_new_string(appConfig.terminalId));
    json_object_object_add(reqBodyObj, "mid", json_object_new_string(appConfig.merchantId));

    json_object *jBatchArrayObject = json_object_new_array();

    for (int i = start; i < (start + trxCount); i++)
    {
        TransactionTable trxTable = trxDataList[i];

        json_object *jDataObject = json_object_new_object();
        json_object *txnHeadObj = json_object_new_object();
        json_object_object_add(txnHeadObj, "mac", json_object_new_string(trxTable.mac));
        json_object_object_add(txnHeadObj, "dataKsn", json_object_new_string(trxTable.ksn));
        json_object_object_add(txnHeadObj, "macKsn", json_object_new_string(trxTable.macKsn));
        json_object_object_add(txnHeadObj, "isP2PEDevice", json_object_new_boolean(true));

        // Add head to data object
        json_object_object_add(jDataObject, "head", txnHeadObj);

        json_object *txnBodyObj = json_object_new_object();
        json_object_object_add(txnBodyObj, "processingCode", json_object_new_string(trxTable.processingCode));
        json_object_object_add(txnBodyObj, "amount", json_object_new_string(trxTable.amount));
        json_object_object_add(txnBodyObj, "stan", json_object_new_string(trxTable.stan));
        json_object_object_add(txnBodyObj, "cardExpiryDate", json_object_new_string(trxTable.expDateEnc));
        json_object_object_add(txnBodyObj, "transactionTime", json_object_new_string(trxTable.time));
        json_object_object_add(txnBodyObj, "transactionDate", json_object_new_string(trxTable.date));
        json_object_object_add(txnBodyObj, "transactionYear", json_object_new_string(trxTable.year));
        json_object_object_add(txnBodyObj, "invoiceNumber", json_object_new_string(trxTable.stan));
        json_object_object_add(txnBodyObj, "posConditionCode", json_object_new_string("00"));
        json_object_object_add(txnBodyObj, "posEntryMode", json_object_new_string("071"));
        json_object_object_add(txnBodyObj, "primaryAccountNumber", json_object_new_string(trxTable.panEncrypted)); // Encrypted
        json_object_object_add(txnBodyObj, "tid", json_object_new_string(appConfig.terminalId));
        json_object_object_add(txnBodyObj, "mid", json_object_new_string(appConfig.merchantId));
        json_object_object_add(txnBodyObj, "iccData", json_object_new_string(trxTable.iccData));

        json_object *txnExtObj = json_object_new_object();
        json_object_object_add(txnExtObj, "orderId", json_object_new_string(trxTable.orderId));
        json_object_object_add(txnExtObj, "gateNumber", json_object_new_string(appConfig.deviceCode));
        json_object_object_add(txnExtObj, "stationId", json_object_new_string(appConfig.stationId));
        json_object_object_add(txnExtObj, "stationName", json_object_new_string(appConfig.stationName));
        json_object_object_add(txnExtObj, "operatorOrderId", json_object_new_string(trxTable.orderId));

        // Add extended info to body
        json_object_object_add(txnBodyObj, "extendInfo", txnExtObj);

        // Add body to data object
        json_object_object_add(jDataObject, "body", txnBodyObj);

        // Add data object to array
        json_object_array_add(jBatchArrayObject, jDataObject);
    }

    // Add the array to body
    json_object_object_add(reqBodyObj, "offlineSaleBatch", jBatchArrayObject);

    // add body to main
    json_object_object_add(jobj, "offlineSaleRequestBody", reqBodyObj);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * To parse the offline sale respinse
 **/
void parseofflineSaleResponse(const char *data, int *count, OfflineSaleResponse offlineSaleResponses[])
{
    json_object *jObject = json_tokener_parse(data);
    char responseTimeStamp[20];

    if (jObject == NULL)
    {
        logError("Unable to parse response");
        *count = 0;
        goto done;
    }

    json_object *jHead = json_object_object_get(jObject, "head");
    if (jHead == NULL)
    {
        logError("Unable to parse head");
        *count = 0;
        goto done;
    }

    if (json_object_object_get(jHead, "responseTimestamp") != NULL)
    {
        strcpy(responseTimeStamp, (char *)json_object_get_string(json_object_object_get(jHead, "responseTimestamp")));
    }

    json_object *jBody = json_object_object_get(jObject, "body");
    if (jBody == NULL)
    {
        logError("Unable to parse body");
        *count = 0;
        goto done;
    }

    // TODO : Verify TID and MID here

    json_object *jResponseList = json_object_object_get(jBody, "offlineSaleBatchResponseList");
    if (jResponseList == NULL)
    {
        *count = 0;
        goto done;
    }

    *count = json_object_array_length(jResponseList);
    logData("Total response parsed : %d", *count);

    json_object *jResponseValue;
    for (int i = 0; i < *count; i++)
    {
        logData("Going to parse the offline response : %d", (i + 1));
        strcpy(offlineSaleResponses[i].responseTimeStamp, responseTimeStamp);

        jResponseValue = json_object_array_get_idx(jResponseList, i);
        if (json_object_object_get(jResponseValue, "resultStatus") != NULL)
        {
            strcpy(offlineSaleResponses[i].resultStatus,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "resultStatus")));
        }

        if (json_object_object_get(jResponseValue, "resultCode") != NULL)
        {
            strcpy(offlineSaleResponses[i].resultCode,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "resultCode")));
        }

        if (json_object_object_get(jResponseValue, "resultCodeId") != NULL)
        {
            strcpy(offlineSaleResponses[i].resultCodeId,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "resultCodeId")));
        }

        memset(offlineSaleResponses[i].orderId, 0, sizeof(offlineSaleResponses[i].orderId));
        if (json_object_object_get(jResponseValue, "orderId") != NULL)
        {
            strcpy(offlineSaleResponses[i].orderId,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "orderId")));
        }
        logData("Order id Received back : %s", offlineSaleResponses[i].orderId);

        if (json_object_object_get(jResponseValue, "resultMsg") != NULL)
        {
            strcpy(offlineSaleResponses[i].resultMessage,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "resultMsg")));
        }

        if (json_object_object_get(jResponseValue, "stan") != NULL)
        {
            strcpy(offlineSaleResponses[i].stan,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "stan")));
        }

        logData("Order id : %s", offlineSaleResponses[i].orderId);
    }

done:
    json_object_put(jObject); // Clear the json memory
}
