#ifndef ABTDBMANAGER_H_
#define ABTDBMANAGER_H_

#include "config.h"
#include "sqlite3.h"
#include "commandmanager.h"

/**
 * Structure of the transaction table
 **/
typedef struct AbtTransactionTableType
{
    long id;
    char transactionId[40];
    long trxCounter;
    char stan[7];
    int batch;
    char amount[13];
    char time[7];
    char date[5];
    char year[5];
    char maskPAN[21];
    char token[65];
    char iccData[4096];
    int iccDataLen;
    char orderId[30];
    int txnStatus;
    char terminalId[9];
    char merchantId[20];
    int hostRetry;
    char deviceId[10];
    char gateStatus[20];
    int trxTypeCode;
    int deviceType;
    char deviceCode[10];
    int locationCode;
    int operatorCode;
    int tariffVersion;
    int deviceModeCode;
    int deviceDateKey;
    char txnDeviceTime[50];
    char hostStatus[30];
    char hostReason[1024];
    int hostUpdateDays;

} AbtTransactionTable;

typedef struct abt_tap_response
{
    char tuid[100];
    char result[100];
    char reason[1024];
} AbtTapResponse;

/**
 ** To get the count of available transaction records
 */
void checkAbtRecordCount();

/**
 * Create a ABT transaction data for offline txn
 **/
void createAbtTransactionData(struct transactionData *trxData);

/**
 * Get the current active pending transactions with hosts in db
 **/
int getAbtTransactionStatusCount(char status[50]);

/**
 * Get the json data of a abt transaction table
 **/
json_object *getAbtJsonTxnData(AbtTransactionTable trxData);

/**
 * populate the table from the db statement
 **/
AbtTransactionTable populateAbtTableData(sqlite3_stmt *statement);

/**
 * Fetch the ABT data to be sent to client
 **/
char *fetchAbtData(struct abtFetchData fData);

/**
 * Initiate the abt pending / nok transactions from a thread
 **/
void *handleAbtTransactions();

/**
 * To Print abt response
 */
void printAbtResponse(AbtTapResponse abtResponse);

/**
 * To save the abt transaction response with the matching record in db
 */
void saveAbtResponseToDb(AbtTapResponse abtResponses[], int count);

/**
 * To get the count of ABT transactions for a particular transaction id
 */
int getAbtTrxCount(char *transactionId);

/**
 * Update the offline transaction status
 **/
void updateAbtTransactionStatus(AbtTapResponse abtTapResponse);

/**
 * To get the transactions that has host status ok and within x days
 * as per the abtDataRetentionPeriodInDays
 */
int getAbtOkFilterDateTransactionsCount();

/**
 * To search the records and delete it
 */
void deleteAbtTransactions();

/**
 * To delete the ABT transactions at a specified time for house keeping
 * Only the OK Transactions will be deleted
 **/
void *houseKeepingAbtTransactions();

/**
 * To increment the host retry count
 */
void updateAbtHostRetry(const char transactionId[50]);

#endif