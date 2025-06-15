#include <string.h>
#include <json-c/json.h>
#include <sys/socket.h>
#include <unistd.h>

#include <feig/fetrm.h>

#include "include/responsemanager.h"
#include "include/commonutil.h"
#include "include/commandmanager.h"
#include "include/logutil.h"
#include "include/config.h"
#include "include/appcodes.h"
#include "include/dboperations.h"
#include "include/errorcodes.h"
#include "include/abtdbmanager.h"

extern struct applicationData appData;
extern struct applicationConfig appConfig;
extern int CLIENT_SOCKET;
extern int DATA_SOCKET_ID;
extern int SERIAL_PORT;
extern int activePendingTxnCount;
extern int IS_SERIAL_CONNECTED;
extern enum device_status DEVICE_STATUS;

/**
 * Build the status response message
 **/
char *buildResponseMessage(const char *status, int errorCode)
{
    json_object *jobj = json_object_new_object();
    json_object *jCommand = json_object_new_string(COMMAND_STATUS);
    json_object *jState = json_object_new_string(status);
    json_object *jErrorCode = json_object_new_int(errorCode);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_STATE, jState);
    json_object_object_add(jobj, COMMAND_ERROR_CODE, jErrorCode);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build the firmware version message
 **/
char *buildFirmwareVersionMessage()
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_GET_FIRMWARE_VERSION);
    json_object *jData = json_object_new_object();

    char version[256] = {0};
    int rc = fetrm_get_firmware_string(version, sizeof(version));
    if (rc != 0)
    {
        strcpy(version, "ERROR");
    }

    json_object *jVersionData = json_object_new_string(version);
    json_object_object_add(jData, VERSION_KEY, jVersionData);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build the product order number message
 **/
char *buildGetProductOrderNumberMessage()
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_GET_PRODUCT_ORDER_NUMBER);
    json_object *jData = json_object_new_object();

    char order[256] = {0};
    int rc = fetrm_get_order_number(order, sizeof(order));

    if (rc != 0)
    {
        strcpy(order, "ERROR");
    }

    json_object_object_add(jData, CODE_KEY, json_object_new_string(order));
    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build the status response message with command
 **/
char *buildCommandResponseMessage(const char *command, const char *status, int errorCode)
{
    json_object *jobj = json_object_new_object();
    json_object *jCommand = json_object_new_string(command);
    json_object *jState = json_object_new_string(status);
    json_object *jErrorCode = json_object_new_int(errorCode);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_STATE, jState);
    json_object_object_add(jobj, COMMAND_ERROR_CODE, jErrorCode);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * To send the file in socket
 */
void sendFileToSocket(const char fileName[])
{
    unsigned char *data = readFile(fileName);
    long int size = findSize(fileName);
    logData("Going to send the data with size : %d", size);
    /*
    char *strData = malloc(size * 2 + 1);
    memset(strData, 0, size + 1);
    strcpy(strData, "");
    for (int i = 0; i < size; i++)
    {
        char temp[3];
        sprintf(temp, "%02X", data[i]);
        strcat(strData, temp);
    }
    strData[size * 2] = '\0';
    printf("String : %s\n", strData);
    printf("String length : %d\n", strlen(strData));
    send(DATA_SOCKET_ID, strData, strlen(strData), 0);
    */
    /*
    int parts = size / 1024;
    for(int i = 0; i < parts; i++)
    {
        unsigned char temp[1024] = {0};
        for (int k = 0; k < 1024; k++)
            temp[i] = data[i * parts + k];

        send(DATA_SOCKET_ID, temp, 1024, 0);
        sleep(1);
    }
    int bal = size % 1024;
    if (bal != 0)
    {
        unsigned char temp[bal];
        for (int k = 0; k < bal; k++)
            temp[k] = data[parts * 1024 + k];
        send(DATA_SOCKET_ID, temp, bal, 0);
    }
    */
    int sent = send(DATA_SOCKET_ID, data, size, 0);
    logData("Total Bytes sent : %d", sent);
    // Send terminating data
    unsigned char endData[4];
    endData[0] = 0xFF;
    endData[1] = 0xFE;
    endData[2] = 0xFD;
    endData[3] = 0xFC;
    sent = send(DATA_SOCKET_ID, endData, 4, 0);
    logData("Sent terminating data of length : %d", sent);
    free(data);
}

/**
 * Build key present message
 **/
char *buildKeyPresentMessage(int result, char *label, bool isBdk)
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_IS_KEY_PRESENT);
    json_object *jData = json_object_new_object();

    json_object_object_add(jData, "label", json_object_new_string(label));
    json_object_object_add(jData, "isBdk", json_object_new_boolean(isBdk));
    if (result == 0)
    {
        json_object_object_add(jData, "result", json_object_new_string("key_present"));
    }
    else
    {
        json_object_object_add(jData, "result", json_object_new_string("key_not_present"));
    }

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build destroy key message
 **/
char *buildDestroyKeyMessage(int result, char *label, bool isBdk)
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_DESTROY_KEY);
    json_object *jData = json_object_new_object();

    json_object_object_add(jData, "label", json_object_new_string(label));
    json_object_object_add(jData, "isBdk", json_object_new_boolean(isBdk));
    if (result == 0)
    {
        json_object_object_add(jData, "result", json_object_new_string("key_destroyed"));
    }
    else
    {
        json_object_object_add(jData, "result", json_object_new_string("key_not_destroyed"));
    }

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build version message
 **/
char *buildVersionMessage()
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_GET_VERSION);
    json_object *jData = json_object_new_object();

    json_object *jVersionData = json_object_new_string(appConfig.version);
    json_object_object_add(jData, VERSION_KEY, jVersionData);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build get device id message
 **/
char *buildDeviceIdMessage()
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_GET_DEVICE_ID);
    json_object *jData = json_object_new_object();

    char deviceId[10];
    getDeviceId(deviceId);
    json_object *jDeviceId = json_object_new_string(deviceId);
    json_object_object_add(jData, DEVICE_ID_KEY, jDeviceId);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * To send a message when there is no reversal
 */
char *buildNoReversalMessage()
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_GET_REVERSAL);
    json_object *jData = json_object_new_object();

    json_object_object_add(jData, "result", json_object_new_string("No Reversal Available"));
    json_object_object_add(jData, "status", json_object_new_string(STATUS_FAILURE));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * To send a delete file response
 */
char *buildDeleteFileResponse(const char *message, int code)
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_DELETE_FILE);
    json_object *jData = json_object_new_object();

    json_object_object_add(jData, "result", json_object_new_string(message));
    if (code == 0)
        json_object_object_add(jData, "status", json_object_new_string(STATUS_SUCCESS));
    else
        json_object_object_add(jData, "status", json_object_new_string(STATUS_FAILURE));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * To send a set time response
 */
char *buildSetTimeResponse(const char *status)
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_SET_TIME);
    json_object *jData = json_object_new_object();

    json_object_object_add(jData, "errorCode", json_object_new_int(0));
    json_object_object_add(jData, "status", json_object_new_string(status));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build verify terminal response message
 **/
char *buildVerifyTerminalResponseMessage(VerifyTerminalResponse response, int midStatus)
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_VERIFY_TERMINAL);
    json_object *jData = json_object_new_object();

    if (response.status == 0)
    {
        json_object_object_add(jData, "verifyTerminalResult", json_object_new_string("Success"));
    }
    else
    {
        json_object_object_add(jData, "verifyTerminalResult", json_object_new_string("Fail"));
    }

    char deviceId[20];
    getDeviceId(deviceId);

    json_object_object_add(jData, "deviceId", json_object_new_string(deviceId));
    json_object_object_add(jData, "responseTimestamp", json_object_new_string(response.timeStamp));
    json_object_object_add(jData, "resultStatus", json_object_new_string(response.resultStatus));
    json_object_object_add(jData, "resultcode", json_object_new_string(response.resultcode));
    json_object_object_add(jData, "resultStatus", json_object_new_string(response.resultStatus));
    json_object_object_add(jData, "resultMsg", json_object_new_string(response.resultMsg));
    json_object_object_add(jData, "bankTid", json_object_new_string(response.bankTid));
    json_object_object_add(jData, "bankMid", json_object_new_string(response.bankMid));
    json_object_object_add(jData, "mid", json_object_new_string(response.mid));
    json_object_object_add(jData, "merchantName", json_object_new_string(response.merchantName));
    json_object_object_add(jData, "merchantAddress", json_object_new_string(response.merchantAddress));
    json_object_object_add(jData, "mccCode", json_object_new_string(response.mccCode));
    if (midStatus == 0)
        json_object_object_add(jData, "midVerifyStatus", json_object_new_string(STATUS_SUCCESS));
    if (midStatus == 1)
        json_object_object_add(jData, "midVerifyStatus", json_object_new_string(STATUS_FAILURE));
    if (midStatus == -1)
        json_object_object_add(jData, "midVerifyStatus", json_object_new_string(STATUS_NA));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build verify terminal response message for airtel
 **/
char *buildAirtelVerifyTerminalResponseMessage(AirtelVerifyTerminalResponse response, int status)
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_AIRTEL_VERIFY_TERMINAL);
    json_object *jData = json_object_new_object();

    if (response.status == 0)
    {
        json_object_object_add(jData, "verifyTerminalResult", json_object_new_string("Success"));
    }
    else
    {
        json_object_object_add(jData, "verifyTerminalResult", json_object_new_string("Fail"));
    }

    char deviceId[20];
    getDeviceId(deviceId);

    json_object_object_add(jData, "deviceId", json_object_new_string(deviceId));
    json_object_object_add(jData, "code", json_object_new_string(response.code));
    json_object_object_add(jData, "description", json_object_new_string(response.description));
    json_object_object_add(jData, "airtelStatus", json_object_new_int(response.status));
    json_object_object_add(jData, "httpStatus", json_object_new_int(status));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build health check response message for airtel
 **/
char *buildAirtelHealthCheckResponseMessage(AirtelHealthCheckResponse response, int status)
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_AIRTEL_HEALTH_CHECK);
    json_object *jData = json_object_new_object();

    if (response.status == 0)
    {
        json_object_object_add(jData, "healthCheckResult", json_object_new_string("Success"));
    }
    else
    {
        json_object_object_add(jData, "healthCheckResult", json_object_new_string("Fail"));
    }

    char deviceId[20];
    getDeviceId(deviceId);

    json_object_object_add(jData, "deviceId", json_object_new_string(deviceId));
    json_object_object_add(jData, "code", json_object_new_string(response.code));
    json_object_object_add(jData, "description", json_object_new_string(response.description));
    json_object_object_add(jData, "airtelStatus", json_object_new_int(response.status));
    json_object_object_add(jData, "serverStatus", json_object_new_string(response.serverStatus));
    json_object_object_add(jData, "serverTime", json_object_new_string(response.serverTime));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Send the card presented message with balance
 **/
void sendBalanceMessage(struct transactionData trxData)
{
    json_object *jobj = json_object_new_object();
    // Root
    json_object *jCommand = json_object_new_string(COMMAND_STATUS);
    json_object *jState = json_object_new_string(STATE_CARD_PRESENTED);

    // Data
    json_object *jAID = json_object_new_string(trxData.aid);
    json_object *jAIDLabel = json_object_new_string(SCHEME_RUPAY);
    json_object *jToken = json_object_new_string(trxData.token);
    json_object *jPan = json_object_new_string(trxData.maskPan);
    json_object *jValidFrom = json_object_new_string(trxData.effectiveDate);
    json_object *jAmount = json_object_new_string(trxData.serviceBalance);
    json_object *jCurrencyCode = json_object_new_string(appConfig.currencyCode);
    json_object *jServiceId = json_object_new_string(trxData.serviceId);
    json_object *jServiceData = json_object_new_string(trxData.serviceData);
    json_object *jServiceControl = json_object_new_string(trxData.serviceControl);
    json_object *jOrderId = json_object_new_string("");

    json_object *jDataObject = json_object_new_object();
    json_object_object_add(jDataObject, "transactionId", json_object_new_string(trxData.transactionId));
    json_object_object_add(jDataObject, "aid", jAID);
    json_object_object_add(jDataObject, "aid_label", jAIDLabel);
    json_object_object_add(jDataObject, "token", jToken);
    json_object_object_add(jDataObject, "pan", jPan);
    json_object_object_add(jDataObject, "valid_from", jValidFrom);
    json_object_object_add(jDataObject, "amount", jAmount);
    json_object_object_add(jDataObject, "currency_code", jCurrencyCode);
    json_object_object_add(jDataObject, "serviceid", jServiceId);
    json_object_object_add(jDataObject, "servicedata", jServiceData);
    json_object_object_add(jDataObject, "servicecontrol", jServiceControl);
    json_object_object_add(jDataObject, "orderId", jOrderId);
    json_object_object_add(jDataObject, "trxIssueDetail", json_object_new_string(trxData.trxIssueDetail));

    char trxDate[20];
    formatTransactionTime(trxData, trxDate);
    json_object_object_add(jDataObject, "transactionTime", json_object_new_string(trxDate));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_STATE, jState);
    json_object_object_add(jobj, COMMAND_DATA, jDataObject);

    const char *result = json_object_to_json_string(jobj);

    // Send the data in socket
    send(CLIENT_SOCKET, result, strlen(result), 0);

    // Write to serial port
    if (IS_SERIAL_CONNECTED == 1)
    {
        write(SERIAL_PORT, result, strlen(result));
    }

    json_object_put(jobj); // clean json memory
}

/**
 * Send the ABT card presented message
 **/
void sendAbtCardPresented(struct transactionData trxData)
{
    json_object *jobj = json_object_new_object();
    // Root
    json_object *jCommand = json_object_new_string(COMMAND_STATUS);
    json_object *jState = json_object_new_string(STATE_CARD_PRESENTED_ABT);

    // Data
    json_object *jToken = json_object_new_string(trxData.plainTrack2);
    json_object *jValidFrom = json_object_new_string(trxData.effectiveDate);
    json_object *jOrderId = json_object_new_string(trxData.orderId);

    json_object *jDataObject = json_object_new_object();
    json_object_object_add(jDataObject, "tuid", json_object_new_string(trxData.transactionId));
    json_object_object_add(jDataObject, "token", jToken);
    json_object_object_add(jDataObject, "valid_from", jValidFrom);
    json_object_object_add(jDataObject, "amount", json_object_new_string("0"));
    json_object_object_add(jDataObject, "orderId", jOrderId);

    logData("GMT Time IS :: %s", trxData.gmtTime);
    char trxDate[20];
    formatTransactionTime(trxData, trxDate);
    json_object_object_add(jDataObject, "transactionTime", json_object_new_string(trxDate));
    json_object_object_add(jDataObject, "transactionTimeUTC", json_object_new_string(trxData.gmtTime));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_STATE, jState);
    json_object_object_add(jobj, COMMAND_DATA, jDataObject);

    const char *result = json_object_to_json_string(jobj);

    // Send the data in socket
    send(CLIENT_SOCKET, result, strlen(result), 0);

    // Write to serial port
    if (IS_SERIAL_CONNECTED == 1)
    {
        write(SERIAL_PORT, result, strlen(result));
    }

    json_object_put(jobj); // clean json memory
}

/**
 * Build pending offline summary message
 **/
char *buildPendingOfflineSummaryMessage()
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_GET_PENDING_OFFLINE);
    json_object *jData = json_object_new_object();

    activePendingTxnCount = getActivePendingTransactions();
    double activePendingAmount = getActivePendingTransactionsAmount() / 100;

    json_object_object_add(jData, "totalPendingTransactions", json_object_new_int(activePendingTxnCount));
    json_object_object_add(jData, "totalPendingTransactions_amount", json_object_new_double(activePendingAmount));
    if (DEVICE_STATUS == STATUS_ONLINE)
    {
        json_object_object_add(jData, "deviceStatus", json_object_new_string("Online"));
    }

    if (DEVICE_STATUS == STATUS_OFFLINE)
    {
        json_object_object_add(jData, "deviceStatus", json_object_new_string("Offline"));
    }

    int failureTxn = getActivePendingHostErrorCategoryTransactions(HOST_ERROR_CATEGORY_FAILED);
    int timeoutTxn = getActivePendingHostErrorCategoryTransactions(HOST_ERROR_CATEGORY_TIMEOUT);
    int pendingTxn = activePendingTxnCount - (failureTxn + timeoutTxn);

    json_object_object_add(jData, "failureTransactions", json_object_new_int(failureTxn));
    json_object_object_add(jData, "timeOutTransactions", json_object_new_int(timeoutTxn));
    json_object_object_add(jData, "pendingWithHostTransactions", json_object_new_int(pendingTxn));

    double failureTxnAmount = getActivePendingHostErrorCategoryTransactionsAmount(HOST_ERROR_CATEGORY_FAILED) / 100;
    double timeoutTxnAmount = getActivePendingHostErrorCategoryTransactionsAmount(HOST_ERROR_CATEGORY_TIMEOUT) / 100;
    double pendingTxnAmount = activePendingAmount - (failureTxnAmount + timeoutTxnAmount);

    json_object_object_add(jData, "failureTransactions_amount", json_object_new_double(failureTxnAmount));
    json_object_object_add(jData, "timeOutTransactions_amount", json_object_new_double(timeoutTxnAmount));
    json_object_object_add(jData, "pendingWithHostTransactions_amount", json_object_new_double(pendingTxnAmount));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Build ABT Summary message
 **/
char *buildAbtTrxSummaryMessage()
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_ABT_SUMMARY);
    json_object *jData = json_object_new_object();

    int pendingCount = getAbtTransactionStatusCount(STATUS_PENDING);
    int okCount = getAbtTransactionStatusCount(STATUS_OK);
    int nOkCount = getAbtTransactionStatusCount(STATUS_NOK);

    json_object_object_add(jData, "totalPendingCount", json_object_new_int(pendingCount));
    json_object_object_add(jData, "totalOkCount", json_object_new_int(okCount));
    json_object_object_add(jData, "totalNOkCount", json_object_new_int(nOkCount));

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Send the transaction processed message with all data
 **/
void sendTransactionProcessedMessage(struct transactionData trxData, const char *outcomeStatus)
{
    logData("Sending transaction processed message");

    json_object *jobj = json_object_new_object();
    // Root
    json_object *jCommand = json_object_new_string(COMMAND_STATUS);
    json_object *jState = json_object_new_string(STATE_CARD_PROCESSED);

    // Data
    json_object *jCounter = json_object_new_int(trxData.txnCounter);
    json_object *jTuid = json_object_new_string(trxData.transactionId);
    json_object *jOutCome = json_object_new_string(outcomeStatus);
    json_object *jToken = json_object_new_string(trxData.token);
    json_object *jOrderId = json_object_new_string(trxData.orderId);
    json_object *jPan = json_object_new_string(trxData.maskPan);

    json_object *jDataObject = json_object_new_object();
    json_object_object_add(jDataObject, "counter", jCounter);
    json_object_object_add(jDataObject, "pan", jPan);
    json_object_object_add(jDataObject, "tuid", jTuid);
    json_object_object_add(jDataObject, "token", jToken);
    json_object_object_add(jDataObject, "outcome", jOutCome);
    json_object_object_add(jDataObject, "orderId", jOrderId);
    json_object_object_add(jDataObject, "trxIssueDetail", json_object_new_string(trxData.trxIssueDetail));

    char trxDate[20];
    formatTransactionTime(trxData, trxDate);
    json_object_object_add(jDataObject, "transactionTime", json_object_new_string(trxDate));

    // verify check date only for purchase
    if (trxData.trxTypeBin == 0x00 && appData.isCheckDateAvailable)
    {
        json_object_object_add(jDataObject, "checkDate", json_object_new_string(trxData.checkDate));
        if (trxData.checkDateResult == 1)
        {
            json_object_object_add(jDataObject, "checkDateBeforeExpiry", json_object_new_string("true"));
        }
        else
        {
            json_object_object_add(jDataObject, "checkDateBeforeExpiry", json_object_new_string("false"));
        }
    }

    if (trxData.isImmedOnline == true)
    {
        logData("Generating data for balance update");
        json_object *jUpdatedAmount = json_object_new_string(trxData.updatedAmount);
        json_object_object_add(jDataObject, "updatedAmount", jUpdatedAmount);
        json_object *jUpdatedBalance = json_object_new_string(trxData.updatedBalance);
        json_object_object_add(jDataObject, "updatedBalance", jUpdatedBalance);

        TransactionTable trxTable = getTransactionTableData(trxData.transactionId);
        json_object *jRRN = json_object_new_string(trxTable.rrn);
        json_object_object_add(jDataObject, "rrn", jRRN);
        json_object *jResponseCode = json_object_new_string(trxTable.hostResultCode);
        json_object_object_add(jDataObject, "responseCode", jResponseCode);
        json_object *jAuthCode = json_object_new_string(trxTable.authCode);
        json_object_object_add(jDataObject, "authCode", jAuthCode);
        json_object *jStan = json_object_new_string(trxTable.stan);
        json_object_object_add(jDataObject, "stan", jStan);
        json_object *jTerminalId = json_object_new_string(trxTable.terminalId);
        json_object_object_add(jDataObject, "terminalId", jTerminalId);
        json_object *jMerchantId = json_object_new_string(trxTable.merchantId);
        json_object_object_add(jDataObject, "merchantId", jMerchantId);
        json_object *jInvoiceNumber = json_object_new_string(trxTable.stan);
        json_object_object_add(jDataObject, "invoiceNo", jInvoiceNumber);
        json_object *jBatch = json_object_new_int(trxTable.batch);
        json_object_object_add(jDataObject, "batchNo", jBatch);

        json_object_object_add(jDataObject, "hostResultMessage", json_object_new_string(trxTable.hostResultMessage));
        json_object_object_add(jDataObject, "hostResultCode", json_object_new_string(trxTable.hostResultCode));
        json_object_object_add(jDataObject, "hostResultCodeId", json_object_new_string(trxTable.hostResultCodeId));
        json_object_object_add(jDataObject, "hostResponseTimeStamp", json_object_new_string(trxTable.hostResponseTimeStamp));
    }

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_STATE, jState);
    json_object_object_add(jobj, COMMAND_DATA, jDataObject);

    logData("JSON generated, going to prepare string");

    const char *result = json_object_to_json_string(jobj);

    logData("Sending to the socket");

    // Send the data in socket
    send(CLIENT_SOCKET, result, strlen(result), 0);

    // Write to serial port
    if (IS_SERIAL_CONNECTED == 1)
    {
        write(SERIAL_PORT, result, strlen(result));
    }

    logData("Message sent");

    json_object_put(jobj); // clean json memory
}

/**
 * Build the get config message
 **/
char *buildGetConfigMessage()
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jCommand = json_object_new_string(COMMAND_GET_CONFIG);
    json_object *jData = json_object_new_object();

    // PSP
    json_object *jPort = json_object_new_int(appConfig.hostPort);
    json_object *jIP = json_object_new_string(appConfig.hostIP);
    json_object *jHttpsHostName = json_object_new_string(appConfig.httpsHostName);
    json_object *jTerminalId = json_object_new_string(appConfig.terminalId);
    json_object *jMerchantId = json_object_new_string(appConfig.merchantId);
    json_object *jHostVersion = json_object_new_string(appConfig.hostVersion);
    json_object *jClientId = json_object_new_string(appConfig.clientId);
    json_object *jClientName = json_object_new_string(appConfig.clientName);
    json_object *jHostTimeOut = json_object_new_int(appConfig.hostTxnTimeout);
    json_object *jHostMaxRetry = json_object_new_int(appConfig.hostMaxRetry);
    json_object *jHostProcessTime = json_object_new_int(appConfig.hostProcessTimeInMinutes);
    json_object *jReversalTime = json_object_new_int(appConfig.reversalTimeInMinutes);
    json_object *jMaxOffline = json_object_new_int(appConfig.maxOfflineTransactions);
    json_object *jMinRequired = json_object_new_int(appConfig.minRequiredForOnline);

    // General
    json_object *jWriteTime = json_object_new_int(appConfig.writeCardWaitTimeMs);

    json_object *jPSPObject = json_object_new_object();
    json_object_object_add(jPSPObject, CONFIG_KEY_PORT, jPort);
    json_object_object_add(jPSPObject, CONFIG_KEY_IP, jIP);
    json_object_object_add(jPSPObject, CONFIG_KEY_HTTPS_HOST_NAME, jHttpsHostName);
    json_object_object_add(jPSPObject, CONFIG_KEY_TERMINALID, jTerminalId);
    json_object_object_add(jPSPObject, CONFIG_KEY_MERCHANT_ID, jMerchantId);
    json_object_object_add(jPSPObject, CONFIG_KEY_HOST_VERSION, jHostVersion);
    json_object_object_add(jPSPObject, CONFIG_KEY_CLIENT_ID, jClientId);
    json_object_object_add(jPSPObject, CONFIG_KEY_CLIENT_NAME, jClientName);
    json_object_object_add(jPSPObject, CONFIG_KEY_TXNTIMEOUT, jHostTimeOut);
    json_object_object_add(jPSPObject, CONFIG_KEY_MAX_RETRY, jHostMaxRetry);
    json_object_object_add(jPSPObject, CONFIG_KEY_HOST_PROCESS_TIMEOUT, jHostProcessTime);
    json_object_object_add(jPSPObject, CONFIG_KEY_REVERSAL_PROCESS_TIMEOUT, jReversalTime);
    json_object_object_add(jPSPObject, CONFIG_KEY_MAX_OFFLINE_TXN, jMaxOffline);
    json_object_object_add(jPSPObject, CONFIG_KEY_MIN_REQUIRED_ONLINE, jMinRequired);

    json_object *jGeneralObject = json_object_new_object();
    json_object_object_add(jGeneralObject, CONFIG_KEY_WAIT_WRITE_CARD, jWriteTime);
    json_object_object_add(jGeneralObject, CONFIG_KEY_KLD_IP, json_object_new_string(appConfig.kldIP));
    json_object_object_add(jGeneralObject, CONFIG_KEY_MAX_KEY_INJECTION_TRY, json_object_new_int(appConfig.maxKeyInjectionTry));
    json_object_object_add(jGeneralObject, CONFIG_KEY_KEYINJECT_RETRY_DELAY_SEC, json_object_new_int(appConfig.keyInjectRetryDelaySec));
    json_object_object_add(jGeneralObject, CONFIG_KEY_BEEP_ON_CARD_FOUND, json_object_new_boolean(appConfig.beepOnCardFound));
    json_object_object_add(jGeneralObject, CONFIG_KEY_PURCHASE_LIMIT, json_object_new_string(appConfig.purchaseLimit));
    json_object_object_add(jGeneralObject, CONFIG_KEY_MONEY_ADD_LIMIT, json_object_new_string(appConfig.moneyAddLimit));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ZERO_VALUE_TXN, json_object_new_boolean(appConfig.ignoreZeroValueTxn));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ENABLE_APDU_LOG, json_object_new_boolean(appConfig.enableApduLog));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AUTO_READ_CARD, json_object_new_boolean(appConfig.autoReadCard));
    json_object_object_add(jGeneralObject, CONFIG_KEY_SOCKET_TIMEOUT, json_object_new_int(appConfig.socketTimeout));
    json_object_object_add(jGeneralObject, CONFIG_KEY_DEVICE_CODE, json_object_new_string(appConfig.deviceCode));
    json_object_object_add(jGeneralObject, CONFIG_KEY_EQUIPMENT_TYPE, json_object_new_string(appConfig.equipmentType));
    json_object_object_add(jGeneralObject, CONFIG_KEY_EQUIPMENT_CODE, json_object_new_string(appConfig.equipmentCode));
    json_object_object_add(jGeneralObject, CONFIG_KEY_STATION_ID, json_object_new_string(appConfig.stationId));
    json_object_object_add(jGeneralObject, CONFIG_KEY_STATION_NAME, json_object_new_string(appConfig.stationName));
    json_object_object_add(jGeneralObject, CONFIG_KEY_PAYTM_LOG_COUNT, json_object_new_int(appConfig.paytmLogCount));
    json_object_object_add(jGeneralObject, CONFIG_KEY_PAYTM_LOG_SIZE, json_object_new_int(appConfig.paytmMaxLogSizeKb));
    json_object_object_add(jGeneralObject, CONFIG_KEY_USE_CONFIG_JSON, json_object_new_boolean(appConfig.useConfigJson));

    json_object_object_add(jGeneralObject, CONFIG_KEY_ENABLE_ABT, json_object_new_boolean(appConfig.enableAbt));
    json_object_object_add(jGeneralObject, CONFIG_KEY_TXN_TYPE_CODE, json_object_new_int(appConfig.txnTypeCode));
    json_object_object_add(jGeneralObject, CONFIG_KEY_DEVICE_TYPE, json_object_new_int(appConfig.deviceType));
    json_object_object_add(jGeneralObject, CONFIG_KEY_LOCATION_CODE, json_object_new_int(appConfig.locationCode));
    json_object_object_add(jGeneralObject, CONFIG_KEY_OPERATOR_CODE, json_object_new_int(appConfig.operatorCode));
    json_object_object_add(jGeneralObject, CONFIG_KEY_TARIFF_VER, json_object_new_int(appConfig.tariffVersion));
    json_object_object_add(jGeneralObject, CONFIG_KEY_DEVICE_MODE_CODE, json_object_new_int(appConfig.deviceModeCode));
    json_object_object_add(jGeneralObject, CONFIG_KEY_GATE_OPEN_WAIT, json_object_new_int(appConfig.gateOpenWaitTimeInMs));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ABT_HOST_IP, json_object_new_string(appConfig.abtIP));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ABT_HOST_NAME, json_object_new_string(appConfig.abtHostName));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ABT_TAP_URL, json_object_new_string(appConfig.abtTapUrl));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ABT_HOST_PORT, json_object_new_int(appConfig.abtPort));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ABT_HOST_WAIT_TIME, json_object_new_int(appConfig.abtHostProcessWaitTimeInMinutes));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ABT_RETENTION_DAYS, json_object_new_int(appConfig.abtDataRetentionPeriodInDays));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ABT_CLEANUP_TIME, json_object_new_string(appConfig.abtCleanupTimeHHMM));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ABT_HOST_PUSH_BATCH_COUNT, json_object_new_int(appConfig.abtHostPushBatchCount));

    json_object_object_add(jGeneralObject, CONFIG_KEY_USE_AIRTEL_HOST, json_object_new_boolean(appConfig.useAirtelHost));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_HOST_IP, json_object_new_string(appConfig.airtelHostIP));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_HOST_PORT, json_object_new_int(appConfig.airtelHostPort));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME, json_object_new_string(appConfig.airtelHttpsHostName));
    json_object_object_add(jGeneralObject, CONFIG_KEY_ARITEL_OFFLINE_URL, json_object_new_string(appConfig.airtelOfflineUrl));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL, json_object_new_string(appConfig.airtelBalanceUpdateUrl));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_MONEY_ADD_URL, json_object_new_string(appConfig.airtelMoneyAddUrl));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL, json_object_new_string(appConfig.airtelServiceCreationUrl));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_REVERSAL_URL, json_object_new_string(appConfig.airtelReversalUrl));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL, json_object_new_string(appConfig.airtelVerifyTerminalUrl));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL, json_object_new_string(appConfig.airtelHealthCheckUrl));
    json_object_object_add(jGeneralObject, CONFIG_KEY_AIRTEL_SIGN_SALT, json_object_new_string(appConfig.airtelSignSalt));
    json_object_object_add(jGeneralObject, CONFIG_KEY_LATITUDE, json_object_new_string(appConfig.latitude));
    json_object_object_add(jGeneralObject, CONFIG_KEY_LONGITUDE, json_object_new_string(appConfig.longitude));

    json_object *jKeyArrayObject = json_object_new_array();

    for (int i = 0; i < appConfig.keysLength; i++)
    {
        KEYDATA *keyData = appConfig.keyDataList[i];
        json_object *jKeyData = json_object_new_object();

        json_object_object_add(jKeyData, CONFIG_KEY_KEY_LABEL, json_object_new_string(keyData->label));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_SLOT, json_object_new_int(keyData->slot));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_MKVERSION, json_object_new_int(keyData->mkVersion));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_AST_ID, json_object_new_string(keyData->astId));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_PKCS_ID, json_object_new_string(keyData->pkcsId));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_TYPE, json_object_new_string(keyData->type));

        if (strcmp(keyData->type, KEY_TYPE_DUKPT_KEY) == 0)
        {
            json_object_object_add(jKeyData, CONFIG_KEY_KEY_KEY_SET_IDENTIFIER, json_object_new_string(keyData->keySetIdentifier));
            json_object_object_add(jKeyData, CONFIG_KEY_IS_MAC, json_object_new_boolean(keyData->isMac));
        }

        json_object_array_add(jKeyArrayObject, jKeyData);
    }

    json_object *jLedConfigArrayObject = json_object_new_array();

    for (int i = 0; i < appConfig.ledLength; i++)
    {
        LEDDATA *ledData = appConfig.ledDataList[i];
        json_object *jLedData = json_object_new_object();
        json_object *jLedDataList = json_object_new_array();

        json_object_array_add(jLedDataList, json_object_new_string(ledData->midLogo));
        json_object_array_add(jLedDataList, json_object_new_string(ledData->led1));
        json_object_array_add(jLedDataList, json_object_new_string(ledData->led2));
        json_object_array_add(jLedDataList, json_object_new_string(ledData->led3));
        json_object_array_add(jLedDataList, json_object_new_string(ledData->led4));

        json_object_object_add(jLedData, ledData->stateName, jLedDataList);
        json_object_array_add(jLedConfigArrayObject, jLedData);
    }

    json_object_object_add(jData, CONFIG_KEY_PSP, jPSPObject);
    json_object_object_add(jData, CONFIG_KEY_GENERAL, jGeneralObject);
    json_object_object_add(jData, CONFIG_KEY_KEYS, jKeyArrayObject);
    json_object_object_add(jData, CONFIG_KEY_LED_CONFIGS, jLedConfigArrayObject);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_DATA, jData);

    const char *jsonData = json_object_to_json_string(jobj);

    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * To send the device offline message, this is after the transaction is done
 * to send to the client directly
 **/
void sendDeviceOfflineMessage()
{
    json_object *jobj = json_object_new_object();
    // Root
    json_object *jCommand = json_object_new_string(COMMAND_STATUS);
    json_object *jState = json_object_new_string(STATUS_DEVICE_OFFLINE);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_STATE, jState);

    const char *result = json_object_to_json_string(jobj);

    // Send the data in socket
    send(CLIENT_SOCKET, result, strlen(result), 0);

    json_object_put(jobj); // clean json memory
}

/**
 * To send the card removed message
 **/
void sendCardRemovedMessage()
{
    json_object *jobj = json_object_new_object();
    // Root
    json_object *jCommand = json_object_new_string(COMMAND_STATUS);
    json_object *jState = json_object_new_string(STATE_CARD_REMOVED);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, COMMAND_STATE, jState);

    const char *result = json_object_to_json_string(jobj);

    // Send the data in socket
    send(CLIENT_SOCKET, result, strlen(result), 0);

    json_object_put(jobj); // clean json memory
}
