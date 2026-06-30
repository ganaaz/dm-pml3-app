#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include <libpay/tlv.h>
#include <feig/fetpf.h>
#include <feig/fetrm.h>

#include "include/errorcodes.h"
#include "include/appcodes.h"
#include "include/commonutil.h"
#include "include/config.h"
#include "include/commandmanager.h"
#include "include/hostmanager.h"
#include "include/feigtransaction.h"
#include "include/keymanager.h"
#include "include/logutil.h"
#include "include/dboperations.h"
#include "include/responsemanager.h"
#include "include/timeutil.h"
#include "include/abtdbmanager.h"

extern struct applicationData appData;
extern struct applicationConfig appConfig;
extern int CLIENT_SOCKET;
extern int SERIAL_PORT;
extern struct pkcs11 *crypto;
extern int activeFetchId;
extern bool canRunTransaction;
extern int IS_SERIAL_CONNECTED;
extern _Atomic enum device_status DEVICE_STATUS;
extern char currentTrxType[100];

int isWriteCardCommand = 0;
int isServiceDataAvailable = 0;
char writeCardAmount[13];
char writeCardServiceData[193];

int isGateOpenAvailable = 0;
bool gateOpenStatus;

int ix = 1;

/**
 * To handle the data socket command
 */
char *handleClientFetchMessage(const char *data)
{
    logDataEx("L3-App", "Command", "Handling Client message for fetch data");
    struct message reqMessage = parseMessage(data);
    logDataEx("L3-App", "Command", "Command : %s", reqMessage.cmd);
    logDataEx("L3-App", "Command", "Current Status : %s", getStatusString(appData.status));

    if (strcmp(reqMessage.cmd, COMMAND_FETCH_AUTH) == 0)
    {
        activeFetchId = reqMessage.fData.fetchid;
        logDataEx("L3-App", "Command", "Starting to process fetch auth  : %d\n", ix++);
        if (strcmp(reqMessage.fData.mode, STATUS_SUCCESS) == 0)
        {
            return fetchHostData(reqMessage.fData.maxrecords, FETCH_MODE_SUCCESS,
                                 reqMessage.fData.isOnline);
        }
        else if (strcmp(reqMessage.fData.mode, STATUS_FAILURE) == 0)
            return fetchHostData(reqMessage.fData.maxrecords, FETCH_MODE_FAILURE,
                                 reqMessage.fData.isOnline);
        else
            return fetchHostData(reqMessage.fData.maxrecords, FETCH_MODE_PENDING,
                                 reqMessage.fData.isOnline);
    }

    if (strcmp(reqMessage.cmd, COMMAND_ABT_FETCH) == 0)
    {
        logDataEx("L3-App", "Command", "Going to fetch abt data");
        return fetchAbtData(reqMessage.abtFetch);
    }

    if (strcmp(reqMessage.cmd, COMMAND_FETCH_ACK) == 0)
    {
        if (activeFetchId != reqMessage.fData.fetchid)
        {
            return buildResponseMessage(STATUS_ERROR, ERR_FETCH_ID_NOT_AVAILABLE);
        }

        return clearFetchedData();
    }

    if (strcmp(reqMessage.cmd, COMMAND_IS_KEY_PRESENT) == 0)
    {
        int result = checkKeyPresent(reqMessage.kData.label, reqMessage.kData.isBdk);
        return buildKeyPresentMessage(result, reqMessage.kData.label, reqMessage.kData.isBdk);
    }

    if (strcmp(reqMessage.cmd, COMMAND_DESTROY_KEY) == 0)
    {
        int result = destroyKey(reqMessage.kData.label, reqMessage.kData.isBdk);
        return buildDestroyKeyMessage(result, reqMessage.kData.label, reqMessage.kData.isBdk);
    }

    if (strcmp(reqMessage.cmd, COMMAND_DOWNLOAD_FILE) == 0)
    {
        logDataEx("L3-App", "Command", "Download file received with file name : %s", reqMessage.dData.fileName);
        if (access(reqMessage.dData.fileName, F_OK) != 0)
        {
            logDataEx("L3-App", "Command", "Requested file not found or no access");
            return buildResponseMessage(STATUS_ERROR, ERR_FILE_NOT_FOUND);
        }

        sendFileToSocket(reqMessage.dData.fileName);
        return NULL;
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_DEVICE_ID) == 0)
    {
        return buildDeviceIdMessage();
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_PENDING_OFFLINE) == 0)
    {
        return buildPendingOfflineSummaryMessage();
    }

    if (strcmp(reqMessage.cmd, COMMAND_ABT_SUMMARY) == 0)
    {
        return buildAbtTrxSummaryMessage();
    }

    if (strcmp(reqMessage.cmd, COMMAND_DO_BEEP) == 0)
    {
        beepInThread();
        return buildResponseMessage(STATUS_SUCCESS, 0);
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_FIRMWARE_VERSION) == 0)
    {
        return buildFirmwareVersionMessage();
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_DISK_SPACE) == 0)
    {
        return buildDiskSpaceMessage(reqMessage.dSpace.path);
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_PRODUCT_ORDER_NUMBER) == 0)
    {
        return buildGetProductOrderNumberMessage();
    }

    if (strcmp(reqMessage.cmd, COMMAND_DELETE_FILE) == 0)
    {
        int code = deleteLogFile(reqMessage);
        if (code == 0)
        {
            return buildDeleteFileResponse("File deleted successfully", code);
        }

        if (code == -1)
        {
            return buildDeleteFileResponse("Error in deleting the file", code);
        }

        if (code == -2)
        {
            return buildDeleteFileResponse("Invalid file name, should be either paytm.log or l3app.log.*", code);
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_SET_TIME) == 0)
    {
        char utc[17];
        safe_strncpy(utc, 17, reqMessage.tData.time, 14);
        logDataEx("L3-App", "Command", "Received time for changing : %s and len : %d", utc, strlen(utc));
        utc[14] = '0';
        utc[15] = '0';
        utc[16] = '\0';
        int res = setReaderTime(utc);
        if (res == 0)
            return buildSetTimeResponse(STATUS_SUCCESS);
        else
            return buildSetTimeResponse(STATUS_ERROR);
    }

    logWarnEx("CommandManager", "", "Going to return default error response");
    return buildResponseMessage(STATUS_ERROR, ERR_GENERIC);
}

/**
 * Handle the message received from the client socket and act on it
 * Return the response as string to be sent in the socket back
 **/
char *handleClientMessage(const char *data)
{
    doLock();
    isWriteCardCommand = 0;
    isServiceDataAvailable = 0;
    doUnLock();

    struct message reqMessage = parseMessage(data);

    logDataEx("L3-App", "Command", "Handling client message in normal socket");
    logDataEx("L3-App", "Command", "Command : %s", reqMessage.cmd);
    logDataEx("L3-App", "Command", "Current Status : %s", getStatusString(appData.status));

    if (strcmp(reqMessage.cmd, COMMAND_CHANGE_LOG_MODE) == 0)
    {
        logDataEx("L3-App", "Command", "Changing log mode to : %s", reqMessage.logData.logMode);
        int r = 0;

        if (strcmp(reqMessage.logData.logMode, LOG_MODE_DEBUG) == 0)
        {
            // r = system("cp log4crc.debug log4crc");
            r = writeLogMode(0);
            logInfoEx("Command", "", "Result of write logmode : %d", r);
        }

        if (strcmp(reqMessage.logData.logMode, LOG_MODE_INFO) == 0)
        {
            r = writeLogMode(1);
            logInfoEx("Command", "", "Result of write logmode : %d", r);
        }

        if (strcmp(reqMessage.logData.logMode, LOG_MODE_ERROR) == 0)
        {
            // r = system("cp log4crc.error log4crc");
            r = writeLogMode(2);
            logInfoEx("Command", "", "Result of write logmode : %d", r);
        }

        if (r != 0)
        {
            return buildCommandResponseMessage(COMMAND_CHANGE_LOG_MODE, STATUS_ERROR, r);
        }

        return buildCommandResponseMessage(COMMAND_CHANGE_LOG_MODE, STATUS_SUCCESS, r);
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_LOG_MODE) == 0)
    {
        int r = getLogMode();
        if (r == 0)
        {
            return buildLogModeResponseMessage(STATUS_SUCCESS, LOG_MODE_DEBUG, 0);
        }
        else if (r == 1)
        {
            return buildLogModeResponseMessage(STATUS_SUCCESS, LOG_MODE_INFO, 0);
        }
        else if (r == 2)
        {
            return buildLogModeResponseMessage(STATUS_SUCCESS, LOG_MODE_ERROR, 0);
        }
        else
        {
            return buildLogModeResponseMessage(STATUS_ERROR, "None", r);
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_STATUS) == 0)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        return buildResponseMessage(getStatusString(), 0);
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_VERSION) == 0)
    {
        return buildVersionMessage();
    }

    if (appData.status == APP_STATUS_RECALC_MAC)
    {
        return buildCommandResponseMessage(COMMAND_RECALC_MAC, STATUS_ERROR, ERR_ONGOING_RECALC_MAC);
    }

    if (strcmp(reqMessage.cmd, COMMAND_RECALC_MAC) == 0)
    {
        if (appData.status != APP_STATUS_READY)
        {
            logDataEx("L3-App", "Command", "Cannot recalculate mac as the status is not ready");
            return buildCommandResponseMessage(COMMAND_RECALC_MAC, STATUS_ERROR, ERR_GENERIC);
        }

        logDataEx("L3-App", "Command", "Initiating recalculate mac");
        performMacRecalculation();
        return buildCommandResponseMessage(COMMAND_RECALC_MAC, STATUS_SUCCESS, 0);
    }

    if (strcmp(reqMessage.cmd, COMMAND_CLEAR_REVERSAL) == 0)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        clearReversalManual();
        return buildCommandResponseMessage(COMMAND_CLEAR_REVERSAL, STATUS_SUCCESS, 0);
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_REVERSAL) == 0)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        return getReversal();
    }

    if (strcmp(reqMessage.cmd, COMMAND_REBOOT) == 0)
    {
        if (appData.status == APP_STATUS_CARD_PRESENTED || appData.status == APP_STATUS_ABT_CARD_PRSENTED)
        {
            return buildCommandResponseMessage(COMMAND_REBOOT, STATUS_ERROR, ERR_CARD_PRESENTED_STATE_CANNOT_REBOOT);
        }

        int result = performReboot();
        if (result == 0)
        {
            // exit(0);
            return buildCommandResponseMessage(COMMAND_REBOOT, STATUS_SUCCESS, 0);
        }
        else
        {
            return buildCommandResponseMessage(COMMAND_REBOOT, STATUS_ERROR, ERR_GENERIC);
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_CHANGE_IP) == 0)
    {
        createAndCopyNetworkInterfaceFile(reqMessage);
        createAndCopyResolvFile(reqMessage);
        return buildCommandResponseMessage(COMMAND_CHANGE_IP, STATUS_SUCCESS, 0);
    }

    if (strcmp(reqMessage.cmd, COMMAND_CHANGE_ONLY_IP) == 0)
    {
        createAndCopyNetworkInterfaceFile(reqMessage);
        return buildCommandResponseMessage(COMMAND_CHANGE_ONLY_IP, STATUS_SUCCESS, 0);
    }

    if (strcmp(reqMessage.cmd, COMMAND_CHANGE_ONLY_DNS) == 0)
    {
        createAndCopyResolvFile(reqMessage);
        return buildCommandResponseMessage(COMMAND_CHANGE_ONLY_DNS, STATUS_SUCCESS, 0);
    }

    if (strcmp(reqMessage.cmd, COMMAND_GET_CONFIG) == 0)
    {
        return buildGetConfigMessage();
    }

    if (strcmp(reqMessage.cmd, COMMAND_SET_CONFIG) == 0)
    {
        int rc = parseConfigAndUpdate(data);
        if (rc != 0)
            return buildCommandResponseMessage(COMMAND_SET_CONFIG, STATUS_ERROR, rc);
        else
            return buildCommandResponseMessage(COMMAND_SET_CONFIG, STATUS_SUCCESS, rc);
    }

    if (appData.status == APP_STATUS_STOPPING_SEARCH)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        logDataEx("L3-App", "Command", "It is in stopping state, wait");
        return buildResponseMessage(STATUS_STOPPING, ERR_DEVICE_IN_STOPPING);
    }

    // if (strcmp(reqMessage.cmd, COMMAND_VERIFY_TERMINAL) == 0)
    // {
    //     if (appData.isKeyInjectionSuccess == false)
    //         return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

    //     if (appData.status == APP_STATUS_AWAIT_CARD)
    //     {
    //         return buildCommandResponseMessage(COMMAND_VERIFY_TERMINAL, STATUS_ERROR, ERR_ALREADY_SEARCHING);
    //     }

    //     return performVerifyTerminal(reqMessage.vData.tid, reqMessage.vData.mid);
    // }

    // if (strcmp(reqMessage.cmd, COMMAND_AIRTEL_VERIFY_TERMINAL) == 0)
    // {
    //     if (appData.isKeyInjectionSuccess == false)
    //         return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

    //     if (appData.status == APP_STATUS_AWAIT_CARD)
    //     {
    //         return buildCommandResponseMessage(COMMAND_AIRTEL_VERIFY_TERMINAL, STATUS_ERROR, ERR_ALREADY_SEARCHING);
    //     }

    //     return performAirtelVerifyTerminal();
    // }

    // if (strcmp(reqMessage.cmd, COMMAND_AIRTEL_HEALTH_CHECK) == 0)
    // {
    //     if (appData.isKeyInjectionSuccess == false)
    //         return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

    //     if (appData.status == APP_STATUS_AWAIT_CARD)
    //     {
    //         return buildCommandResponseMessage(COMMAND_AIRTEL_HEALTH_CHECK, STATUS_ERROR, ERR_ALREADY_SEARCHING);
    //     }

    //     return performAirtelHealthCheck();
    //}

    if (appData.status == APP_STATUS_TID_MID_EMPTY)
    {
        logDataEx("L3-App", "Command", "TID or MID is empty");
        return buildResponseMessage(STATUS_TID_MID_EMPTY, ERR_ALREADY_SEARCHING);
    }

    if (appData.status == APP_STATUS_AWAIT_CARD &&
        strcmp(reqMessage.cmd, COMMAND_SEARCH_CARD) == 0)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        logDataEx("L3-App", "Command", "Already searching for card");
        return buildResponseMessage(STATUS_ERROR, ERR_ALREADY_SEARCHING);
    }

    if ((appData.status == APP_STATUS_CARD_PRESENTED ||
         appData.status == APP_STATUS_ABT_CARD_PRSENTED) &&
        strcmp(reqMessage.cmd, COMMAND_SEARCH_CARD) == 0)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        logDataEx("L3-App", "Command", "Transaction ongoing");
        return buildResponseMessage(STATUS_ERROR, ERR_ONGOING_TRANSACTION);
    }

    if (strcmp(reqMessage.cmd, COMMAND_STOP) == 0)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        if (appData.status == APP_STATUS_AWAIT_CARD)
        {
            stopCardSearch();
            return buildCommandResponseMessage(COMMAND_STOP, STATUS_SUCCESS, 0);
        }
        else
        {
            return buildCommandResponseMessage(COMMAND_STOP, STATUS_ERROR, ERR_NOT_IN_LOOP_MODE);
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_SEARCH_CARD) == 0)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        if (appData.status == APP_STATUS_AWAIT_CARD)
        {
            return buildCommandResponseMessage(COMMAND_SEARCH_CARD, STATUS_ERROR, ERR_ALREADY_SEARCHING);
        }

        if (DEVICE_STATUS == STATUS_OFFLINE)
        {
            return buildResponseMessage(STATUS_DEVICE_OFFLINE, 0);
        }

        return handleSearchCard(reqMessage);
    }

    if (strcmp(reqMessage.cmd, COMMAND_WRITE_CARD) == 0)
    {
        if (appData.isKeyInjectionSuccess == false)
            return buildResponseMessage(STATUS_KEYS_NOT_LOADED, 0);

        if (DEVICE_STATUS == STATUS_OFFLINE)
        {
            return buildResponseMessage(STATUS_DEVICE_OFFLINE, 0);
        }

        if (appData.status != APP_STATUS_CARD_PRESENTED)
        {
            logDataEx("L3-App", "Command", "There is no card presented, so nothing to write");
            return buildResponseMessage(STATUS_ERROR, ERR_CARD_NOT_PRESENTED);
        }
        safe_strcpy(writeCardAmount, sizeof(writeCardAmount), reqMessage.wCard.amount);
        toUpper(reqMessage.wCard.serviceData);

        logDataEx("L3-App", "Command", "Service data available : %d", isServiceDataAvailable);
        logDataEx("L3-App", "Command", "Service data value : %s", reqMessage.wCard.serviceData);

        if (isServiceDataAvailable == 1)
        {
            safe_strcpy(writeCardServiceData, sizeof(writeCardServiceData), reqMessage.wCard.serviceData);
            logDataEx("L3-App", "Command", "Service data available : %s", writeCardServiceData);
        }
        else
        {
            logDataEx("L3-App", "Command", "No Service data available");
        }

        logInfoEx("Command", "", "Write card message received with amount %s, will be processed by transaction.", writeCardAmount);

        doLock();
        isWriteCardCommand = 1;
        doUnLock();
        return "";
    }

    if (strcmp(reqMessage.cmd, COMMAND_GATE_OPEN) == 0)
    {
        if (DEVICE_STATUS == STATUS_OFFLINE)
        {
            return buildResponseMessage(STATUS_DEVICE_OFFLINE, 0);
        }

        if (appData.status != APP_STATUS_ABT_CARD_PRSENTED)
        {
            logDataEx("L3-App", "Command", "There is no card presented, so nothing to write");
            return buildResponseMessage(STATUS_ERROR, ERR_CARD_NOT_PRESENTED);
        }
        gateOpenStatus = reqMessage.gateOpenData.status;

        logDataEx("L3-App", "Command", "Gate open message is received");
        if (gateOpenStatus)
            logDataEx("L3-App", "Command", "Gate open status is true");
        else
            logDataEx("L3-App", "Command", "Gate open status is false");

        doLock();
        isGateOpenAvailable = 1;
        doUnLock();
        return "";
    }

    logWarnEx("CommandManager", "", "Going to return default error response");
    return buildResponseMessage(STATUS_ERROR, ERR_GENERIC);
}

/**
 * Stop the card seaarch action
 **/
void stopCardSearch()
{
    if (appData.status != APP_STATUS_AWAIT_CARD)
    {
        logDataEx("L3-App", "Command", "Not in awaiting mode, nothing to stop");
        return;
    }

    logInfoEx("Command", "", "Going to stop the transaction in search thread");
    changeAppState(APP_STATUS_STOPPING_SEARCH);
    doLock();
    canRunTransaction = false;
    doUnLock();
}

/**
 * Handle the search card message
 **/
char *handleSearchCard(struct message reqMessage)
{
    // reset the app data
    appData.PRMAcqKeyIndex[0] = 0x01;
    appData.amountAuthorizedBin = 0;
    appData.amountKnownAtStart = 0;
    appData.searchTimeout = 10;
    appData.writeCardWaitTimeMs = appConfig.writeCardWaitTimeMs;
    memset(appData.trxType, 0, sizeof(appData.trxType));
    memset(appData.moneyAddTrxType, 0, sizeof(appData.moneyAddTrxType));
    memset(appData.sourceTxnId, 0, sizeof(appData.sourceTxnId));

    logDataEx("L3-App", "Command", "Going to handle search card");
    if (strcmp(reqMessage.sCard.mode, SEARCH_SINGLE) == 0)
    {
        logDataEx("L3-App", "Command", "Search card mode single requested");
        appData.searchMode = SEARCH_MODE_SINGLE;
    }

    if (strcmp(reqMessage.sCard.mode, SEARCH_LOOP) == 0)
    {
        logDataEx("L3-App", "Command", "Search card mode loop requested");
        appData.searchMode = SEARCH_MODE_LOOP;
    }

    appData.searchTimeout = reqMessage.sCard.timeout;

    if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_BALANCE_INQUIRY) == 0)
    {
        appData.trxTypeBin = 0x31;
        if (appData.searchMode == SEARCH_MODE_LOOP)
        {
            return buildResponseMessage(STATUS_ERROR, ERR_BALANCE_INQUIRY_LOOP_NOT_SUPPORTED);
        }
    }

    safe_strcpy(currentTrxType, sizeof(currentTrxType), reqMessage.sCard.trxtype);

    /*
    if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_BALANCE_UPDATE) == 0)
    {
        appData.trxTypeBin = 0x28;
        if (appData.searchMode == SEARCH_MODE_LOOP)
        {
            return buildResponseMessage(STATUS_ERROR, ERR_BALANCE_UPDATE_LOOP_NOT_SUPPORTED);
        }
    }*/

    logDataEx("L3-App", "Command", "TRX TYPE : %s", reqMessage.sCard.trxtype);
    safe_strcpy(appData.trxType, sizeof(appData.trxType), reqMessage.sCard.trxtype);

    if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_PURCHASE) == 0)
    {
        appData.trxTypeBin = 0x00;
        appData.amountKnownAtStart = 0;
        appData.isCheckDateAvailable = false;

        if (reqMessage.sCard.amountAvailable == 1)
        {
            appData.amountKnownAtStart = 1;
            appData.amountAuthorizedBin = strtol(reqMessage.sCard.amount, NULL, 10);
            logDataEx("L3-App", "Command", "Purchase Amount at start :  %llu.%02llu",
                      appData.amountAuthorizedBin / 100, appData.amountAuthorizedBin % 100);
        }

        if (reqMessage.sCard.isCheckDateAvailable)
        {
            appData.isCheckDateAvailable = true;
            safe_strcpy(appData.checkDate, sizeof(appData.checkDate), reqMessage.sCard.checkDate);
        }
        else
        {
            memset(appData.checkDate, 0, sizeof(appData.checkDate));
        }
    }

    if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_MONEY_ADD) == 0)
    {
        appData.trxTypeBin = 0x28;
        appData.amountKnownAtStart = 0;

        char transactionId[38];
        if (isTherePendingReversal(transactionId) == true)
        {
            logInfoEx("Command", "", "There is a pending reversal, trying to send to host");
            logInfoEx("Command", "", "Transaction id for reversal : %s", transactionId);
            // performReversal(transactionId, true, "E2");
            return buildResponseMessage(STATUS_REVERSAL_PENDING, 0);
        }

        if (reqMessage.sCard.amountAvailable == 1)
        {
            appData.amountKnownAtStart = 1;
            appData.amountAuthorizedBin = strtol(reqMessage.sCard.amount, NULL, 10);
            logDataEx("L3-App", "Command", "Money Add Amount at start :  %llu.%02llu",
                      appData.amountAuthorizedBin / 100, appData.amountAuthorizedBin % 100);

            if (appData.amountAuthorizedBin == 0)
            {
                logDataEx("L3-App", "Command", "Money add, amount is 0");
                return buildResponseMessage(STATUS_ERROR, ERR_MONEY_ADD_AMOUNT_NOT_AVAILABLE);
            }

            // if (isValidTrxMode(reqMessage.mData.trxMode) == false)
            // {
            //     logDataEx("L3-App", "Command", "Money add, invalid trx mode");
            //     return buildResponseMessage(STATUS_ERROR, ERR_MONEY_ADD_WRONG_TYPE);
            // }

            safe_strcpy(appData.moneyAddTrxType, sizeof(appData.moneyAddTrxType), reqMessage.mData.trxMode);
            logDataEx("L3-App", "Command", "Money add transaction type : %s", appData.moneyAddTrxType);

            safe_strcpy(appData.moneyAddTid, sizeof(appData.moneyAddTid), reqMessage.mData.moneyAddTid);
            logDataEx("L3-App", "Command", "Money add tid : %s", appData.moneyAddTid);

            safe_strcpy(appData.moneyAddRRN, sizeof(appData.moneyAddRRN), reqMessage.mData.moneyAddRRn);
            logDataEx("L3-App", "Command", "Money add rrn : %s", appData.moneyAddRRN);

            // if (strlen(reqMessage.mData.sourceTxnId) == 0)
            // {
            //     logDataEx("L3-App", "Command", "Money add source txn id missing");
            //     return buildResponseMessage(STATUS_ERROR, ERR_MONEY_MISSING_SOURCE_TXN);
            // }

            // safe_strcpy(appData.sourceTxnId, reqMessage.mData.sourceTxnId);
            // logDataEx("L3-App", "Command", "Money add source txn id : %s", appData.sourceTxnId);
        }
        else
        {
            logDataEx("L3-App", "Command", "Money add amount missing");
            return buildResponseMessage(STATUS_ERROR, ERR_MONEY_ADD_AMOUNT_NOT_AVAILABLE);
        }
    }

    if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_BALANCE_UPDATE) == 0)
    {
        appData.trxTypeBin = 0x29;

        char transactionId[38];
        if (isTherePendingReversal(transactionId) == true)
        {
            logInfoEx("Command", "", "There is a pending reversal, trying to send to host");
            logInfoEx("Command", "", "Transaction id for reversal : %s", transactionId);
            // performReversal(transactionId, true, "E2");
            return buildResponseMessage(STATUS_REVERSAL_PENDING, 0);
        }
        logInfoEx("Command", "", "There are no pending reversal, going for balance update");
    }

    memset(appData.createServiceId, 0, sizeof(appData.createServiceId));
    if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_SERVICE_CREATE) == 0)
    {
        char transactionId[38];
        if (isTherePendingReversal(transactionId) == true)
        {
            logInfoEx("Command", "", "There is a pending reversal, trying to send to host");
            logInfoEx("Command", "", "Transaction id for reversal : %s", transactionId);
            // performReversal(transactionId, true, "E2");
            return buildResponseMessage(STATUS_REVERSAL_PENDING, 0);
        }

        appData.trxTypeBin = 0x83;
        // appData.amountKnownAtStart = 1;
        appData.isServiceBlock = reqMessage.sCard.isServiceBlock;
        safe_strcpy(appData.createServiceId, sizeof(appData.createServiceId), reqMessage.sCard.serviceId);
    }

    logDataEx("L3-App", "Command", "Transaction Type to be processed : %02x", appData.trxTypeBin);

    logDataEx("L3-App", "Command", "Setting the variable for the thread to run the transaction");
    doLock();
    canRunTransaction = true;
    doUnLock();
    return "";
}

/**
 * To check whether its a valid txn mode
 */
bool isValidTrxMode(const char mode[20])
{
    return strcmp(mode, "CC") == 0 ||
           strcmp(mode, "DC") == 0 ||
           strcmp(mode, "UPI") == 0 ||
           strcmp(mode, "CASH") == 0 ||
           strcmp(mode, "ACCOUNT") == 0;
}

/**
 * Parse the json data and get the message structure
 **/
struct message parseMessage(const char *data)
{
    logDataEx("L3-App", "Command", "Going to parse the message : %s", data);
    struct message reqMessage;
    json_object *jObject = json_tokener_parse(data);

    safe_strcpy(reqMessage.cmd, sizeof(reqMessage.cmd),
                (char *)json_object_get_string(json_object_object_get(jObject, COMMAND)));

    logDataEx("L3-App", "Command", "Command : %s", reqMessage.cmd);

    if (strcmp(reqMessage.cmd, COMMAND_GET_DISK_SPACE) == 0)
    {
        if (json_object_get_string(json_object_object_get(jObject, DISK_SPACE_PATH)) != NULL)
        {
            safe_strcpy(reqMessage.dSpace.path, sizeof(reqMessage.dSpace.path), (char *)json_object_get_string(json_object_object_get(jObject, DISK_SPACE_PATH)));
        }
        else
        {
            logDataEx("L3-App", "Command", "Path is missing, so setting to default /");
            safe_strcpy(reqMessage.dSpace.path, sizeof(reqMessage.dSpace.path), "/");
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_VERIFY_TERMINAL) == 0)
    {
        if (json_object_get_string(json_object_object_get(jObject, VERIFY_TRM_TID)) != NULL)
        {
            safe_strcpy(reqMessage.vData.tid, sizeof(reqMessage.vData.tid), (char *)json_object_get_string(json_object_object_get(jObject, VERIFY_TRM_TID)));
        }

        memset(reqMessage.vData.mid, 0, sizeof(reqMessage.vData.mid));
        if (json_object_get_string(json_object_object_get(jObject, VERIFY_TRM_MID)) != NULL)
        {
            safe_strcpy(reqMessage.vData.mid, sizeof(reqMessage.vData.mid),
                        (char *)json_object_get_string(json_object_object_get(jObject, VERIFY_TRM_MID)));
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_CHANGE_LOG_MODE) == 0)
    {
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_LOG_MODE)) != NULL)
        {
            safe_strcpy(reqMessage.logData.logMode, sizeof(reqMessage.logData.logMode),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_LOG_MODE)));
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_DOWNLOAD_FILE) == 0)
    {
        if (json_object_get_string(json_object_object_get(jObject, DOWNLOAD_FILE_NAME)) != NULL)
        {
            safe_strcpy(reqMessage.dData.fileName, sizeof(reqMessage.dData.fileName),
                        json_object_get_string(json_object_object_get(jObject, DOWNLOAD_FILE_NAME)));
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_DELETE_FILE) == 0)
    {
        if (json_object_get_string(json_object_object_get(jObject, DELETE_FILE_NAME)) != NULL)
        {
            safe_strcpy(reqMessage.delFileData.fileName, sizeof(reqMessage.delFileData.fileName),
                        json_object_get_string(json_object_object_get(jObject, DELETE_FILE_NAME)));
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_CHANGE_IP) == 0)
    {
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_MODE)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.ipMode, sizeof(reqMessage.ipData.ipMode),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_MODE)));
        }

        memset(reqMessage.ipData.dns, 0, sizeof(reqMessage.ipData.dns));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.dns, sizeof(reqMessage.ipData.dns),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS)));
        }

        memset(reqMessage.ipData.dns2, 0, sizeof(reqMessage.ipData.dns2));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS2)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.dns2, sizeof(reqMessage.ipData.dns2),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS2)));
        }

        memset(reqMessage.ipData.dns3, 0, sizeof(reqMessage.ipData.dns3));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS3)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.dns3, sizeof(reqMessage.ipData.dns3),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS3)));
        }

        memset(reqMessage.ipData.dns4, 0, sizeof(reqMessage.ipData.dns4));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS4)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.dns4, sizeof(reqMessage.ipData.dns4),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS4)));
        }

        memset(reqMessage.ipData.searchDomain, 0, sizeof(reqMessage.ipData.searchDomain));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_SEARCH_DOMAIN)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.searchDomain, sizeof(reqMessage.ipData.searchDomain),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_SEARCH_DOMAIN)));
        }

        memset(reqMessage.ipData.ipAddress, 0, sizeof(reqMessage.ipData.ipAddress));
        memset(reqMessage.ipData.netmask, 0, sizeof(reqMessage.ipData.netmask));
        memset(reqMessage.ipData.gateway, 0, sizeof(reqMessage.ipData.gateway));
        if (strcmp(reqMessage.ipData.ipMode, IP_MODE_STATIC) == 0)
        {
            if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_ADDRESS)) != NULL)
            {
                safe_strcpy(reqMessage.ipData.ipAddress, sizeof(reqMessage.ipData.ipAddress),
                            json_object_get_string(json_object_object_get(jObject, CHANGE_IP_ADDRESS)));
            }

            if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_NETMASK)) != NULL)
            {
                safe_strcpy(reqMessage.ipData.netmask, sizeof(reqMessage.ipData.netmask),
                            json_object_get_string(json_object_object_get(jObject, CHANGE_IP_NETMASK)));
            }

            if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_GATEWAY)) != NULL)
            {
                safe_strcpy(reqMessage.ipData.gateway, sizeof(reqMessage.ipData.gateway),
                            json_object_get_string(json_object_object_get(jObject, CHANGE_IP_GATEWAY)));
            }
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_CHANGE_ONLY_DNS) == 0)
    {
        memset(reqMessage.ipData.dns, 0, sizeof(reqMessage.ipData.dns));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.dns, sizeof(reqMessage.ipData.dns),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS)));
        }

        memset(reqMessage.ipData.dns2, 0, sizeof(reqMessage.ipData.dns2));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS2)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.dns2, sizeof(reqMessage.ipData.dns2),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS2)));
        }

        memset(reqMessage.ipData.dns3, 0, sizeof(reqMessage.ipData.dns3));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS3)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.dns3, sizeof(reqMessage.ipData.dns3),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS3)));
        }

        memset(reqMessage.ipData.dns4, 0, sizeof(reqMessage.ipData.dns4));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS4)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.dns4, sizeof(reqMessage.ipData.dns4),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_DNS4)));
        }

        memset(reqMessage.ipData.searchDomain, 0, sizeof(reqMessage.ipData.searchDomain));
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_SEARCH_DOMAIN)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.searchDomain, sizeof(reqMessage.ipData.searchDomain),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_SEARCH_DOMAIN)));
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_CHANGE_ONLY_IP) == 0)
    {
        if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_MODE)) != NULL)
        {
            safe_strcpy(reqMessage.ipData.ipMode, sizeof(reqMessage.ipData.ipMode),
                        json_object_get_string(json_object_object_get(jObject, CHANGE_IP_MODE)));
        }

        memset(reqMessage.ipData.ipAddress, 0, sizeof(reqMessage.ipData.ipAddress));
        memset(reqMessage.ipData.netmask, 0, sizeof(reqMessage.ipData.netmask));
        memset(reqMessage.ipData.gateway, 0, sizeof(reqMessage.ipData.gateway));
        if (strcmp(reqMessage.ipData.ipMode, IP_MODE_STATIC) == 0)
        {
            if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_ADDRESS)) != NULL)
            {
                safe_strcpy(reqMessage.ipData.ipAddress, sizeof(reqMessage.ipData.ipAddress),
                            json_object_get_string(json_object_object_get(jObject, CHANGE_IP_ADDRESS)));
            }

            if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_NETMASK)) != NULL)
            {
                safe_strcpy(reqMessage.ipData.netmask, sizeof(reqMessage.ipData.netmask),
                            json_object_get_string(json_object_object_get(jObject, CHANGE_IP_NETMASK)));
            }

            if (json_object_get_string(json_object_object_get(jObject, CHANGE_IP_GATEWAY)) != NULL)
            {
                safe_strcpy(reqMessage.ipData.gateway, sizeof(reqMessage.ipData.gateway),
                            json_object_get_string(json_object_object_get(jObject, CHANGE_IP_GATEWAY)));
            }
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_SET_TIME) == 0)
    {
        safe_strcpy(reqMessage.tData.time, sizeof(reqMessage.tData.time),
                    (char *)json_object_get_string(json_object_object_get(jObject, TIME_DATA_KEY)));
    }

    if (strcmp(reqMessage.cmd, COMMAND_IS_KEY_PRESENT) == 0 ||
        strcmp(reqMessage.cmd, COMMAND_DESTROY_KEY) == 0)
    {
        safe_strcpy(reqMessage.kData.label, sizeof(reqMessage.kData.label),
                    (char *)json_object_get_string(json_object_object_get(jObject, KEY_DATA_LABEL)));

        json_bool jIsBdk = json_object_get_boolean(json_object_object_get(jObject, KEY_DATA_ISBDK));

        if (jIsBdk == TRUE)
            reqMessage.kData.isBdk = true;
        else
            reqMessage.kData.isBdk = false;
    }

    if (strcmp(reqMessage.cmd, COMMAND_SEARCH_CARD) == 0)
    {
        safe_strcpy(reqMessage.sCard.mode, sizeof(reqMessage.sCard.mode),
                    (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_CARD_MODE)));
        reqMessage.sCard.timeout = json_object_get_int(json_object_object_get(jObject, SEARCH_CARD_TIMEOUT));

        safe_strcpy(reqMessage.sCard.trxtype, sizeof(reqMessage.sCard.trxtype),
                    (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_CARD_TRXTYPE)));
        reqMessage.sCard.amountAvailable = 0;

        if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_PURCHASE) == 0 ||
            strcmp(reqMessage.sCard.trxtype, TRXTYPE_MONEY_ADD) == 0)
        {
            if (json_object_get_string(json_object_object_get(jObject, SEARCH_CARD_AMOUNT)) != NULL)
            {
                reqMessage.sCard.amountAvailable = 1;
                safe_strcpy(reqMessage.sCard.amount, sizeof(reqMessage.sCard.amount),
                            (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_CARD_AMOUNT)));
            }
            else
            {
                logDataEx("L3-App", "Command", "AMOUNT NOT PROVIDED AT START");
            }

            if (json_object_get_string(json_object_object_get(jObject, SEARCH_CARD_CHECK_DATE)) != NULL)
            {
                reqMessage.sCard.isCheckDateAvailable = true;
                safe_strcpy(reqMessage.sCard.checkDate, sizeof(reqMessage.sCard.checkDate),
                            (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_CARD_CHECK_DATE)));
            }
            else
            {
                reqMessage.sCard.isCheckDateAvailable = false;
                logDataEx("L3-App", "Command", "Check date not provided");
            }
        }

        if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_MONEY_ADD) == 0)
        {
            if (json_object_get_string(json_object_object_get(jObject, SEARCH_MONEY_ADD_TYPE)) != NULL)
            {
                safe_strcpy(reqMessage.mData.trxMode, sizeof(reqMessage.mData.trxMode),
                            (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_MONEY_ADD_TYPE)));
            }

            if (json_object_get_string(json_object_object_get(jObject, SEARCH_MONEY_ADD_TID)) != NULL)
            {
                safe_strcpy(reqMessage.mData.moneyAddTid, sizeof(reqMessage.mData.moneyAddTid),
                            (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_MONEY_ADD_TID)));
            }

            if (json_object_get_string(json_object_object_get(jObject, SEARCH_MONEY_ADD_RRN)) != NULL)
            {
                safe_strcpy(reqMessage.mData.moneyAddRRn, sizeof(reqMessage.mData.moneyAddRRn),
                            (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_MONEY_ADD_RRN)));
            }

            // if (json_object_get_string(json_object_object_get(jObject, SEARCH_MONEY_ADD_SOURCE_TXN_ID)) != NULL)
            // {
            //     safe_strcpy(reqMessage.mData.sourceTxnId, (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_MONEY_ADD_SOURCE_TXN_ID)));
            // }
        }

        if (strcmp(reqMessage.sCard.trxtype, TRXTYPE_SERVICE_CREATE) == 0)
        {
            if (json_object_get_string(json_object_object_get(jObject, SEARCH_SERVICE_ID)) != NULL)
            {
                safe_strcpy(reqMessage.sCard.serviceId, sizeof(reqMessage.sCard.serviceId),
                            (char *)json_object_get_string(json_object_object_get(jObject, SEARCH_SERVICE_ID)));
            }

            reqMessage.sCard.isServiceBlock = 0;
            if (json_object_object_get(jObject, SEARCH_IS_SERVICE_BLOCK) != NULL)
            {
                reqMessage.sCard.isServiceBlock = json_object_get_int(json_object_object_get(jObject, SEARCH_IS_SERVICE_BLOCK));
            }
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_GATE_OPEN) == 0)
    {
        logDataEx("L3-App", "Command", "Going to read the gate open status");
        reqMessage.gateOpenData.status = false;
        if (json_object_object_get(jObject, GATE_OPEN_GATE_STATUS) != NULL)
        {
            json_bool jGateStatus = json_object_get_boolean(json_object_object_get(jObject, GATE_OPEN_GATE_STATUS));
            if (jGateStatus == TRUE)
                reqMessage.gateOpenData.status = true;
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_WRITE_CARD) == 0)
    {
        logDataEx("L3-App", "Command", "Going to read the amount");
        memset(reqMessage.wCard.serviceData, 0, 200);

        safe_strcpy(reqMessage.wCard.amount, sizeof(reqMessage.wCard.amount),
                    (char *)json_object_get_string(json_object_object_get(jObject, WRITE_CARD_AMOUNT)));
        if (json_object_get_string(json_object_object_get(jObject, WRITE_CARD_SERVICE_DATA)) == NULL)
        {
            logDataEx("L3-App", "Command", "No service data available");
            isServiceDataAvailable = 0;
            reqMessage.wCard.serviceData[0] = '\0';
        }
        else
        {
            isServiceDataAvailable = 1;
            logDataEx("L3-App", "Command", "Service data provided : %s",
                      (char *)json_object_get_string(json_object_object_get(jObject, WRITE_CARD_SERVICE_DATA)));
            safe_strcpy(reqMessage.wCard.serviceData, sizeof(reqMessage.wCard.serviceData),
                        (char *)json_object_get_string(json_object_object_get(jObject, WRITE_CARD_SERVICE_DATA)));
        }
        logDataEx("L3-App", "Command", "Data read is : %s", reqMessage.wCard.serviceData);
    }

    if (strcmp(reqMessage.cmd, COMMAND_FETCH_AUTH) == 0)
    {
        reqMessage.fData.fetchid = json_object_get_int(json_object_object_get(jObject, FETCH_AUTH_ID));
        reqMessage.fData.maxrecords = json_object_get_int(json_object_object_get(jObject, FETCH_MAX_RECORDS));
        safe_strcpy(reqMessage.fData.mode, sizeof(reqMessage.fData.mode),
                    (char *)json_object_get_string(json_object_object_get(jObject, FETCH_MODE)));

        reqMessage.fData.isOnline = false;
        if (json_object_object_get(jObject, FETCH_MODE_ONLINE) != NULL)
        {
            json_bool jIsOnline = json_object_get_boolean(json_object_object_get(jObject, FETCH_MODE_ONLINE));
            if (jIsOnline == TRUE)
            {
                reqMessage.fData.isOnline = true;
            }
        }
    }

    if (strcmp(reqMessage.cmd, COMMAND_ABT_FETCH) == 0)
    {
        reqMessage.abtFetch.skipRecords = json_object_get_int(json_object_object_get(jObject, FETCH_SKIP));
        reqMessage.abtFetch.maxrecords = json_object_get_int(json_object_object_get(jObject, FETCH_MAX_RECORDS));
        safe_strcpy(reqMessage.abtFetch.mode, sizeof(reqMessage.abtFetch.mode),
                    (char *)json_object_get_string(json_object_object_get(jObject, FETCH_MODE)));
    }

    if (strcmp(reqMessage.cmd, COMMAND_FETCH_ACK) == 0)
    {
        reqMessage.fData.fetchid = json_object_get_int(json_object_object_get(jObject, FETCH_AUTH_ID));
    }

    json_object_put(jObject); // Clear the json memory
    return reqMessage;
}

/**
 * Do a reboot
 **/
int performReboot()
{
    logDataEx("L3-App", "Command", "Going to reboot the terminal");
    int result = fetrm_reboot();
    logDataEx("L3-App", "Command", "Reboot Result : %d", result);
    return result;
}

/**
 * Change the app state and send the message to client
 **/
void changeAppState(enum app_status status)
{
    if (status == appData.status)
    {
        logDataEx("L3-App", "Command", "Current Status : %s ", getStatusString());
        logDataEx("L3-App", "Command", "Both the status are same, so nothing to send");
        return;
    }

    logDataEx("L3-App", "Command", "Current Status : %s ", getStatusString());
    appData.status = status;
    logDataEx("L3-App", "Command", "Changed Status : %s ", getStatusString());

    char *response = buildResponseMessage(getStatusString(), 0);
    send(CLIENT_SOCKET, response, strlen(response), 0);

    // Write to serial port

    if (IS_SERIAL_CONNECTED == 1)
    {
        logInfoEx("Command", "", "Writing status to serial port.");
        write(SERIAL_PORT, response, strlen(response));
    }
    else
    {
        logDataEx("L3-App", "Command", "Serial port not connected, not writing anything");
    }

    free(response);
}
