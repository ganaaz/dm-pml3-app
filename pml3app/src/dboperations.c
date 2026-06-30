#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <uuid/uuid.h>
#include <libpay/tlv.h>
#include <pthread.h>
#include <unistd.h>
#include <json-c/json.h>
#include <openssl/hmac.h>
#include <stdbool.h>

#include "include/dboperations.h"
#include "include/commonutil.h"
#include "include/config.h"
#include "include/sqlite3.h"
#include "include/hostmanager.h"
#include "include/logutil.h"
#include "include/responsemanager.h"
#include "include/commandmanager.h"
#include "include/errorcodes.h"
#include "include/appcodes.h"
#include "http-parser/http_util.h"

#define MAX_FETCH_COUNT 50
#define HMAC_HEX_SIZE (EVP_MAX_MD_SIZE * 2 + 1)

extern sqlite3 *sqlite3Db;
extern struct applicationConfig appConfig;
extern struct applicationData appData;
extern _Atomic int activePendingTxnCount;
extern _Atomic enum device_status DEVICE_STATUS;
extern struct transactionData currentTxnData;

int activeFetchId = 0;
int fetchedCount = 0;

FetchTrxId fetchTransactionIdList[MAX_FETCH_COUNT];

extern char currentTrxType[100];
extern char lastPanDigit[10];

/**
 * To check if the column exists
 */
bool columnExists(sqlite3 *db, const char *tableName, const char *columnName)
{
    sqlite3_stmt *stmt = NULL;
    char query[256];

    snprintf(query, sizeof(query), "PRAGMA table_info(%s);", tableName);

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK)
        return false;

    bool exists = false;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *colName = sqlite3_column_text(stmt, 1);
        if (colName && strcmp((const char *)colName, columnName) == 0)
        {
            exists = true;
            break;
        }
    }

    sqlite3_finalize(stmt);
    return exists;
}

/**
 * Add a column if not exists
 */
int addColumnIfNotExists(sqlite3 *db,
                         const char *tableName,
                         const char *columnName,
                         const char *columnDefinition)
{
    if (columnExists(db, tableName, columnName))
    {
        logDataEx(currentTrxType, lastPanDigit, "Column %s already exists", columnName);
        return SQLITE_OK;
    }

    char query[512];
    snprintf(query, sizeof(query),
             "ALTER TABLE %s ADD COLUMN %s %s;",
             tableName, columnName, columnDefinition);

    char *errMsg = NULL;
    int rc = sqlite3_exec(db, query, NULL, NULL, &errMsg);

    if (rc != SQLITE_OK)
    {
        logDataEx(currentTrxType, lastPanDigit, "Failed to add column %s: %s", columnName, errMsg);
        sqlite3_free(errMsg);
        return rc;
    }

    logDataEx(currentTrxType, lastPanDigit, "Column %s added successfully", columnName);
    return SQLITE_OK;
}

/**
 ** To get the count of available transaction records
 */
void checkRecordCount()
{
    const char *query = "SELECT count(*) FROM Transactions ";
    sqlite3_stmt *statement;
    int count = 0;

    logInfoEx(currentTrxType, lastPanDigit, "Going to get the transaction record count with query : %s", query);

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare checkRecordCount query !!!");
        return;
    }

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int(statement, 0);
        break;
    }

    sqlite3_finalize(statement);

    logInfoEx(currentTrxType, lastPanDigit, "Total Transaction records available are : %d", count);
}

/**
 * Create a transaction data for offline txn
 **/
void createTransactionData(struct transactionData *trxData)
{
    char *insertQuery = "INSERT INTO Transactions("
                        "TransactionId,"
                        "TrxType, "
                        "TrxBin, "
                        "ProcessingCode, "
                        "TrxCounter, "
                        "Stan,"
                        "Batch,"
                        "Amount,"
                        "Time,"
                        "Date,"
                        "Year,"
                        "Aid,"
                        "MaskedPan,"
                        "Token, "
                        "EffectiveDate,"
                        "ServiceId,"
                        "ServiceIndex,"
                        "ServiceData,"
                        "ServiceControl,"
                        "ServiceBalance,"
                        "ICCData,"
                        "ICCDataLen,"
                        "KSN, "
                        "PanEncrypted, "
                        "ExpDateEnc, "
                        "MacKsn, "
                        "Mac, "
                        "OrderId, "
                        "TxnStatus,"
                        "HostStatus, "
                        "TerminalId,"
                        "MerchantId,"
                        "HostRetry, "
                        "HostError, "
                        "AcqTransactionId, "
                        "AcqUniqueTransactionId "
                        ") "
                        "VALUES("
                        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *statement;
    int result = sqlite3_prepare_v2(
        sqlite3Db,
        insertQuery,
        -1,
        &statement,
        NULL);

    logDataEx(currentTrxType, lastPanDigit, "Transaction Status : %s", trxData->txnStatus);
    char trxBin[3];
    sprintf(trxBin, "%02X", trxData->trxTypeBin);

    if (result == SQLITE_OK)
    {
        sqlite3_bind_text(statement, 1, trxData->transactionId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, trxData->trxType, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 3, trxBin, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 4, trxData->processingCode, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 5, trxData->txnCounter);
        sqlite3_bind_text(statement, 6, trxData->stan, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 7, appData.batchCounter);
        sqlite3_bind_text(statement, 8, trxData->sAmount, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 9, trxData->time, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 10, trxData->date, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 11, trxData->year, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 12, trxData->aid, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 13, trxData->maskPan, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 14, trxData->token, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 15, trxData->effectiveDate, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 16, trxData->serviceId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 17, trxData->serviceIndex, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 18, trxData->serviceData, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 19, trxData->serviceControl, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 20, trxData->serviceBalance, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 21, trxData->iccData, trxData->iccDataLen, SQLITE_STATIC);
        sqlite3_bind_int(statement, 22, trxData->iccDataLen);
        sqlite3_bind_text(statement, 23, trxData->ksn, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 24, trxData->panEncrypted, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 25, trxData->expDateEnc, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 26, trxData->macKsn, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 27, trxData->mac, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 28, trxData->orderId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 29, trxData->txnStatus, -1, SQLITE_STATIC);
        if (strcmp(trxData->txnStatus, STATUS_SUCCESS) == 0) // Host Status
            sqlite3_bind_text(statement, 30, STATUS_PENDING, -1, SQLITE_STATIC);
        else
            sqlite3_bind_text(statement, 30, STATUS_NA, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 31, appConfig.terminalId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 32, appConfig.merchantId, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 33, 0);                                                   // Host Retry
        sqlite3_bind_text(statement, 34, "", -1, SQLITE_STATIC);                              // Host Error
        sqlite3_bind_text(statement, 35, trxData->acqTransactionId, -1, SQLITE_STATIC);       // Acq Trx Id
        sqlite3_bind_text(statement, 36, trxData->acqUniqueTransactionId, -1, SQLITE_STATIC); // Acq Unit trx id

        logDataEx(currentTrxType, lastPanDigit, "Going to perform the insert of offline transaction date");
        int result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        logDataEx(currentTrxType, lastPanDigit, "Insert result : %d", result);

        if (result == SQLITE_DONE)
        {
            logInfoEx(currentTrxType, lastPanDigit, "Transaction data inserted successfully : %s", trxData->transactionId);
        }
        else
        {
            logWarnEx(currentTrxType, lastPanDigit, "Insert data failed !!!");
        }
    }
    else
    {
        logWarnEx(currentTrxType, lastPanDigit, "Unable to prepare the insert query !!!");
    }
}

/**
 * Create a balance update / Service Creation txn data
 **/
void createTxnDataForOnline(struct transactionData trxData)
{
    char *insertQuery = "INSERT INTO Transactions("
                        "TransactionId,"
                        "TrxType, "
                        "TrxBin, "
                        "ProcessingCode, "
                        "TrxCounter, "
                        "Stan,"
                        "Batch,"
                        "Amount,"
                        "Time,"
                        "Date,"
                        "Year,"
                        "Aid,"
                        "MaskedPan,"
                        "Token, "
                        "EffectiveDate,"
                        "ServiceId,"
                        "ServiceIndex,"
                        "ServiceData,"
                        "ServiceControl,"
                        "ServiceBalance,"
                        "ICCData,"
                        "ICCDataLen,"
                        "KSN, "
                        "PanEncrypted, "
                        "Track2Enc, "
                        "ExpDateEnc, "
                        "MacKsn, "
                        "Mac, "
                        "OrderId, "
                        "TxnStatus,"
                        "HostStatus, "
                        "TerminalId,"
                        "MerchantId,"
                        "HostRetry, "
                        "ReversalStatus, "
                        "HostError, "
                        "MoneyAddTrxType, "
                        "MoneyAddRRN, "
                        "MoneyAddTid, "
                        "AcqTransactionId, "
                        "AcqUniqueTransactionId "
                        ") "
                        "VALUES("
                        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *statement;
    int result = sqlite3_prepare_v2(
        sqlite3Db,
        insertQuery,
        -1,
        &statement,
        NULL);

    logDataEx(currentTrxType, lastPanDigit, "Transaction Status : %s", trxData.txnStatus);
    char trxBin[3];
    sprintf(trxBin, "%02X", trxData.trxTypeBin);

    if (result == SQLITE_OK)
    {
        sqlite3_bind_text(statement, 1, trxData.transactionId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, trxData.trxType, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 3, trxBin, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 4, trxData.processingCode, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 5, trxData.txnCounter);
        sqlite3_bind_text(statement, 6, trxData.stan, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 7, appData.batchCounter);
        sqlite3_bind_text(statement, 8, trxData.sAmount, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 9, trxData.time, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 10, trxData.date, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 11, trxData.year, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 12, trxData.aid, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 13, trxData.maskPan, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 14, trxData.token, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 15, trxData.effectiveDate, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 16, trxData.serviceId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 17, trxData.serviceIndex, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 18, trxData.serviceData, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 19, trxData.serviceControl, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 20, trxData.serviceBalance, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 21, trxData.iccData, trxData.iccDataLen, SQLITE_STATIC);
        sqlite3_bind_int(statement, 22, trxData.iccDataLen);
        sqlite3_bind_text(statement, 23, trxData.ksn, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 24, trxData.panEncrypted, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 25, trxData.track2Enc, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 26, trxData.expDateEnc, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 27, trxData.macKsn, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 28, trxData.mac, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 29, trxData.orderId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 30, trxData.txnStatus, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 31, STATUS_PENDING, -1, SQLITE_STATIC); // Host Status
        sqlite3_bind_text(statement, 32, appConfig.terminalId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 33, appConfig.merchantId, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 34, 0); // Host Retry
        // sqlite3_bind_text(statement, 35, STATUS_PENDING, -1, SQLITE_STATIC); // Reversal Status
        sqlite3_bind_text(statement, 35, "", -1, SQLITE_STATIC); // Reversal Status should be empty by default
        sqlite3_bind_text(statement, 36, "", -1, SQLITE_STATIC); // Host Error
        sqlite3_bind_text(statement, 37, trxData.moneyAddTrxType, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 38, trxData.moneyAddRRN, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 39, trxData.moneyAddTid, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 40, trxData.acqTransactionId, -1, SQLITE_STATIC);       // Acq Trx Id
        sqlite3_bind_text(statement, 41, trxData.acqUniqueTransactionId, -1, SQLITE_STATIC); // Acq Unit trx id

        logDataEx(currentTrxType, lastPanDigit, "Going to perform insert of transaction for online");
        int result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        logDataEx(currentTrxType, lastPanDigit, "Insert result : %d", result);

        if (result == SQLITE_DONE)
        {
            logInfoEx(currentTrxType, lastPanDigit, "Transaction data for online is inserted successfully : %s",
                      trxData.transactionId);
        }
        else
        {
            logWarnEx(currentTrxType, lastPanDigit, "Insert data failed for online txn !!!");
        }
    }
    else
    {
        logWarnEx(currentTrxType, lastPanDigit, "Unable to prepare the insert query for online txn !!!");
    }
}

/**
 * Get the current active pending transactions with hosts in db
 **/
int getActivePendingTransactions()
{
    const char *query = "SELECT count(*) FROM Transactions WHERE HostStatus = 'Pending' and TxnStatus = 'Success' ";
    sqlite3_stmt *statement;
    int count = 0;

    logInfoEx(currentTrxType, lastPanDigit, "Going to get the active pending transactions with host");

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare getActivePendingTransactions query !!!");
        return -1;
    }

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int(statement, 0);
        break;
    }

    sqlite3_finalize(statement);

    logInfoEx(currentTrxType, lastPanDigit, "Total active pending transactions found are : %d", count);
    return count;
}

/**
 * Get the current active pending transactions amount total with hosts in db
 **/
double getActivePendingTransactionsAmount()
{
    const char *query = "SELECT sum(Amount) FROM Transactions WHERE HostStatus = 'Pending' and TxnStatus = 'Success' ";
    sqlite3_stmt *statement;
    double total = 0;

    logInfoEx(currentTrxType, lastPanDigit, "Going to get the active pending transactions amount with host");

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare getActivePendingTransactionsAmount query !!!");
        return -1;
    }

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        total = sqlite3_column_double(statement, 0);
        break;
    }

    sqlite3_finalize(statement);

    logInfoEx(currentTrxType, lastPanDigit, "Total Amount of active pending transactions found are : %f", total);
    return total;
}

/**
 * Get the current active pending transactions with required host error category with hosts in db
 **/
int getActivePendingHostErrorCategoryTransactions(const char *errorCategory)
{
    const char *query = "SELECT count(*) FROM Transactions WHERE HostStatus = 'Pending' "
                        "and TxnStatus = 'Success' and HostErrorCategory = ? ";
    sqlite3_stmt *statement;
    int count = 0;

    logInfoEx(currentTrxType, lastPanDigit, "Going to getActivePendingHostErrorCategoryTransactions with host");

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare getActivePendingTransactions query !!!");
        return -1;
    }

    sqlite3_bind_text(statement, 1, errorCategory, -1, SQLITE_STATIC);

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int(statement, 0);
        break;
    }

    sqlite3_finalize(statement);

    logInfoEx(currentTrxType, lastPanDigit, "Total getActivePendingHostErrorCategoryTransactions found are : %d", count);
    return count;
}

/**
 * Get the current active pending transactions total amount with required host error category with hosts in db
 **/
double getActivePendingHostErrorCategoryTransactionsAmount(const char *errorCategory)
{
    const char *query = "SELECT sum(Amount) FROM Transactions WHERE HostStatus = 'Pending' "
                        "and TxnStatus = 'Success' and HostErrorCategory = ? ";
    sqlite3_stmt *statement;
    double total = 0;

    logInfoEx(currentTrxType, lastPanDigit, "Going to getActivePendingHostErrorCategoryTransactionsAmount with host");

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare getActivePendingHostErrorCategoryTransactionsAmount query !!!");
        return -1;
    }

    sqlite3_bind_text(statement, 1, errorCategory, -1, SQLITE_STATIC);

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        total = sqlite3_column_double(statement, 0);
        break;
    }

    sqlite3_finalize(statement);

    logInfoEx(currentTrxType, lastPanDigit, "Total getActivePendingHostErrorCategoryTransactionsAmount found are : %f", total);
    return total;
}

/**
 * Get the transaction data count of offline pending transactions
 **/
int getOfflinePendingTrxCount()
{
    const char *query = "SELECT Count(*) FROM Transactions WHERE "
                        "HostStatus = 'Pending' "
                        "and ProcessingCode = '000000' "
                        "and TxnStatus = 'Success' order by RowId asc";

    sqlite3_stmt *statement;
    logDataEx(currentTrxType, lastPanDigit, "Going to get the count for %s", query);

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare getOfflinePendingTrxCount query !!!");
        return 0;
    }

    int count = 0;

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int64(statement, 0);
        break;
    }

    sqlite3_finalize(statement);
    logDataEx(currentTrxType, lastPanDigit, "Count received : %d", count);
    return count;
}

/**
 * Process all the pending transactions with host - offline sale
 **/
void processHostPendingTransactions()
{
    int trxDataCount = getOfflinePendingTrxCount();
    logDataEx(currentTrxType, lastPanDigit, "Total pending offline transaction count received : %d", trxDataCount);

    const char *query = "SELECT * FROM Transactions WHERE "
                        "HostStatus = 'Pending' "
                        "and ProcessingCode = '000000' "
                        "and TxnStatus = 'Success' order by RowId asc";
    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, 0) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare read query !!!");
        return;
    }

    TransactionTable trxDataList[trxDataCount + 100];
    int index = 0;
    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        if (index > trxDataCount)
        {
            logErrorEx("Offline", "processHostPendingTransactions",
                       "Extra data found on getting offline transactions, ignoring and picked up next.");
            break;
        }
        TransactionTable trxData = populateTableData(statement);
        trxDataList[index] = trxData;
        index++;
    }

    sqlite3_finalize(statement);

    logDataEx(currentTrxType, lastPanDigit, "Total transactions available : %d", trxDataCount);
    for (int i = 0; i < trxDataCount; i++)
    {
        logDataEx(currentTrxType, lastPanDigit, "======================================================");
        printTransactionRow(trxDataList[i]);
    }

    if (trxDataCount == 0)
    {
        logInfoEx(currentTrxType, lastPanDigit, "There are no offline transactions available to send to host");
    }
    else
    {
        if (appConfig.useISOHost)
        {
            logDataEx(currentTrxType, lastPanDigit, "Using ISO Host for sending offline transactions");
            for (int index = 0; index < trxDataCount; index++)
            {
                TransactionTable trxData = trxDataList[index];
                trxData = processHostOfflineTxn(trxData);
                updateHostResponse(trxData);
            }
        }
        else
        {
            logDataEx(currentTrxType, lastPanDigit, "Not supported");
        }
    }
}

/**
 * To save the offline response with the matching record in db
 */
void saveOfflineResponseToDb(OfflineSaleResponse offlineResponses[], int count)
{
    logDataEx(currentTrxType, lastPanDigit, "-------------------------------------------");
    for (int i = 0; i < count; i++)
    {
        logDataEx(currentTrxType, lastPanDigit, "Processing offline response : %d", i);
        printOfflineResponse(offlineResponses[i]);
        char transactionId[40];
        if (strlen(offlineResponses[i].orderId) == 0)
        {
            logDataEx(currentTrxType, lastPanDigit, "Order id not received, cannot update status. Stan might be 0");
        }
        else
        {
            int trxCount = getTrxTableDataCount(offlineResponses[i].orderId,
                                                offlineResponses[i].stan, "00", STATUS_SUCCESS, STATUS_PENDING, transactionId);
            if (trxCount == 1)
            {
                logDataEx(currentTrxType, lastPanDigit, "Found 1 item as expected. Going to update");
                logDataEx(currentTrxType, lastPanDigit, "Found transaction id : %s", transactionId);
                updateOfflineTransactionStatus(transactionId, offlineResponses[i]);
            }
        }
        logDataEx(currentTrxType, lastPanDigit, "-------------------------------------------");
    }
}

/**
 * To print the offline response information
 **/
void printOfflineResponse(OfflineSaleResponse offlineResponse)
{
    logDataEx(currentTrxType, lastPanDigit, "Order Id : %s", offlineResponse.orderId);
    logDataEx(currentTrxType, lastPanDigit, "Stan : %s", offlineResponse.stan);
    logDataEx(currentTrxType, lastPanDigit, "Response Time Stamp : %s", offlineResponse.responseTimeStamp);
    logDataEx(currentTrxType, lastPanDigit, "Result Code : %s", offlineResponse.resultCode);
    logDataEx(currentTrxType, lastPanDigit, "Result Code Id : %s", offlineResponse.resultCodeId);
    logDataEx(currentTrxType, lastPanDigit, "Result Message : %s", offlineResponse.resultMessage);
    logDataEx(currentTrxType, lastPanDigit, "Result Status : %s", offlineResponse.resultStatus);
    logDataEx(currentTrxType, lastPanDigit, "Airtel Txn Status : %d", offlineResponse.airtelTxnStatus);
    logDataEx(currentTrxType, lastPanDigit, "Airtel Txn Id : %s", offlineResponse.airtelTxnId);
}

/**
 * Update the offline transaction status
 **/
void updateOfflineTransactionStatus(const char *transactionId, OfflineSaleResponse offlineSaleResponse)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the transaction status for : %s", transactionId);

    const char *query = "UPDATE Transactions "
                        "SET TxnStatus = ? , "
                        "HostStatus = ? , "
                        "HostResponseTimeStamp = ? , "
                        "HostResultStatus = ? , "
                        "HostResultMessage = ? , "
                        "HostResultCode = ? , "
                        "HostResultCodeId = ?, "
                        "HostErrorCategory = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateOfflineTransactionStatus query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, STATUS_SUCCESS, -1, SQLITE_STATIC); // Txn Status
    if (strcmp(offlineSaleResponse.resultCode, "S") == 0)
    {
        doLock();
        activePendingTxnCount--;
        logDataEx(currentTrxType, lastPanDigit, "Offline trxn result is success");
        logDataEx(currentTrxType, lastPanDigit, "Active pending transaction count decreased and now is : %d", activePendingTxnCount);
        if (activePendingTxnCount < appConfig.minRequiredForOnline)
        {
            logWarnEx(currentTrxType, lastPanDigit, "Now the transaction is below minRequiredForOnline, making the device online");
            DEVICE_STATUS = STATUS_ONLINE;
        }
        printDeviceStatus();
        doUnLock();

        sqlite3_bind_text(statement, 2, STATUS_SUCCESS, -1, SQLITE_STATIC); // Host Status
        sqlite3_bind_text(statement, 8, "", -1, SQLITE_STATIC);             // Host Error Category
    }
    else // Host Status as pending if the result is fail
    {
        sqlite3_bind_text(statement, 2, STATUS_PENDING, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 8, HOST_ERROR_CATEGORY_FAILED, -1, SQLITE_STATIC); // Host Error Category
    }

    sqlite3_bind_text(statement, 3, offlineSaleResponse.responseTimeStamp, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, offlineSaleResponse.resultStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, offlineSaleResponse.resultMessage, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 6, offlineSaleResponse.resultCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 7, offlineSaleResponse.resultCodeId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 9, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "updateOfflineTransactionStatus success");
    sqlite3_finalize(statement);
}

/**
 * Get the transaction data
 **/
TransactionTable getTransactionTableData(const char *transactionId)
{
    TransactionTable trxData;
    trxData.id = -1;
    const char *query = "SELECT * FROM Transactions WHERE TransactionId = ?";
    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare read query !!!");
        return trxData;
    }

    sqlite3_bind_text(statement, 1, transactionId, -1, SQLITE_STATIC);

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        trxData = populateTableData(statement);
        printTransactionRow(trxData);
        logDataEx(currentTrxType, lastPanDigit, "Data received from db for : %s", trxData.transactionId);
        break;
    }

    sqlite3_finalize(statement);
    return trxData;
}

/**
 * Get the transaction data count
 **/
int getTrxTableDataCount(const char *orderId, const char *stan, const char *trxBin,
                         const char *txnStatus, const char *hostStatus, char *transactionId)
{
    const char *query = "SELECT Count(*), TransactionId FROM Transactions WHERE "
                        "TrxBin = ? and "
                        "OrderId = ? and "
                        "Stan = ? and "
                        "TxnStatus = ? and "
                        "HostStatus = ? ";
    sqlite3_stmt *statement;
    logDataEx(currentTrxType, lastPanDigit, "Going to get the count for %s", query);

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare getTrxTableDataCount query !!!");
        return 0;
    }

    sqlite3_bind_text(statement, 1, trxBin, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, orderId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, stan, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, txnStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, hostStatus, -1, SQLITE_STATIC);

    int count = 0;

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int64(statement, 0);
        sprintf(transactionId, "%s", sqlite3_column_text(statement, 1));
        break;
    }

    sqlite3_finalize(statement);
    logDataEx(currentTrxType, lastPanDigit, "Count received : %d", count);
    logDataEx(currentTrxType, lastPanDigit, "Transaction id : %s", transactionId);
    return count;
}

/**
 * to update the host error category
 */
void updateHostErrorCategory(const char *transactionId, const char *hostErrorCategory)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the host error category for : %s", transactionId);
    logDataEx(currentTrxType, lastPanDigit, "HostError Category : %s", hostErrorCategory);

    const char *query = "UPDATE Transactions "
                        "SET HostErrorCategory = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateHostErrorCategory query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, hostErrorCategory, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "updateHostErrorCategory success");
    sqlite3_finalize(statement);
}

/**
 * Update the transaction status
 **/
void updateTransactionStatus(const char *transactionId, const char *txnStatus, const char *updatedBalance)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the transaction status for : %s", transactionId);
    logDataEx(currentTrxType, lastPanDigit, "Status : %s", txnStatus);

    const char *query = "UPDATE Transactions "
                        "SET TxnStatus = ?, "
                        "UpdatedBalance = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateTransactionStatus query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, txnStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, updatedBalance, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "updateTransactionStatus success");
    sqlite3_finalize(statement);
}

/**
 * Update the reversal status
 **/
void updateReversalStatusOnly(const char *transactionId, const char *reversalStatus)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the reversal status for : %s", transactionId);
    logDataEx(currentTrxType, lastPanDigit, "Status : %s", reversalStatus);

    const char *query = "UPDATE Transactions "
                        "SET ReversalStatus = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateReversalStatus only status query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, reversalStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "updateReversalStatus only status is success");
    sqlite3_finalize(statement);
}

/**
 * Update the reversal data
 **/
void updateReversalStatus(const char *transactionId, const char *reversalStatus,
                          const char *txnStatus, const char *rrn, const char *authCode, const char *responseCode)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the reversal status for : %s", transactionId);
    logDataEx(currentTrxType, lastPanDigit, "Status : %s", reversalStatus);
    logDataEx(currentTrxType, lastPanDigit, "Txn Status : %s", txnStatus);

    const char *query = "UPDATE Transactions "
                        "SET ReversalStatus = ?, "
                        "TxnStatus = ?, "
                        "HostStatus = ?, "
                        "ReversalRRN = ?, "
                        "ReversalAuthCode = ?, "
                        "ReversalResponsecode = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateReversalStatus query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, reversalStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, txnStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, STATUS_SUCCESS, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, rrn, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, authCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 6, responseCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 7, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "updateReversalStatus success");
    sqlite3_finalize(statement);
}

/**
 * Update reversal response back to db
 **/
void updateReversalResponse(ReversalResponse reversalResponse, const char *transactionId)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the updateReversalResponse for : %s", transactionId);

    const char *query = "UPDATE Transactions "
                        "SET ReversalStatus = ?, "
                        "HostStatus = ?, "
                        "ReversalResponseCode = ?, "
                        "ReversalRRN = ?, "
                        "ReversalAuthCode = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateReversalResponse query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, reversalResponse.status == 0 ? STATUS_SUCCESS : STATUS_PENDING, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, STATUS_SUCCESS, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, reversalResponse.bankResultCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, reversalResponse.retrievalReferenceNumber, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, reversalResponse.authorizationCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 6, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "updateReversalResponse success");
    sqlite3_finalize(statement);
}

/**
 * Make the reversal status empty
 **/
void resetReversalStatus(const char *status, const char *transactionId)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the reverstal status to empty for : %s", transactionId);

    const char *query = "UPDATE Transactions "
                        "SET ReversalStatus = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare resetReversalStatus query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "resetReversalStatus success");
    sqlite3_finalize(statement);
}

/**
 * Update the reversal status
 **/
void updateReversalPreData(TransactionTable trxTable)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the updateReversalPreData for : %s", trxTable.transactionId);

    const char *query = "UPDATE Transactions "
                        "SET reversalStatus = ?, "
                        "ReversalMac = ?, "
                        "ReversalKsn = ?, "
                        "EchoMac = ?, "
                        "EchoKsn = ?, "
                        "ReversalInputResponseCode = ? "
                        "WHERE transactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateReversalPreData only status query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, trxTable.reversalStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, trxTable.reversalMac, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, trxTable.reversalKsn, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, trxTable.echoMac, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, trxTable.echoKsn, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 6, trxTable.reversalInputResponseCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 7, trxTable.transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "updateReversalPreData success");
    sqlite3_finalize(statement);
}

/**
 * Check is there any pending reversal
 **/
bool isTherePendingReversal(char *transactionId)
{
    bool result = false;
    logInfoEx(currentTrxType, lastPanDigit, "Going to check is there a pending reversal data");

    const char *query = "Select * from Transactions where ReversalStatus = 'Pending' ";
    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logDataEx(currentTrxType, lastPanDigit, "Failed to prepare isTherePendingReversal query !!!");
        return false;
    }

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        logInfoEx(currentTrxType, lastPanDigit, "Reversal pending transaction id found : %s", sqlite3_column_text(statement, 1));
        sprintf(transactionId, "%s", sqlite3_column_text(statement, 1));
        result = true;
        break;
    }

    sqlite3_finalize(statement);
    logInfoEx(currentTrxType, lastPanDigit, "Reversal pending result : %d", result);

    return result;
}

/**
 * Check is there any pending reversal and get or no reversal message
 **/
char *getReversal()
{
    char transactionId[38] = {0};
    if (isTherePendingReversal(transactionId) == true)
    {
        TransactionTable trxTable = getTransactionTableData(transactionId);

        // Generate Json Message
        json_object *jobj = json_object_new_object();
        json_object *jCommand = json_object_new_string(COMMAND_GET_REVERSAL);
        json_object_object_add(jobj, COMMAND, jCommand);
        json_object_object_add(jobj, COMMAND_DATA, getJsonTxnData(trxTable));

        const char *jsonData = json_object_to_json_string(jobj);

        int len = strlen(jsonData);
        char *data = malloc(len + 1);

        safe_strncpy(data, len + 1, jsonData, len);
        json_object_put(jobj); // Clear JSON memory
        return data;
    }
    else
    {
        return buildNoReversalMessage();
    }
}

/**
 * Clear the reversal manually
 */
void clearReversalManual()
{
    char transactionId[38];
    if (isTherePendingReversal(transactionId) == true)
    {
        logInfoEx(currentTrxType, lastPanDigit, "Transaction id for reversal to be cleared: %s", transactionId);
    }
    else
    {
        logWarnEx(currentTrxType, lastPanDigit, "No Reversal is there to be cleared");
        return;
    }

    logDataEx(currentTrxType, lastPanDigit, "Going to update the reversal status for : %s", transactionId);
    logDataEx(currentTrxType, lastPanDigit, "Status : Failure");

    const char *query = "UPDATE Transactions "
                        "SET ReversalStatus = ?, "
                        "ReversalManualCleared = 1 "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare clearReversalManual only status query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, STATUS_FAILURE, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "clearReversalManual status is success");
    sqlite3_finalize(statement);
}

/**
 * To update host response in database
 **/
void updateHostResponseInDb(HostResponse hostResponse, char transactionId[40])
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the recevied host response for transaction : %s", transactionId);
    const char *query = "UPDATE Transactions "
                        "SET rrn = ?, "
                        "hostRetry = ?, "
                        "authCode = ?, "
                        "hostResponseTimeStamp = ?, "
                        "hostResultStatus = ?, "
                        "hostResultMessage = ?, "
                        "hostResultCode = ?, "
                        "hostResultCodeId = ?, "
                        "hostIccData = ?, "
                        "hostError = ?, "
                        "hostStatus = ?, "
                        "updateAmount = ?, "
                        "AcquirementId = ?, "
                        "ReversalStatus = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateHostResponseInDb query !!!");
        return;
    }

    // Get the update amount from icc data
    char updateAmount[13] = {0};
    int j = 0;
    int iccLen = strlen(hostResponse.iccData);
    for (int i = iccLen - 12; i < iccLen; i++)
    {
        updateAmount[j++] = hostResponse.iccData[i];
    }
    updateAmount[j] = '\0';
    logDataEx(currentTrxType, lastPanDigit, "Updated amount received : %s", updateAmount);

    sqlite3_bind_text(statement, 1, hostResponse.retrievalReferenceNumber, -1, SQLITE_STATIC);
    sqlite3_bind_int(statement, 2, 0);
    sqlite3_bind_text(statement, 3, hostResponse.authorizationCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, hostResponse.hostResponseTimeStamp, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, hostResponse.resultStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 6, hostResponse.resultMsg, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 7, hostResponse.bankResultCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 8, hostResponse.resultCodeId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 9, hostResponse.iccData, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 10, "", -1, SQLITE_STATIC);             // Host Error
    sqlite3_bind_text(statement, 11, STATUS_SUCCESS, -1, SQLITE_STATIC); // Host Status
    sqlite3_bind_text(statement, 12, updateAmount, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 13, hostResponse.acquirementId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 14, "", -1, SQLITE_STATIC); // Reversal status is empty on host success
    sqlite3_bind_text(statement, 15, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "Update success");
    sqlite3_finalize(statement);
}

/**
 * Update the host response received in db
 **/
void updateHostResponse(TransactionTable trxData)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the transaction : %s", trxData.transactionId);
    logDataEx(currentTrxType, lastPanDigit, "Host Status : %s", trxData.hostStatus);
    logDataEx(currentTrxType, lastPanDigit, "Transaction Id : %s", trxData.transactionId);

    const char *query = "UPDATE Transactions "
                        "SET RRN = ?, "
                        "AuthCode = ?, "
                        "HostResultCode = ?, "
                        "HostStatus = ?, "
                        "HostError = ?, "
                        "HostRetry = ?, "
                        "UpdateAmount = ?, "
                        "ReversalStatus = ?, "
                        "HostErrorCategory = ? "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare update query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, trxData.rrn, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, trxData.authCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, trxData.hostResultCode, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, trxData.hostStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, trxData.hostError, -1, SQLITE_STATIC);
    sqlite3_bind_int(statement, 6, trxData.hostRetry);
    sqlite3_bind_text(statement, 7, trxData.updateAmount, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 8, trxData.reversalStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 9, trxData.hostErrorCategory, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 10, trxData.transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to update record : %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "Update success");
    sqlite3_finalize(statement);
}

/**
 * Clear the approved fetched data by the client
 **/
char *clearFetchedData()
{
    logInfoEx(currentTrxType, lastPanDigit, "Going to clear all the records for the Fetch Id %d", activeFetchId);
    logDataEx(currentTrxType, lastPanDigit, "Total records to be cleared :  %d", fetchedCount);

    for (int i = 0; i < fetchedCount; i++)
    {
        const char *query = "DELETE FROM Transactions WHERE TransactionId = ?";
        sqlite3_stmt *statement;

        if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
        {
            logErrorEx("FetchData", "Clear", "Failed to prepare delete query !!!");
            return buildResponseMessage(STATUS_ERROR, ERR_SQL_STATEMENT_PREPARE_FAILED);
        }

        logDataEx(currentTrxType, lastPanDigit, "Going to delete record with Id : %s", fetchTransactionIdList[i].trxId);
        sqlite3_bind_text(statement, 1, fetchTransactionIdList[i].trxId, -1, SQLITE_STATIC);

        int result = sqlite3_step(statement);
        if (result != SQLITE_DONE)
        {
            logErrorEx("FetchData", "Clear", "Failed to delete record : %d", result);
            return buildResponseMessage(STATUS_ERROR, ERR_SQL_DELETE_FAILED);
        }
        logInfoEx(currentTrxType, lastPanDigit, "Delete success");
        sqlite3_finalize(statement);
    }

    activeFetchId = 0;
    logInfoEx(currentTrxType, lastPanDigit, "Fetch id reseted");
    for (int i = 0; i < MAX_FETCH_COUNT; i++)
    {
        memset(fetchTransactionIdList[i].trxId, 0, sizeof(fetchTransactionIdList[i].trxId));
    }

    doVaccumTrxDb();

    return buildResponseMessage(STATUS_SUCCESS, 0);
}

/**
 * Get the fetch query
 **/
// const char *getFetchquery(enum fetch_mode fetchMode)
// {
//     if (fetchMode == FETCH_MODE_FAILURE)
//     {
//         return "SELECT * FROM Transactions WHERE (HostStatus = ? or TxnStatus = ?) "
//                " and TrxBin = '00' "
//                " order by RowId asc LIMIT ? ";
//     }

//     if (fetchMode == FETCH_MODE_PENDING)
//     {
//         return "SELECT * FROM Transactions WHERE HostStatus = 'Pending' and TrxBin = '00' "
//                " order by RowId asc LIMIT ? ";
//     }

//     return "SELECT * FROM Transactions WHERE HostStatus = ? and TrxBin = '00' order by RowId asc LIMIT ? ";
// }

void getFetchquery(enum fetch_mode fetchMode,
                   bool isOnline,
                   char *buffer,
                   size_t size)
{
    const char *trxCondition = isOnline ? "TrxBin != '00'" : "TrxBin = '00'";

    if (fetchMode == FETCH_MODE_FAILURE)
    {
        snprintf(buffer, size,
                 "SELECT * FROM Transactions "
                 "WHERE (HostStatus = ? or TxnStatus = ?) "
                 "and %s "
                 "order by RowId asc LIMIT ? ",
                 trxCondition);
        return;
    }

    if (fetchMode == FETCH_MODE_PENDING)
    {
        snprintf(buffer, size,
                 "SELECT * FROM Transactions "
                 "WHERE HostStatus = 'Pending' "
                 "and %s "
                 "order by RowId asc LIMIT ? ",
                 trxCondition);
        return;
    }

    snprintf(buffer, size,
             "SELECT * FROM Transactions "
             "WHERE HostStatus = ? "
             "and %s "
             "order by RowId asc LIMIT ? ",
             trxCondition);
}

/**
 * Fetch the data to be sent to client
 **/
char *fetchHostData(int maxRecordsRequested, enum fetch_mode fetchMode, bool isOnline)
{
    json_object *jobj = json_object_new_object();
    json_object *jDataArrayObject = json_object_new_array();

    logDataEx(currentTrxType, lastPanDigit, "Active Fetch ID : %d", activeFetchId);
    logDataEx(currentTrxType, lastPanDigit, "Number of data requested : %d", maxRecordsRequested);

    int maxRecords = maxRecordsRequested;
    if (maxRecords > MAX_FETCH_COUNT)
    {
        logInfoEx(currentTrxType, lastPanDigit, "Maximum allowed records is : %d", MAX_FETCH_COUNT);
        logInfoEx(currentTrxType, lastPanDigit, "Resetting to the above value");
        maxRecords = MAX_FETCH_COUNT;
    }
    fetchedCount = 0;

    if (fetchMode == FETCH_MODE_FAILURE)
        logDataEx(currentTrxType, lastPanDigit, "Failure transaction data requested");
    else if (fetchMode == FETCH_MODE_PENDING)
        logDataEx(currentTrxType, lastPanDigit, "Pending transactions data requested");
    else
        logDataEx(currentTrxType, lastPanDigit, "Success transaction data requested");

    char query[512];
    getFetchquery(fetchMode, isOnline, query, sizeof(query));
    logDataEx(currentTrxType, lastPanDigit, "Query prepared : %s", query);
    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logDataEx(currentTrxType, lastPanDigit, "Failed to prepare read query !!!");
        return buildResponseMessage(STATUS_ERROR, ERR_SQL_STATEMENT_PREPARE_FAILED);
    }

    if (fetchMode == FETCH_MODE_FAILURE)
    {
        sqlite3_bind_text(statement, 1, STATUS_FAILURE, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, STATUS_FAILURE, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 3, maxRecords);
    }
    else if (fetchMode == FETCH_MODE_PENDING)
    {
        sqlite3_bind_int(statement, 1, maxRecords);
    }
    else
    {
        sqlite3_bind_text(statement, 1, STATUS_SUCCESS, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 2, maxRecords);
    }

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        TransactionTable trxData = populateTableData(statement);
        safe_strcpy(fetchTransactionIdList[fetchedCount].trxId, sizeof(fetchTransactionIdList[fetchedCount].trxId),
                    trxData.transactionId);
        json_object_array_add(jDataArrayObject, getJsonTxnData(trxData));
        printTransactionRow(trxData);
        fetchedCount++;
    }

    sqlite3_finalize(statement);

    logDataEx(currentTrxType, lastPanDigit, "Max Records Requested : %d, and total fetched : %d", maxRecords, fetchedCount);

    for (int i = 0; i < fetchedCount; i++)
    {
        logDataEx(currentTrxType, lastPanDigit, "FETCHED ID : %s", fetchTransactionIdList[i].trxId);
    }

    // Generate Json
    json_object *jCommand = json_object_new_string(COMMAND_FETCH_AUTH);
    json_object *jFetchId = json_object_new_int(activeFetchId);

    json_object_object_add(jobj, COMMAND, jCommand);

    if (fetchMode == FETCH_MODE_FAILURE)
        json_object_object_add(jobj, FETCH_MODE, json_object_new_string(STATUS_FAILURE));
    else if (fetchMode == FETCH_MODE_PENDING)
        json_object_object_add(jobj, FETCH_MODE, json_object_new_string(STATUS_PENDING));
    else
        json_object_object_add(jobj, FETCH_MODE, json_object_new_string(STATUS_SUCCESS));

    json_object_object_add(jobj, FETCH_AUTH_ID, jFetchId);
    json_object_object_add(jobj, COMMAND_DATA, jDataArrayObject);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    safe_strncpy(data, len + 1, jsonData, len);
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * To do the recalculation of mac
 */
void performMacRecalculation()
{
    changeAppState(APP_STATUS_RECALC_MAC);
    // Get all the transactions that are to be processed
    // TxnStatus = Success, HostStatus = Pending

    int trxDataCount = getOfflinePendingTrxCount();
    logDataEx(currentTrxType, lastPanDigit, "Total pending offline transactions for mac recalc are : %d", trxDataCount);

    const char *query = "SELECT * FROM Transactions WHERE "
                        "HostStatus = 'Pending' "
                        "and ProcessingCode = '000000' "
                        "and TxnStatus = 'Success' order by RowId asc";
    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, 0) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare read query !!!");
        return;
    }

    TransactionTable trxDataList[trxDataCount + 100];
    int index = 0;
    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        if (index > trxDataCount)
        {
            logErrorEx("L3-App", "MacRecalc", "Extra data found on getting offline transactions, ignoring and picked up next.");
            break;
        }
        TransactionTable trxData = populateTableData(statement);
        trxDataList[index] = trxData;
        index++;
    }

    sqlite3_finalize(statement);

    logDataEx(currentTrxType, lastPanDigit, "Total transactions loaded for mac recalc : %d", trxDataCount);
    for (int i = 0; i < trxDataCount; i++)
    {
        logDataEx(currentTrxType, lastPanDigit, "======================================================");
        printTransactionRow(trxDataList[i]);
    }

    if (trxDataCount == 0)
    {
        logInfoEx(currentTrxType, lastPanDigit, "There are no pending transactions available for mac recalc");
    }
    else
    {
        for (int index = 0; index < trxDataCount; index++)
        {
            logDataEx(currentTrxType, lastPanDigit, "Doing recalculation of transaction : %d of %d", (index + 1), trxDataCount);
            TransactionTable trxTable = trxDataList[index];
            logDataEx(currentTrxType, lastPanDigit, "Processing transaction id : %s", trxTable.transactionId);
            resetTransactionData();

            // Set the values for recalc
            safe_strcpy(currentTxnData.transactionId, sizeof(currentTxnData.transactionId), trxTable.transactionId);
            safe_strcpy(currentTxnData.processingCode, sizeof(currentTxnData.processingCode), trxTable.processingCode);
            logDataEx(currentTrxType, lastPanDigit, "String amount : %s", trxTable.amount);
            currentTxnData.amount = strtoull(trxTable.amount, NULL, 10);
            logDataEx(currentTrxType, lastPanDigit, "Converted amount : %" PRIu64, currentTxnData.amount);
            safe_strcpy(currentTxnData.sAmount, sizeof(currentTxnData.sAmount), trxTable.amount);
            safe_strcpy(currentTxnData.stan, sizeof(currentTxnData.stan), trxTable.stan);
            safe_strcpy(currentTxnData.expDateEnc, sizeof(currentTxnData.expDateEnc), trxTable.expDateEnc);

            safe_strcpy(currentTxnData.time, sizeof(currentTxnData.time), trxTable.time);
            safe_strcpy(currentTxnData.date, sizeof(currentTxnData.date), trxTable.date);
            safe_strcpy(currentTxnData.year, sizeof(currentTxnData.year), trxTable.year);

            safe_strcpy(currentTxnData.panEncrypted, sizeof(currentTxnData.panEncrypted), trxTable.panEncrypted);
            safe_strcpy(currentTxnData.iccData, sizeof(currentTxnData.iccData), trxTable.iccData);
            safe_strcpy(currentTxnData.orderId, sizeof(currentTxnData.orderId), trxTable.orderId);

            logDataEx(currentTrxType, lastPanDigit, "Generating mac data now");
            logDataEx(currentTrxType, lastPanDigit, "Mac data is generated, now updating the db");

            // Update the db back
            updateReCalcMacData(currentTxnData);
        }
    }

    logDataEx(currentTrxType, lastPanDigit, "Recalculation completed and changing status back to ready");
    changeAppState(APP_STATUS_READY);
}

/**
 * To update the mac recalculation to db
 */
void updateReCalcMacData(struct transactionData trxData)
{
    logDataEx(currentTrxType, lastPanDigit, "Going to update the updateReCalcMacData in db for : %s", trxData.transactionId);

    const char *query = "UPDATE Transactions "
                        "SET mac = ?, "
                        "macKsn = ? "
                        "WHERE transactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3Db, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarnEx(currentTrxType, lastPanDigit, "Failed to prepare updateReversalPreData only status query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, trxData.mac, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, trxData.macKsn, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, trxData.transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logErrorEx("L3-App", "MacRecalc", "Failed to update record for mac recalc: %d", result);
        return;
    }
    logDataEx(currentTrxType, lastPanDigit, "updateReCalcMacData success");
    sqlite3_finalize(statement);
}

/**
 * populate the table from the db statement
 **/
TransactionTable populateTableData(sqlite3_stmt *statement)
{
    TransactionTable trxData;

    trxData.id = sqlite3_column_int64(statement, 0);
    sprintf(trxData.transactionId, "%s", sqlite3_column_text(statement, 1));
    sprintf(trxData.trxType, "%s", sqlite3_column_text(statement, 2));
    sprintf(trxData.trxBin, "%s", sqlite3_column_text(statement, 3));
    sprintf(trxData.processingCode, "%s", sqlite3_column_text(statement, 4));
    trxData.trxCounter = sqlite3_column_int64(statement, 5);
    sprintf(trxData.stan, "%s", sqlite3_column_text(statement, 6));
    trxData.batch = sqlite3_column_int(statement, 7);
    sprintf(trxData.amount, "%s", sqlite3_column_text(statement, 8));
    sprintf(trxData.time, "%s", sqlite3_column_text(statement, 9));
    sprintf(trxData.date, "%s", sqlite3_column_text(statement, 10));
    sprintf(trxData.year, "%s", sqlite3_column_text(statement, 11));
    sprintf(trxData.aid, "%s", sqlite3_column_text(statement, 12));
    sprintf(trxData.maskPAN, "%s", sqlite3_column_text(statement, 13));

    if (sqlite3_column_text(statement, 14) != NULL)
        sprintf(trxData.token, "%s", sqlite3_column_text(statement, 14));
    else
        memset(trxData.token, 0, sizeof(trxData.token));

    if (sqlite3_column_text(statement, 15) != NULL)
        sprintf(trxData.effectiveDate, "%s", sqlite3_column_text(statement, 15));
    else
        memset(trxData.effectiveDate, 0, sizeof(trxData.effectiveDate));

    if (sqlite3_column_text(statement, 16) != NULL)
        sprintf(trxData.serviceId, "%s", sqlite3_column_text(statement, 16));
    else
        memset(trxData.serviceId, 0, sizeof(trxData.serviceId));

    if (sqlite3_column_text(statement, 17) != NULL)
        sprintf(trxData.serviceIndex, "%s", sqlite3_column_text(statement, 17));
    else
        memset(trxData.serviceIndex, 0, sizeof(trxData.serviceIndex));

    if (sqlite3_column_text(statement, 18) != NULL)
        sprintf(trxData.serviceData, "%s", sqlite3_column_text(statement, 18));
    else
        memset(trxData.serviceData, 0, sizeof(trxData.serviceData));

    if (sqlite3_column_text(statement, 19) != NULL)
        sprintf(trxData.serviceControl, "%s", sqlite3_column_text(statement, 19));
    else
        memset(trxData.serviceControl, 0, sizeof(trxData.serviceControl));

    if (sqlite3_column_text(statement, 20) != NULL)
        sprintf(trxData.serviceBalance, "%s", sqlite3_column_text(statement, 20));
    else
        memset(trxData.serviceBalance, 0, sizeof(trxData.serviceBalance));

    sprintf(trxData.iccData, "%s", sqlite3_column_text(statement, 21));
    trxData.iccDataLen = sqlite3_column_int(statement, 22);

    if (sqlite3_column_text(statement, 23) != NULL)
        sprintf(trxData.ksn, "%s", sqlite3_column_text(statement, 23));
    else
        memset(trxData.ksn, 0, sizeof(trxData.ksn));

    if (sqlite3_column_text(statement, 24) != NULL)
        sprintf(trxData.panEncrypted, "%s", sqlite3_column_text(statement, 24));
    else
        memset(trxData.panEncrypted, 0, sizeof(trxData.panEncrypted));

    if (sqlite3_column_text(statement, 25) != NULL)
        sprintf(trxData.track2Enc, "%s", sqlite3_column_text(statement, 25));
    else
        memset(trxData.track2Enc, 0, sizeof(trxData.track2Enc));

    if (sqlite3_column_text(statement, 26) != NULL)
        sprintf(trxData.expDateEnc, "%s", sqlite3_column_text(statement, 26));
    else
        memset(trxData.expDateEnc, 0, sizeof(trxData.expDateEnc));

    if (sqlite3_column_text(statement, 27) != NULL)
        sprintf(trxData.macKsn, "%s", sqlite3_column_text(statement, 27));
    else
        memset(trxData.macKsn, 0, sizeof(trxData.macKsn));

    if (sqlite3_column_text(statement, 28) != NULL)
        sprintf(trxData.mac, "%s", sqlite3_column_text(statement, 28));
    else
        memset(trxData.mac, 0, sizeof(trxData.mac));

    if (sqlite3_column_text(statement, 29) != NULL)
        sprintf(trxData.orderId, "%s", sqlite3_column_text(statement, 29));
    else
        memset(trxData.orderId, 0, sizeof(trxData.orderId));

    sprintf(trxData.txnStatus, "%s", sqlite3_column_text(statement, 30));
    sprintf(trxData.hostStatus, "%s", sqlite3_column_text(statement, 31));

    if (sqlite3_column_text(statement, 32) != NULL)
        sprintf(trxData.updateAmount, "%s", sqlite3_column_text(statement, 32));
    else
        memset(trxData.updateAmount, 0, sizeof(trxData.updateAmount));

    if (sqlite3_column_text(statement, 33) != NULL)
        sprintf(trxData.updatedBalance, "%s", sqlite3_column_text(statement, 33));
    else
        memset(trxData.updatedBalance, 0, sizeof(trxData.updatedBalance));

    sprintf(trxData.terminalId, "%s", sqlite3_column_text(statement, 34));
    sprintf(trxData.merchantId, "%s", sqlite3_column_text(statement, 35));
    trxData.hostRetry = sqlite3_column_int(statement, 36);

    if (sqlite3_column_text(statement, 37) != NULL)
        sprintf(trxData.rrn, "%s", sqlite3_column_text(statement, 37));
    else
        memset(trxData.rrn, 0, sizeof(trxData.rrn));

    if (sqlite3_column_text(statement, 38) != NULL)
        sprintf(trxData.authCode, "%s", sqlite3_column_text(statement, 38));
    else
        memset(trxData.authCode, 0, sizeof(trxData.authCode));

    if (sqlite3_column_text(statement, 39) != NULL)
        sprintf(trxData.hostResponseTimeStamp, "%s", sqlite3_column_text(statement, 39));
    else
        memset(trxData.hostResponseTimeStamp, 0, sizeof(trxData.hostResponseTimeStamp));

    if (sqlite3_column_text(statement, 40) != NULL)
        sprintf(trxData.hostResultStatus, "%s", sqlite3_column_text(statement, 40));
    else
        memset(trxData.hostResultStatus, 0, sizeof(trxData.hostResultStatus));

    if (sqlite3_column_text(statement, 41) != NULL)
        sprintf(trxData.hostResultMessage, "%s", sqlite3_column_text(statement, 41));
    else
        memset(trxData.hostResultMessage, 0, sizeof(trxData.hostResultMessage));

    if (sqlite3_column_text(statement, 42) != NULL)
        sprintf(trxData.hostResultCode, "%s", sqlite3_column_text(statement, 42));
    else
        memset(trxData.hostResultCode, 0, sizeof(trxData.hostResultCode));

    if (sqlite3_column_text(statement, 43) != NULL)
        sprintf(trxData.hostResultCodeId, "%s", sqlite3_column_text(statement, 43));
    else
        memset(trxData.hostResultCodeId, 0, sizeof(trxData.hostResultCodeId));

    if (sqlite3_column_text(statement, 44) != NULL)
        sprintf(trxData.hostIccData, "%s", sqlite3_column_text(statement, 44));
    else
        memset(trxData.hostIccData, 0, sizeof(trxData.hostIccData));

    if (sqlite3_column_text(statement, 45) != NULL)
        sprintf(trxData.hostError, "%s", sqlite3_column_text(statement, 45));
    else
        memset(trxData.hostError, 0, sizeof(trxData.hostError));

    if (sqlite3_column_text(statement, 46) != NULL)
        sprintf(trxData.reversalStatus, "%s", sqlite3_column_text(statement, 46));
    else
        memset(trxData.reversalStatus, 0, sizeof(trxData.reversalStatus));

    if (sqlite3_column_text(statement, 47) != NULL)
        sprintf(trxData.moneyAddTrxType, "%s", sqlite3_column_text(statement, 47));
    else
        memset(trxData.moneyAddTrxType, 0, sizeof(trxData.moneyAddTrxType));

    if (sqlite3_column_text(statement, 48) != NULL)
        sprintf(trxData.acquirementId, "%s", sqlite3_column_text(statement, 48));
    else
        memset(trxData.acquirementId, 0, sizeof(trxData.acquirementId));

    if (sqlite3_column_text(statement, 49) != NULL)
        sprintf(trxData.reversalResponsecode, "%s", sqlite3_column_text(statement, 49));
    else
        memset(trxData.reversalResponsecode, 0, sizeof(trxData.reversalResponsecode));

    if (sqlite3_column_text(statement, 50) != NULL)
        sprintf(trxData.reversalRRN, "%s", sqlite3_column_text(statement, 50));
    else
        memset(trxData.reversalRRN, 0, sizeof(trxData.reversalRRN));

    if (sqlite3_column_text(statement, 51) != NULL)
        sprintf(trxData.reversalAuthCode, "%s", sqlite3_column_text(statement, 51));
    else
        memset(trxData.reversalAuthCode, 0, sizeof(trxData.reversalAuthCode));

    trxData.reversalManualCleared = sqlite3_column_int(statement, 52);

    if (sqlite3_column_text(statement, 53) != NULL)
        sprintf(trxData.reversalMac, "%s", sqlite3_column_text(statement, 53));
    else
        memset(trxData.reversalMac, 0, sizeof(trxData.reversalMac));

    if (sqlite3_column_text(statement, 54) != NULL)
        sprintf(trxData.reversalKsn, "%s", sqlite3_column_text(statement, 54));
    else
        memset(trxData.reversalKsn, 0, sizeof(trxData.reversalKsn));

    if (sqlite3_column_text(statement, 55) != NULL)
        sprintf(trxData.echoMac, "%s", sqlite3_column_text(statement, 55));
    else
        memset(trxData.echoMac, 0, sizeof(trxData.echoMac));

    if (sqlite3_column_text(statement, 56) != NULL)
        sprintf(trxData.echoKsn, "%s", sqlite3_column_text(statement, 56));
    else
        memset(trxData.echoKsn, 0, sizeof(trxData.echoKsn));

    if (sqlite3_column_text(statement, 57) != NULL)
        sprintf(trxData.reversalInputResponseCode, "%s", sqlite3_column_text(statement, 57));
    else
        memset(trxData.reversalInputResponseCode, 0, sizeof(trxData.reversalInputResponseCode));

    if (sqlite3_column_text(statement, 58) != NULL)
        sprintf(trxData.hostErrorCategory, "%s", sqlite3_column_text(statement, 58));
    else
        memset(trxData.hostErrorCategory, 0, sizeof(trxData.hostErrorCategory));

    trxData.airtelTxnStatus = sqlite3_column_int(statement, 59);

    if (sqlite3_column_text(statement, 60) != NULL)
        sprintf(trxData.airtelTxnId, "%s", sqlite3_column_text(statement, 60));
    else
        memset(trxData.airtelTxnId, 0, sizeof(trxData.airtelTxnId));

    if (sqlite3_column_text(statement, 61) != NULL)
        sprintf(trxData.airtelRequestData, "%s", sqlite3_column_text(statement, 61));
    else
        memset(trxData.airtelRequestData, 0, sizeof(trxData.airtelRequestData));

    if (sqlite3_column_text(statement, 62) != NULL)
        sprintf(trxData.airtelResponseData, "%s", sqlite3_column_text(statement, 62));
    else
        memset(trxData.airtelResponseData, 0, sizeof(trxData.airtelResponseData));

    if (sqlite3_column_text(statement, 63) != NULL)
        sprintf(trxData.airtelResponseCode, "%s", sqlite3_column_text(statement, 63));
    else
        memset(trxData.airtelResponseCode, 0, sizeof(trxData.airtelResponseCode));

    if (sqlite3_column_text(statement, 64) != NULL)
        sprintf(trxData.airtelResponseDesc, "%s", sqlite3_column_text(statement, 64));
    else
        memset(trxData.airtelResponseDesc, 0, sizeof(trxData.airtelResponseDesc));

    if (sqlite3_column_text(statement, 65) != NULL)
        sprintf(trxData.airtelSwitchResponseCode, "%s", sqlite3_column_text(statement, 65));
    else
        memset(trxData.airtelSwitchResponseCode, 0, sizeof(trxData.airtelSwitchResponseCode));

    if (sqlite3_column_text(statement, 66) != NULL)
        sprintf(trxData.airtelSwitchTerminalId, "%s", sqlite3_column_text(statement, 66));
    else
        memset(trxData.airtelSwitchTerminalId, 0, sizeof(trxData.airtelSwitchTerminalId));

    if (sqlite3_column_text(statement, 67) != NULL)
        sprintf(trxData.airtelSwichMerchantId, "%s", sqlite3_column_text(statement, 67));
    else
        memset(trxData.airtelSwichMerchantId, 0, sizeof(trxData.airtelSwichMerchantId));

    if (sqlite3_column_text(statement, 68) != NULL)
        sprintf(trxData.airtelAckTxnType, "%s", sqlite3_column_text(statement, 68));
    else
        memset(trxData.airtelAckTxnType, 0, sizeof(trxData.airtelAckTxnType));

    if (sqlite3_column_text(statement, 69) != NULL)
        sprintf(trxData.airtelAckPaymentMode, "%s", sqlite3_column_text(statement, 69));
    else
        memset(trxData.airtelAckPaymentMode, 0, sizeof(trxData.airtelAckPaymentMode));

    if (sqlite3_column_text(statement, 70) != NULL)
        sprintf(trxData.airtelAckRefundId, "%s", sqlite3_column_text(statement, 70));
    else
        memset(trxData.airtelAckRefundId, 0, sizeof(trxData.airtelAckRefundId));

    if (sqlite3_column_text(statement, 71) != NULL)
        sprintf(trxData.acqTransactionId, "%s", sqlite3_column_text(statement, 71));
    else
        memset(trxData.acqTransactionId, 0, sizeof(trxData.acqTransactionId));

    if (sqlite3_column_text(statement, 72) != NULL)
        sprintf(trxData.acqUniqueTransactionId, "%s", sqlite3_column_text(statement, 72));
    else
        memset(trxData.acqUniqueTransactionId, 0, sizeof(trxData.acqUniqueTransactionId));

    if (sqlite3_column_text(statement, 73) != NULL)
        sprintf(trxData.moneyAddRRN, "%s", sqlite3_column_text(statement, 73));
    else
        memset(trxData.moneyAddRRN, 0, sizeof(trxData.moneyAddRRN));

    if (sqlite3_column_text(statement, 74) != NULL)
        sprintf(trxData.moneyAddTid, "%s", sqlite3_column_text(statement, 74));
    else
        memset(trxData.moneyAddTid, 0, sizeof(trxData.moneyAddTid));

    return trxData;
}

/**
 * Get the json data of a transaction table
 **/
json_object *getJsonTxnData(TransactionTable trxData)
{
    char dateYear[10];
    sprintf(dateYear, "%s%s", trxData.date, trxData.year);
    logDataEx(currentTrxType, lastPanDigit, "Transaction date to be returned in json : %s", dateYear);

    json_object *jTuid = json_object_new_string(trxData.transactionId);
    json_object *jAid = json_object_new_string(trxData.aid);
    json_object *jToken = json_object_new_string(trxData.token);
    json_object *jPAN = json_object_new_string(trxData.maskPAN);
    json_object *jValidFrom = json_object_new_string(trxData.effectiveDate);
    json_object *jMessageType = json_object_new_string(trxData.trxType);
    json_object *jProcessingCode = json_object_new_string(trxData.processingCode);
    json_object *jAmount = json_object_new_string(trxData.amount);
    json_object *jTime = json_object_new_string(trxData.time);
    json_object *jDate = json_object_new_string(dateYear);
    json_object *jStan = json_object_new_string(trxData.stan);
    json_object *jAuthCode = json_object_new_string(trxData.authCode);
    json_object *jResponseCode = json_object_new_string(trxData.hostResultCode);
    json_object *jRRN = json_object_new_string(trxData.rrn);
    json_object *jTerminalId = json_object_new_string(trxData.terminalId);
    json_object *jMerchantId = json_object_new_string(trxData.merchantId);
    json_object *jBatchNo = json_object_new_int(trxData.batch);
    json_object *jInvoiceNo = json_object_new_string(trxData.stan);
    json_object *jUpdateAmount = json_object_new_string(trxData.updateAmount);
    json_object *jReversalStatus = json_object_new_string(trxData.reversalStatus);
    json_object *jOrderId = json_object_new_string(trxData.orderId);

    json_object *jDataObject = json_object_new_object();
    json_object_object_add(jDataObject, "tuid", jTuid);
    json_object_object_add(jDataObject, "aid", jAid);
    json_object_object_add(jDataObject, "orderId", jOrderId);
    json_object_object_add(jDataObject, "token", jToken);
    json_object_object_add(jDataObject, "pan", jPAN);
    json_object_object_add(jDataObject, "valid_from", jValidFrom);
    json_object_object_add(jDataObject, "message_type", jMessageType);
    json_object_object_add(jDataObject, "processing_code", jProcessingCode);
    json_object_object_add(jDataObject, "amount", jAmount);
    json_object_object_add(jDataObject, "txnTime", jTime);
    json_object_object_add(jDataObject, "txnDate", jDate);
    json_object_object_add(jDataObject, "stan", jStan);
    json_object_object_add(jDataObject, "auth_code", jAuthCode);
    json_object_object_add(jDataObject, "resp_code", jResponseCode);
    json_object_object_add(jDataObject, "rrn", jRRN);
    json_object_object_add(jDataObject, "terminalId", jTerminalId);
    json_object_object_add(jDataObject, "merchantId", jMerchantId);
    json_object_object_add(jDataObject, "batch_no", jBatchNo);
    json_object_object_add(jDataObject, "invoice_no", jInvoiceNo);
    json_object_object_add(jDataObject, "update_amount", jUpdateAmount);
    json_object_object_add(jDataObject, "reversal_status", jReversalStatus);

    json_object_object_add(jDataObject, "txnStatus", json_object_new_string(trxData.txnStatus));
    json_object_object_add(jDataObject, "hostStatus", json_object_new_string(trxData.hostStatus));
    json_object_object_add(jDataObject, "updatedAmount", json_object_new_string(trxData.updateAmount));
    json_object_object_add(jDataObject, "updatedBalance", json_object_new_string(trxData.updatedBalance));
    json_object_object_add(jDataObject, "iccData", json_object_new_string(trxData.iccData));
    json_object_object_add(jDataObject, "iccDataLen", json_object_new_int64(trxData.iccDataLen));
    json_object_object_add(jDataObject, "hostErrorCategory", json_object_new_string(trxData.hostErrorCategory));
    json_object_object_add(jDataObject, "resversalInputResponseCode", json_object_new_string(trxData.reversalInputResponseCode));
    json_object_object_add(jDataObject, "reversalAuthCode", json_object_new_string(trxData.reversalAuthCode));
    json_object_object_add(jDataObject, "reversalRRN", json_object_new_string(trxData.reversalRRN));
    json_object_object_add(jDataObject, "reversalManualCleared", json_object_new_int(trxData.reversalManualCleared));
    json_object_object_add(jDataObject, "reversalResponsecode", json_object_new_string(trxData.reversalResponsecode));
    json_object_object_add(jDataObject, "acquirementId", json_object_new_string(trxData.acquirementId));
    json_object_object_add(jDataObject, "hostError", json_object_new_string(trxData.hostError));
    json_object_object_add(jDataObject, "hostResultCodeId", json_object_new_string(trxData.hostResultCodeId));
    json_object_object_add(jDataObject, "hostResultCode", json_object_new_string(trxData.hostResultCode));
    json_object_object_add(jDataObject, "hostResultMessage", json_object_new_string(trxData.hostResultMessage));
    json_object_object_add(jDataObject, "hostResultStatus", json_object_new_string(trxData.hostResultStatus));
    json_object_object_add(jDataObject, "hostResponseTimeStamp", json_object_new_string(trxData.hostResponseTimeStamp));
    json_object_object_add(jDataObject, "hostIccData", json_object_new_string(trxData.hostIccData));
    json_object_object_add(jDataObject, "hostRetry", json_object_new_int(trxData.hostRetry));
    json_object_object_add(jDataObject, "hostStatus", json_object_new_string(trxData.hostStatus));
    json_object_object_add(jDataObject, "txnStatus", json_object_new_string(trxData.txnStatus));
    json_object_object_add(jDataObject, "expDateEnc", json_object_new_string(trxData.expDateEnc));
    json_object_object_add(jDataObject, "track2Enc", json_object_new_string(trxData.track2Enc));
    json_object_object_add(jDataObject, "panEncrypted", json_object_new_string(trxData.panEncrypted));
    json_object_object_add(jDataObject, "ksn", json_object_new_string(trxData.ksn));
    json_object_object_add(jDataObject, "mac", json_object_new_string(trxData.mac));
    json_object_object_add(jDataObject, "macKsn", json_object_new_string(trxData.macKsn));
    json_object_object_add(jDataObject, "reversalKsn", json_object_new_string(trxData.reversalKsn));
    json_object_object_add(jDataObject, "reversalMac", json_object_new_string(trxData.reversalMac));
    json_object_object_add(jDataObject, "moneyAddTrxType", json_object_new_string(trxData.moneyAddTrxType));
    json_object_object_add(jDataObject, "moneyAddRRN", json_object_new_string(trxData.moneyAddRRN));
    json_object_object_add(jDataObject, "moneyAddTid", json_object_new_string(trxData.moneyAddTid));
    // json_object_object_add(jDataObject, "airtelTxnStatus", json_object_new_int(trxData.airtelTxnStatus));
    // json_object_object_add(jDataObject, "airtelTxnId", json_object_new_string(trxData.airtelTxnId));
    // json_object_object_add(jDataObject, "airtelRequestData", json_object_new_string(trxData.airtelRequestData));
    // json_object_object_add(jDataObject, "airtelResponseData", json_object_new_string(trxData.airtelResponseData));
    // json_object_object_add(jDataObject, "airtelResponseCode", json_object_new_string(trxData.airtelResponseCode));
    // json_object_object_add(jDataObject, "airtelResponseDesc", json_object_new_string(trxData.airtelResponseDesc));
    // json_object_object_add(jDataObject, "airtelSwitchResponseCode", json_object_new_string(trxData.airtelSwitchResponseCode));
    // json_object_object_add(jDataObject, "airtelSwitchTerminalId", json_object_new_string(trxData.airtelSwitchTerminalId));
    // json_object_object_add(jDataObject, "airtelSwichMerchantId", json_object_new_string(trxData.airtelSwichMerchantId));
    // json_object_object_add(jDataObject, "airtelAckTxnType", json_object_new_string(trxData.airtelAckTxnType));
    // json_object_object_add(jDataObject, "airtelAckPaymentMode", json_object_new_string(trxData.airtelAckPaymentMode));
    // json_object_object_add(jDataObject, "airtelAckRefundId", json_object_new_string(trxData.airtelAckRefundId));
    json_object_object_add(jDataObject, "acqTransactionId", json_object_new_string(trxData.acqTransactionId));
    json_object_object_add(jDataObject, "acqUniqueTransactionId", json_object_new_string(trxData.acqUniqueTransactionId));

    return jDataObject;
}

/**
 * Print the transaction row
 **/
void printTransactionRow(TransactionTable trxTable)
{
    logDataEx(currentTrxType, lastPanDigit, "===========================================");

    logDataEx(currentTrxType, lastPanDigit, "ID : %d", trxTable.id);
    logDataEx(currentTrxType, lastPanDigit, "Transaction Id : %s", trxTable.transactionId);
    logDataEx(currentTrxType, lastPanDigit, "Transaction Type : %s", trxTable.trxType);
    logDataEx(currentTrxType, lastPanDigit, "Transaction Bin : %s", trxTable.trxBin);
    logDataEx(currentTrxType, lastPanDigit, "Processing Code : %s", trxTable.processingCode);
    logDataEx(currentTrxType, lastPanDigit, "Transaction Counter : %d", trxTable.trxCounter);
    logDataEx(currentTrxType, lastPanDigit, "Stan : %s", trxTable.stan);
    logDataEx(currentTrxType, lastPanDigit, "Batch : %d", trxTable.batch);
    logDataEx(currentTrxType, lastPanDigit, "Amount : %s", trxTable.amount);
    logDataEx(currentTrxType, lastPanDigit, "Time : %s", trxTable.time);
    logDataEx(currentTrxType, lastPanDigit, "Date : %s", trxTable.date);
    logDataEx(currentTrxType, lastPanDigit, "Year : %s", trxTable.year);
    logDataEx(currentTrxType, lastPanDigit, "AID : %s", trxTable.aid);
    logDataEx(currentTrxType, lastPanDigit, "Mask PAN : %s", trxTable.maskPAN);
    logDataEx(currentTrxType, lastPanDigit, "Token : %s", trxTable.token);
    logDataEx(currentTrxType, lastPanDigit, "Effective Date : %s", trxTable.effectiveDate);
    logDataEx(currentTrxType, lastPanDigit, "Service Id : %s", trxTable.serviceId);
    logDataEx(currentTrxType, lastPanDigit, "Service Index : %s", trxTable.serviceIndex);
    logDataEx(currentTrxType, lastPanDigit, "Service Data : %s", trxTable.serviceData);
    logDataEx(currentTrxType, lastPanDigit, "Service Control : %s", trxTable.serviceControl);
    logDataEx(currentTrxType, lastPanDigit, "Service Balance : %s", trxTable.serviceBalance);
    logDataEx(currentTrxType, lastPanDigit, "ICC Data : %s", trxTable.iccData);
    logDataEx(currentTrxType, lastPanDigit, "ICC Data Len : %d", trxTable.iccDataLen);
    logDataEx(currentTrxType, lastPanDigit, "KSN : %s", trxTable.ksn);
    logDataEx(currentTrxType, lastPanDigit, "Pan Encrypted : %s", trxTable.panEncrypted);
    logDataEx(currentTrxType, lastPanDigit, "Track 2 Enc : %s", trxTable.track2Enc);
    logDataEx(currentTrxType, lastPanDigit, "Exp date encrypted : %s", trxTable.expDateEnc);
    logDataEx(currentTrxType, lastPanDigit, "Mac KSN : %s", trxTable.macKsn);
    logDataEx(currentTrxType, lastPanDigit, "Mac Value : %s", trxTable.mac);
    logDataEx(currentTrxType, lastPanDigit, "Order Id : %s", trxTable.orderId);
    logDataEx(currentTrxType, lastPanDigit, "Txn Status : %s", trxTable.txnStatus);
    logDataEx(currentTrxType, lastPanDigit, "Host Status : %s", trxTable.hostStatus);
    logDataEx(currentTrxType, lastPanDigit, "Update Amount : %s", trxTable.updateAmount);
    logDataEx(currentTrxType, lastPanDigit, "Updated Balance : %s", trxTable.updatedBalance);
    logDataEx(currentTrxType, lastPanDigit, "Terminal Id : %s", trxTable.terminalId);
    logDataEx(currentTrxType, lastPanDigit, "Merchant : %s", trxTable.merchantId);
    logDataEx(currentTrxType, lastPanDigit, "Host Retry : %d", trxTable.hostRetry);
    logDataEx(currentTrxType, lastPanDigit, "RRN : %s", trxTable.rrn);
    logDataEx(currentTrxType, lastPanDigit, "Auth Code : %s", trxTable.authCode);
    logDataEx(currentTrxType, lastPanDigit, "Host Response Time Stamp : %s", trxTable.hostResponseTimeStamp);
    logDataEx(currentTrxType, lastPanDigit, "Host Result Status : %s", trxTable.hostResultStatus);
    logDataEx(currentTrxType, lastPanDigit, "Host Result Message : %s", trxTable.hostResultMessage);
    logDataEx(currentTrxType, lastPanDigit, "Host Result Code : %s", trxTable.hostResultCode);
    logDataEx(currentTrxType, lastPanDigit, "Host Result Code Id : %s", trxTable.hostResultCodeId);
    logDataEx(currentTrxType, lastPanDigit, "Host ICCData : %s", trxTable.hostIccData);
    logDataEx(currentTrxType, lastPanDigit, "Host Error : %s", trxTable.hostError);
    logDataEx(currentTrxType, lastPanDigit, "Reversal Status : %s", trxTable.reversalStatus);
    logDataEx(currentTrxType, lastPanDigit, "Money Add Trx Type : %s", trxTable.moneyAddTrxType);
    logDataEx(currentTrxType, lastPanDigit, "Acquirement Id : %s", trxTable.acquirementId);
    logDataEx(currentTrxType, lastPanDigit, "Reversal Response Code : %s", trxTable.reversalResponsecode);
    logDataEx(currentTrxType, lastPanDigit, "Reversal RRN : %s", trxTable.reversalRRN);
    logDataEx(currentTrxType, lastPanDigit, "Reversal Auth Code : %s", trxTable.reversalAuthCode);
    logDataEx(currentTrxType, lastPanDigit, "Reversal Manual Cleared : %d", trxTable.reversalManualCleared);
    logDataEx(currentTrxType, lastPanDigit, "Reversal Ksn : %s", trxTable.reversalKsn);
    logDataEx(currentTrxType, lastPanDigit, "Reversal Mac : %s", trxTable.reversalMac);
    logDataEx(currentTrxType, lastPanDigit, "Echo Ksn : %s", trxTable.echoKsn);
    logDataEx(currentTrxType, lastPanDigit, "Echo Mac : %s", trxTable.echoMac);
    logDataEx(currentTrxType, lastPanDigit, "Reversal Input Code : %s", trxTable.reversalInputResponseCode);
    logDataEx(currentTrxType, lastPanDigit, "Host Error Category : %s", trxTable.hostErrorCategory);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Txn Status : %d", trxTable.airtelTxnStatus);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Txn Id : %s", trxTable.airtelTxnId);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Request Data : %s", trxTable.airtelRequestData);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Response Data : %s", trxTable.airtelResponseData);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Response Code : %s", trxTable.airtelResponseCode);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Response Desc : %s", trxTable.airtelResponseDesc);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Response Switch Code : %s", trxTable.airtelSwitchResponseCode);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Response Switch Terminal Id : %s", trxTable.airtelSwitchTerminalId);
    logDataEx(currentTrxType, lastPanDigit, "Host Airtel Response Switch Merchant Id : %s", trxTable.airtelSwichMerchantId);
    logDataEx(currentTrxType, lastPanDigit, "Acquirer Transaction Id : %s", trxTable.acqTransactionId);
    logDataEx(currentTrxType, lastPanDigit, "Acquirer Unique Transaction Id : %s", trxTable.acqUniqueTransactionId);
    logDataEx(currentTrxType, lastPanDigit, "Money Add TID : %s", trxTable.moneyAddTid);
    logDataEx(currentTrxType, lastPanDigit, "Mony Add RRN : %s", trxTable.moneyAddRRN);

    logDataEx(currentTrxType, lastPanDigit, "===========================================");
}

// /**
//  * Create a number of transaction data for offline txn for testing
//  **/
// void createTestTransactionData()
// {
//     for (int i = 0; i < 100000; i++)
//     {

//       char *insertQuery = "INSERT INTO Transactions("
//                           "TransactionId,"
//                           "TrxType, "
//                           "TrxBin, "
//                           "ProcessingCode, "
//                           "TrxCounter, "
//                           "Stan,"
//                           "Batch,"
//                           "Amount,"
//                           "Time,"
//                           "Date,"
//                           "Year,"
//                           "Aid,"
//                           "MaskedPan,"
//                           "Token, "
//                           "EffectiveDate,"
//                           "ServiceId,"
//                           "ServiceIndex,"
//                           "ServiceData,"
//                           "ServiceControl,"
//                           "ServiceBalance,"
//                           "ICCData,"
//                           "ICCDataLen,"
//                           "KSN, "
//                           "PanEncrypted, "
//                           "ExpDateEnc, "
//                           "MacKsn, "
//                           "Mac, "
//                           "OrderId, "
//                           "TxnStatus,"
//                           "HostStatus, "
//                           "TerminalId,"
//                           "MerchantId,"
//                           "HostRetry, "
//                           "HostError "
//                           ") "
//                           "VALUES("
//                           "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

//       sqlite3_stmt *statement;
//       int result = sqlite3_prepare_v2(
//           sqlite3Db,
//           insertQuery,
//           -1,
//           &statement,
//           NULL);

//       char trxBin[3];
//       sprintf(trxBin, "%02X", 0);

//       char *transactionId = malloc(UUID_STR_LEN);
//       generateUUID(transactionId);
//       logInfoEx(currentTrxType, lastPanDigit, "Unique Transaction Id : %s", transactionId);

//       if (result == SQLITE_OK)
//       {
//           sqlite3_bind_text(statement, 1, transactionId, -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 2, "Type", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 3, trxBin, -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 4, "000000", -1, SQLITE_STATIC);
//           sqlite3_bind_int(statement, 5, 1);
//           sqlite3_bind_text(statement, 6, "000000", -1, SQLITE_STATIC);
//           sqlite3_bind_int(statement, 7, appData.batchCounter);
//           sqlite3_bind_text(statement, 8, "000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 9, "000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 10, "0000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 11, "0000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 12, "00000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 13, "00000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 14, "00000000000000000000000000000000000000000000000000000000000000000000000000000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 15, "0000000000000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 16, "0000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 17, "00", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 18, "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 19, "0000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 20, "00000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 21, "000000000000000000000000", -1, SQLITE_STATIC);
//           sqlite3_bind_int(statement, 22, 10);
//           sqlite3_bind_text(statement, 23, "000000000000000000000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 24, "0000000000000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 25, "00000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 26, "00000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 27, "00000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 28, "0000000000000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 29, "00000000", -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 30, STATUS_PENDING, -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 31, appConfig.terminalId, -1, SQLITE_STATIC);
//           sqlite3_bind_text(statement, 32, appConfig.merchantId, -1, SQLITE_STATIC);
//           sqlite3_bind_int(statement, 33, 0); // Host Retry
//           sqlite3_bind_int(statement, 34, 0); // Host Error

//           logDataEx(currentTrxType, lastPanDigit, "Going to perform the insert of offline transaction date");
//           int result = sqlite3_step(statement);
//           sqlite3_finalize(statement);
//           logDataEx(currentTrxType, lastPanDigit, "Insert result : %d", result);

//           if (result == SQLITE_DONE)
//           {
//               logInfoEx(currentTrxType, lastPanDigit, "Transaction data inserted successfully : %d", i);
//           }
//           else
//           {
//               logWarnEx(currentTrxType, lastPanDigit, "Insert data failed !!! : %d", i);
//           }
//       }
//       else
//       {
//           logWarnEx(currentTrxType, lastPanDigit, "Unable to prepare the insert query !!!");
//       }
//       free(transactionId);
//     }
// }
