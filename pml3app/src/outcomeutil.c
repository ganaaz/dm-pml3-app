#define _XOPEN_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpay/tlv.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <libpay/tlv.h>
#include <feig/emvco_ep.h>
#include <feig/emvco_tags.h>
#include <feig/feig_tags.h>
#include <feig/feig_e2ee_tags.h>
#include <feig/feig_trace_tags.h>
#include <feig/buzzer.h>
#include <feig/leds.h>
#include <libpay/tlv.h>

#include "include/outcomeutil.h"
#include "include/logutil.h"
#include "include/config.h"
#include "include/commandmanager.h"
#include "include/commonutil.h"
#include "include/hostmanager.h"
#include "include/responsemanager.h"
#include "include/appcodes.h"
#include "include/tlvhelper.h"
#include "include/dukpt-util.h"
#include "JHost/jhost_interface.h"
#include "JAirtelHost/jairtel_host_interface.h"
#include "include/abtmanager.h"

#define EMV_TRACK2 "\x57"
#define EMV_EXPIRY_DATE "\x5F\x24"

extern struct applicationConfig appConfig;
extern struct applicationData appData;
extern struct transactionData currentTxnData;
extern long long startTrxTime;
extern int activePendingTxnCount;
extern long long trxEndTime;
extern enum device_status DEVICE_STATUS;

/**
 * Get the updated balance
 **/
char *getUpdatedBalance(struct tlv *tlv_outcome)
{
    logData("Going to get the updated balance");
    struct tlv *tlv_obj = NULL;
    uint8_t buffer[1024];
    size_t buffer_len = sizeof(buffer);

    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_DATA_RECORD);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);

    struct tlv *tlv_data_record = NULL;

    int rc = tlv_parse(buffer, buffer_len, &tlv_data_record);
    if (TLV_RC_OK != rc)
    {
        logError("%s: data record format is corrupted", __func__);
        return "";
    }

    tlv_obj = tlv_find(tlv_data_record, EMV_ID_ISSUER_APPLICATION_DATA);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    tlv_free(tlv_data_record);

    logData("Buffer len : %d", buffer_len);
    logHexData("IAD : ", buffer, buffer_len);

    logData("Updated balance received");
    char *balance = malloc(13);
    sprintf(balance, "%02X%02X%02X%02X%02X%02X", buffer[11], buffer[12], buffer[13], buffer[14], buffer[15], buffer[16]);
    logData("Value : %s", balance);
    return balance;
}

/**
 * Handle the transaction completion message
 **/
void handleTransactionCompletion(const void *outcome, size_t outcomeLen)
{
    if ((NULL == outcome) || (0 == outcomeLen))
    {
        return;
    }

    if (currentTxnData.isRupayTxn == false && appConfig.enableAbt == true)
    {
        logData("Its abt so incrementing the transaction counter manually here and setting time");
        currentTxnData = updateTransactionDateTime(currentTxnData);
        incrementTransactionCounter();
    }

    int rc = TLV_RC_OK;
    struct tlv *tlv_outcome = NULL;
    struct tlv *tlv_obj = NULL;
    uint8_t buffer[4 * 1024];
    size_t buffer_len = sizeof(buffer);

    rc = tlv_parse(outcome, outcomeLen, &tlv_outcome);
    if (TLV_RC_OK != rc)
    {
        logData("%s: outcome format is corrupted", __func__);
        return;
    }

    TransactionTable trxTable;

    // only for offline sale
    if (currentTxnData.trxTypeBin == 0x00)
    {
        logInfo("Local Transaction Counter : %ld", currentTxnData.txnCounter);
        logData("Already generated transaction id : %s", currentTxnData.transactionId);
        // char *transactionId = malloc(UUID_STR_LEN);
        // generateUUID(transactionId);
        // logInfo("Unique Transaction Id : %s", transactionId);
        // strcpy(currentTxnData.transactionId, transactionId);
        // free(transactionId);

        tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_DATA_RECORD);
        buffer_len = sizeof(buffer);
        memset(buffer, 0x00, sizeof(buffer));
        rc = tlv_encode_value(tlv_obj, buffer, &buffer_len);

        removeIccTags(buffer, buffer_len);

        // byteToHex(buffer, buffer_len, currentTxnData.iccData);
        // currentTxnData.iccDataLen = buffer_len * 2;

        generateOrderId();
        logData("Order id of txn data : %s", currentTxnData.orderId);
        populateCardExpiry(buffer, buffer_len);

        if (appData.isCheckDateAvailable)
        {
            performCheckDate();
        }
    }
    else
    {
        logInfo("Transaction id already created : %s", currentTxnData.transactionId);
        trxTable = getTransactionTableData(currentTxnData.transactionId);
        if (appConfig.useAirtelHost)
            strcpy(currentTxnData.hostResponseCode, trxTable.airtelSwitchResponseCode);
        else
            strcpy(currentTxnData.hostResponseCode, trxTable.hostResultCode);

        logData("Host Response : %s", currentTxnData.hostResponseCode);

        if (strcmp(currentTxnData.trxType, TRXTYPE_BALANCE_UPDATE) == 0 ||
            strcmp(currentTxnData.trxType, TRXTYPE_MONEY_ADD) == 0)
        {
            strcpy(currentTxnData.updatedAmount, trxTable.updateAmount);
            logData("Updated Amount Found : %s", currentTxnData.updatedAmount);
            char *updatedBal = getUpdatedBalance(tlv_outcome);
            strcpy(currentTxnData.updatedBalance, updatedBal);
            free(updatedBal);
            logData("Updated Balance Found : %s", currentTxnData.updatedBalance);
        }
    }

    if (currentTxnData.isRupayTxn == false && appConfig.enableAbt == true)
    {
        logData("On Transaction completion, this is an ABT so handle accordingly");
        handleAbtCompletion(outcome, outcomeLen);
        return;
    }

    const char *outcomeStatus = getOutcomeStatus(tlv_outcome);
    logData("OUTCOME STATUS : %s", outcomeStatus);

    tlv_free(tlv_obj);
    tlv_free(tlv_outcome);

    if (strcmp(outcomeStatus, STATUS_APPROVED) == 0)
    {
        displayLight(LED_ST_CARD_PROCESSED_SUCCESS);
        strcpy(currentTxnData.txnStatus, STATUS_SUCCESS);
    }
    else
    {
        displayLight(LED_ST_CARD_PROCESSED_FAILURE);
        strcpy(currentTxnData.updatedAmount, "000000000000");
        strcpy(currentTxnData.txnStatus, STATUS_FAILURE);
    }

    printCurrentTxnData(currentTxnData);

    // for offline sale it is created at the last
    // for other types transaction id is already available
    if (currentTxnData.trxTypeBin != 0x00 && currentTxnData.trxTypeBin != 0x31)
    {
        logData("Going to update the status for the balance update / service creation / money add");
        updateTransactionStatus(currentTxnData.transactionId, currentTxnData.txnStatus, currentTxnData.updatedBalance);
    }

    long long endTrxTime = getCurrentSeconds();
    if (appConfig.isTimingEnabled)
    {
        logTimeWarnData("Transaction Completion End : %lld", endTrxTime);
        logTimeWarnData("Time taken for compeltion part : %lld", (endTrxTime - trxEndTime));
        logTimeWarnData("Transaction processing completed : %lld\n", endTrxTime);

        logTimeWarnData("Total time taken : %lld", endTrxTime - startTrxTime);
        printf("Total time taken : %lld\n", endTrxTime - startTrxTime);
        if (appConfig.isPrintDetailTimingLogs)
        {
            printTimeLogData();
        }

        clearTimeLogData();
        initTimeLogData();
    }

    logData("TXN Status : %s", currentTxnData.txnStatus);
    logData("Host Response : %s", currentTxnData.hostResponseCode);

    // Only for online scenario
    if (strcmp(currentTxnData.txnStatus, STATUS_FAILURE) == 0 &&
        strcmp(currentTxnData.hostResponseCode, "00") == 0)
    {
        logInfo("Host is success, but card failed, so generating reversal with code : %s", REVERSAL_CODE_E1);
        if (appConfig.useAirtelHost)
        {
            strcpy(trxTable.reversalMac, "NoMac");
        }
        else
        {
            trxTable = generateMacReversal(trxTable, REVERSAL_CODE_E1);
            trxTable = generateMacEcho(trxTable, REVERSAL_CODE_E1);
        }
        strcpy(trxTable.reversalInputResponseCode, REVERSAL_CODE_E1);
        strcpy(trxTable.reversalStatus, STATUS_PENDING);
        updateReversalPreData(trxTable);
        // performReversal(trxTable);
    }

    logInfo("Transaction completed with status : %s", currentTxnData.txnStatus);
    sendTransactionProcessedMessage(currentTxnData, outcomeStatus);
    displayLight(LED_ST_CARD_PROCESSED_MSG_SENT);

    // only for offline sale
    if (currentTxnData.trxTypeBin == 0x00)
    {
        if (currentTxnData.amount == 0 && appConfig.ignoreZeroValueTxn == true)
        {
            logInfo("Zero value transaction, so it is not store in DB");
            logInfo("This transaction is ignored and not sent to host or Fetch");
        }
        else
        {
            doLock();
            if (strcmp(currentTxnData.txnStatus, STATUS_SUCCESS) == 0)
            {
                activePendingTxnCount++;
                logData("Active pending transaction count increased and now : %d", activePendingTxnCount);
                if (activePendingTxnCount > appConfig.maxOfflineTransactions)
                {
                    logWarn("Exceeded the number of maxOfflineTransactions, setting device offline");
                    DEVICE_STATUS = STATUS_OFFLINE;
                }
            }
            printDeviceStatus();
            doUnLock();

            encryptPanExpDate();
            if (!appConfig.useAirtelHost)
            {
                generateMacOfflineSale(currentTxnData);
            }
            createTransactionData(&currentTxnData);
        }
    }

    // generate log file, this is only for certification testing
    if (appConfig.enableApduLog && appConfig.isDebugEnabled == 1)
    {
        char logFile[50];
        sprintf(logFile, "log_%s", currentTxnData.transactionId);
        logData("Generating APDU Log");
        generateLog(outcome, outcomeLen, logFile, currentTxnData.txnCounter);
    }
}

void performCheckDate()
{
    logData("Expiry Date : %s", currentTxnData.plainExpDate);
    logData("Check Date : %s", currentTxnData.checkDate);

    struct tm ckDate = {0}, expDate = {0};
    strptime(currentTxnData.checkDate, "%Y-%m-%d", &ckDate);
    logData("Year : %d", ckDate.tm_year);
    logData("Month : %d", ckDate.tm_mon);
    logData("Day : %d", ckDate.tm_mday);

    strptime(currentTxnData.plainExpDate, "%y%m%d", &expDate);
    // logData("Year : %d", expDate.tm_year);
    // logData("Month : %d", expDate.tm_mon);
    // logData("Day : %d", expDate.tm_mday);

    time_t start_time, end_time;
    start_time = mktime(&expDate);
    end_time = mktime(&ckDate);

    double seconds = difftime(start_time, end_time);
    logData("Seconds : %f", seconds);
    if (seconds > 0)
    {
        currentTxnData.checkDateResult = 1;
    }
    else
    {
        currentTxnData.checkDateResult = 0;
    }
}

void populateCardExpiry(uint8_t *buffer, size_t buffer_len)
{
    uint8_t dataBuffer[10];
    size_t dataBufferLen = sizeof(dataBuffer);
    char expDate[7];
    struct tlv *tlv_date_outcome;
    tlv_parse(buffer, buffer_len, &tlv_date_outcome);
    struct tlv *tlv_date = tlv_find(tlv_date_outcome, EMV_EXPIRY_DATE);
    dataBufferLen = sizeof(dataBuffer);
    memset(dataBuffer, 0x00, sizeof(dataBuffer));
    tlv_encode_value(tlv_date, dataBuffer, &dataBufferLen);
    byteToHex(dataBuffer, dataBufferLen, expDate);
    // logData("Expiry Date : %s", expDate); // TODO : Remove
    tlv_free(tlv_date);
    tlv_free(tlv_date_outcome);
    strcpy(currentTxnData.plainExpDate, expDate);
    // logData("Plain Expiry Date : %s", currentTxnData.plainExpDate);
}

/**
 * Handle the completion when the card is not presented again and do reversal
 */
void handleSecondTapNotPresented()
{
    logInfo("Second tap card not presented, txn to be failed and reversal to be generated");
    logInfo("Second tap, transaction id already created : %s", currentTxnData.transactionId);
    TransactionTable trxTable = getTransactionTableData(currentTxnData.transactionId);

    strcpy(currentTxnData.updatedAmount, trxTable.updateAmount);
    if (appConfig.useAirtelHost)
        strcpy(currentTxnData.hostResponseCode, trxTable.airtelSwitchResponseCode);
    else
        strcpy(currentTxnData.hostResponseCode, trxTable.hostResultCode);

    logData("Updated Amount : %s", currentTxnData.updatedAmount);
    logData("Host Response : %s", currentTxnData.hostResponseCode);
    strcpy(currentTxnData.updatedAmount, "000000000000");
    strcpy(currentTxnData.txnStatus, STATUS_FAILURE);

    logData("Going to update the status for the  balance update / money add / service creation");
    // Update data for balance update / money add / service creation
    updateTransactionStatus(currentTxnData.transactionId, currentTxnData.txnStatus, trxTable.updatedBalance);

    if (appConfig.isTimingEnabled)
    {
        long long endTrxTime = getCurrentSeconds();
        logTimeWarnData("Transaction processing complete : %lld\n", endTrxTime);
        logTimeWarnData("Total Time taken : %lld\n", endTrxTime - startTrxTime);
    }

    logData("TXN Status : %s", currentTxnData.txnStatus);
    logData("Host Response : %s", currentTxnData.hostResponseCode);

    if (strcmp(currentTxnData.txnStatus, STATUS_FAILURE) == 0 &&
        strcmp(currentTxnData.hostResponseCode, "00") == 0)
    {
        logInfo("Host is success, but card failed, so generating reversal with code : %s", REVERSAL_CODE_E2);
        if (appConfig.useAirtelHost)
        {
            strcpy(trxTable.reversalMac, "NoMac");
        }
        else
        {
            trxTable = generateMacReversal(trxTable, REVERSAL_CODE_E2);
            trxTable = generateMacEcho(trxTable, REVERSAL_CODE_E2);
        }
        strcpy(trxTable.reversalInputResponseCode, REVERSAL_CODE_E2);
        strcpy(trxTable.reversalStatus, STATUS_PENDING);
        updateReversalPreData(trxTable);
        // performReversal(trxTable);
    }

    logInfo("Transaction completed with status : %s", currentTxnData.txnStatus);
    sendTransactionProcessedMessage(currentTxnData, "DECLINED");
    displayLight(LED_ST_CARD_PROCESSED_MSG_SENT);
}