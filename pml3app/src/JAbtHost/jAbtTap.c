#include <json-c/json.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "jAbtInterface.h"
#include "jAbtHostUtil.h"
#include "../include/dboperations.h"
#include "../include/config.h"
#include "../include/commonutil.h"
#include "../include/logutil.h"
#include "../include/abtdbmanager.h"

extern struct applicationConfig appConfig;

/**
 * To genreate ABT tab request that can be sent to the server
 */
char *generateAbtTapRequest(AbtTransactionTable trxDataList[], int start, int trxCount)
{
    char deviceId[10];
    getDeviceId(deviceId);

    // Top level object
    json_object *jobj = json_object_new_object();

    json_object_object_add(jobj, "cmd", json_object_new_string("tapAck"));

    json_object *jBatchArrayObject = json_object_new_array();

    for (int i = start; i < (start + trxCount); i++)
    {
        AbtTransactionTable trxTable = trxDataList[i];

        json_object *txnBodyObj = json_object_new_object();
        char timeData[22];
        populateCurrentTime(timeData);
        strcat(timeData, "Z");
        json_object_object_add(txnBodyObj, "date_time", json_object_new_string(timeData));
        json_object_object_add(txnBodyObj, "device_id", json_object_new_string(deviceId));
        if (strcmp(trxTable.gateStatus, "true") == 0)
            json_object_object_add(txnBodyObj, "gate_open", json_object_new_boolean(TRUE));
        else
            json_object_object_add(txnBodyObj, "gate_open", json_object_new_boolean(FALSE));

        json_object_object_add(txnBodyObj, "token", json_object_new_string(trxTable.token));
        json_object_object_add(txnBodyObj, "tuid", json_object_new_string(trxTable.transactionId));

        json_object *transitDataObject = json_object_new_object();
        json_object_object_add(transitDataObject, "TransactionTypeCode", json_object_new_int(appConfig.txnTypeCode));
        json_object_object_add(transitDataObject, "DeviceTxnSequenceNo", json_object_new_int64(trxTable.trxCounter));
        json_object_object_add(transitDataObject, "TokenizedCardNumber", json_object_new_string(trxTable.token));
        json_object_object_add(transitDataObject, "DeviceType", json_object_new_int(appConfig.deviceType));
        json_object_object_add(transitDataObject, "DeviceCode", json_object_new_string(appConfig.deviceCode));
        json_object_object_add(transitDataObject, "LocationCode", json_object_new_int(appConfig.locationCode));
        json_object_object_add(transitDataObject, "OperatorCode", json_object_new_int(appConfig.operatorCode));
        json_object_object_add(transitDataObject, "TxnStatus", json_object_new_int(trxTable.txnStatus));
        json_object_object_add(transitDataObject, "TariffVersion", json_object_new_int(appConfig.tariffVersion));
        json_object_object_add(transitDataObject, "DeviceModeCode", json_object_new_int(appConfig.deviceModeCode));
        json_object_object_add(transitDataObject, "TxnDeviceDateKey", json_object_new_int(trxTable.deviceDateKey));
        json_object_object_add(transitDataObject, "TxnDeviceTime", json_object_new_string(trxTable.txnDeviceTime));
        json_object_object_add(transitDataObject, "Tid", json_object_new_string(appConfig.terminalId));
        json_object_object_add(transitDataObject, "Mid", json_object_new_string(appConfig.merchantId));
        json_object_object_add(txnBodyObj, "transit_data", transitDataObject);

        // Add data object to array
        json_object_array_add(jBatchArrayObject, txnBodyObj);
    }

    // Add the array to body
    json_object_object_add(jobj, "taps", jBatchArrayObject);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * To parse the abt response
 **/
void parseAbtTapResponse(const char *data, int *count, AbtTapResponse abtTapResponses[])
{
    json_object *jObject = json_tokener_parse(data);
    logData("JSON Object Created for the response data");

    if (jObject == NULL)
    {
        logError("Unable to parse response");
        *count = 0;
        goto done;
    }

    json_object *responses = json_object_object_get(jObject, "responses");

    if (responses == NULL)
    {
        logError("Unable to parse response");
        *count = 0;
        goto done;
    }

    *count = json_object_array_length(responses);
    logData("Total response parsed : %d", *count);

    json_object *jResponseValue;
    for (int i = 0; i < *count; i++)
    {
        logData("Going to parse the tap response : %d", (i + 1));

        jResponseValue = json_object_array_get_idx(responses, i);
        if (json_object_object_get(jResponseValue, "tuid") != NULL)
        {
            strcpy(abtTapResponses[i].tuid,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "tuid")));
        }

        if (json_object_object_get(jResponseValue, "result") != NULL)
        {
            strcpy(abtTapResponses[i].result,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "result")));
        }

        if (json_object_object_get(jResponseValue, "reason") != NULL)
        {
            strcpy(abtTapResponses[i].reason,
                   (char *)json_object_get_string(json_object_object_get(jResponseValue, "reason")));
        }
        else
        {
            strcpy(abtTapResponses[i].reason, "");
        }
    }

done:
    json_object_put(jObject); // Clear the json memory
}
