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

/**
 * Balance update request for Airtel
 */
char *generateAirtelBalanceUpdateRequest(struct transactionData trxData)
{
    // Top level object
    json_object *jobj = json_object_new_object();

    // char updatedAmount[13];
    // trimAmount(trxData.amount, updatedAmount);
    // logData("Original amount : %s, and updated amount ; %s", trxData.amount, updatedAmount);

    char firmwareVersion[256] = {0};
    int rc = fetrm_get_firmware_string(firmwareVersion, sizeof(firmwareVersion));
    if (rc != 0)
    {
        strcpy(firmwareVersion, "ERROR");
    }

    // balance update body

    json_object_object_add(jobj, "stan", json_object_new_string(trxData.stan));
    json_object_object_add(jobj, "amount", json_object_new_string("0"));
    json_object_object_add(jobj, "invoiceNo", json_object_new_string(trxData.stan));
    json_object_object_add(jobj, "time", json_object_new_string(trxData.time));
    json_object_object_add(jobj, "date", json_object_new_string(trxData.date));
    json_object_object_add(jobj, "year", json_object_new_string(trxData.year));
    json_object_object_add(jobj, "operatorOrderId", json_object_new_string(trxData.orderId));

    // Add card details to body
    json_object *cardDetailsObj = json_object_new_object();
    json_object_object_add(cardDetailsObj, "encExpiryDate", json_object_new_string(trxData.expDateEnc));
    json_object_object_add(cardDetailsObj, "encPAN", json_object_new_string(trxData.panEncrypted));
    json_object_object_add(cardDetailsObj, "encTrack2", json_object_new_string(trxData.track2Enc));
    json_object_object_add(cardDetailsObj, "iccData", json_object_new_string(trxData.iccData));
    json_object_object_add(cardDetailsObj, "aid", json_object_new_string(trxData.aid));
    json_object_object_add(cardDetailsObj, "posConditionCode", json_object_new_string("02"));
    json_object_object_add(cardDetailsObj, "posEntryMode", json_object_new_string("071"));
    json_object_object_add(cardDetailsObj, "dataKsn", json_object_new_string(trxData.ksn));
    // json_object_object_add(cardDetailsObj, "chipBalance", json_object_new_string(trxData.chipBalance));
    json_object_object_add(cardDetailsObj, "maskCardNo", json_object_new_string(trxData.maskPan));
    // json_object_object_add(cardDetailsObj, "maskCardNo", json_object_new_string("608326******0066"));

    json_object_object_add(jobj, "cardDetails", cardDetailsObj);

    // Add txn addn details to body
    json_object *txnAddDetails = json_object_new_object();
    json_object_object_add(txnAddDetails, "stationId", json_object_new_string(appConfig.stationId));
    json_object_object_add(txnAddDetails, "gateNo", json_object_new_string(appConfig.deviceCode));
    json_object_object_add(txnAddDetails, "stationName", json_object_new_string(appConfig.stationName));
    // json_object_object_add(txnAddDetails, "shiftNo", json_object_new_string(appConfig.shiftNo));
    json_object_object_add(txnAddDetails, "latitude", json_object_new_string(appConfig.latitude));
    json_object_object_add(txnAddDetails, "longitude", json_object_new_string(appConfig.longitude));

    json_object_object_add(jobj, "txnAddnDetails", txnAddDetails);

    json_object_object_add(jobj, "terminalId", json_object_new_string(appConfig.terminalId));
    json_object_object_add(jobj, "merchantId", json_object_new_string(appConfig.merchantId));
    json_object_object_add(jobj, "terminalFirmwareVersion", json_object_new_string(firmwareVersion));
    json_object_object_add(jobj, "terminalAppVersion", json_object_new_string(appConfig.version));

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Common parsing for Balance update, service creation, money add for Airtel
 **/
AirtelHostResponse parseAirtelHostResponseData(const char *data)
{
    AirtelHostResponse hostResponse;
    hostResponse.status = -1;
    memset(hostResponse.hostResponseTimeStamp, 0, sizeof(hostResponse.hostResponseTimeStamp));
    memset(hostResponse.responseCode, 0, sizeof(hostResponse.responseCode));
    memset(hostResponse.responseDesc, 0, sizeof(hostResponse.responseDesc));
    memset(hostResponse.switchResponseCode, 0, sizeof(hostResponse.switchResponseCode));
    memset(hostResponse.txnId, 0, sizeof(hostResponse.txnId));
    memset(hostResponse.switchTerminalId, 0, sizeof(hostResponse.switchTerminalId));
    memset(hostResponse.rrn, 0, sizeof(hostResponse.rrn));
    memset(hostResponse.stan, 0, sizeof(hostResponse.stan));
    memset(hostResponse.switchMerchantId, 0, sizeof(hostResponse.switchMerchantId));
    memset(hostResponse.authCode, 0, sizeof(hostResponse.authCode));

    char responseTimeStamp[20];
    populateCurrentTime(responseTimeStamp);
    strcpy(hostResponse.hostResponseTimeStamp, responseTimeStamp);

    json_object *jObject = json_tokener_parse(data);
    if (jObject == NULL)
    {
        goto done;
    }

    json_object *jHead = json_object_object_get(jObject, "meta");
    if (jHead == NULL)
    {
        goto done;
    }

    if (json_object_object_get(jHead, "status") != NULL)
    {
        hostResponse.status = json_object_get_int(json_object_object_get(jHead, "status"));
    }

    if (json_object_object_get(jHead, "code") != NULL)
    {
        strcpy(hostResponse.responseCode,
               (char *)json_object_get_string(json_object_object_get(jHead, "code")));
    }

    if (json_object_object_get(jHead, "description") != NULL)
    {
        strcpy(hostResponse.responseDesc,
               (char *)json_object_get_string(json_object_object_get(jHead, "description")));
    }

    json_object *jBody = json_object_object_get(jObject, "data");
    if (jBody == NULL)
    {
        goto done;
    }

    if (json_object_object_get(jBody, "switchResponseCode") != NULL)
    {
        strcpy(hostResponse.switchResponseCode,
               (char *)json_object_get_string(json_object_object_get(jBody, "switchResponseCode")));
    }

    if (json_object_object_get(jBody, "iccData") != NULL)
    {
        strcpy(hostResponse.iccData,
               (char *)json_object_get_string(json_object_object_get(jBody, "iccData")));
    }

    if (json_object_object_get(jBody, "txnId") != NULL)
    {
        strcpy(hostResponse.txnId,
               (char *)json_object_get_string(json_object_object_get(jBody, "txnId")));
    }

    if (json_object_object_get(jBody, "switchTerminalId") != NULL)
    {
        strcpy(hostResponse.switchTerminalId,
               (char *)json_object_get_string(json_object_object_get(jBody, "switchTerminalId")));
    }

    if (json_object_object_get(jBody, "rrn") != NULL)
    {
        strcpy(hostResponse.rrn,
               (char *)json_object_get_string(json_object_object_get(jBody, "rrn")));
    }

    if (json_object_object_get(jBody, "stan") != NULL)
    {
        strcpy(hostResponse.stan,
               (char *)json_object_get_string(json_object_object_get(jBody, "stan")));
    }

    if (json_object_object_get(jBody, "switchMerchantId") != NULL)
    {
        strcpy(hostResponse.switchMerchantId,
               (char *)json_object_get_string(json_object_object_get(jBody, "switchMerchantId")));
    }

    if (json_object_object_get(jBody, "authCode") != NULL)
    {
        strcpy(hostResponse.authCode,
               (char *)json_object_get_string(json_object_object_get(jBody, "authCode")));
    }

    if (json_object_object_get(jBody, "txnType") != NULL)
    {
        strcpy(hostResponse.txnType,
               (char *)json_object_get_string(json_object_object_get(jBody, "txnType")));
    }

    if (json_object_object_get(jBody, "paymentMode") != NULL)
    {
        strcpy(hostResponse.paymentMode,
               (char *)json_object_get_string(json_object_object_get(jBody, "paymentMode")));
    }

    if (json_object_object_get(jBody, "refundId") != NULL)
    {
        strcpy(hostResponse.refundId,
               (char *)json_object_get_string(json_object_object_get(jBody, "refundId")));
    }

done:
    json_object_put(jObject); // Clear the json memory
    return hostResponse;
}
