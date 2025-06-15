#ifndef DBOPERATIONS_H_
#define DBOPERATIONS_H_

#include <json-c/json.h>
#include <stdbool.h>

#include "config.h"
#include "sqlite3.h"

/**
 * Structure of the transaction table
 **/
typedef struct TransactionTableType
{
    long id;
    char transactionId[40];
    char trxType[50];
    char trxBin[3];
    char processingCode[7];
    long trxCounter;
    char stan[7];
    int batch;
    char amount[13];
    char time[7];
    char date[5];
    char year[5];
    char aid[20];
    char maskPAN[21];
    char token[65];
    char effectiveDate[11];
    char serviceId[5];
    char serviceIndex[3];
    char serviceData[193];
    char serviceControl[5];
    char serviceBalance[13];
    char iccData[4096];
    int iccDataLen;
    char ksn[21];
    char panEncrypted[50];
    char track2Enc[100];
    char expDateEnc[17];
    char macKsn[21];
    char mac[17];
    char orderId[30];
    char txnStatus[20];
    char hostStatus[20];
    char updateAmount[13];
    char updatedBalance[13];
    char terminalId[9];
    char merchantId[20];
    int hostRetry;
    char rrn[13];
    char authCode[7];
    char hostResponseTimeStamp[20];
    char hostResultStatus[20];
    char hostResultMessage[1024];
    char hostResultCode[3];
    char hostResultCodeId[20];
    char hostIccData[512];
    int hostError;
    char reversalStatus[50];
    char moneyAddTrxType[20];
    char acquirementId[50];
    char reversalResponsecode[3];
    char reversalRRN[13];
    char reversalAuthCode[7];
    int reversalManualCleared;
    char reversalKsn[21];
    char reversalMac[17];
    char echoKsn[21];
    char echoMac[17];
    char reversalInputResponseCode[3];
    char hostErrorCategory[50];
    int airtelTxnStatus;
    char airtelTxnId[100];
    char airtelRequestData[1024 * 24];
    char airtelResponseData[10240];
    char airtelResponseCode[100];
    char airtelResponseDesc[1024];
    char airtelSwitchResponseCode[100];
    char airtelSwitchTerminalId[100];
    char airtelSwichMerchantId[100];
    char airtelAckTxnType[100];
    char airtelAckPaymentMode[100];
    char airtelAckRefundId[100];

} TransactionTable;

typedef struct host_response
{
    int status; // 0 - Success, -1 - Fail
    char hostResponseTimeStamp[20];
    char resultStatus[30];
    char bankResultCode[3];
    char invoiceNumber[7];
    char retrievalReferenceNumber[13];
    char authorizationCode[7];
    char iccData[128];
    char acquirementId[50];
    char resultCodeId[50];
    char resultMsg[200];
} HostResponse;

typedef struct airtel_host_response
{
    int status; // 0 - Success, -1 - Fail
    char hostResponseTimeStamp[20];
    char responseCode[50];
    char responseDesc[200];
    char switchResponseCode[20];
    char txnId[100];
    char iccData[1024];
    char switchTerminalId[100];
    char rrn[100];
    char stan[20];
    char switchMerchantId[100];
    char authCode[100];
    char txnType[100];
    char paymentMode[100];
    char refundId[200];

} AirtelHostResponse;

typedef struct offline_sale_response
{
    char resultStatus[30];
    char resultMessage[1024];
    char resultCode[10];
    char resultCodeId[50];
    char stan[7];
    char orderId[30];
    char responseTimeStamp[20];
    int airtelTxnStatus;
    char airtelTxnId[100];
} OfflineSaleResponse;

typedef struct reversal_response
{
    int status; // 0 - Success, -1 - Fail
    char bankResultCode[3];
    char retrievalReferenceNumber[13];
    char authorizationCode[7];
} ReversalResponse;

typedef struct verify_terminal_response
{
    int status; // 0 - Success, -1 - Fail
    char timeStamp[20];
    char resultStatus[10];
    char resultMsg[50];
    char resultcode[2];
    char bankTid[30];
    char bankMid[30];
    char mid[50];
    char merchantName[50];
    char merchantAddress[300];
    char mccCode[10];
} VerifyTerminalResponse;

typedef struct airtel_verify_terminal_response
{
    int status;
    char code[100];
    char description[1024];
} AirtelVerifyTerminalResponse;

typedef struct airtel_health_check_response
{
    int status;
    char code[100];
    char description[1024];
    char serverTime[100];
    char serverStatus[100];
} AirtelHealthCheckResponse;

typedef struct fetch_trx_id
{
    char trxId[40];
} FetchTrxId;

/**
 ** To get the count of available transaction records
 */
void checkRecordCount();

/**
 * Create a transaction data in db
 **/
void createTransactionData(struct transactionData *trxData);

/**
 * Print a single row
 **/
void printTransactionRow(TransactionTable trxData);

/**
 * Get the active pending transactions count that are yet to be processed
 * by host
 **/
int getActivePendingTransactions();

/**
 * Get the current active pending transactions amount total with hosts in db
 **/
double getActivePendingTransactionsAmount();

/**
 * Get the current active pending transactions total amount with required host error category with hosts in db
 **/
double getActivePendingHostErrorCategoryTransactionsAmount(const char *errorCategory);

/**
 * Process the pending transactions with host
 **/
void processHostPendingTransactions();

/**
 * Get the fetch query to retrieve the data from db
 **/
const char *getFetchquery(enum fetch_mode fetchMode);

/**
 * Fetch the data that is be returned to the client
 **/
char *fetchHostData(int maxRecords, enum fetch_mode fetchMode);

/**
 * Get the json format of the transaction data
 **/
json_object *getJsonTxnData(TransactionTable trxData);

/**
 * Clear up the local store fetch keys
 **/
char *clearFetchedData();

/**
 * Populate the table structure from the db query
 **/
TransactionTable populateTableData(sqlite3_stmt *statement);

/**
 * Get the transaction data count
 **/
int getTrxTableDataCount(const char *orderId, const char *stan, const char *trxBin,
                         const char *txnStatus, const char *hostStatus, char *transactionId);

/**
 * Update the offline transaction status
 **/
void updateOfflineTransactionStatus(const char *transactionId, OfflineSaleResponse offlineSaleResponse);

/**
 * Get the transaction data count of offline pending transactions
 **/
int getOfflinePendingTrxCount();

/**
 * To update host response in database
 **/
void updateHostResponseInDb(HostResponse hostResponse, char transactionId[40]);

/**
 * To save the offline response with the matching record in db
 */
void saveOfflineResponseToDb(OfflineSaleResponse offlineResponses[], int count);

/**
 * To print the offline response information
 **/
void printOfflineResponse(OfflineSaleResponse offlineResponse);

/**
 * Create a balance update / Service Creation txn data
 **/
void createTxnDataForOnline(struct transactionData trxData);

/**
 * Check is there any pending reversal and get or no reversal message
 **/
char *getReversal();

/**
 * Make the reversal status empty
 **/
void resetReversalStatus(const char *status, const char *transactionId);

/**
 * to update the host error category
 */
void updateHostErrorCategory(const char *transactionId, const char *hostErrorCategory);

/**
 * Get the current active pending transactions with required host error category with hosts in db
 **/
int getActivePendingHostErrorCategoryTransactions(const char *errorCategory);

/**
 * Get the transaction table data for the requested id
 **/
TransactionTable getTransactionTableData(const char *transactionId);

/**
 * Update the transaction status
 **/
void updateTransactionStatus(const char *transactionId, const char *txnStatus, const char *updatedBalance);

/**
 * Update reversal response back to db
 **/
void updateReversalResponse(ReversalResponse reversalResponse, const char *transactionId);

/**
 * Update the reversal data
 **/
void updateReversalPreData(TransactionTable trxTable);

/**
 * Check whether is there any reversal pending
 **/
bool isTherePendingReversal(char *transactionId);

/**
 * Clear the reversal manually
 */
void clearReversalManual();

/**
 * To perform mac recalculation
 */
void performMacRecalculation();

/**
 * To update the mac recalculation to db
 */
void updateReCalcMacData(struct transactionData trxData);

/**
 * To process paytm host and send to paytm and receive response
 */
void processPayTmHost(int index, int max, TransactionTable trxDataList[]);

/**
 * To process airtel host and send to airtel and receive response
 */
void processAirtelHost(int index, int max, TransactionTable trxDataList[]);

/**
 * To save the Airtel offline response with the matching record in db
 */
void saveAirtelOfflineResponseToDb(OfflineSaleResponse offlineResponses[], int count);

/**
 * Update the offline transaction status for Airtel Response
 **/
void updateAirtelOfflineTransactionStatus(const char *transactionId, OfflineSaleResponse offlineSaleResponse);

/**
 * Get the transaction data count For Airtel
 **/
int getTrxTableDataCountForAirtel(const char *orderId, const char *trxBin,
                                  const char *txnStatus, const char *hostStatus, char *transactionId);

void updateAirtelResponseData(char transactionId[38], char responseData[1024 * 10]);

void updateAirtelRequestData(char transactionId[38], char requestData[1024 * 24]);

/**
 * To update host response in database for Airtel
 **/
void updateAirtelHostResponseInDb(AirtelHostResponse hostResponse, char transactionId[40]);

/**
 * Update reversal response back to db for Airtel
 **/
void updateAirelReversalResponse(AirtelHostResponse airtelHostResponse, const char *transactionId);

/*
 * To create test transaction data for offline
 */
// void createTestTransactionData();

// void deleteAllTrx();

#endif