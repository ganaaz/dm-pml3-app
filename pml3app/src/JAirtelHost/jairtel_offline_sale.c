#include <json-c/json.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <feig/fetrm.h>

#include "jairtel_host_interface.h"
#include "jairtel_hostutil.h"
#include "../include/dboperations.h"
#include "../include/config.h"
#include "../include/commonutil.h"
#include "../include/logutil.h"

extern struct applicationConfig appConfig;

char *generateAirtelOfflineSaleRequest(TransactionTable trxDataList[], int start, int trxCount)
{
    // Top level object
    json_object *jobj = json_object_new_object();

    char firmwareVersion[256] = {0};
    int rc = fetrm_get_firmware_string(firmwareVersion, sizeof(firmwareVersion));
    if (rc != 0)
    {
        strcpy(firmwareVersion, "ERROR");
    }

    // offlineSaleRequestBody
    json_object_object_add(jobj, "terminalFirmwareVersion", json_object_new_string(firmwareVersion));
    json_object_object_add(jobj, "terminalAppVersion", json_object_new_string(appConfig.version));
    json_object_object_add(jobj, "merchantId", json_object_new_string(appConfig.merchantId));
    json_object_object_add(jobj, "terminalId", json_object_new_string(appConfig.terminalId));

    json_object *jBatchArrayObject = json_object_new_array();

    for (int i = start; i < (start + trxCount); i++)
    {
        TransactionTable trxTable = trxDataList[i];

        char updatedAmount[13];
        trimAmount(trxTable.amount, updatedAmount);
        logData("Original amount : %s, and updated amount ; %s", trxTable.amount, updatedAmount);

        json_object *jDataObject = json_object_new_object();
        json_object_object_add(jDataObject, "amount", json_object_new_string(updatedAmount));
        json_object_object_add(jDataObject, "time", json_object_new_string(trxTable.time));
        json_object_object_add(jDataObject, "date", json_object_new_string(trxTable.date));
        json_object_object_add(jDataObject, "year", json_object_new_string(trxTable.year));
        json_object_object_add(jDataObject, "stan", json_object_new_string(trxTable.stan));
        json_object_object_add(jDataObject, "operatorOrderId", json_object_new_string(trxTable.orderId));

        json_object *jCardDetails = json_object_new_object();
        json_object_object_add(jCardDetails, "aid", json_object_new_string(trxTable.aid));
        json_object_object_add(jCardDetails, "encPAN", json_object_new_string(trxTable.panEncrypted));
        json_object_object_add(jCardDetails, "encExpiryDate", json_object_new_string(trxTable.expDateEnc));
        json_object_object_add(jCardDetails, "iccData", json_object_new_string(trxTable.iccData));
        json_object_object_add(jCardDetails, "posConditionCode", json_object_new_string("00"));
        json_object_object_add(jCardDetails, "posEntryMode", json_object_new_string("071"));
        json_object_object_add(jCardDetails, "dataKsn", json_object_new_string(trxTable.ksn));
        json_object_object_add(jCardDetails, "maskCardNo", json_object_new_string(trxTable.maskPAN));

        // Add Card details to body
        json_object_object_add(jDataObject, "cardDetails", jCardDetails);

        logData("Lat : %s", appConfig.latitude);
        logData("long : %s", appConfig.longitude);

        json_object *jTxnAddDetails = json_object_new_object();
        json_object_object_add(jTxnAddDetails, "stationId", json_object_new_string(appConfig.stationId));
        json_object_object_add(jTxnAddDetails, "gateNumber", json_object_new_string(appConfig.deviceCode));
        json_object_object_add(jTxnAddDetails, "stationName", json_object_new_string(appConfig.stationName));
        json_object_object_add(jTxnAddDetails, "latitude", json_object_new_string(appConfig.latitude));
        json_object_object_add(jTxnAddDetails, "longitude", json_object_new_string(appConfig.longitude));

        // Add Card details to body
        json_object_object_add(jDataObject, "txnAddnDetails", jTxnAddDetails);

        // Add data object to array
        json_object_array_add(jBatchArrayObject, jDataObject);
    }

    // Add the array to body
    json_object_object_add(jobj, "offlineSaleBatch", jBatchArrayObject);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    printf("Offline String generated : %s", data);

    return data;
}

/**
 * To parse the offline sale response for Airtel
 **/
void parseAirtelOfflineSaleResponse(const char *data, int *count, OfflineSaleResponse offlineSaleResponses[])
{
    json_object *jObject = json_tokener_parse(data);
    char responseTimeStamp[20];
    populateCurrentTime(responseTimeStamp);

    if (jObject == NULL)
    {
        logError("Unable to parse response");
        *count = 0;
        goto done;
    }

    json_object *jBody = json_object_object_get(jObject, "data");
    if (jBody == NULL)
    {
        logError("Unable to parse data");
        *count = 0;
        goto done;
    }

    // TODO : Verify TID and MID here

    json_object *jResponseList = json_object_object_get(jBody, "offlineSaleTxnList");
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
        if (json_object_object_get(jResponseValue, "txnStatus") != NULL)
        {
            offlineSaleResponses[i].airtelTxnStatus = json_object_get_int(
                json_object_object_get(jResponseValue, "txnStatus"));
        }

        if (json_object_object_get(jResponseValue, "txnCode") != NULL)
        {
            strcpy(offlineSaleResponses[i].resultCodeId,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "txnCode")));
        }

        if (json_object_object_get(jResponseValue, "txnDescription") != NULL)
        {
            strcpy(offlineSaleResponses[i].resultMessage,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "txnDescription")));
        }

        memset(offlineSaleResponses[i].orderId, 0, sizeof(offlineSaleResponses[i].orderId));
        if (json_object_object_get(jResponseValue, "operatorOrderId") != NULL)
        {
            strcpy(offlineSaleResponses[i].orderId,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "operatorOrderId")));
        }
        logData("Order id Received back : %s", offlineSaleResponses[i].orderId);

        if (json_object_object_get(jResponseValue, "txnId") != NULL)
        {
            strcpy(offlineSaleResponses[i].airtelTxnId,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "txnId")));
        }
    }

done:
    json_object_put(jObject); // Clear the json memory
}
