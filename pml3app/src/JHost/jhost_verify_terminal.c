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
 * Generate the request for verify terminal
 */
char *generateVerifyTerminalRequest(struct transactionData trxData, const char tid[50])
{
    // Top level object
    json_object *jobj = json_object_new_object();

    // balance update header
    json_object *reqHeaderObj = json_object_new_object();
    json_object_object_add(reqHeaderObj, "clientId", json_object_new_string(appConfig.clientId));
    json_object_object_add(reqHeaderObj, "macKsn", json_object_new_string(trxData.macKsn));
    json_object_object_add(reqHeaderObj, "mac", json_object_new_string(trxData.mac));
    json_object_object_add(reqHeaderObj, "version", json_object_new_string(appConfig.hostVersion));
    json_object_object_add(reqHeaderObj, "isP2PEDevice", json_object_new_boolean(true));
    json_object_object_add(jobj, "head", reqHeaderObj);

    // verify update body
    json_object *reqBodyObj = json_object_new_object();

    char deviceId[20];
    getDeviceId(deviceId);

    json_object_object_add(reqBodyObj, "tid", json_object_new_string(tid));
    json_object_object_add(reqBodyObj, "serialNo", json_object_new_string(deviceId));
    json_object_object_add(reqBodyObj, "stan", json_object_new_string(trxData.stan));
    json_object_object_add(reqBodyObj, "vendorName", json_object_new_string(appConfig.clientName));

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
 * To parse the response of the verify terminal API
 **/
VerifyTerminalResponse parseVerifyTerminalResponse(const char *data)
{
    logData("Parsing VerifyTerminalResponse");
    VerifyTerminalResponse responseData;

    responseData.status = -1;
    memset(responseData.bankMid, 0, sizeof(responseData.bankMid));
    memset(responseData.bankTid, 0, sizeof(responseData.bankTid));
    memset(responseData.mccCode, 0, sizeof(responseData.mccCode));
    memset(responseData.merchantAddress, 0, sizeof(responseData.merchantAddress));
    memset(responseData.merchantName, 0, sizeof(responseData.merchantName));
    memset(responseData.mid, 0, sizeof(responseData.mid));
    memset(responseData.resultMsg, 0, sizeof(responseData.resultMsg));
    memset(responseData.resultStatus, 0, sizeof(responseData.resultStatus));
    memset(responseData.timeStamp, 0, sizeof(responseData.timeStamp));
    memset(responseData.resultcode, 0, sizeof(responseData.resultcode));

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
        strcpy(responseData.timeStamp,
               (char *)json_object_get_string(json_object_object_get(jHead, "responseTimestamp")));
    }

    json_object *jBody = json_object_object_get(jObject, "body");
    if (jBody == NULL)
    {
        goto done;
    }

    if (json_object_object_get(jBody, "resultStatus") != NULL)
    {
        strcpy(responseData.resultStatus,
               (char *)json_object_get_string(json_object_object_get(jBody, "resultStatus")));
    }

    if (json_object_object_get(jBody, "resultcode") != NULL)
    {
        strcpy(responseData.resultcode,
               (char *)json_object_get_string(json_object_object_get(jBody, "resultcode")));

        if (strcmp(responseData.resultcode, "S") == 0)
        {
            responseData.status = 0;
        }
    }

    if (json_object_object_get(jBody, "resultMsg") != NULL)
    {
        strcpy(responseData.resultMsg,
               (char *)json_object_get_string(json_object_object_get(jBody, "resultMsg")));
    }

    if (json_object_object_get(jBody, "bankTid") != NULL)
    {
        strcpy(responseData.bankTid,
               (char *)json_object_get_string(json_object_object_get(jBody, "bankTid")));
    }

    if (json_object_object_get(jBody, "bankMid") != NULL)
    {
        strcpy(responseData.bankMid,
               (char *)json_object_get_string(json_object_object_get(jBody, "bankMid")));
    }

    if (json_object_object_get(jBody, "mid") != NULL)
    {
        strcpy(responseData.mid,
               (char *)json_object_get_string(json_object_object_get(jBody, "mid")));
    }

    if (json_object_object_get(jBody, "merchantName") != NULL)
    {
        strcpy(responseData.merchantName,
               (char *)json_object_get_string(json_object_object_get(jBody, "merchantName")));
    }

    if (json_object_object_get(jBody, "merchantAddress") != NULL)
    {
        strcpy(responseData.merchantAddress,
               (char *)json_object_get_string(json_object_object_get(jBody, "merchantAddress")));
    }

    if (json_object_object_get(jBody, "mccCode") != NULL)
    {
        strcpy(responseData.mccCode,
               (char *)json_object_get_string(json_object_object_get(jBody, "mccCode")));
    }

    logData("Parsing completed");

done:

    json_object_put(jObject); // Clear the json memory
    return responseData;
}
