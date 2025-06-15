#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <uuid/uuid.h>

#include <feig/fetpf.h>
#include <libpay/tlv.h>
#include <feig/feig_tags.h>
#include <feig/feig_e2ee_tags.h>
#include <feig/feig_trace_tags.h>
#include <feig/emvco_ep.h>
#include <feig/emvco_tags.h>
#include <feig/sdk.h>
#include <feig/fetrm.h>

#include "include/logutil.h"
#include "include/commonutil.h"
#include "include/pkcshelper.h"
#include "include/hostmanager.h"
#include "include/config.h"
#include "include/feiginit.h"
#include "include/feigtransaction.h"
#include "include/tlvhelper.h"
#include "include/outcomeutil.h"
#include "include/dataexchange.h"
#include "include/rupayservice.h"
#include "include/responsemanager.h"
#include "include/commandmanager.h"
#include "include/dboperations.h"
#include "include/appcodes.h"
#include "include/dukpt-util.h"
#include "ISO/ISO8583_interface.h"
#include "ISO/log.h"
#include "ISO/utils.h"
#include "JHost/jhost_interface.h"

#define EMVCO_MAX_OUTCOME_LENGTH 4096
#define EMVCO_MAX_UI_REQUEST_LENGTH 1024
#define EMVCO_MAX_ONLINE_RESPONSE_LENGTH 1024
#define EMV_TRACK2 "\x57"
#define EMV_TERMINAL_FLOOR_LIMT "\x9F\x1B"
#define FETPF_PROCEED_DONE_WITHOUT_CARD_REMOVAL "\xDF\xFE\xF1\x00\x01\x02\xDF\xFE\xF1\x02\x03\x00\x00\x00"
#define FETPF_PROCEED_TRY_NEXT_WITH_CARD_FOUND_CB_AT_EVERY_RETRY "\xDF\xFE\xF1\x00\x01\x04"

extern struct applicationData appData;
extern struct applicationConfig appConfig;
extern struct pkcs11 *crypto;
extern struct fetpf *fetpf;
extern pthread_attr_t threadAttr;
extern enum device_status DEVICE_STATUS;
extern pthread_mutex_t lockFeigTrx;

struct transactionData currentTxnData;
bool canRunTransaction = false;
bool isWaitingForFirstEvent = false;
long long startTrxTime;
long long deEvent;
long long deEndEvent;
long long deAzCompletedTime;
long long trxEndTime;

/**
 * Initiate a transaction
 **/
void *processTransaction(void *arg)
{
    logInfo("Starting Thread processTransaction");

    int retVal = 0;
    int rc = 0;
    uint64_t amount_authorized_bin = 0;
    const struct fetpf_ep_transaction_callbacks fetpfCallBacks = {
        .size = sizeof(struct fetpf_ep_transaction_callbacks),
        .surrender = callBackSurrender,
        .card_found = NULL,
        .application_selection = NULL,
        .track2 = callBacktrack2,
        .online_request = callBackOnlineRequest,
        .data_exchange = callBackDataExchange,
        .final_outcome = callBackFinalOutcome,
        .pin_entry = NULL,
        .card_removed = NULL,
        .type_approval_event = NULL,
    };
    int index = 1;

    // Infinite loop to run the transaction and wait for card
    while (1)
    {
        bool canRun = false;
        enum device_status devStatus;
        pthread_mutex_lock(&lockFeigTrx);
        canRun = canRunTransaction;
        devStatus = DEVICE_STATUS;
        pthread_mutex_unlock(&lockFeigTrx);

        // If there is no request to run then dont run anything
        if (canRun == false)
        {
            continue;
        }

        if (devStatus == STATUS_OFFLINE)
        {
            continue;
        }

        logInfo("Going to run the transaction");
        displayLight(LED_ST_AWAITING_CARD_SUCCESS);

        amount_authorized_bin = appData.amountAuthorizedBin;
        changeAppState(APP_STATUS_AWAIT_CARD);
        resetTransactionData();
        isWaitingForFirstEvent = true;

        updateServiceType(appData.trxTypeBin, appData.currencyCode,
                          appData.PRMAcqKeyIndex, appData.serviceBalanceLimit);

        populateTrxDetails(amount_authorized_bin);

        // Perform the actual transaction process
        void *data = NULL;
        size_t dataLen = 0;
        uint8_t outcome[EMVCO_MAX_OUTCOME_LENGTH * 2] = {0};
        uint8_t onlineResponse[EMVCO_MAX_ONLINE_RESPONSE_LENGTH] = {0};
        size_t onlineResponseLen = sizeof(onlineResponse);
        size_t outcome_len = sizeof(outcome);
        struct tlv *tlv_pre_outcome_tmp = NULL;
        struct tlv *tlv_outcome_tmp = NULL;
        struct tlv *tlv = NULL;

        logData("Amount = %llu,%02llu Currency Code : %0llu\n",
                amount_authorized_bin / 100,
                amount_authorized_bin % 100,
                appData.currCodeBin);

        logData("Doing pre processing ...");

        rc = generatePreProcessData(&data, &dataLen);
        if (rc)
        {
            logWarn("Preprocess data generation failed");
            break;
        }

        rc = fetpf_ep_preprocess(fetpf, data, dataLen, outcome, &outcome_len);
        free(data);
        data = NULL;

        if (rc != FETPF_RC_OK)
        {
            logError("fetpf_ep_preprocess failed with rc :  %d", rc);
            break;
        }

        tlv_parse(outcome, outcome_len, &tlv_pre_outcome_tmp);
        tlv_outcome_tmp = tlv_find(tlv_pre_outcome_tmp, FEIG_ID_OUTCOME);
        tlv_free(tlv_pre_outcome_tmp);

        if (NULL != tlv_outcome_tmp)
        {
            processOutcome(outcome, outcome_len);
            goto next;
        }

        logData("Preprocess success, now going to poll for the card");
        memset(outcome, 0, sizeof(outcome));
        outcome_len = sizeof(outcome);

        tlv = buildTransactionTlv();
        rc = serializeTlv(tlv, &data, &dataLen);
        if (rc)
        {
            goto next;
        }
        tlv_free(tlv);
        tlv = NULL;

        int timeOut = appData.searchTimeout;

        if (appData.searchMode == SEARCH_MODE_LOOP)
        {
            timeOut = 30 * 1000;
        }

        logInfo("Waiting for the card in the reader \n");
        logInfo("Search time out : %d milli seconds", timeOut);

        if (appConfig.isTimingEnabled)
        {
            long long start = getCurrentSeconds();
            logTimeWarnData("Before invoking the fetpf_ep_transaction : %lld", start);
        }

        rc = fetpf_ep_transaction(fetpf, data, dataLen, outcome, &outcome_len,
                                  onlineResponse, &onlineResponseLen, timeOut, &fetpfCallBacks);

        logInfo("Result of Transaction : %d", rc);
        trxEndTime = getCurrentSeconds();
        if (appConfig.isTimingEnabled)
        {
            logTimeWarnData("Transaction api completed : %lld", trxEndTime);
            logTimeWarnData("Time taken by kernel to complete the api : %lld", (trxEndTime - deEndEvent));
            logTimeWarnData("After feig completion : %lld", getCurrentSeconds());
        }

        if (rc != 0 && outcome_len == 0 && checkIsInSecondTap() == true)
        {
            handleSecondTapNotPresented();
        }
        else
        {
            handleTransactionCompletion(outcome, outcome_len);
        }

        free(data);
        data = NULL;

        // Only for debug printing
        processOutcome(outcome, outcome_len);
        logError("Completed count : %d", index++);

    next:
        pthread_mutex_lock(&lockFeigTrx);

        if (appData.searchMode == SEARCH_MODE_SINGLE || retVal == EXIT_FAILURE)
        {
            logInfo("Thread stop requested, so setting the value of not to run txn");
            canRunTransaction = false;
            logInfo("Setting app status to Ready now");
            changeAppState(APP_STATUS_READY);
            displayLight(LED_ST_APP_STARTED);
        }
        else if (canRunTransaction == false)
        {
            logInfo("Cancel requested, so not running the transaction.");
            changeAppState(APP_STATUS_READY);
            displayLight(LED_ST_APP_STARTED);
        }
        else
        {
            logInfo("Going to wait for another card");
        }

        if (appConfig.isTimingEnabled)
        {
            clearTimeLogData();
            initTimeLogData();
        }

        if (DEVICE_STATUS == STATUS_OFFLINE)
        {
            logData("Device status is offline in feigtransaction. sending message");
            sendDeviceOfflineMessage();
        }

        pthread_mutex_unlock(&lockFeigTrx);
        isWaitingForFirstEvent = false;
    }

    if (NULL != fetpf)
    {
        fetpf_free(fetpf);
    }

    pkcs11_free(&crypto);
    return (void *)retVal;
}

int closed_loop_card_found(struct fetpf *client,
                           uint64_t tech, const void *in_data, size_t in_data_len,
                           void *out_data, size_t *out_data_len)
{
    /*
    memcpy(out_data, FETPF_PROCEED_DONE_WITHOUT_CARD_REMOVAL,
        sizeof(FETPF_PROCEED_DONE_WITHOUT_CARD_REMOVAL) - 1);
    *out_data_len = sizeof(FETPF_PROCEED_DONE_WITHOUT_CARD_REMOVAL) - 1;
    */
    (void)client;
    fprintf(stderr, "%s() called\n", __func__);

    *out_data_len = 6;
    /* Set new value to tell FETPF that card_found callback is needed again*/
    printf("\t++++ FETPF_PROCEED_TRY_NEXT_WITH_CARD_FOUND_CB_AT_EVERY_RETRY ++++\n");

    memcpy(out_data, FETPF_PROCEED_TRY_NEXT_WITH_CARD_FOUND_CB_AT_EVERY_RETRY, 6);

    return FETPF_RC_OK;
}

int callBackFinalOutcome(struct fetpf *client, const void *outcome,
                         size_t outcome_len, const void *online_response, size_t online_response_len,
                         void *out_data, size_t *out_data_len)
{
    (void)client;
    (void)online_response;
    (void)online_response_len;
    (void)out_data;
    logData("%s() called this time", __func__);

    processOutcome(outcome, outcome_len);

    if (appConfig.autoReadCard)
    {
        *out_data_len = 14;
        logData("Auto read card is set");
        memcpy(out_data, FETPF_PROCEED_RESTART_WITH_CARD_RESET, 14);
    }
    else
    {
        logData("Auto read card is NOT set");
    }
    return FETPF_RC_OK;
}

/**
 * Update amount, trx type, trx bin, processing code of
 * current transaction
 **/
void populateTrxDetails(uint64_t amount_authorized_bin)
{
    currentTxnData.amount = amount_authorized_bin;
    currentTxnData.trxTypeBin = appData.trxTypeBin;
    char sAmount[13];
    convertAmount(currentTxnData.amount, sAmount);
    strcpy(currentTxnData.sAmount, sAmount);
    strcpy(currentTxnData.trxType, appData.trxType);

    if (appData.isCheckDateAvailable)
        strcpy(currentTxnData.checkDate, appData.checkDate);

    if (currentTxnData.trxTypeBin == 0x00)
        strcpy(currentTxnData.processingCode, "000000");
    if (currentTxnData.trxTypeBin == 0x83)
        strcpy(currentTxnData.processingCode, "830000");
    if (currentTxnData.trxTypeBin == 0x31)
        strcpy(currentTxnData.processingCode, "");
    if (currentTxnData.trxTypeBin == 0x29)
        strcpy(currentTxnData.processingCode, "840000");
    if (currentTxnData.trxTypeBin == 0x28)
        strcpy(currentTxnData.processingCode, "820000");

    currentTxnData.txnCounter = appData.transactionCounter;
    char stan[7];
    snprintf(stan, 7, "%06ld", currentTxnData.txnCounter);
    logData("Stan : %s", stan);
    strcpy(currentTxnData.stan, stan);
    strcpy(currentTxnData.moneyAddTrxType, appData.moneyAddTrxType);
    strcpy(currentTxnData.sourceTxnId, appData.sourceTxnId);
}

/**
 * Read the limit from the config file and used for validation
 * when the write amount is received
 **/
/*
void readAndStoreLimit(void *config, size_t configLen)
{
    struct tlv *tlv_outcome = NULL;
    tlv_parse(config, configLen, &tlv_outcome);
    struct tlv *tlvData = tlv_deep_find(tlv_outcome, FEIG_ID_EP_READER_CONTACTLESS_TRANSACTION_LIMIT);
    uint8_t buffer[6];
    size_t bufferLen = sizeof(buffer);
    char limitStr[13] = "";
    bufferLen = sizeof(buffer);
    memset(buffer, 0x00, sizeof(buffer));
    tlv_encode_value(tlvData, buffer, &bufferLen);
    for (int i = 0; i < bufferLen; i++)
    {
        char temp[3];
        sprintf(temp, "%02X", ((uint8_t *)buffer)[i]);
        strcat(limitStr, temp);
    }

    logData("Contactless limit string : %s", limitStr);
    appConfig.contactlessLimit = strtol(limitStr, NULL, 10);

    logData("Contactless Limit : %llu.%02llu", appConfig.contactlessLimit/100, appConfig.contactlessLimit % 100);
}
*/

/**
 * Read the cvm limit from the config file and used for validation
 * once the write amount is received
 **/
/*
void readAndStoreCVMLimit(void *config, size_t configLen)
{
    struct tlv *tlv_outcome = NULL;
    tlv_parse(config, configLen, &tlv_outcome);
    struct tlv *tlvData = tlv_deep_find(tlv_outcome, FEIG_ID_EP_READER_CVM_REQUIRED_LIMIT);
    uint8_t cvmBuffer[6];
    size_t bufferLen = sizeof(cvmBuffer);
    char cvmLimitStr[13] = "";
    bufferLen = sizeof(cvmBuffer);
    memset(cvmBuffer, 0x00, sizeof(cvmBuffer));
    tlv_encode_value(tlvData, cvmBuffer, &bufferLen);
    for (int i = 0; i < bufferLen; i++)
    {
        char data[3];
        sprintf(data, "%02X", ((uint8_t *)cvmBuffer)[i]);
        strcat(cvmLimitStr, data);
    }
    cvmLimitStr[12] = '\0';

    logData("CVM Limit String : %s", cvmLimitStr);

    appConfig.cvmLimit = strtol(cvmLimitStr, NULL, 10);
    logData("CVM Limit : %llu.%02llu", appConfig.cvmLimit/100, appConfig.cvmLimit % 100);
}
*/

/**
 * Read the floor limit from the config file and used for validation
 * once the write amount is received
 **/
/*
void readAndStoreFloorLimit(void *config, size_t configLen)
{
    struct tlv *tlv_outcome = NULL;
    tlv_parse(config, configLen, &tlv_outcome);
    struct tlv *tlvData = tlv_deep_find(tlv_outcome, EMV_TERMINAL_FLOOR_LIMT);
    uint8_t floorBuffer[3];
    size_t bufferLen = sizeof(floorBuffer);
    char floorLimitStr[9] = "";
    bufferLen = sizeof(floorBuffer);
    memset(floorBuffer, 0x00, sizeof(floorBuffer));
    tlv_encode_value(tlvData, floorBuffer, &bufferLen);
    for (int i = 0; i < bufferLen; i++)
    {
        char data[3];
        sprintf(data, "%02X", ((uint8_t *)floorBuffer)[i]);
        strcat(floorLimitStr, data);
    }
    floorLimitStr[8] = '\0';
    logData("Floor limit string : %s", floorLimitStr);
    //strcpy(floorLimitStr, "00004E20");
    strcpy(floorLimitStr, "00030D40");
    logData("Floor limit string : %s", floorLimitStr);

    appConfig.floorLimit = strtol(floorLimitStr, NULL, 16);
    logData("Terminal Floor Limit : %llu.%02llu", appConfig.floorLimit/100, appConfig.floorLimit % 100);
}
*/

/**
 * Data exchange event from kernel
 **/
int callBackDataExchange(struct fetpf *client,
                         const void *source, size_t source_len,
                         const void *request, size_t request_len,
                         void *reply, size_t *reply_len)
{
    uint8_t kid[8] = {0};
    size_t kid_sz = 0;
    uint8_t df_name[16] = {0};
    size_t df_name_sz = 0;
    long long startEvent = getCurrentSeconds();

    if (isWaitingForFirstEvent)
    {
        startTrxTime = startEvent;
        deEvent = startEvent;
        isWaitingForFirstEvent = false;
        incrementTransactionCounter();
    }
    else
    {
        deEvent = startEvent;
    }

    if (appConfig.isTimingEnabled)
    {
        if (isWaitingForFirstEvent)
        {
            logTimeWarnData("Data Exchange, Card Detected : %lld", startEvent);
        }
        else
        {
            logTimeWarnData("Time taken by kernel for the call back event : %lld", (startEvent - deEndEvent));
            logTimeWarnData("Data Exchange, Start : %lld", startEvent);
        }
    }

    if (source && (source_len > 0))
    {
        struct tlv *tlv_source = NULL;
        tlv_parse_ex(source, source_len, 0, &tlv_source);

        struct tlv *tlv_kid = tlv_find(tlv_source, FEIG_ID_EP_KERNEL_ID);
        if (tlv_kid)
        {
            kid_sz = sizeof(kid);
            tlv_encode_value(tlv_kid, kid, &kid_sz);
        }

        struct tlv *tlv_fci = tlv_find(tlv_source, FEIG_ID_EP_FCI_CONTAINER);
        if (tlv_fci)
        {
            uint8_t d1[255] = {0};
            size_t t1 = sizeof(d1);
            char d1_str[(2 * t1) + 1];
            tlv_encode_value(tlv_fci, d1, &t1);
            logData("%s(): Source: Kernel ID: %s",
                    __func__, libtlv_bin_to_hex(d1, t1, d1_str));
        }

        struct tlv *tlv_df_name = tlv_find(tlv_source, EMV_ID_DF_NAME);
        if (tlv_df_name)
        {
            df_name_sz = sizeof(df_name);
            tlv_encode_value(tlv_df_name, df_name, &df_name_sz);
        }

        if (kid_sz > 0)
        {
            char kid_str[(2 * kid_sz) + 1];
            logData("%s(): Source: Kernel ID: %s",
                    __func__, libtlv_bin_to_hex(kid, kid_sz, kid_str));
        }

        if (df_name_sz > 0)
        {
            char df_name_str[(2 * df_name_sz) + 1];
            logData("%s(): Source: DF-Name %s",
                    __func__, libtlv_bin_to_hex(df_name, df_name_sz, df_name_str));

            strncpy(currentTxnData.aid, df_name_str, (2 * df_name_sz) + 1);
        }

        tlv_free(tlv_source);
    }

    logInfo("Kernel id : %02x", kid[0]);

    // Rupay kernel
    if (kid[0] == 0x0D)
    {
        currentTxnData.isRupayTxn = true;
        logData("Rupay card detected");
        currentTxnData = updateTransactionDateTime(currentTxnData);
        int rc = handleDataExchange(client, request, request_len, reply, reply_len);
        long long endEvent = getCurrentSeconds();
        if (appConfig.isTimingEnabled)
        {
            logTimeWarnData("Data Exchange, time taken by kernel to complete : %lld", (endEvent - deAzCompletedTime));
            logTimeWarnData("Data Exchange Completed : %lld\n", endEvent);
            logTimeWarnData("Data Exchange time took : %lld\n", (endEvent - startEvent));
        }
        deEndEvent = endEvent;

        return rc;
    }
    else
    {
        logError("Unsupported card");
        return EMVCO_RC_FAIL;
    }
}

/**
 * Online request event from kernel
 **/
int callBackOnlineRequest(struct fetpf *client,
                          const void *outcome, size_t outcome_len,
                          void *response, size_t *response_len,
                          void *out_data, size_t *out_data_len)
{
    (void)client;
    (void)out_data;
    int rc = 0;
    *out_data_len = 0;
    logData("%s() called\n", __func__);

    // logHexData("Online outcome : ", outcome, outcome_len);
    // processOutcome(outcome, outcome_len);
    if (currentTxnData.isRupayTxn == false && appConfig.enableAbt == true)
    {
        logData("This is a ABT transaction so no online required");
        *response_len = 0;
        return rc;
    }

    rc = handle_online_request(outcome, outcome_len, (uint8_t *)response, response_len);
    setSecondTap();
    return rc;
}

/**
 * Handle the online request with host
 **/
int handle_online_request(const void *outcome, size_t outcome_len,
                          uint8_t *response, size_t *response_len)
{
    logInfo("Sending online data to host");
    long long startOnlineReqTime = getCurrentSeconds();
    logTimeWarnData("Inititating online request calculations : %lld", startOnlineReqTime);

    struct tlv *tlv_obj = NULL;
    struct tlv *tlv_outcome = NULL;
    uint8_t buffer[4 * 1024];
    size_t buffer_len = sizeof(buffer);

    // Parse the out come received
    int rc = tlv_parse(outcome, outcome_len, &tlv_outcome);

    // Retrieve the ICC Data
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_DATA_RECORD);
    buffer_len = sizeof(buffer);
    memset(buffer, 0x00, sizeof(buffer));
    rc = tlv_encode_value(tlv_obj, buffer, &buffer_len);

    removeIccTags(buffer, buffer_len);

    // byteToHex(buffer, buffer_len, currentTxnData.iccData);
    // currentTxnData.iccDataLen = buffer_len * 2;

    // logData("ICC Data : %s", currentTxnData.iccData);
    // logData("ICC Data Len : %d", currentTxnData.iccDataLen);

    // To get the card expiry from the ICC Data
    populateCardExpiry(buffer, buffer_len);

    // Get the track 2 data from ICC and remove F padding
    struct tlv *tlv_data_record = NULL;
    rc = tlv_parse(buffer, buffer_len, &tlv_data_record);
    tlv_obj = tlv_find(tlv_data_record, EMV_ID_TRACK2_EQUIVALENT_DATA);
    if (tlv_obj != NULL)
    {
        char track2[40];
        buffer_len = sizeof(buffer);
        tlv_encode_value(tlv_obj, buffer, &buffer_len);
        byteToHex(buffer, buffer_len, track2);
        char *updatedTrack2 = removeTrack2FPadding(track2, buffer_len * 2);
        strcpy(currentTxnData.plainTrack2, updatedTrack2);
        currentTxnData.plainTrack2[strlen(updatedTrack2)] = '\0';
        free(updatedTrack2);
        // logData("Plain Track 2 received : %s", currentTxnData.plainTrack2); // TODO : Remove
        // logData("Plain Track 2 len : %d", strlen(currentTxnData.plainTrack2));
    }

    // tlv_free(tlv_obj);
    tlv_free(tlv_data_record);
    tlv_free(tlv_outcome);

    logData("Local Transaction Counter : %ld", currentTxnData.txnCounter);
    logData("Batch counter : %ld", appData.batchCounter);
    currentTxnData.isImmedOnline = true;

    generateOrderId();
    logData("Order id of txn data : %s", currentTxnData.orderId);
    encryptPanTrack2ExpDate();

    // Save the txn to db
    strcpy(currentTxnData.txnStatus, STATUS_PENDING);
    logData("Already generated txn id : %s", currentTxnData.transactionId);
    // char *transactionId = malloc(UUID_STR_LEN);
    // generateUUID(transactionId);
    // logInfo("Unique Transaction Id : %s", transactionId);
    // strcpy(currentTxnData.transactionId, transactionId);
    // free(transactionId);

    if (strcmp(currentTxnData.trxType, TRXTYPE_BALANCE_UPDATE) == 0)
    {
        if (!appConfig.useAirtelHost)
        {
            logData("Balance update, generating mac for paytm");
            generateMacBalanceUpdate(currentTxnData);
            createTxnDataForOnline(currentTxnData);
            long long endTime = getCurrentSeconds();
            logTimeWarnData("Calculation completed : %lld", endTime);
            logTimeWarnData("Time took for preparation : %lld", (endTime - startOnlineReqTime));
            performHostUpdate(currentTxnData, appData.batchCounter, response, response_len, BALANCE_UPDATE);
        }
        else
        {
            logData("Balance update, generating for airtel");
            createTxnDataForOnline(currentTxnData);
            long long endTime = getCurrentSeconds();
            logTimeWarnData("Calculation completed : %lld", endTime);
            logTimeWarnData("Time took for preparation : %lld", (endTime - startOnlineReqTime));
            logData("Going to perform airtel balance update");
            performAirtelHostUpdate(currentTxnData, appData.batchCounter, response, response_len, BALANCE_UPDATE);
        }
    }

    if (strcmp(currentTxnData.trxType, TRXTYPE_SERVICE_CREATE) == 0)
    {
        if (!appConfig.useAirtelHost)
        {
            logData("Service create, generating mac");
            generateMacServiceCreation(currentTxnData);
            createTxnDataForOnline(currentTxnData);

            long long endTime = getCurrentSeconds();
            logTimeWarnData("Calculation completed : %lld", endTime);
            logTimeWarnData("Time took for preparation : %lld", (endTime - startOnlineReqTime));

            performHostUpdate(currentTxnData, appData.batchCounter, response, response_len, SERVICE_CREATION);
        }
        else
        {
            logData("Service Create, generating for airtel");
            createTxnDataForOnline(currentTxnData);
            long long endTime = getCurrentSeconds();
            logTimeWarnData("Calculation completed : %lld", endTime);
            logTimeWarnData("Time took for preparation : %lld", (endTime - startOnlineReqTime));
            logData("Going to perform airtel service create");
            performAirtelHostUpdate(currentTxnData, appData.batchCounter, response, response_len, SERVICE_CREATION);
        }
    }

    if (strcmp(currentTxnData.trxType, TRXTYPE_MONEY_ADD) == 0)
    {
        if (!appConfig.useAirtelHost)
        {
            logData("Money add, generating mac");
            generateMacMoneyAdd(currentTxnData);
            createTxnDataForOnline(currentTxnData);
            long long endTime = getCurrentSeconds();
            logTimeWarnData("Calculation completed : %lld", endTime);
            logTimeWarnData("Time took for preparation : %lld", (endTime - startOnlineReqTime));

            performHostUpdate(currentTxnData, appData.batchCounter, response, response_len, MONEY_ADD);
        }
        else
        {
            logData("Money Add, generating for airtel");
            createTxnDataForOnline(currentTxnData);
            long long endTime = getCurrentSeconds();
            logTimeWarnData("Calculation completed : %lld", endTime);
            logTimeWarnData("Time took for preparation : %lld", (endTime - startOnlineReqTime));
            logData("Going to perform airtel Money Add");
            performAirtelHostUpdate(currentTxnData, appData.batchCounter, response, response_len, MONEY_ADD);
        }
    }

    logData("Sending responses back to kernel");

    long long t1 = getCurrentSeconds();
    logTimeWarnData("Sending the data back to kernel : %lld", t1);

    return rc;
}

/**
 * Generate the pre process data used for the transaction
 **/
int generatePreProcessData(void **preData, size_t *preDataLen)
{
    uint64_t trx_type = 0;
    int rc = 0;
    struct tlv *tlv = NULL;
    void *data = NULL;
    size_t dataLen = 0;

    logData("Transaction type in Pre Process : %02X", appData.trxTypeBin);

    rc = libtlv_bcd_to_u64(&appData.trxTypeBin, sizeof(appData.trxTypeBin), &trx_type);
    if (rc)
    {
        logError("Failure in converting trx type to u64");
        return EXIT_FAILURE;
    }

    uint64_t amoutBin = appData.amountAuthorizedBin;
    // If money add dont set the amount at start
    // It is set in rupay service
    if (appData.trxTypeBin == 0x028)
    {
        logData("Transaction type is purchase so setting the amount as 0");
        amoutBin = 0;
    }

    tlv = buildPreprocessTlv(trx_type,
                             amoutBin,
                             0,
                             appData.currCodeBin,
                             0x02);

    rc = serializeTlv(tlv, &data, &dataLen);
    if (rc)
    {
        if (tlv)
        {
            tlv_free(tlv);
        }

        if (data)
        {
            free(data);
        }
        return EXIT_FAILURE;
    }
    tlv_free(tlv);
    tlv = NULL;

    *preData = data;
    *preDataLen = dataLen;
    return EXIT_SUCCESS;
}

/**
 * Call back track 2 event received from kernel, where the encrypted PAN
 * or the plain track2 is received
 **/
void callBacktrack2(struct fetpf *client, const void *track2, size_t track_len)
{
    (void)client;
    logData("%s() called", __func__);

    const uint8_t *data = (uint8_t *)track2;
    logData("Track 2 Length : %d", track_len);

    uint8_t dataBuffer[100];
    size_t dataBufferLen = sizeof(dataBuffer);
    char track2Data[40];
    struct tlv *tlv_track2_outcome;
    tlv_parse(data, track_len, &tlv_track2_outcome);
    struct tlv *tlv_track2 = tlv_find(tlv_track2_outcome, EMV_TRACK2);
    dataBufferLen = sizeof(dataBuffer);
    memset(dataBuffer, 0x00, sizeof(dataBuffer));
    tlv_encode_value(tlv_track2, dataBuffer, &dataBufferLen);
    byteToHex(dataBuffer, dataBufferLen, track2Data);
    logData("Track 2 Parsed and received : %s", track2Data); // TODO : Remove
    // tlv_free(tlv_track2);
    tlv_free(tlv_track2_outcome);
    char *updatedTrack2 = removeTrack2FPadding(track2Data, dataBufferLen * 2);
    strcpy(currentTxnData.plainTrack2, updatedTrack2);
    currentTxnData.plainTrack2[strlen(updatedTrack2)] = '\0';
    free(updatedTrack2);

    if (currentTxnData.isRupayTxn == false && appConfig.enableAbt == true)
    {
        logData("Its an ABT on callback track 2, so generating token");
        logData("Plain track 2 after F removed : %s", currentTxnData.plainTrack2);
        strcpy(currentTxnData.token, currentTxnData.plainTrack2);

        char delimiter = 'D';
        char *token = strtok(currentTxnData.plainTrack2, &delimiter);
        strncpy(currentTxnData.maskPan, token, 20);
        currentTxnData.maskPan[sizeof(currentTxnData.maskPan) - 1] = '\0';

        // char plainTrack2[track_len * 2];
        // memset(plainTrack2, 0, track_len * 2);
        // for (int i = 0; i < track_len; i++)
        // {
        //     char temp[3];
        //     sprintf(temp, "%02X", data[i]);
        //     strcat(plainTrack2, temp);
        // }

        // plainTrack2[track_len * 2] = '\0';
        // logData("Plain Track 2 received : %s", plainTrack2);
        // logData("Plain text track 2 len : %d", strlen(plainTrack2));

        // char sha[65];
        // generatePanToken(currentTxnData.plainPan, plainTrack2, sha);
        // logData("SHA Generated for Pan : %s\n", sha);
        // strcpy(currentTxnData.token, sha);
    }

    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    /*
    const uint8_t *data = (uint8_t *)track2;
    logData("\t+++++++++++++ Track 2 Data ++++++++++++++");
    logData("\tTrack 2 Data (len: %zu):\t", track_len);
    for (int i = 0; i < track_len; i++)
    {
        printf("%02X", data[i]);
    }
    printf("\n");
    logData("\n+++++++++++++++++++++++++++++++++++++++\n");
    */
}

/**
 * Cancel event from kernel
 **/
int callBackSurrender(struct fetpf *client)
{
    (void)client;
    logData("Cancelling the polling, due to timeout");
    if (currentTxnData.cardPresentedSent == true)
    {
        sendCardRemovedMessage();
    }
    return FETPF_RC_CANCEL;
}

/**
 * Read the emv configuration file
 **/
int readEmvConfig(const char *path, void **config, size_t *configLen)
{
    int fd;
    struct stat st;
    int rc = 0;

    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "config file open(%s) failed: %m\n", path);
        return fd;
    }

    do
    {
        void *buf;
        size_t len;

        rc = fstat(fd, &st);
        if (rc < 0)
        {
            fprintf(stderr, "config file stat() failed: %m\n");
            break;
        }
        len = st.st_size;

        buf = malloc(len + 1);
        if (!buf)
        {
            fprintf(stderr, "malloc failed\n");
            rc = -1;
            break;
        }

        rc = read(fd, buf, len);
        if (rc != (int)len)
        {
            if (rc < 0)
                fprintf(stderr, "config file read() failed: %m\n");
            else
                fprintf(stderr, "config file read() incomplete\n");
            rc = -1;
            free(buf);
            break;
        }

        if (!memcmp(path + strlen(path) - 4, ".txt", 4))
        {
            ((char *)buf)[len] = 0; /* 0 termination in case of ascii */
            /* path ends with .txt - assumed to be hex (ASCII) */
            size_t bin_len = len;
            void *bin_buf = malloc(bin_len);

            if (!bin_buf)
            {
                fprintf(stderr, "malloc failed\n");
                rc = -1;
                free(buf);
                break;
            }
            if (!libtlv_hex_to_bin((char *)buf, bin_buf, &bin_len))
            {
                fprintf(stderr, "libtlv_hex_to_bin failed\n");
                rc = -1;
                free(buf);
                free(bin_buf);
                break;
            }
            free(buf);
            buf = bin_buf;
            len = bin_len;
        }

        *config = buf;
        *configLen = len;
        rc = 0;
    } while (0);

    close(fd);

    return rc;
}
