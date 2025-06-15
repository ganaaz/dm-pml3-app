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
 * Airtel reversal request
 */
char *generateAirtelReversalRequest(TransactionTable trxTable)
{
    // Top level object
    json_object *jobj = json_object_new_object();

    char firmwareVersion[256] = {0};
    int rc = fetrm_get_firmware_string(firmwareVersion, sizeof(firmwareVersion));
    if (rc != 0)
    {
        strcpy(firmwareVersion, "ERROR");
    }

    char updatedBalance[13];
    trimAmount(trxTable.serviceBalance, updatedBalance);
    logData("Original balance : %s, and updated balance ; %s", trxTable.serviceBalance, updatedBalance);

    // body

    json_object_object_add(jobj, "operatorOrderId", json_object_new_string(trxTable.orderId));
    json_object_object_add(jobj, "txnId", json_object_new_string(trxTable.airtelTxnId));
    json_object_object_add(jobj, "terminalId", json_object_new_string(appConfig.terminalId));
    json_object_object_add(jobj, "merchantId", json_object_new_string(appConfig.merchantId));
    json_object_object_add(jobj, "code", json_object_new_string(trxTable.reversalInputResponseCode));
    json_object_object_add(jobj, "terminalFirmwareVersion", json_object_new_string(firmwareVersion));
    json_object_object_add(jobj, "terminalAppVersion", json_object_new_string(appConfig.version));

    json_object *cardDetailsObj = json_object_new_object();
    json_object_object_add(cardDetailsObj, "iccData", json_object_new_string(trxTable.iccData));
    json_object_object_add(cardDetailsObj, "chipBalance", json_object_new_string(updatedBalance));
    json_object_object_add(jobj, "cardDetails", cardDetailsObj);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}
