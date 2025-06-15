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
#include "../include/appcodes.h"

extern struct applicationConfig appConfig;

/**
 * Generate Reversal request
 **/
char *generateReversalRequest(TransactionTable trxTable)
{
    logData("Generating reversal request");

    // Top level object
    json_object *jobj = json_object_new_object();

    // Top header
    json_object *headerObj = json_object_new_object();
    json_object_object_add(headerObj, "version", json_object_new_string(appConfig.hostVersion));
    json_object_object_add(headerObj, "clientId", json_object_new_string(appConfig.clientId));
    json_object_object_add(jobj, "head", headerObj);

    // Top body
    json_object *bodyObj = json_object_new_object();

    // Echo object
    json_object *echoObj = json_object_new_object();

    // Reversal Head
    json_object *echoHeadObj = json_object_new_object();
    json_object_object_add(echoHeadObj, "clientId", json_object_new_string(appConfig.clientId));
    json_object_object_add(echoHeadObj, "mac", json_object_new_string(trxTable.echoMac));
    json_object_object_add(echoHeadObj, "macKsn", json_object_new_string(trxTable.echoKsn));
    json_object_object_add(echoHeadObj, "isP2PEDevice", json_object_new_boolean(true));
    json_object_object_add(echoHeadObj, "version", json_object_new_string(appConfig.hostVersion));
    json_object_object_add(echoObj, "head", echoHeadObj);

    // Reversal Body
    json_object *echoBodyObj = json_object_new_object();

    json_object_object_add(echoBodyObj, "tid", json_object_new_string(appConfig.terminalId));
    json_object_object_add(echoBodyObj, "mid", json_object_new_string(appConfig.merchantId));
    json_object_object_add(echoBodyObj, "stan", json_object_new_string(trxTable.stan));
    json_object_object_add(echoBodyObj, "time", json_object_new_string(trxTable.time));
    json_object_object_add(echoBodyObj, "date", json_object_new_string(trxTable.date));
    json_object_object_add(echoBodyObj, "year", json_object_new_string(trxTable.year));
    json_object_object_add(echoBodyObj, "invoiceNumber", json_object_new_string(trxTable.stan));
    json_object_object_add(echoBodyObj, "txnMode", json_object_new_string(TXNMODE_CARD));
    json_object_object_add(echoBodyObj, "txnStatus", json_object_new_string(HOST_TXN_FAIL));

    if (strcmp(trxTable.trxType, TRXTYPE_BALANCE_UPDATE) == 0)
    {
        json_object_object_add(echoBodyObj, "txnType", json_object_new_string(HOST_TXN_BALANCE_UPDATE));
    }

    if (strcmp(trxTable.trxType, TRXTYPE_MONEY_ADD) == 0)
    {
        json_object_object_add(echoBodyObj, "txnType", json_object_new_string(HOST_TXN_MONEY_ADD));
    }

    json_object_object_add(echoBodyObj, "acquirementId", json_object_new_string(trxTable.acquirementId));

    json_object *echoExtInfoObj = json_object_new_object();
    json_object_object_add(echoExtInfoObj, "DE39", json_object_new_string(trxTable.reversalInputResponseCode));
    json_object_object_add(echoExtInfoObj, "orderId", json_object_new_string(trxTable.orderId));
    json_object_object_add(echoBodyObj, "extendInfo", echoExtInfoObj);

    json_object_object_add(echoObj, "body", echoBodyObj);

    json_object_object_add(bodyObj, "echo", echoObj);

    // Reversal object
    json_object *reversalObj = json_object_new_object();

    // Reversal Head
    json_object *reversalHeadObj = json_object_new_object();
    json_object_object_add(reversalHeadObj, "clientId", json_object_new_string(appConfig.clientId));
    json_object_object_add(reversalHeadObj, "dataKsn", json_object_new_string(trxTable.ksn));
    json_object_object_add(reversalHeadObj, "mac", json_object_new_string(trxTable.reversalMac));
    json_object_object_add(reversalHeadObj, "macKsn", json_object_new_string(trxTable.reversalKsn));
    json_object_object_add(reversalHeadObj, "isP2PEDevice", json_object_new_boolean(true));
    json_object_object_add(reversalHeadObj, "version", json_object_new_string(appConfig.hostVersion));
    json_object_object_add(reversalObj, "head", reversalHeadObj);

    // Reversal Body
    json_object *reversalBodyObj = json_object_new_object();

    json_object_object_add(reversalBodyObj, "tid", json_object_new_string(appConfig.terminalId));
    json_object_object_add(reversalBodyObj, "mid", json_object_new_string(appConfig.merchantId));
    json_object_object_add(reversalBodyObj, "stan", json_object_new_string(trxTable.stan));
    json_object_object_add(reversalBodyObj, "time", json_object_new_string(trxTable.time));
    json_object_object_add(reversalBodyObj, "date", json_object_new_string(trxTable.date));
    json_object_object_add(reversalBodyObj, "year", json_object_new_string(trxTable.year));
    json_object_object_add(reversalBodyObj, "invoiceNumber", json_object_new_string(trxTable.stan));
    json_object_object_add(reversalBodyObj, "posConditionCode", json_object_new_string("02"));
    json_object_object_add(reversalBodyObj, "posEntryMode", json_object_new_string("071"));
    json_object_object_add(reversalBodyObj, "primaryAccountNr", json_object_new_string(trxTable.panEncrypted));

    json_object *extendInfoObj = json_object_new_object();
    json_object_object_add(extendInfoObj, "DE39", json_object_new_string(trxTable.reversalInputResponseCode));
    json_object_object_add(extendInfoObj, "orderId", json_object_new_string(trxTable.orderId));
    json_object_object_add(reversalBodyObj, "extendInfo", extendInfoObj);

    json_object_object_add(reversalObj, "body", reversalBodyObj);

    json_object_object_add(bodyObj, "reversal", reversalObj);
    json_object_object_add(jobj, "body", bodyObj);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Parsing of the reversal host response
 **/
ReversalResponse parseReversalHostResponseData(const char *data)
{
    ReversalResponse reversalResponse;
    reversalResponse.status = -1;
    memset(reversalResponse.authorizationCode, 0, sizeof(reversalResponse.authorizationCode));
    memset(reversalResponse.bankResultCode, 0, sizeof(reversalResponse.bankResultCode));
    memset(reversalResponse.retrievalReferenceNumber, 0, sizeof(reversalResponse.retrievalReferenceNumber));

    json_object *jObject = json_tokener_parse(data);
    if (jObject == NULL)
    {
        goto done;
    }

    json_object *jMainBody = json_object_object_get(jObject, "body");
    if (jMainBody == NULL)
    {
        goto done;
    }

    json_object *jReversal = json_object_object_get(jMainBody, "reversal");
    if (jReversal == NULL)
    {
        goto done;
    }

    json_object *jBody = json_object_object_get(jReversal, "body");
    if (jBody == NULL)
    {
        goto done;
    }

    if (json_object_object_get(jBody, "resultCode") != NULL)
    {
        char resultCode[2] = {0};
        strcpy(resultCode,
               (char *)json_object_get_string(json_object_object_get(jBody, "resultCode")));
        if (strcmp(resultCode, "S") == 0)
            reversalResponse.status = 0;
    }

    if (json_object_object_get(jBody, "bankResultCode") != NULL)
    {
        strcpy(reversalResponse.bankResultCode,
               (char *)json_object_get_string(json_object_object_get(jBody, "bankResultCode")));
    }

    if (json_object_object_get(jBody, "retrievalReferenceNumber") != NULL)
    {
        strcpy(reversalResponse.retrievalReferenceNumber,
               (char *)json_object_get_string(json_object_object_get(jBody, "retrievalReferenceNumber")));
    }

    if (json_object_object_get(jBody, "authorizationCode") != NULL)
    {
        strcpy(reversalResponse.authorizationCode,
               (char *)json_object_get_string(json_object_object_get(jBody, "authorizationCode")));
    }

done:

    json_object_put(jObject); // Clear the json memory
    return reversalResponse;
}
