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
        if (isReversalOngoing == true)
        {
            logData("There is already ongoing reversal, so thread no need to do again.");
            return;
        }
        TransactionTable trxTable = getTransactionTableData(transactionId);

        if (strlen(trxTable.reversalMac) == 0)
        {
            if (appConfig.useAirtelHost)
            {
                strcpy(trxTable.reversalMac, "NoMac");
            }
            else
            {
                logData("There is no reversal mac present so generating it");
                trxTable = generateMacReversal(trxTable, REVERSAL_CODE_22);
                trxTable = generateMacEcho(trxTable, REVERSAL_CODE_22);
            }
            strcpy(trxTable.reversalInputResponseCode, REVERSAL_CODE_22);
            strcpy(trxTable.reversalStatus, STATUS_PENDING);
            updateReversalPreData(trxTable);
        }
        performReversal(trxTable);
    }
    else
    {
        logData("There are no pending reversals");
    }
}

/**
 * Perform the reversal with host connection
 **/
void performReversal(TransactionTable trxTable)
{
    logInfo("Going to perform the reversal for transaction : %s", trxTable.transactionId);
    isReversalOngoing = true;

    char responseMessage[1024 * 5] = {0};
    if (!appConfig.useAirtelHost)
    {
        char *message = generateReversalRequest(trxTable);
        printf("Reversal Message : \n");
        printf(message);
        printf("\n");

        logData("Sending data to PayTM");
        sendHostRequest(message, appConfig.reversalUrl, responseMessage);
        logData("Response length from server : %d", strlen(responseMessage));
        printf("Response Message : %s", responseMessage);
        free(message);
    }
    else
    {
        char *message = generateAirtelReversalRequest(trxTable);
        printf("Reversal Message : \n");
        printf(message);
        printf("\n");

        removeSpaces(message);
        char hmac_hex[HMAC_HEX_SIZE];
        calculate_hmac_sha256(appConfig.airtelSignSalt, message, hmac_hex);
        logData("Hmac hex of message : %s", hmac_hex);

        char *uniqueId = malloc(UUID_STR_LEN);
        generateUUID(uniqueId);
        logInfo("Unique request Id : %s", uniqueId);

        char body[1024 * 24] = {0};
        strcpy(body, message);
        free(message);
        logData("Sending data to Airtel for reversal");
        updateAirtelRequestData(trxTable.transactionId, body);
        sendAirtelHostRequest(body, appConfig.airtelReversalUrl, responseMessage,
                              uniqueId, hmac_hex);

        logData("Response length from server : %d", strlen(responseMessage));
        printf("Response Message : %s\n", responseMessage);
        updateAirtelResponseData(trxTable.transactionId, responseMessage);
    }

    if (strlen(responseMessage) == 0)
    {
        isReversalOngoing = false;
        logError("There is no response to parse for reversal");
        return;
    }

    HttpResponseData httpResponseData = parseHttpResponse(responseMessage);

    if (httpResponseData.code == 200 && httpResponseData.messageLen != 0)
    {
        if (appConfig.useAirtelHost == false)
        {
            ReversalResponse reversalResponse = parseReversalHostResponseData(httpResponseData.message);
            updateReversalResponse(reversalResponse, trxTable.transactionId);
        }
        else
        {
            logData("Parsing reversal / ack for airtel");
            AirtelHostResponse hostResponse = parseAirtelHostResponseData(httpResponseData.message);
            updateAirelReversalResponse(hostResponse, trxTable.transactionId);
        }
    }
    else if (httpResponseData.code == 400 && httpResponseData.messageLen != 0)
    {
        if (appConfig.useAirtelHost)
        {
            logData("Parsing reversal error for airtel with code as 400");
            AirtelHostResponse hostResponse = parseAirtelHostResponseData(httpResponseData.message);
            updateAirelReversalResponse(hostResponse, trxTable.transactionId);
        }
    }
    else
    {
        logError("Http response error for reversal");
    }

    isReversalOngoing = false;

    free(httpResponseData.message);
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
