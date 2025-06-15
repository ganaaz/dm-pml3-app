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
 * Money add request for Airtel
 */
char *generateAirtelMoneyAddRequest(struct transactionData trxData)
{
    // Top level object
    json_object *jobj = json_object_new_object();

    char updatedAmount[13];
    trimAmount(trxData.sAmount, updatedAmount);
    logData("Original amount : %s, and updated amount ; %s", trxData.sAmount, updatedAmount);

    char firmwareVersion[256] = {0};
    int rc = fetrm_get_firmware_string(firmwareVersion, sizeof(firmwareVersion));
    if (rc != 0)
    {
        strcpy(firmwareVersion, "ERROR");
    }

    // body

    json_object_object_add(jobj, "stan", json_object_new_string(trxData.stan));
    json_object_object_add(jobj, "amount", json_object_new_string(updatedAmount));
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
    json_object_object_add(txnAddDetails, "sourceTxnId", json_object_new_string(trxData.sourceTxnId));

    json_object_object_add(jobj, "txnAddnDetails", txnAddDetails);

    json_object_object_add(jobj, "terminalId", json_object_new_string(appConfig.terminalId));
    json_object_object_add(jobj, "merchantId", json_object_new_string(appConfig.merchantId));
    json_object_object_add(jobj, "terminalFirmwareVersion", json_object_new_string(firmwareVersion));
    json_object_object_add(jobj, "terminalAppVersion", json_object_new_string(appConfig.version));

    // Possible Values: CC, DC, UPI, CASH, ACCOUNT
    json_object_object_add(jobj, "paymentMode", json_object_new_string(trxData.moneyAddTrxType));

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}
