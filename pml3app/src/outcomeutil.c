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
#include "include/abtmanager.h"

#define EMV_TRACK2 "\x57"
#define EMV_EXPIRY_DATE "\x5F\x24"

extern struct applicationConfig appConfig;
extern struct applicationData appData;
extern struct transactionData currentTxnData;
extern long long startTrxTime;
extern _Atomic int activePendingTxnCount;
extern long long trxEndTime;
extern _Atomic enum device_status DEVICE_STATUS;

extern char currentTrxType[100];
extern char lastPanDigit[10];

/**
 * Get the updated balance
 **/
char *getUpdatedBalance(struct tlv *tlv_outcome)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to get the updated balance");
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
        logErrorEx(currentTrxType, lastPanDigit, "%s: data record format is corrupted", __func__);
        return "";
    }

    tlv_obj = tlv_find(tlv_data_record, EMV_ID_ISSUER_APPLICATION_DATA);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    tlv_free(tlv_data_record);

    logDataEx(currentTrxType, lastPanDigit, "Buffer len : %d", buffer_len);
    logHexData("IAD : ", buffer, buffer_len);

    logDataEx(currentTrxType, lastPanDigit, "Updated balance received");
    char *balance = malloc(13);
    sprintf(balance, "%02X%02X%02X%02X%02X%02X", buffer[11], buffer[12], buffer[13], buffer[14], buffer[15], buffer[16]);
    logDataEx(currentTrxType, lastPanDigit, "Value : %s", balance);
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
        logDataEx(currentTrxType, lastPanDigit, "Its abt so incrementing the transaction counter manually here and setting time");
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
        logDataEx(currentTrxType, lastPanDigit, "%s: outcome format is corrupted", __func__);
        return;
    }

    TransactionTable trxTable;

    // only for offline sale
    if (currentTxnData.trxTypeBin == 0x00)
    {
        logInfoEx(currentTrxType, lastPanDigit, "Local Transaction Counter : %ld", currentTxnData.txnCounter);
        logDataEx(currentTrxType, lastPanDigit, "Already generated transaction id : %s", currentTxnData.transactionId);
        // char *transactionId = malloc(UUID_STR_LEN);
        // generateUUID(transactionId);
        // logInfoEx(currentTrxType, lastPanDigit, "Unique Transaction Id : %s", transactionId);
        // safe_strcpy(currentTxnData.transactionId, transactionId);
        // free(transactionId);

        tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_DATA_RECORD);
        buffer_len = sizeof(buffer);
        memset(buffer, 0x00, sizeof(buffer));
        rc = tlv_encode_value(tlv_obj, buffer, &buffer_len);

        removeIccTags(buffer, buffer_len);

        // byteToHex(buffer, buffer_len, currentTxnData.iccData);
        // currentTxnData.iccDataLen = buffer_len * 2;

        generateOrderId();
        logDataEx(currentTrxType, lastPanDigit, "Order id of txn data : %s", currentTxnData.orderId);
        populateCardExpiry(buffer, buffer_len);

        if (appData.isCheckDateAvailable)
        {
            performCheckDate();
        }
    }
    else
    {
        logInfoEx(currentTrxType, lastPanDigit, "Transaction id already created : %s", currentTxnData.transactionId);
        trxTable = getTransactionTableData(currentTxnData.transactionId);
        if (appConfig.useAirtelHost)
            safe_strcpy(currentTxnData.hostResponseCode, sizeof(currentTxnData.hostResponseCode),
                        trxTable.airtelSwitchResponseCode);
        else
            safe_strcpy(currentTxnData.hostResponseCode, sizeof(currentTxnData.hostResponseCode),
                        trxTable.hostResultCode);

        logDataEx(currentTrxType, lastPanDigit, "Host Response : %s", currentTxnData.hostResponseCode);

        if (strcmp(currentTxnData.trxType, TRXTYPE_BALANCE_UPDATE) == 0 ||
            strcmp(currentTxnData.trxType, TRXTYPE_MONEY_ADD) == 0)
        {
            safe_strcpy(currentTxnData.updatedAmount, sizeof(currentTxnData.updatedAmount), trxTable.updateAmount);
            logDataEx(currentTrxType, lastPanDigit, "Updated Amount Found : %s", currentTxnData.updatedAmount);
            char *updatedBal = getUpdatedBalance(tlv_outcome);
            safe_strcpy(currentTxnData.updatedBalance, sizeof(currentTxnData.updatedBalance), updatedBal);
            free(updatedBal);
            logDataEx(currentTrxType, lastPanDigit, "Updated Balance Found : %s", currentTxnData.updatedBalance);
        }
    }

    if (currentTxnData.isRupayTxn == false && appConfig.enableAbt == true)
    {
        logDataEx(currentTrxType, lastPanDigit, "On Transaction completion, this is an ABT so handle accordingly");
        handleAbtCompletion(outcome, outcomeLen);
        return;
    }

    const char *outcomeStatus = getOutcomeStatus(tlv_outcome);
    logDataEx(currentTrxType, lastPanDigit, "OUTCOME STATUS : %s", outcomeStatus);

    tlv_free(tlv_obj);
    tlv_free(tlv_outcome);

    if (strcmp(outcomeStatus, STATUS_APPROVED) == 0)
    {
        displayLight(LED_ST_CARD_PROCESSED_SUCCESS);
        safe_strcpy(currentTxnData.txnStatus, sizeof(currentTxnData.txnStatus), STATUS_SUCCESS);
    }
    else
    {
        displayLight(LED_ST_CARD_PROCESSED_FAILURE);
        safe_strcpy(currentTxnData.updatedAmount, sizeof(currentTxnData.updatedAmount), "000000000000");
        safe_strcpy(currentTxnData.txnStatus, sizeof(currentTxnData.txnStatus), STATUS_FAILURE);
    }

    printCurrentTxnData(currentTxnData);

    // for offline sale it is created at the last
    // for other types transaction id is already available
    if (currentTxnData.trxTypeBin != 0x00 && currentTxnData.trxTypeBin != 0x31)
    {
        logDataEx(currentTrxType, lastPanDigit, "Going to update the status for the balance update / service creation / money add");
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

    logDataEx(currentTrxType, lastPanDigit, "TXN Status : %s", currentTxnData.txnStatus);
    logDataEx(currentTrxType, lastPanDigit, "Host Response : %s", currentTxnData.hostResponseCode);

    // Only for online scenario
    if (strcmp(currentTxnData.txnStatus, STATUS_FAILURE) == 0 &&
        strcmp(currentTxnData.hostResponseCode, "00") == 0)
    {
        logInfoEx(currentTrxType, lastPanDigit, "Host is success, but card failed, so generating reversal with code : %s", REVERSAL_CODE_E1);
        updateReversalStatusOnly(currentTxnData.transactionId, STATUS_PENDING);
        logInfoEx(currentTrxType, lastPanDigit, "Host is success, but card failed, so generating reversal with E1");
        performReversal(currentTxnData.transactionId, false, "E1");
    }

    logInfoEx(currentTrxType, lastPanDigit, "Transaction completed with status : %s", currentTxnData.txnStatus);
    sendTransactionProcessedMessage(currentTxnData, outcomeStatus);
    displayLight(LED_ST_CARD_PROCESSED_MSG_SENT);

    // only for offline sale
    if (currentTxnData.trxTypeBin == 0x00)
    {
        if (currentTxnData.amount == 0 && appConfig.ignoreZeroValueTxn == true)
        {
            logInfoEx(currentTrxType, lastPanDigit, "Zero value transaction, so it is not store in DB");
            logInfoEx(currentTrxType, lastPanDigit, "This transaction is ignored and not sent to host or Fetch");
        }
        else
        {
            doPanEncryption();
            doLock();
            if (strcmp(currentTxnData.txnStatus, STATUS_SUCCESS) == 0)
            {
                activePendingTxnCount++;
                logDataEx(currentTrxType, lastPanDigit, "Active pending transaction count increased and now : %d", activePendingTxnCount);
                if (activePendingTxnCount > appConfig.maxOfflineTransactions)
                {
                    logWarnEx(currentTrxType, lastPanDigit, "Exceeded the number of maxOfflineTransactions, setting device offline");
                    DEVICE_STATUS = STATUS_OFFLINE;
                }
            }
            printDeviceStatus();
            doUnLock();

            createTransactionData(&currentTxnData);
        }
    }

    // generate log file, this is only for certification testing
    if (appConfig.enableApduLog && appConfig.isDebugEnabled == 1)
    {
        char logFile[50];
        sprintf(logFile, "log_%s", currentTxnData.transactionId);
        logDataEx(currentTrxType, lastPanDigit, "Generating APDU Log");
        generateLog(outcome, outcomeLen, logFile, currentTxnData.txnCounter);
    }
}

/**
 * To perform the pan encryption for offline sale in Hitachi / SBI
 */
void doPanEncryption()
{
    char pan[21];
    safe_strcpy(pan, sizeof(pan), currentTxnData.plainPan);
    logDataEx(currentTrxType, lastPanDigit, "Performing pan encryption for purchase transaction");
    unsigned char ksn[10];
    char hex[256], hex2[100];
    char panWithTag[25];
    safe_strcpy(panWithTag, sizeof(panWithTag), "5A");

    logDataEx(currentTrxType, lastPanDigit, "Mod val : %d", strlen(pan) % 2);
    if (strlen(pan) % 2 != 0)
        safe_strcat(pan, sizeof(pan), "F");

    char panHexLen[12];
    sprintf(panHexLen, "%02X", strlen(pan) / 2);
    logDataEx(currentTrxType, lastPanDigit, "Hex length of Pan : %s", panHexLen);
    safe_strcat(panWithTag, sizeof(panWithTag), panHexLen);
    safe_strcat(panWithTag, sizeof(panWithTag), pan);
    safe_strcpy(currentTxnData.plainPanWithTag, sizeof(currentTxnData.plainPanWithTag), panWithTag);
    logDataEx(currentTrxType, lastPanDigit, "Pan with tag used for encryption : %s", currentTxnData.plainPanWithTag);

    encryptPan(panWithTag, ksn, hex);
    logDataEx(currentTrxType, lastPanDigit, "Encrypt Result : %s", hex);
    byteToHex(ksn, 10, hex2);
    logDataEx(currentTrxType, lastPanDigit, "KSN Received : %s", hex2);
    logDataEx(currentTrxType, lastPanDigit, "KSN Len : %d", strlen(hex2));
    safe_strcpy(currentTxnData.ksn, sizeof(currentTxnData.ksn), "00000000000000000000");
    safe_strcat(currentTxnData.ksn, sizeof(currentTxnData.ksn), hex2);
    logDataEx(currentTrxType, lastPanDigit, "TXN Data KSN Value : %s", currentTxnData.ksn);
    logDataEx(currentTrxType, lastPanDigit, "TXN Data KSN Len : %d", strlen(currentTxnData.ksn));

    safe_strcpy(currentTxnData.panEncrypted, sizeof(currentTxnData.panEncrypted), hex);
    logDataEx(currentTrxType, lastPanDigit, "TXN Pan Encrypted Value : %s", currentTxnData.panEncrypted);
    logDataEx(currentTrxType, lastPanDigit, "TXN Pan Encrypted Len : %d", strlen(currentTxnData.panEncrypted));
}

void performCheckDate()
{
    logDataEx(currentTrxType, lastPanDigit, "Performing check date");
    // logDataEx(currentTrxType, lastPanDigit, "Expiry Date : %s", currentTxnData.plainExpDate);
    logDataEx(currentTrxType, lastPanDigit, "Check Date : %s", currentTxnData.checkDate);

    char fullExpiry[9];
    safe_strcpy(fullExpiry, sizeof(fullExpiry), "20");
    safe_strcat(fullExpiry, sizeof(fullExpiry), currentTxnData.plainExpDate);
    // logDataEx(currentTrxType, lastPanDigit, "Full Expiry : %s", fullExpiry);

    char checkDateClean[9] = {0};
    int j = 0;
    for (int i = 0; currentTxnData.checkDate[i] != '\0'; i++)
    {
        if (currentTxnData.checkDate[i] != '-')
        {
            checkDateClean[j++] = currentTxnData.checkDate[i];
        }
    }
    checkDateClean[j] = '\0';
    logDataEx(currentTrxType, lastPanDigit, "Clean Check Date : %s", checkDateClean);

    long long expVal = atoll(fullExpiry);
    long long chkVal = atoll(checkDateClean);

    // logDataEx(currentTrxType, lastPanDigit, "Expiry (long): %lld", expVal);
    logDataEx(currentTrxType, lastPanDigit, "Check  (long): %lld", chkVal);

    if (expVal >= chkVal)
        currentTxnData.checkDateResult = 1; // valid
    else
        currentTxnData.checkDateResult = 0; // expired

    logDataEx(currentTrxType, lastPanDigit, "Check Date Result : %d", currentTxnData.checkDateResult);
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
    // logDataEx(currentTrxType, lastPanDigit, "Expiry Date : %s", expDate); // TODO : Remove
    tlv_free(tlv_date);
    tlv_free(tlv_date_outcome);
    safe_strcpy(currentTxnData.plainExpDate, sizeof(currentTxnData.plainExpDate), expDate);
    // logDataEx(currentTrxType, lastPanDigit, "Plain Expiry Date : %s", currentTxnData.plainExpDate);
}

/**
 * Handle the completion when the card is not presented again and do reversal
 */
void handleSecondTapNotPresented()
{
    logInfoEx(currentTrxType, lastPanDigit, "Second tap card not presented, txn to be failed and reversal to be generated");
    logInfoEx(currentTrxType, lastPanDigit, "Second tap, transaction id already created : %s", currentTxnData.transactionId);
    TransactionTable trxTable = getTransactionTableData(currentTxnData.transactionId);

    // Update the reversal status to pending
    updateReversalStatusOnly(currentTxnData.transactionId, STATUS_PENDING);

    safe_strcpy(currentTxnData.updatedAmount, sizeof(currentTxnData.updatedAmount), trxTable.updateAmount);
    safe_strcpy(currentTxnData.hostResponseCode, sizeof(currentTxnData.hostResponseCode), trxTable.hostResultCode);
    logDataEx(currentTrxType, lastPanDigit, "Updated Amount : %s", currentTxnData.updatedAmount);
    logDataEx(currentTrxType, lastPanDigit, "Host Response : %s", currentTxnData.hostResponseCode);
    safe_strcpy(currentTxnData.updatedAmount, sizeof(currentTxnData.updatedAmount), "000000000000");
    safe_strcpy(currentTxnData.txnStatus, sizeof(currentTxnData.txnStatus), STATUS_FAILURE);

    logDataEx(currentTrxType, lastPanDigit, "Going to update the status for the balance update");
    // Update data for balance update
    updateTransactionStatus(currentTxnData.transactionId, currentTxnData.txnStatus, trxTable.updatedBalance);

    if (appConfig.isTimingEnabled)
    {
        long long endTrxTime = getCurrentSeconds();
        logTimeWarnData("Transaction processing complete : %lld\n", endTrxTime);
        logTimeWarnData("Total Time taken : %lld\n", endTrxTime - startTrxTime);
    }

    logDataEx(currentTrxType, lastPanDigit, "TXN Status : %s", currentTxnData.txnStatus);
    logDataEx(currentTrxType, lastPanDigit, "Host Response : %s", currentTxnData.hostResponseCode);

    if (strcmp(currentTxnData.trxType, TRXTYPE_SERVICE_CREATE) == 0)
    {
        logErrorEx(currentTrxType, lastPanDigit, "Service creation host success, but card failed, no need for any reversal");
    }
    else
    {
        if (strcmp(currentTxnData.txnStatus, STATUS_FAILURE) == 0 &&
            strcmp(currentTxnData.hostResponseCode, "00") == 0)
        {
            logInfoEx(currentTrxType, lastPanDigit, "Host is success, but card failed, so generating reversal with code : %s", REVERSAL_CODE_E2);
            updateReversalStatusOnly(currentTxnData.transactionId, STATUS_PENDING);
            logInfoEx(currentTrxType, lastPanDigit, "Host is success, but card failed, so generating reversal");
            performReversal(currentTxnData.transactionId, false, "E2");
        }
    }

    logInfoEx(currentTrxType, lastPanDigit, "Transaction completed with status : %s", currentTxnData.txnStatus);
    sendTransactionProcessedMessage(currentTxnData, "DECLINED");
    displayLight(LED_ST_CARD_PROCESSED_MSG_SENT);
}