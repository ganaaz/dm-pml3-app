#include <unistd.h>
#include <string.h>
#include <openssl/hmac.h>

#include <libpay/tlv.h>

#include "include/hostmanager.h"
#include "include/config.h"
#include "include/commonutil.h"
#include "include/logutil.h"
#include "include/dboperations.h"
#include "include/aztimer.h"
#include "include/appcodes.h"
#include "JHost/jhost_interface.h"
#include "JAirtelHost/jairtel_host_interface.h"
#include "JAirtelHost/jairtel_hostutil.h"
#include "JHost/jhostutil.h"
#include "http-parser/http_util.h"
#include "include/responsemanager.h"
#include "ISO/ISO8583_interface.h"
#include "ISO/log.h"
#include "ISO/utils.h"

#define HMAC_HEX_SIZE (EVP_MAX_MD_SIZE * 2 + 1)

extern struct applicationConfig appConfig;
extern struct transactionData currentTxnData;
extern struct applicationData appData;
extern int activePendingTxnCount;
extern enum device_status DEVICE_STATUS;

bool isReversalOngoing = false;

/**
 * Initialize the static data for the host communication
 **/
int initializeHostStaticData()
{
    ISO8583_STATIC_DATA sd;
    memcpy(sd.TPDU, appConfig.tpdu, sizeof(appConfig.tpdu));
    memcpy(sd.DE22_POS_ENTRY_MODE, "72", sizeof("72"));
    memcpy(sd.DE24_NII, appConfig.nii, sizeof(appConfig.nii));
    memcpy(sd.DE25_POS_CONDITION_CODE, "02", sizeof("02"));
    memcpy(sd.DE41_TERMINAL_ID, appConfig.terminalId, sizeof(appConfig.terminalId));
    memcpy(sd.DE42_CARD_ACCEPTOR_ID, appConfig.merchantId, sizeof(appConfig.merchantId));
    memcpy(sd.HOST_IP_ADDRESS, appConfig.hostIP, sizeof(appConfig.hostIP));
    sd.HOST_PORT = appConfig.hostPort;
    sd.TRANSACTION_TIMOUT = appConfig.hostTxnTimeout;

    if (initialize_static_data(&sd) != TXN_SUCCESS)
    {
        return TXN_FAILED;
    }
    return TXN_SUCCESS;
}

/**
 * Process the host offline transaction with host
 **/
TransactionTable processHostOfflineTxn(TransactionTable trxData)
{
    logInfo("Processing transaction : %s", trxData.transactionId);

    char batch[7];
    sprintf(batch, "%06d", trxData.batch);

    OFFLINE_SALE_REQUEST offline_sale_req;
    // memcpy(offline_sale_req.DE02_PAN_NUMBER, trxData.PAN, sizeof(trxData.PAN));
    memcpy(offline_sale_req.DE04_TXN_AMOUNT, trxData.amount, sizeof(trxData.amount));
    memcpy(offline_sale_req.DE11_STAN, trxData.stan, sizeof(trxData.stan));
    memcpy(offline_sale_req.DE12_TXN_TIME, trxData.time, sizeof(trxData.time));
    memcpy(offline_sale_req.DE13_TXN_DATE, trxData.date, sizeof(trxData.date));

    memcpy(offline_sale_req.DE02_PAN_NUMBER, trxData.panEncrypted, sizeof(offline_sale_req.DE02_PAN_NUMBER));
    // memcpy(offline_sale_req.DE35_TRACK_2_DATA, trxData.track2Enc, sizeof(trxData.track2Enc));

    char ksn[45];
    strcpy(ksn, "0020");
    strcat(ksn, trxData.ksn);
    memcpy(offline_sale_req.DE53_SECURITY_DATA, ksn, sizeof(ksn));

    memcpy(offline_sale_req.DE56_BATCH_NUMBER, batch, sizeof(batch));

    offline_sale_req.DE55_ICC_DATA.value = (char *)malloc(trxData.iccDataLen + 1);
    memcpy(offline_sale_req.DE55_ICC_DATA.value, trxData.iccData, trxData.iccDataLen);
    offline_sale_req.DE55_ICC_DATA.value[trxData.iccDataLen] = '\0';
    offline_sale_req.DE55_ICC_DATA.len = trxData.iccDataLen;

    memcpy(offline_sale_req.DE62_INVOICE_NUMBER, trxData.stan, sizeof(trxData.stan));
    memset(offline_sale_req.DE37_RRN, 0x00, sizeof(offline_sale_req.DE37_RRN));

    // Generate narration data for Field 63
    // EXT 120 GLB DR 2700           22122317000012345678902312000001
    // char narrData[] = "EXT 120 GLB DR 2700           22122317000012345678902312000001";
    char narration[63];
    generateNarrationData(appConfig.stationId, trxData.acqTransactionId, trxData.acqUniqueTransactionId,
                          trxData.amount, narration);
    /*
    strcpy(narration, "EXT ");
    strcat(narration, appConfig.stationId);
    strcat(narration, " GLB DR ");
    char onlyAmount[5];
    memcpy(onlyAmount, &trxData.amount[8], 4);
    strcat(narration, onlyAmount);
    int len = strlen(narration);
    int max = 30 - len;
    for (int i = 0; i < max; i++)
    {
        strcat(narration, " ");
    }
    strcat(narration, trxData.acqTransactionId);
    strcat(narration, trxData.acqUniqueTransactionId);

    logData("Narration data generated : %s", narration);
    logData("Narration length : %d", strlen(narration));
    */

    char narrationHex[125];
    string2hexString(narration, narrationHex);
    logData("Narration in hex : %s", narrationHex);

    offline_sale_req.DE63_NARRATION_DATA.len = 124;
    offline_sale_req.DE63_NARRATION_DATA.value = (char *)malloc(124 + 1);
    memcpy(offline_sale_req.DE63_NARRATION_DATA.value, narrationHex, 124);
    offline_sale_req.DE63_NARRATION_DATA.value[124] = '\0';

    OFFLINE_SALE_RESPONSE offline_sale_resp;
    ISO8583_ERROR_CODES ret = TXN_FAILED;

    ret = process_offline_sale_transaction(&offline_sale_req, &offline_sale_resp);

    free(offline_sale_req.DE55_ICC_DATA.value);
    free(offline_sale_req.DE63_NARRATION_DATA.value);

    if (ret == TXN_SUCCESS)
    {
        sprintf(trxData.hostErrorCategory, "%s", "");
        logData("Amount Received : %s", offline_sale_resp.DE04_TXN_AMOUNT);
        logData("Stan Received: %s", offline_sale_resp.DE11_STAN);
        logData("Txn Time Received : %s", offline_sale_resp.DE12_TXN_TIME);
        logData("Txn Date Received : %s", offline_sale_resp.DE13_TXN_DATE);

        if (offline_sale_resp.DE37_RRN != NULL)
        {
            logInfo("RRN Received : %s", offline_sale_resp.DE37_RRN);
            sprintf(trxData.rrn, "%s", offline_sale_resp.DE37_RRN);
        }
        else
        {
            logWarn("RRN Not Received");
        }

        if (offline_sale_resp.DE38_AUTH_CODE != NULL)
        {
            logInfo("Authcode Received : %s", offline_sale_resp.DE38_AUTH_CODE);
            sprintf(trxData.authCode, "%s", offline_sale_resp.DE38_AUTH_CODE);
        }
        else
        {
            logWarn("Authcode Not Received %s.", "");
        }

        if (offline_sale_resp.DE39_RESPONSE_CODE != NULL)
        {
            logInfo("Response Code Received : %s", offline_sale_resp.DE39_RESPONSE_CODE);
            sprintf(trxData.hostResultCode, "%s", offline_sale_resp.DE39_RESPONSE_CODE);
        }
        else
        {
            strcpy(trxData.hostResultCode, "");
            logWarn("Response Code Not Received %s.", "");
        }

        if (strcmp(trxData.hostResultCode, "00") == 0)
        {
            sprintf(trxData.hostStatus, "%s", STATUS_SUCCESS);
            sprintf(trxData.hostError, "%s", "");

            doLock();
            activePendingTxnCount--;
            logData("Offline trxn result is success");
            logData("Active pending transaction count decreased and now is : %d", activePendingTxnCount);
            if (activePendingTxnCount < appConfig.minRequiredForOnline)
            {
                logWarn("Now the transaction is below minRequiredForOnline, making the device online");
                DEVICE_STATUS = STATUS_ONLINE;
            }
            printDeviceStatus();
            doUnLock();
        }
        else
        {
            logWarn("Response code received : %s", trxData.hostResultCode);
            logWarn("Response code received is not approved, so left it as in pending");
        }
    }
    else
    {
        sprintf(trxData.hostError, "%s", getHostErrorString(ret));
        logWarn("Host error : %s", getHostErrorString(ret));
        logWarn("Max Retry for failure : %d", appConfig.hostMaxRetry);

        if (ret == TXN_HOST_CONNECTION_TIMEOUT || ret == TXN_RECEIVE_FROM_HOST_TIMEOUT)
        {
            sprintf(trxData.hostErrorCategory, "%s", HOST_ERROR_CATEGORY_TIMEOUT);
        }

        /*
        // No check for retry as discussed
        if (trxData.hostRetry == appConfig.hostMaxRetry)
        {
            sprintf(trxData.hostErrorCategory, "%s", HOST_ERROR_CATEGORY_FAILED);
            sprintf(trxData.hostStatus, "%s", STATUS_FAILURE);
            logError("Host Failed");
        }
        else
        {
            trxData.hostRetry++;
        }*/
        trxData.hostRetry++;
    }

    return trxData;
}

/**
 * Perform the balance update or service creation with airtel host
 **/
void performAirtelHostUpdate(struct transactionData trxData, long batchCounter,
                             uint8_t *response, size_t *response_len, enum host_trx_type hostTrxType)
{
    *response_len = 0;
    char responseMessage[1024 * 32] = {0};

    long long startOnlineReqTime = getCurrentSeconds();
    logTimeWarnData("Sending request to Airtel Host : %lld", startOnlineReqTime);
    logData("Starting host update for airtel");
    int retStatus = 0;

    if (hostTrxType == SERVICE_CREATION)
    {
        char *message = generateAirtelServiceCreationRequest(trxData);
        printf("Service Creation Message : \n");
        printf(message);
        printf("\n");

        removeSpaces(message);
        char hmac_hex[HMAC_HEX_SIZE];
        calculate_hmac_sha256(appConfig.airtelSignSalt, message, hmac_hex);
        logData("Hmac hex of message : %s", hmac_hex);

        char body[1024 * 24] = {0};
        strcpy(body, message);
        free(message);
        logData("Sending data to Airtel for service creation");
        updateAirtelRequestData(trxData.transactionId, body);
        retStatus = sendAirtelHostRequest(body, appConfig.airtelServiceCreationUrl, responseMessage,
                                          trxData.orderId, hmac_hex);

        logData("Response length from server : %d", strlen(responseMessage));
        printf("Response Message : %s\n", responseMessage);
        updateAirtelResponseData(trxData.transactionId, responseMessage);
    }

    if (hostTrxType == BALANCE_UPDATE)
    {
        char *message = generateAirtelBalanceUpdateRequest(trxData);
        printf("Balance Update Message : \n");
        printf(message);
        printf("\n");

        removeSpaces(message);
        char hmac_hex[HMAC_HEX_SIZE];
        calculate_hmac_sha256(appConfig.airtelSignSalt, message, hmac_hex);
        logData("Hmac hex of message : %s", hmac_hex);

        char body[1024 * 24] = {0};
        strcpy(body, message);
        free(message);
        logData("Sending data to Airtel for balance update");
        updateAirtelRequestData(trxData.transactionId, body);
        retStatus = sendAirtelHostRequest(body, appConfig.airtelBalanceUpdateUrl, responseMessage,
                                          trxData.orderId, hmac_hex);

        logData("Response length from server : %d", strlen(responseMessage));
        printf("Response Message : %s\n", responseMessage);
        updateAirtelResponseData(trxData.transactionId, responseMessage);
    }

    if (hostTrxType == MONEY_ADD)
    {
        char *message = generateAirtelMoneyAddRequest(trxData);
        printf("Money Add Message : \n");
        printf(message);
        printf("\n");

        removeSpaces(message);
        char hmac_hex[HMAC_HEX_SIZE];
        calculate_hmac_sha256(appConfig.airtelSignSalt, message, hmac_hex);
        logData("Hmac hex of message : %s", hmac_hex);

        char body[1024 * 24] = {0};
        strcpy(body, message);
        free(message);
        logData("Sending data to Airtel for balance update");
        updateAirtelRequestData(trxData.transactionId, body);
        retStatus = sendAirtelHostRequest(body, appConfig.airtelMoneyAddUrl, responseMessage,
                                          trxData.orderId, hmac_hex);

        logData("Response length from server : %d", strlen(responseMessage));
        printf("Response Message : %s\n", responseMessage);
        updateAirtelResponseData(trxData.transactionId, responseMessage);
    }

    if (retStatus == -1)
    {
        logError("There is issue in connectivity with host");
        logError("This transaction is not qualified for reversal");
    }

    if (retStatus == -2)
    {
        logError("There is issue in getting the data from host, where we sent the message");
        logError("This transaction is qualified for reversal");
        resetReversalStatus(STATUS_PENDING, trxData.transactionId);
    }

    if (strlen(responseMessage) == 0)
    {
        logError("There is no response to parse");
        return;
    }

    long long endTime = getCurrentSeconds();
    logTimeWarnData("Response received from Airtel : %lld", endTime);
    logTimeWarnData("Time taken by Airtel : %lld", (endTime - startOnlineReqTime));

    HttpResponseData httpResponseData = parseHttpResponse(responseMessage);

    if (httpResponseData.code == 200 && httpResponseData.messageLen != 0)
    {
        AirtelHostResponse hostResponse = parseAirtelHostResponseData(httpResponseData.message);
        if (strlen(hostResponse.switchResponseCode) != 0)
        {
            *response_len = 4;
            response[0] = 0x8A;
            response[1] = 0x02;
            response[2] = (uint8_t)hostResponse.switchResponseCode[0];
            response[3] = (uint8_t)hostResponse.switchResponseCode[1];
        }

        if (strlen(hostResponse.iccData) != 0)
        {
            int iccLen = strlen(hostResponse.iccData);
            *response_len = 4 + iccLen / 2;
            uint8_t issuer_authentication_data[96] = {0};
            size_t issuer_authentication_data_len = iccLen / 2;

            libtlv_hex_to_bin(hostResponse.iccData, issuer_authentication_data, &issuer_authentication_data_len);
            logData("91 Length : %d", issuer_authentication_data_len);
            for (int i = 0; i < issuer_authentication_data_len; i++)
            {
                response[i + 4] = issuer_authentication_data[i];
            }
        }

        // logData("Response len : %d", *response_len);
        // for (int i = 0; i < *response_len; i++)
        // {
        //     printf("%02X ", response[i]);
        // }
        // printf("\n");

        updateAirtelHostResponseInDb(hostResponse, trxData.transactionId);
    }
    else
    {
        logError("Http response error");
        *response_len = 0;
    }

    // TODO On time out reversal

    free(httpResponseData.message);
}

/**
 * Perform the balance update or service creation with host
 **/
void performHostUpdate(struct transactionData trxData, long batchCounter,
                       uint8_t *response, size_t *response_len, enum host_trx_type hostTrxType)
{
    *response_len = 0;
    char responseMessage[1024 * 12] = {0};

    long long startOnlineReqTime = getCurrentSeconds();
    logTimeWarnData("Sending request to PayTM : %lld", startOnlineReqTime);
    int retStatus = 0;

    if (hostTrxType == SERVICE_CREATION)
    {
        char *message = generateServiceCreationRequest(trxData);
        printf("Service Creation Message : \n");
        printf(message);
        printf("\n");

        logData("Sending data to PayTM");
        retStatus = sendHostRequest(message, appConfig.serviceCreationUrl, responseMessage);
        logData("Response length from server : %d", strlen(responseMessage));
        printf("Response Message : %s\n", responseMessage);
        free(message);
    }

    if (hostTrxType == BALANCE_UPDATE)
    {
        char *message = generateBalanceUpdateRequest(trxData);
        printf("Balance Update Message : \n");
        printf(message);
        printf("\n");

        logData("Sending data to PayTM");
        retStatus = sendHostRequest(message, appConfig.balanceUpdateUrl, responseMessage);
        logData("Response length from server : %d", strlen(responseMessage));
        printf("Response Message : %s\n", responseMessage);
        free(message);
    }

    if (hostTrxType == MONEY_ADD)
    {
        char *message = generateMoneyAddRequest(trxData);
        printf("Money Add Message : \n");
        printf(message);
        printf("\n");

        logData("Sending data to PayTM");
        retStatus = sendHostRequest(message, appConfig.moneyLoadUrl, responseMessage);
        logData("Response length from server : %d", strlen(responseMessage));
        printf("Response Message : %s\n", responseMessage);
        free(message);
    }

    if (retStatus == -1)
    {
        logError("There is issue in connectivity with host");
        logError("This transaction is not qualified for reversal");
    }

    if (retStatus == -2)
    {
        logError("There is issue in getting the data from host, where we sent the message");
        logError("This transaction is qualified for reversal");
        resetReversalStatus(STATUS_PENDING, trxData.transactionId);
    }

    if (strlen(responseMessage) == 0)
    {
        logError("There is no response to parse");
        return;
    }

    long long endTime = getCurrentSeconds();
    logTimeWarnData("Response received from PayTM : %lld", endTime);
    logTimeWarnData("Time taken by Paytm : %lld", (endTime - startOnlineReqTime));

    HttpResponseData httpResponseData = parseHttpResponse(responseMessage);

    if (httpResponseData.code == 200 && httpResponseData.messageLen != 0)
    {
        HostResponse hostResponse = parseHostResponseData(httpResponseData.message);
        if (strlen(hostResponse.bankResultCode) != 0)
        {
            *response_len = 4;
            response[0] = 0x8A;
            response[1] = 0x02;
            response[2] = (uint8_t)hostResponse.bankResultCode[0];
            response[3] = (uint8_t)hostResponse.bankResultCode[1];
        }

        if (strlen(hostResponse.iccData) != 0)
        {
            int iccLen = strlen(hostResponse.iccData);
            *response_len = 4 + iccLen / 2;
            uint8_t issuer_authentication_data[96] = {0};
            size_t issuer_authentication_data_len = iccLen / 2;

            libtlv_hex_to_bin(hostResponse.iccData, issuer_authentication_data, &issuer_authentication_data_len);
            logData("91 Length : %d", issuer_authentication_data_len);
            for (int i = 0; i < issuer_authentication_data_len; i++)
            {
                response[i + 4] = issuer_authentication_data[i];
            }
        }

        logData("Response len : %d", *response_len);
        for (int i = 0; i < *response_len; i++)
        {
            printf("%02X ", response[i]);
        }
        printf("\n");

        updateHostResponseInDb(hostResponse, trxData.transactionId);
    }
    else
    {
        logError("Http response error");
        *response_len = 0;
    }

    // TODO On time out reversal

    free(httpResponseData.message);
}

/**
 * Initiate the host pending transactions from a thread
 **/
// void handleHostOfflineTransactions(size_t timer_id, void * user_data)
void *handleHostOfflineTransactions()
{
    logData("Host offline sale transaction thread triggered");
    logData("Host process interval : %d", appConfig.hostProcessTimeInMinutes);
    int waitTime = appConfig.hostProcessTimeInMinutes * 60;
    // waitTime = 0;
    int counter = 1;
    while (1)
    {
        // sleep(waitTime);
        logData("Initiating the host pending transaction, counter : %d", counter);
        processHostPendingTransactions();
        logData("Host pending transaction process completed. Now sleeping for %d seconds.", waitTime);
        counter++;
        sleep(waitTime);

        // if (counter % 200 == 0)
        //     printf("Count is : %d\n", counter);
    }
}

/**
 *  To initiate a verify terminal and get the response
 */
char *performVerifyTerminal(const char tid[50], const char mid[50])
{
    logData("Going to perform the verify terminal");
    incrementTransactionCounter();
    resetTransactionData();
    currentTxnData.txnCounter = appData.transactionCounter;
    char stan[7];
    snprintf(stan, 7, "%06ld", currentTxnData.txnCounter);
    logData("Stan : %s", stan);
    strcpy(currentTxnData.stan, stan);

    generateMacVerifyTerminal(currentTxnData, tid);

    char responseMessage[1024 * 5] = {0};
    char *message = generateVerifyTerminalRequest(currentTxnData, tid);
    printf("Verify terminal Message : \n");
    printf(message);
    printf("\n");

    logData("Sending data to PayTM");
    sendHostRequest(message, appConfig.verifyTerminalUrl, responseMessage);
    logData("Response length from server : %d", strlen(responseMessage));
    printf("Response Message : %s", responseMessage);
    free(message);
    VerifyTerminalResponse response = {0};
    response.status = -1;

    if (strlen(responseMessage) == 0)
    {
        logError("There is no response to parse for reversal");
        return buildVerifyTerminalResponseMessage(response, 1);
    }

    HttpResponseData httpResponseData = parseHttpResponse(responseMessage);
    if (httpResponseData.code == 200 && httpResponseData.messageLen != 0)
    {
        response = parseVerifyTerminalResponse(httpResponseData.message);
    }
    else
    {
        logError("Http response error for reversal");
    }
    free(httpResponseData.message);

    int midStatus = -1; // NA
    if (strlen(mid) != 0)
    {
        logData("MID Provided, validating it");
        if (strcmp(mid, response.mid) == 0)
            midStatus = 0; // Success
        else
            midStatus = 1; // Fail
    }

    if (midStatus == 0 && strcmp(response.resultcode, "S") == 0 && response.status == 0)
    {
        logData("MID Status is success and result is Success, so updating the config");
        strcpy(appConfig.terminalId, tid);
        strcpy(appConfig.merchantId, mid);
        saveConfig();
    }

    return buildVerifyTerminalResponseMessage(response, midStatus);
}

/**
 *  To initiate a verify terminal and get the response for Airtel
 */
char *performAirtelVerifyTerminal()
{
    logData("Going to perform the Airtel verify terminal for TID : %s and MID : %s",
            appConfig.terminalId, appConfig.merchantId);
    resetTransactionData();

    char responseMessage[1024 * 5] = {0};
    char *message = generateAirtelVerifyTerminalRequest();
    printf("Verify terminal Message : \n");
    printf(message);
    printf("\n");

    removeSpaces(message);
    char hmac_hex[HMAC_HEX_SIZE];
    calculate_hmac_sha256(appConfig.airtelSignSalt, message, hmac_hex);
    logData("Hmac hex of message : %s", hmac_hex);

    char *uniqueId = malloc(UUID_STR_LEN);
    generateUUID(uniqueId);
    logInfo("Unique Order Id : %s", uniqueId);

    char body[1024 * 24] = {0};
    strcpy(body, message);
    free(message);
    logData("Sending data to Airtel for verify terminal");
    int retStatus = sendAirtelHostRequest(body, appConfig.airtelVerifyTerminalUrl, responseMessage,
                                          uniqueId, hmac_hex);
    free(uniqueId);

    AirtelVerifyTerminalResponse response;
    response.status = -1;
    strcpy(response.code, "");
    strcpy(response.description, "");
    if (retStatus != 0)
    {
        logError("Http response error for reversal");
        return buildAirtelVerifyTerminalResponseMessage(response, retStatus);
    }

    logData("Response length from server : %d", strlen(responseMessage));
    printf("Response Message : %s\n", responseMessage);

    HttpResponseData httpResponseData = parseHttpResponse(responseMessage);
    if (httpResponseData.messageLen != 0)
    {
        response = parseAirtelHostVerifyTerminalResponse(httpResponseData.message);
    }
    else
    {
        logError("Http response error for verify terminal");
        return buildAirtelVerifyTerminalResponseMessage(response, httpResponseData.code);
    }
    free(httpResponseData.message);
    return buildAirtelVerifyTerminalResponseMessage(response, httpResponseData.code);
}

/**
 *  To initiate a health check and get the response for Airtel host
 */
char *performAirtelHealthCheck()
{
    logData("Going to perform the Airtel health check for TID : %s and MID : %s",
            appConfig.terminalId, appConfig.merchantId);
    resetTransactionData();

    char responseMessage[1024 * 5] = {0};
    char *message = generateAirtelHealthCheckRequest();
    printf("Health check Message : \n");
    printf(message);
    printf("\n");

    removeSpaces(message);
    char hmac_hex[HMAC_HEX_SIZE];
    calculate_hmac_sha256(appConfig.airtelSignSalt, message, hmac_hex);
    logData("Hmac hex of message : %s", hmac_hex);

    char *uniqueId = malloc(UUID_STR_LEN);
    generateUUID(uniqueId);
    logInfo("Unique Order Id : %s", uniqueId);

    char body[1024 * 24] = {0};
    strcpy(body, message);
    free(message);
    logData("Sending data to Airtel for health check");
    int retStatus = sendAirtelHostRequest(body, appConfig.airtelHealthCheckUrl, responseMessage,
                                          uniqueId, hmac_hex);
    free(uniqueId);

    AirtelHealthCheckResponse response;
    response.status = -1;
    strcpy(response.code, "");
    strcpy(response.description, "");
    if (retStatus != 0)
    {
        logError("Http response error for reversal");
        return buildAirtelHealthCheckResponseMessage(response, retStatus);
    }

    logData("Response length from server : %d", strlen(responseMessage));
    printf("Response Message : %s\n", responseMessage);

    HttpResponseData httpResponseData = parseHttpResponse(responseMessage);
    if (httpResponseData.messageLen != 0)
    {
        response = parseAirtelHostHealthCheckResponse(httpResponseData.message);
    }
    else
    {
        logError("Http response error for health check");
        return buildAirtelHealthCheckResponseMessage(response, httpResponseData.code);
    }
    free(httpResponseData.message);
    return buildAirtelHealthCheckResponseMessage(response, httpResponseData.code);
}

/**
 * Start the reversal process with sleep time
 */
void *startReversalThread()
{
    logData("Reversal Thread Triggered");
    logData("Reversal process interval : %d", appConfig.reversalTimeInMinutes);
    int waitTime = appConfig.reversalTimeInMinutes * 60;
    int counter = 1;
    while (1)
    {
        logData("Initiating the reversal process, counter : %d", counter);
        verifyAndDoReversal();
        logData("Reversal process done. Now sleeping for %d seconds.", waitTime);
        counter++;
        sleep(waitTime);
    }
}

/**
 * To verify whether we need to do reversal and do it
 */
void verifyAndDoReversal()
{
    char transactionId[38] = {0};
    if (isTherePendingReversal(transactionId) == true)
    {
        logInfo("There is a pending reversal, trying to send to host");
        logInfo("Transaction id for reversal : %s", transactionId);
        performReversal(transactionId, true, "E2");

        // if (isReversalOngoing == true)
        // {
        //     logData("There is already ongoing reversal, so thread no need to do again.");
        //     return;
        // }
        // TransactionTable trxTable = getTransactionTableData(transactionId);

        // if (strlen(trxTable.reversalMac) == 0)
        // {
        //     if (appConfig.useAirtelHost)
        //     {
        //         strcpy(trxTable.reversalMac, "NoMac");
        //     }
        //     else
        //     {
        //         logData("There is no reversal mac present so generating it");
        //         // trxTable = generateMacReversal(trxTable, REVERSAL_CODE_22);
        //         // trxTable = generateMacEcho(trxTable, REVERSAL_CODE_22);
        //     }
        //     strcpy(trxTable.reversalInputResponseCode, REVERSAL_CODE_22);
        //     strcpy(trxTable.reversalStatus, STATUS_PENDING);
        //     updateReversalPreData(trxTable);
        // }
        // performReversal(trxTable);
    }
    else
    {
        logData("There are no pending reversals");
    }
}

/**
 * Perform the reversal with host
 **/
void performReversal(const char *transactionId, bool isTimeOut, const char *responseCode)
{
    logInfo("Going to perform the reversal for transaction : %s", transactionId);
    logInfo("Time out scenario : %d", isTimeOut);

    TransactionTable trxTable = getTransactionTableData(transactionId);
    logData("STAN : %s", trxTable.stan);

    REVERSAL_REQUEST reversal_request;

    memcpy(reversal_request.DE03_PROC_CODE, trxTable.processingCode, sizeof(reversal_request.DE03_PROC_CODE));
    memcpy(reversal_request.DE04_TXN_AMOUNT, trxTable.amount, sizeof(reversal_request.DE04_TXN_AMOUNT));
    memcpy(reversal_request.DE11_STAN, trxTable.stan, sizeof(reversal_request.DE11_STAN));
    memcpy(reversal_request.DE12_TXN_TIME, trxTable.time, sizeof(reversal_request.DE12_TXN_TIME));
    memcpy(reversal_request.DE13_TXN_DATE, trxTable.date, sizeof(reversal_request.DE13_TXN_DATE));

    // memcpy(reversal_request.DE35_TRACK_2_DATA, trxTable.track2, trxTable.track2Len);
    // reversal_request.DE35_TRACK_2_DATA[trxTable.track2Len - 1] = '\0';
    if (isTimeOut == false)
    {
        // responseCode : E1 in general
        memcpy(reversal_request.DE37_RRN, trxTable.rrn, sizeof(reversal_request.DE37_RRN));
        memcpy(reversal_request.DE38_AUTH_CODE, trxTable.authCode, sizeof(reversal_request.DE38_AUTH_CODE));
        // memcpy(reversal_request.DE39_RESPONSE_CODE, trxTable.responseCode, sizeof(reversal_request.DE39_RESPONSE_CODE));
        memcpy(reversal_request.DE39_RESPONSE_CODE, responseCode, sizeof(reversal_request.DE39_RESPONSE_CODE));
    }
    else
    {
        // responseCode : E2 in general
        memcpy(reversal_request.DE39_RESPONSE_CODE, responseCode, sizeof(reversal_request.DE39_RESPONSE_CODE));
        logInfo("Time out scenario so no RRN / Auth is sent to host only response code E2");
    }

    reversal_request.DE55_ICC_DATA.value = (char *)malloc(trxTable.iccDataLen + 1);
    memcpy(reversal_request.DE55_ICC_DATA.value, trxTable.iccData, trxTable.iccDataLen);
    reversal_request.DE55_ICC_DATA.value[trxTable.iccDataLen] = '\0';
    reversal_request.DE55_ICC_DATA.len = trxTable.iccDataLen;

    memcpy(reversal_request.DE62_INVOICE_NUMBER, trxTable.stan, sizeof(reversal_request.DE62_INVOICE_NUMBER));

    reversal_request.DE63_PRIVATE_DATA.value = (char *)malloc(3);
    strcpy(reversal_request.DE63_PRIVATE_DATA.value, "03");
    reversal_request.DE63_PRIVATE_DATA.value[2] = '\0';
    reversal_request.DE63_PRIVATE_DATA.len = 2;

    REVERSAL_RESPONSE reversal_response;
    int txn = REVERSAL;
    if (isTimeOut == true)
        txn = REVERSAL_TIMEOUT;

    ISO8583_ERROR_CODES ret = process_reversal_transaction(&reversal_request, &reversal_response, txn);

    logData("Reversal Return code : %d", ret);

    free(reversal_request.DE55_ICC_DATA.value);
    free(reversal_request.DE63_PRIVATE_DATA.value);

    if (ret == TXN_SUCCESS)
    {
        logData("Amount Received: %s", reversal_response.DE04_TXN_AMOUNT);
        logData("Stan Received : %s", reversal_response.DE11_STAN);
        logData("Txn Time Received : %s", reversal_response.DE12_TXN_TIME);
        logData("Txn Date Received : %s", reversal_response.DE13_TXN_DATE);
        logData("RRN Received : %s", reversal_response.DE37_RRN);
        logData("Auth Code Received : %s", reversal_response.DE38_AUTH_CODE);
        logData("Response Code Received : %s", reversal_response.DE39_RESPONSE_CODE);

        if (strcmp(reversal_response.DE39_RESPONSE_CODE, "00") == 0)
        {
            char txnStatus[50];
            strcpy(txnStatus, trxTable.txnStatus);
            logData("Current transaction status : %s", txnStatus);
            if (strcmp(trxTable.txnStatus, STATUS_PENDING) == 0)
            {
                strcpy(txnStatus, STATUS_FAILURE);
            }
            logData("Updated transaction status : %s", txnStatus);

            updateReversalStatus(transactionId, STATUS_SUCCESS, txnStatus,
                                 reversal_response.DE37_RRN, reversal_response.DE38_AUTH_CODE,
                                 reversal_response.DE39_RESPONSE_CODE);
        }
    }
    else
    {
        logData("Reversal Error  : %d", ret);
    }
}

/**
 * Get the  host error as string
 **/
const char *getHostErrorString(ISO8583_ERROR_CODES errorCode)
{
    switch (errorCode)
    {
    case TXN_HOST_CONNECTION_FAILED:
        return "Host connection failed";
        break;

    case TXN_SEND_TO_HOST_FAILED:
        return "Sending data to host failed";
        break;

    case TXN_RECEIVE_FROM_HOST_FAILED:
        return "Received from host failed";
        break;

    case TXN_HOST_CONNECTION_TIMEOUT:
        return "Host connection timeout";
        break;

    case TXN_RECEIVE_FROM_HOST_TIMEOUT:
        return "Receive from host timeout";
        break;

    case TXN_PACK_VALIDATION_FAILED:
    case TXN_PACK_FAILED:
        return "Data pack validation failed";
        break;

    case TXN_PARSE_VALIDATION_FAILED:
    case TXN_PARSE_FAILED:
        return "Parse validation failed";
        break;

    default:
        return "Error in processing with the host.";
        break;
    }
}

/**
 * Perform the balance update with host
 **/
void performHostBalanceUpdate(struct transactionData trxData, long batchCounter,
                              uint8_t *response, size_t *response_len)
{
    char stan[7];
    sprintf(stan, "%06ld", trxData.txnCounter);
    logData("STAN : %s", stan);

    char batch[7];
    sprintf(batch, "%06ld", batchCounter);
    logData("Batch : %s", batch);

    BALANCE_UPDATE_REQUEST bal_update_req;
    memcpy(bal_update_req.DE11_STAN, stan, sizeof(stan));

    // Amount is always 0 for balance update
    memcpy(bal_update_req.DE04_TXN_AMOUNT, "0", sizeof("0"));

    memcpy(bal_update_req.DE35_TRACK_2_DATA, trxData.track2Enc, sizeof(trxData.track2Enc));

    char ksn[45];
    strcpy(ksn, "0020");
    strcat(ksn, trxData.ksn);
    memcpy(bal_update_req.DE53_SECURITY_DATA, ksn, sizeof(ksn));

    bal_update_req.DE55_ICC_DATA.value = (char *)malloc(trxData.iccDataLen + 1);
    memcpy(bal_update_req.DE55_ICC_DATA.value, trxData.iccData, trxData.iccDataLen);
    bal_update_req.DE55_ICC_DATA.value[trxData.iccDataLen] = '\0';
    bal_update_req.DE55_ICC_DATA.len = trxData.iccDataLen;

    memcpy(bal_update_req.DE56_BATCH_NUMBER, batch, sizeof(batch));
    memcpy(bal_update_req.DE62_INVOICE_NUMBER, stan, sizeof(stan));

    createTxnDataForOnline(trxData);

    BALANCE_UPDATE_RESPONSE bal_update_resp;
    ISO8583_ERROR_CODES ret = process_balance_update_transaction(&bal_update_req, &bal_update_resp);

    free(bal_update_req.DE55_ICC_DATA.value);

    if (ret == TXN_SUCCESS)
    {
        logData("Stan Received: %s", bal_update_resp.DE11_STAN);
        logData("Txn Time Received : %s", bal_update_resp.DE12_TXN_TIME);
        logData("Txn Date Received : %s", bal_update_resp.DE13_TXN_DATE);
        logData("RRN Received : %s", bal_update_resp.DE37_RRN);
        logData("Authcode Received : %s", bal_update_resp.DE38_AUTH_CODE);
        logData("bal_update_response Code Received : %s", bal_update_resp.DE39_RESPONSE_CODE);
        logData("Field 55 Received : %s", bal_update_resp.DE55_ICC_DATA.value);

        *response_len = 4;
        response[0] = 0x8A;
        response[1] = 0x02;
        response[2] = (uint8_t)bal_update_resp.DE39_RESPONSE_CODE[0];
        response[3] = (uint8_t)bal_update_resp.DE39_RESPONSE_CODE[1];

        if (bal_update_resp.DE55_ICC_DATA.value != NULL)
        {
            *response_len = 4 + bal_update_resp.DE55_ICC_DATA.len / 2;
            uint8_t issuer_authentication_data[96] = {0};
            size_t issuer_authentication_data_len = bal_update_resp.DE55_ICC_DATA.len / 2;

            libtlv_hex_to_bin(bal_update_resp.DE55_ICC_DATA.value, issuer_authentication_data,
                              &issuer_authentication_data_len);

            logData("91 Length : %d", issuer_authentication_data_len);
            for (int i = 0; i < issuer_authentication_data_len; i++)
            {
                response[i + 4] = issuer_authentication_data[i];
            }
        }
    }
    else
    {
        logError("Host data failed with error code : %d", ret);
    }

    doBalanceUpdateHostResponseInDb(bal_update_resp, trxData.transactionId, ret);
}

/**
 * Update the balance update txn host response in db
 **/
void doBalanceUpdateHostResponseInDb(BALANCE_UPDATE_RESPONSE bal_update_resp,
                                     const char *transactionId, int hostResult)
{
    TransactionTable trxTable = getTransactionTableData(transactionId);
    logData("Transaction id received : %s", trxTable.transactionId);
    sprintf(trxTable.reversalStatus, "%s", "");
    bool isReversal = false;

    if (hostResult == 0)
    {
        if (bal_update_resp.DE37_RRN != NULL)
        {
            sprintf(trxTable.rrn, "%s", bal_update_resp.DE37_RRN);
        }
        else
        {
            memset(trxTable.rrn, 0, sizeof(trxTable.rrn));
            logWarn("RRN Not Received");
        }

        if (bal_update_resp.DE38_AUTH_CODE != NULL)
        {
            sprintf(trxTable.authCode, "%s", bal_update_resp.DE38_AUTH_CODE);
        }
        else
        {
            memset(trxTable.authCode, 0, sizeof(trxTable.authCode));
            logWarn("Authcode Not Received %s.", "");
        }

        if (bal_update_resp.DE39_RESPONSE_CODE != NULL)
        {
            sprintf(trxTable.hostResultCode, "%s", bal_update_resp.DE39_RESPONSE_CODE);
        }
        else
        {
            memset(trxTable.hostResultCode, 0, sizeof(trxTable.hostResultCode));
            logWarn("Response Code Not Received %s.", "");
        }

        if (bal_update_resp.DE55_ICC_DATA.value != NULL)
        {
            // Get the update amount from the last 6 byte data
            // TODO : Parse using TLV
            char updateAmount[13];
            int j = 0;
            int iccLen = bal_update_resp.DE55_ICC_DATA.len;
            for (int i = iccLen - 12; i < iccLen; i++)
            {
                updateAmount[j++] = bal_update_resp.DE55_ICC_DATA.value[i];
            }
            updateAmount[j] = '\0';
            logData("Updated amount received : %s", updateAmount);
            sprintf(trxTable.updateAmount, "%s", updateAmount);
        }
        else
        {
            memset(trxTable.updateAmount, 0, sizeof(trxTable.updateAmount));
        }

        if (strcmp(trxTable.hostResultCode, "00") == 0)
        {
            sprintf(trxTable.hostStatus, "%s", STATUS_SUCCESS);
        }
        else
        {
            sprintf(trxTable.hostStatus, "%s", STATUS_FAILURE);
        }
    }
    else
    {
        sprintf(trxTable.hostError, "%s", getHostErrorString(hostResult));
        logWarn("Host error : %s", getHostErrorString(hostResult));
        sprintf(trxTable.hostStatus, "%s", STATUS_FAILURE);
        logError("Host Failed");
        trxTable.hostRetry++;

        memset(trxTable.rrn, 0, sizeof(trxTable.rrn));
        if (bal_update_resp.DE37_RRN != NULL)
        {
            sprintf(trxTable.rrn, "%s", bal_update_resp.DE37_RRN);
        }
        else
        {
            logWarn("RRN Not Received");
        }

        memset(trxTable.authCode, 0, sizeof(trxTable.authCode));
        if (bal_update_resp.DE38_AUTH_CODE != NULL)
        {
            sprintf(trxTable.authCode, "%s", bal_update_resp.DE38_AUTH_CODE);
        }
        else
        {
            logWarn("Authcode Not Received %s.", "");
        }

        memset(trxTable.hostResultCode, 0, sizeof(trxTable.hostResultCode));
        if (bal_update_resp.DE39_RESPONSE_CODE != NULL)
        {
            sprintf(trxTable.hostResultCode, "%s", bal_update_resp.DE39_RESPONSE_CODE);
        }
        else
        {
            logWarn("Response Code Not Received %s.", "");
        }

        memset(trxTable.reversalStatus, 0, sizeof(trxTable.reversalStatus));
        if (hostResult == TXN_RECEIVE_FROM_HOST_FAILED ||
            hostResult == TXN_RECEIVE_FROM_HOST_TIMEOUT)
        {
            sprintf(trxTable.reversalStatus, STATUS_PENDING);
            isReversal = true;
        }
        else
        {
            logInfo("Host failed with error, No Reversal required");
        }
    }

    updateHostResponse(trxTable);

    if (isReversal)
    {
        // performReversal(transactionId, true, "E2");
    }
}

/**
 * Perform the service create with host
 **/
void performHostServiceCreate(struct transactionData trxData, long batchCounter,
                              uint8_t *response, size_t *response_len)
{
    char stan[7];
    sprintf(stan, "%06ld", trxData.txnCounter);
    logData("STAN : %s", stan);

    char batch[7];
    sprintf(batch, "%06ld", batchCounter);
    logData("Batch : %s", batch);

    SERVICE_CREATE_REQUEST service_create_req;
    memcpy(service_create_req.DE11_STAN, stan, sizeof(stan));

    // Amount is always 0 for service create
    memcpy(service_create_req.DE04_TXN_AMOUNT, "0", sizeof("0"));

    memcpy(service_create_req.DE35_TRACK_2_DATA, trxData.track2Enc, sizeof(trxData.track2Enc));
    logData("Track 2 : %s", service_create_req.DE35_TRACK_2_DATA);

    char ksn[45];
    strcpy(ksn, "0020");
    strcat(ksn, trxData.ksn);
    memcpy(service_create_req.DE53_SECURITY_DATA, ksn, sizeof(ksn));
    logData("KSN : %s", service_create_req.DE53_SECURITY_DATA);

    service_create_req.DE55_ICC_DATA.value = (char *)malloc(trxData.iccDataLen + 1);
    memcpy(service_create_req.DE55_ICC_DATA.value, trxData.iccData, trxData.iccDataLen);
    service_create_req.DE55_ICC_DATA.value[trxData.iccDataLen] = '\0';
    service_create_req.DE55_ICC_DATA.len = trxData.iccDataLen;

    memcpy(service_create_req.DE56_BATCH_NUMBER, batch, sizeof(batch));
    memcpy(service_create_req.DE62_INVOICE_NUMBER, stan, sizeof(stan));

    createTxnDataForOnline(trxData);

    SERVICE_CREATE_RESPONSE service_create_resp;
    ISO8583_ERROR_CODES ret = process_service_create_transaction(&service_create_req, &service_create_resp);

    free(service_create_req.DE55_ICC_DATA.value);

    if (ret == TXN_SUCCESS)
    {
        logData("Stan Received: %s", service_create_resp.DE11_STAN);
        logData("Txn Time Received : %s", service_create_resp.DE12_TXN_TIME);
        logData("Txn Date Received : %s", service_create_resp.DE13_TXN_DATE);
        logData("RRN Received : %s", service_create_resp.DE37_RRN);
        logData("Authcode Received : %s", service_create_resp.DE38_AUTH_CODE);
        logData("Response Code Received : %s", service_create_resp.DE39_RESPONSE_CODE);
        logData("Field 55 Received : %s", service_create_resp.DE55_ICC_DATA.value);

        *response_len = 4;
        response[0] = 0x8A;
        response[1] = 0x02;
        response[2] = (uint8_t)service_create_resp.DE39_RESPONSE_CODE[0];
        response[3] = (uint8_t)service_create_resp.DE39_RESPONSE_CODE[1];

        if (service_create_resp.DE55_ICC_DATA.value != NULL)
        {
            *response_len = 4 + service_create_resp.DE55_ICC_DATA.len / 2;
            uint8_t issuer_authentication_data[96] = {0};
            size_t issuer_authentication_data_len = service_create_resp.DE55_ICC_DATA.len / 2;

            libtlv_hex_to_bin(service_create_resp.DE55_ICC_DATA.value, issuer_authentication_data,
                              &issuer_authentication_data_len);

            logData("91 Length : %d", issuer_authentication_data_len);
            for (int i = 0; i < issuer_authentication_data_len; i++)
            {
                response[i + 4] = issuer_authentication_data[i];
            }
        }
    }
    else
    {
        logError("Service Create Host data failed with error code : %d", ret);
    }

    doServiceCreateUpdateHostResponseInDb(service_create_resp, trxData.transactionId, ret);
}

/**
 * Update the service create txn host response in db
 **/
void doServiceCreateUpdateHostResponseInDb(SERVICE_CREATE_RESPONSE service_create_resp,
                                           const char *transactionId, int hostResult)
{
    TransactionTable trxTable = getTransactionTableData(transactionId);
    logData("Transaction id received : %s", trxTable.transactionId);
    sprintf(trxTable.reversalStatus, "%s", "");
    bool isReversal = false;

    if (hostResult == 0)
    {
        if (service_create_resp.DE37_RRN != NULL)
        {
            sprintf(trxTable.rrn, "%s", service_create_resp.DE37_RRN);
        }
        else
        {
            memset(trxTable.rrn, 0, sizeof(trxTable.rrn));
            logWarn("RRN Not Received");
        }

        if (service_create_resp.DE38_AUTH_CODE != NULL)
        {
            sprintf(trxTable.authCode, "%s", service_create_resp.DE38_AUTH_CODE);
        }
        else
        {
            memset(trxTable.authCode, 0, sizeof(trxTable.authCode));
            logWarn("Authcode Not Received %s.", "");
        }

        if (service_create_resp.DE39_RESPONSE_CODE != NULL)
        {
            sprintf(trxTable.hostResultCode, "%s", service_create_resp.DE39_RESPONSE_CODE);
        }
        else
        {
            memset(trxTable.hostResultCode, 0, sizeof(trxTable.hostResultCode));
            logWarn("Response Code Not Received %s.", "");
        }

        memset(trxTable.updateAmount, 0, sizeof(trxTable.updateAmount));

        if (strcmp(trxTable.hostResultCode, "00") == 0)
        {
            sprintf(trxTable.hostStatus, "%s", STATUS_SUCCESS);
        }
        else
        {
            sprintf(trxTable.hostStatus, "%s", STATUS_FAILURE);
        }
    }
    else
    {
        sprintf(trxTable.hostError, "%s", getHostErrorString(hostResult));
        logWarn("Host error : %s", getHostErrorString(hostResult));
        sprintf(trxTable.hostStatus, "%s", STATUS_FAILURE);
        logError("Host Failed");
        trxTable.hostRetry++;

        memset(trxTable.rrn, 0, sizeof(trxTable.rrn));
        if (service_create_resp.DE37_RRN != NULL)
        {
            sprintf(trxTable.rrn, "%s", service_create_resp.DE37_RRN);
        }
        else
        {
            logWarn("RRN Not Received");
        }

        memset(trxTable.authCode, 0, sizeof(trxTable.authCode));
        if (service_create_resp.DE38_AUTH_CODE != NULL)
        {
            sprintf(trxTable.authCode, "%s", service_create_resp.DE38_AUTH_CODE);
        }
        else
        {
            logWarn("Authcode Not Received %s.", "");
        }

        memset(trxTable.hostResultCode, 0, sizeof(trxTable.hostResultCode));
        if (service_create_resp.DE39_RESPONSE_CODE != NULL)
        {
            sprintf(trxTable.hostResultCode, "%s", service_create_resp.DE39_RESPONSE_CODE);
        }
        else
        {
            logWarn("Response Code Not Received %s.", "");
        }

        memset(trxTable.reversalStatus, 0, sizeof(trxTable.reversalStatus));
        if (hostResult == TXN_RECEIVE_FROM_HOST_FAILED ||
            hostResult == TXN_RECEIVE_FROM_HOST_TIMEOUT)
        {
            logInfo("Service create no need for reversal");
            // sprintf(trxTable.reversalStatus, STATUS_PENDING);
            // isReversal = true;
        }
        else
        {
            logInfo("Host failed with error, No Reversal required");
        }
    }

    updateHostResponse(trxTable);

    if (isReversal)
    {
        // performReversal(transactionId, true, "E2");
    }
}

/**
 * Perform the money add with host
 **/
void performHostMoneyAdd(struct transactionData trxData, long batchCounter,
                         uint8_t *response, size_t *response_len)
{
    char stan[7];
    sprintf(stan, "%06ld", trxData.txnCounter);
    logData("STAN : %s", stan);

    char batch[7];
    sprintf(batch, "%06ld", batchCounter);
    logData("Batch : %s", batch);

    MONEY_ADD_REQUEST money_add_req;
    memcpy(money_add_req.DE11_STAN, stan, sizeof(stan));

    char sAmount[13];
    convertAmount(trxData.amount, sAmount);

    logData("Amount for money add : %s", sAmount);
    memcpy(money_add_req.DE04_TXN_AMOUNT, sAmount, sizeof(sAmount));

    memcpy(money_add_req.DE35_TRACK_2_DATA, trxData.track2Enc, sizeof(trxData.track2Enc));

    char ksn[45];
    strcpy(ksn, "0020");
    strcat(ksn, trxData.ksn);
    memcpy(money_add_req.DE53_SECURITY_DATA, ksn, sizeof(ksn));

    money_add_req.DE55_ICC_DATA.value = (char *)malloc(trxData.iccDataLen + 1);
    memcpy(money_add_req.DE55_ICC_DATA.value, trxData.iccData, trxData.iccDataLen);
    money_add_req.DE55_ICC_DATA.value[trxData.iccDataLen] = '\0';
    money_add_req.DE55_ICC_DATA.len = trxData.iccDataLen;

    memcpy(money_add_req.DE56_BATCH_NUMBER, batch, sizeof(batch));
    memcpy(money_add_req.DE62_INVOICE_NUMBER, stan, sizeof(stan));

    createTxnDataForOnline(trxData);

    MONEY_ADD_RESPONSE money_add_resp;
    ISO8583_ERROR_CODES ret = process_money_add_transaction(&money_add_req, &money_add_resp);

    free(money_add_req.DE55_ICC_DATA.value);

    if (ret == TXN_SUCCESS)
    {
        logData("Stan Received: %s", money_add_resp.DE11_STAN);
        logData("Txn Time Received : %s", money_add_resp.DE12_TXN_TIME);
        logData("Txn Date Received : %s", money_add_resp.DE13_TXN_DATE);
        logData("RRN Received : %s", money_add_resp.DE37_RRN);
        logData("Authcode Received : %s", money_add_resp.DE38_AUTH_CODE);
        logData("Money add Code Received : %s", money_add_resp.DE39_RESPONSE_CODE);
        logData("Field 55 Received : %s", money_add_resp.DE55_ICC_DATA.value);

        *response_len = 4;
        response[0] = 0x8A;
        response[1] = 0x02;
        response[2] = (uint8_t)money_add_resp.DE39_RESPONSE_CODE[0];
        response[3] = (uint8_t)money_add_resp.DE39_RESPONSE_CODE[1];

        if (money_add_resp.DE55_ICC_DATA.value != NULL)
        {
            *response_len = 4 + money_add_resp.DE55_ICC_DATA.len / 2;
            uint8_t issuer_authentication_data[96] = {0};
            size_t issuer_authentication_data_len = money_add_resp.DE55_ICC_DATA.len / 2;

            libtlv_hex_to_bin(money_add_resp.DE55_ICC_DATA.value, issuer_authentication_data,
                              &issuer_authentication_data_len);

            logData("91 Length : %d", issuer_authentication_data_len);
            for (int i = 0; i < issuer_authentication_data_len; i++)
            {
                response[i + 4] = issuer_authentication_data[i];
            }
        }
    }
    else
    {
        logError("Host data failed with error code : %d", ret);
    }

    doMoneyAddHostResponseInDb(money_add_resp, trxData.transactionId, ret);
}

/**
 * Update the money add txn host response in db
 **/
void doMoneyAddHostResponseInDb(MONEY_ADD_RESPONSE money_add_resp,
                                const char *transactionId, int hostResult)
{
    TransactionTable trxTable = getTransactionTableData(transactionId);
    logData("Transaction id received : %s", trxTable.transactionId);
    sprintf(trxTable.reversalStatus, "%s", "");
    bool isReversal = false;

    if (hostResult == 0)
    {
        if (money_add_resp.DE37_RRN != NULL)
        {
            sprintf(trxTable.rrn, "%s", money_add_resp.DE37_RRN);
        }
        else
        {
            memset(trxTable.rrn, 0, sizeof(trxTable.rrn));
            logWarn("RRN Not Received");
        }

        if (money_add_resp.DE38_AUTH_CODE != NULL)
        {
            sprintf(trxTable.authCode, "%s", money_add_resp.DE38_AUTH_CODE);
        }
        else
        {
            memset(trxTable.authCode, 0, sizeof(trxTable.authCode));
            logWarn("Authcode Not Received %s.", "");
        }

        if (money_add_resp.DE39_RESPONSE_CODE != NULL)
        {
            sprintf(trxTable.hostResultCode, "%s", money_add_resp.DE39_RESPONSE_CODE);
        }
        else
        {
            memset(trxTable.hostResultCode, 0, sizeof(trxTable.hostResultCode));
            logWarn("Response Code Not Received %s.", "");
        }

        if (money_add_resp.DE55_ICC_DATA.value != NULL)
        {
            // Get the update amount from the last 6 byte data
            // TODO : Parse using TLV
            char updateAmount[13];
            int j = 0;
            int iccLen = money_add_resp.DE55_ICC_DATA.len;
            for (int i = iccLen - 12; i < iccLen; i++)
            {
                updateAmount[j++] = money_add_resp.DE55_ICC_DATA.value[i];
            }
            updateAmount[j] = '\0';
            logData("Updated amount received : %s", updateAmount);
            sprintf(trxTable.updateAmount, "%s", updateAmount);
        }
        else
        {
            memset(trxTable.updateAmount, 0, sizeof(trxTable.updateAmount));
        }

        if (strcmp(trxTable.hostResultCode, "00") == 0)
        {
            sprintf(trxTable.hostStatus, "%s", STATUS_SUCCESS);
        }
        else
        {
            sprintf(trxTable.hostStatus, "%s", STATUS_FAILURE);
        }
    }
    else
    {
        sprintf(trxTable.hostError, "%s", getHostErrorString(hostResult));
        logWarn("Host error : %s", getHostErrorString(hostResult));
        sprintf(trxTable.hostStatus, "%s", STATUS_FAILURE);
        logError("Host Failed");
        trxTable.hostRetry++;

        memset(trxTable.rrn, 0, sizeof(trxTable.rrn));
        if (money_add_resp.DE37_RRN != NULL)
        {
            sprintf(trxTable.rrn, "%s", money_add_resp.DE37_RRN);
        }
        else
        {
            logWarn("RRN Not Received");
        }

        memset(trxTable.authCode, 0, sizeof(trxTable.authCode));
        if (money_add_resp.DE38_AUTH_CODE != NULL)
        {
            sprintf(trxTable.authCode, "%s", money_add_resp.DE38_AUTH_CODE);
        }
        else
        {
            logWarn("Authcode Not Received %s.", "");
        }

        memset(trxTable.hostResultCode, 0, sizeof(trxTable.hostResultCode));
        if (money_add_resp.DE39_RESPONSE_CODE != NULL)
        {
            sprintf(trxTable.hostResultCode, "%s", money_add_resp.DE39_RESPONSE_CODE);
        }
        else
        {
            logWarn("Response Code Not Received %s.", "");
        }

        memset(trxTable.reversalStatus, 0, sizeof(trxTable.reversalStatus));
        if (hostResult == TXN_RECEIVE_FROM_HOST_FAILED ||
            hostResult == TXN_RECEIVE_FROM_HOST_TIMEOUT)
        {
            sprintf(trxTable.reversalStatus, STATUS_PENDING);
            isReversal = true;
        }
        else
        {
            logInfo("Host failed with error, No Reversal required");
        }
    }

    updateHostResponse(trxTable);

    if (isReversal)
    {
        performReversal(transactionId, true, "E2");
    }
}
