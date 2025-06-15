#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <uuid/uuid.h>
#include <libpay/tlv.h>
#include <pthread.h>
#include <unistd.h>
#include <json-c/json.h>

#include "include/abtdbmanager.h"
#include "include/commonutil.h"
#include "include/config.h"
#include "include/sqlite3.h"
#include "include/hostmanager.h"
#include "include/logutil.h"
#include "include/responsemanager.h"
#include "include/commandmanager.h"
#include "include/errorcodes.h"
#include "include/appcodes.h"
#include "JAbtHost/jAbtInterface.h"
#include "JAbtHost/jAbtHostUtil.h"
#include "http-parser/http_util.h"

#define MAX_FETCH_COUNT 50

extern sqlite3 *sqlite3AbtDb;
extern struct applicationConfig appConfig;
extern struct applicationData appData;

/**
 * Create a ABT transaction data for offline txn
 **/
void createAbtTransactionData(struct transactionData *trxData)
{
    char *insertQuery = "INSERT INTO AbtTransactions("
                        "TransactionId,"
                        "TrxCounter, "
                        "Stan,"
                        "Batch,"
                        "Amount,"
                        "Time,"
                        "Date,"
                        "Year,"
                        "MaskedPan,"
                        "Token, "
                        "ICCData,"
                        "ICCDataLen,"
                        "OrderId, "
                        "TxnStatus,"
                        "TerminalId,"
                        "MerchantId,"
                        "HostRetry, "
                        "DeviceId, "
                        "GateStatus, "
                        "TrxTypeCode, "
                        "DeviceType, "
                        "DeviceCode, "
                        "LocationCode, "
                        "OperatorCode, "
                        "TraiffVersion, "
                        "DeviceModeCode, "
                        "TxnDeviceDateKey, "
                        "TxnDeviceTime, "
                        "HostStatus, "
                        "HostFailReason, "
                        "HostUpdateDays "
                        ") "
                        "VALUES("
                        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                        "? );";

    sqlite3_stmt *statement;
    int result = sqlite3_prepare_v2(
        sqlite3AbtDb,
        insertQuery,
        -1,
        &statement,
        NULL);

    logData("ABT Transaction Status : %s", trxData->txnStatus);
    char trxBin[3];
    sprintf(trxBin, "%02X", trxData->trxTypeBin);
    char deviceId[10];
    getDeviceId(deviceId);
    char timeStr[50]; // Make sure the array is large enough to hold the string
    snprintf(timeStr, sizeof(timeStr), "%lld", getSecondsSinceStartOfDay());

    if (result == SQLITE_OK)
    {
        sqlite3_bind_text(statement, 1, trxData->transactionId, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 2, trxData->txnCounter);
        sqlite3_bind_text(statement, 3, trxData->stan, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 4, appData.batchCounter);
        sqlite3_bind_text(statement, 5, trxData->sAmount, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 6, trxData->time, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 7, trxData->date, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 8, trxData->year, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 9, trxData->maskPan, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 10, trxData->token, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 11, trxData->iccData, trxData->iccDataLen, SQLITE_STATIC);
        sqlite3_bind_int(statement, 12, trxData->iccDataLen);
        sqlite3_bind_text(statement, 13, trxData->orderId, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 14, 1); // Always 1 for Txn Status
        sqlite3_bind_text(statement, 15, appConfig.terminalId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 16, appConfig.merchantId, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 17, 0); // Host Retry default to 0
        sqlite3_bind_text(statement, 18, deviceId, -1, SQLITE_STATIC);

        if (trxData->isGateOpen)
            sqlite3_bind_text(statement, 19, "true", -1, SQLITE_STATIC);
        else
            sqlite3_bind_text(statement, 19, "false", -1, SQLITE_STATIC);

        sqlite3_bind_int(statement, 20, appConfig.txnTypeCode);
        sqlite3_bind_int(statement, 21, appConfig.deviceType);
        sqlite3_bind_text(statement, 22, deviceId, -1, SQLITE_STATIC);
        sqlite3_bind_int(statement, 23, appConfig.locationCode);
        sqlite3_bind_int(statement, 24, appConfig.operatorCode);
        sqlite3_bind_int(statement, 25, appConfig.tariffVersion);
        sqlite3_bind_int(statement, 26, appConfig.deviceModeCode);
        sqlite3_bind_int(statement, 27, getDaysFromJan12020());              // Date key
        sqlite3_bind_text(statement, 28, timeStr, -1, SQLITE_STATIC);        // time in ms TxnDeviceTime
        sqlite3_bind_text(statement, 29, STATUS_PENDING, -1, SQLITE_STATIC); // Host Status
        sqlite3_bind_text(statement, 30, "", -1, SQLITE_STATIC);             // Host Reason
        sqlite3_bind_int(statement, 31, 0);                                  // Host Update Days default to 0

        logData("Going to perform the insert of offline transaction date");
        int result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        logData("Insert result : %d", result);

        if (result == SQLITE_DONE)
        {
            logInfo("ABT Transaction data inserted successfully : %s", trxData->transactionId);
        }
        else
        {
            logError("ABT Insert data failed !!!");
        }
    }
    else
    {
        logError("Unable to prepare the ABT insert query !!!");
    }
}

/**
 * To search the records and delete it
 */
void deleteAbtTransactions()
{
    int count = getAbtOkFilterDateTransactionsCount();
    char transactionIdList[count + 1][50];
    sqlite3_stmt *statement;

    // Get all the transaction ids
    const char *queryTemplate = "SELECT TransactionId FROM AbtTransactions WHERE HostStatus = 'OK' and (%d - HostUpdateDays) > %d ";
    int currentDays = getDaysFromJan12020();
    char query[512];
    snprintf(query, 512, queryTemplate, currentDays, appConfig.abtDataRetentionPeriodInDays);

    logInfo("Going to get ABT ok data with query : %s", query);

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logError("Failed to prepare deleteAbtTransactions query to get transaction id!!! %s",
                 sqlite3_errmsg(sqlite3AbtDb));
        return;
    }

    count = 0;
    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        sprintf(transactionIdList[count], "%s", sqlite3_column_text(statement, 0));
        count++;
    }
    logData("Total count of ok received : %d", count);

    logData("Printing all transaction ids to delete");
    for (int i = 0; i < count; i++)
    {
        logData("Transaction id : %s", transactionIdList[i]);
    }

    sqlite3_finalize(statement);

    for (int i = 0; i < count; i++)
    {
        const char *deleteQuery = "DELETE FROM AbtTransactions WHERE TransactionId = ?";
        logData("Going to delete record with Id : %s", transactionIdList[i]);
        logData("Delete query : %s", deleteQuery);

        sqlite3_stmt *deleteStatement;
        if (sqlite3_prepare_v2(sqlite3AbtDb, deleteQuery, -1, &deleteStatement, NULL) != SQLITE_OK)
        {
            logError("Failed to prepare delete query for ABT transaction !!!");
            continue;
        }

        sqlite3_bind_text(deleteStatement, 1, transactionIdList[i], -1, SQLITE_STATIC);

        int result = sqlite3_step(deleteStatement);
        if (result != SQLITE_DONE)
        {
            logError("Failed to delete record : %d", result);
        }
        else
        {
            logInfo("Delete success");
        }
        sqlite3_finalize(statement);
    }
    logData("All records are deleted");
}

/**
 * To get the transactions that has host status ok and within x days
 * as per the abtDataRetentionPeriodInDays
 */
int getAbtOkFilterDateTransactionsCount()
{
    const char *queryTemplate = "SELECT count(*) FROM AbtTransactions WHERE HostStatus = 'OK' and (%d - HostUpdateDays) > %d ";
    int currentDays = getDaysFromJan12020();
    char query[512];
    snprintf(query, 512, queryTemplate, currentDays, appConfig.abtDataRetentionPeriodInDays);

    sqlite3_stmt *statement;
    int count = 0;

    logInfo("Going to getAbtOkFilterDateTransactionsCount with query : %s", query);

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarn("Failed to prepare getAbtOkFilterDateTransactionsCount query !!! %s", sqlite3_errmsg(sqlite3AbtDb));
        return -1;
    }

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int(statement, 0);
        break;
    }

    sqlite3_finalize(statement);
    logInfo("Total transactions found are : %d", count);
    return count;
}

/**
 * Get the current active pending transactions with hosts in db
 **/
int getAbtTransactionStatusCount(char status[50])
{
    const char *queryTemplate = "SELECT count(*) FROM AbtTransactions WHERE HostStatus = '%s' ";
    char query[256];
    snprintf(query, 256, queryTemplate, status);
    sqlite3_stmt *statement;
    int count = 0;

    logInfo("Going to getAbtTransactionStatusCount with query : %s", query);

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarn("Failed to prepare getAbtTransactionStatusCount query !!! %s", sqlite3_errmsg(sqlite3AbtDb));
        return -1;
    }

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int(statement, 0);
        break;
    }

    sqlite3_finalize(statement);

    logInfo("Total transactions found are : %d", count);
    return count;
}

/**
 * Fetch the ABT data to be sent to client
 **/
char *fetchAbtData(struct abtFetchData fData)
{
    int max = fData.maxrecords;
    if (max > MAX_FETCH_COUNT)
        max = MAX_FETCH_COUNT;

    const char *queryTemplate = "SELECT * FROM AbtTransactions WHERE HostStatus = '%s' Order by ID DESC LIMIT %d OFFSET %d";
    char query[256];
    snprintf(query, 256, queryTemplate, fData.mode, max, fData.skipRecords);

    logData("Going to fetchAbtData with query : %s", query);

    json_object *jobj = json_object_new_object();
    json_object *jDataArrayObject = json_object_new_array();

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logData("Failed to prepare read query !!!, %s", sqlite3_errmsg(sqlite3AbtDb));
        return buildResponseMessage(STATUS_ERROR, ERR_SQL_STATEMENT_PREPARE_FAILED);
    }

    int fetchedCount = 0;
    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        AbtTransactionTable trxData = populateAbtTableData(statement);
        json_object_array_add(jDataArrayObject, getAbtJsonTxnData(trxData));
        fetchedCount++;
    }

    sqlite3_finalize(statement);

    logData("Max Records Requested : %d, and total fetched : %d", fData.maxrecords, fetchedCount);

    // Generate Json
    json_object *jCommand = json_object_new_string(COMMAND_ABT_FETCH);

    json_object_object_add(jobj, COMMAND, jCommand);
    json_object_object_add(jobj, FETCH_MODE, json_object_new_string(fData.mode));
    json_object_object_add(jobj, COMMAND_DATA, jDataArrayObject);

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * populate the table from the db statement
 **/
AbtTransactionTable populateAbtTableData(sqlite3_stmt *statement)
{
    AbtTransactionTable trxData;

    trxData.id = sqlite3_column_int64(statement, 0);
    sprintf(trxData.transactionId, "%s", sqlite3_column_text(statement, 1));
    trxData.trxCounter = sqlite3_column_int64(statement, 2);
    sprintf(trxData.stan, "%s", sqlite3_column_text(statement, 3));
    trxData.batch = sqlite3_column_int(statement, 4);
    sprintf(trxData.amount, "%s", sqlite3_column_text(statement, 5));
    sprintf(trxData.time, "%s", sqlite3_column_text(statement, 6));
    sprintf(trxData.date, "%s", sqlite3_column_text(statement, 7));
    sprintf(trxData.year, "%s", sqlite3_column_text(statement, 8));
    sprintf(trxData.maskPAN, "%s", sqlite3_column_text(statement, 9));

    if (sqlite3_column_text(statement, 10) != NULL)
        sprintf(trxData.token, "%s", sqlite3_column_text(statement, 10));
    else
        memset(trxData.token, 0, sizeof(trxData.token));

    sprintf(trxData.iccData, "%s", sqlite3_column_text(statement, 11));
    trxData.iccDataLen = sqlite3_column_int(statement, 12);

    if (sqlite3_column_text(statement, 13) != NULL)
        sprintf(trxData.orderId, "%s", sqlite3_column_text(statement, 13));
    else
        memset(trxData.orderId, 0, sizeof(trxData.orderId));

    trxData.txnStatus = sqlite3_column_int(statement, 14);
    sprintf(trxData.terminalId, "%s", sqlite3_column_text(statement, 15));
    sprintf(trxData.merchantId, "%s", sqlite3_column_text(statement, 16));
    trxData.hostRetry = sqlite3_column_int(statement, 17);
    sprintf(trxData.deviceId, "%s", sqlite3_column_text(statement, 18));
    sprintf(trxData.gateStatus, "%s", sqlite3_column_text(statement, 19));
    trxData.trxTypeCode = sqlite3_column_int(statement, 20);
    trxData.deviceType = sqlite3_column_int(statement, 21);
    sprintf(trxData.deviceCode, "%s", sqlite3_column_text(statement, 22));
    trxData.locationCode = sqlite3_column_int(statement, 23);
    trxData.operatorCode = sqlite3_column_int(statement, 24);
    trxData.tariffVersion = sqlite3_column_int(statement, 25);
    trxData.deviceModeCode = sqlite3_column_int(statement, 26);
    trxData.deviceDateKey = sqlite3_column_int(statement, 27);
    sprintf(trxData.txnDeviceTime, "%s", sqlite3_column_text(statement, 28));
    sprintf(trxData.hostStatus, "%s", sqlite3_column_text(statement, 29));
    sprintf(trxData.hostReason, "%s", sqlite3_column_text(statement, 30));
    trxData.hostUpdateDays = sqlite3_column_int(statement, 31);

    return trxData;
}

/**
 * Get the json data of a abt transaction table
 **/
json_object *getAbtJsonTxnData(AbtTransactionTable trxData)
{
    char dateYear[10];
    sprintf(dateYear, "%s%s", trxData.date, trxData.year);
    logData("ABT Transaction date to be returned in json : %s", dateYear);

    json_object *jDataObject = json_object_new_object();
    json_object_object_add(jDataObject, "tuid", json_object_new_string(trxData.transactionId));
    json_object_object_add(jDataObject, "counter", json_object_new_int(trxData.trxCounter));
    json_object_object_add(jDataObject, "stan", json_object_new_string(trxData.stan));
    json_object_object_add(jDataObject, "batch", json_object_new_int(trxData.batch));
    json_object_object_add(jDataObject, "amount", json_object_new_string(trxData.amount));
    json_object_object_add(jDataObject, "date", json_object_new_string(dateYear));
    json_object_object_add(jDataObject, "time", json_object_new_string(trxData.time));
    json_object_object_add(jDataObject, "maskPan", json_object_new_string(trxData.maskPAN));
    json_object_object_add(jDataObject, "token", json_object_new_string(trxData.token));
    json_object_object_add(jDataObject, "iccData", json_object_new_string(trxData.iccData));
    json_object_object_add(jDataObject, "orderId", json_object_new_string(trxData.orderId));
    json_object_object_add(jDataObject, "status", json_object_new_int(trxData.txnStatus));
    json_object_object_add(jDataObject, "terminalId", json_object_new_string(trxData.terminalId));
    json_object_object_add(jDataObject, "merchantId", json_object_new_string(trxData.merchantId));
    json_object_object_add(jDataObject, "hostRetry", json_object_new_int(trxData.hostRetry));
    json_object_object_add(jDataObject, "deviceId", json_object_new_string(trxData.deviceId));
    json_object_object_add(jDataObject, "gateStatus", json_object_new_string(trxData.gateStatus));
    json_object_object_add(jDataObject, "txnTypeCode", json_object_new_int(trxData.trxTypeCode));
    json_object_object_add(jDataObject, "deviceType", json_object_new_int(trxData.deviceType));
    json_object_object_add(jDataObject, "deviceCode", json_object_new_string(trxData.deviceCode));
    json_object_object_add(jDataObject, "locationCode", json_object_new_int(trxData.locationCode));
    json_object_object_add(jDataObject, "operatorCode", json_object_new_int(trxData.operatorCode));
    json_object_object_add(jDataObject, "tariffVersion", json_object_new_int(trxData.tariffVersion));
    json_object_object_add(jDataObject, "deviceModeCode", json_object_new_int(trxData.deviceModeCode));
    json_object_object_add(jDataObject, "deviceDateKey", json_object_new_int(trxData.deviceDateKey));
    json_object_object_add(jDataObject, "deviceTime", json_object_new_string(trxData.txnDeviceTime));
    json_object_object_add(jDataObject, "hostStatus", json_object_new_string(trxData.hostStatus));
    json_object_object_add(jDataObject, "hostFailReason", json_object_new_string(trxData.hostReason));
    json_object_object_add(jDataObject, "hostUpdateDay", json_object_new_int(trxData.hostUpdateDays));

    return jDataObject;
}

/**
 ** To get the count of available abt transaction records
 */
void checkAbtRecordCount()
{
    const char *query = "SELECT count(*) FROM AbtTransactions ";
    sqlite3_stmt *statement;
    int count = 0;

    logInfo("Going to get the abt transaction record count with query : %s", query);

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarn("Failed to prepare checkAbtRecordCount query !!!");
        return;
    }

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int(statement, 0);
        break;
    }

    sqlite3_finalize(statement);

    logInfo("Total ABT Transaction records available are : %d", count);
}

/**
 * To print ABT transaction row
 */
void printAbtTransactionRow(AbtTransactionTable trxTable)
{
    logData("===========================================");

    logData("ID : %d", trxTable.id);
    logData("Transaction Id : %s", trxTable.transactionId);
    logData("Trx Counter : %d", trxTable.trxCounter);
    logData("Stan : %s", trxTable.stan);
    logData("Batch : %d", trxTable.batch);
    logData("Amount : %s", trxTable.amount);
    logData("Time : %s", trxTable.time);
    logData("Date : %s", trxTable.date);
    logData("Year : %s", trxTable.year);
    logData("Mask Pan : %s", trxTable.maskPAN);
    logData("Token : %s", trxTable.token);
    logData("ICC Data : %s", trxTable.iccData);
    logData("ICC Data Len : %d", trxTable.iccDataLen);
    logData("Order Id : %s", trxTable.orderId);
    logData("Txn Status : %d", trxTable.txnStatus);
    logData("TerminalId : %s", trxTable.terminalId);
    logData("MerchantId : %s", trxTable.merchantId);
    logData("Host Retry : %d", trxTable.hostRetry);
    logData("Device Id : %s", trxTable.deviceId);
    logData("Gate Status : %s", trxTable.gateStatus);
    logData("TrxTypeCode : %d", trxTable.trxTypeCode);
    logData("DeviceType : %d", trxTable.deviceType);
    logData("DeviceCode : %s", trxTable.deviceCode);
    logData("LocationCode : %d", trxTable.locationCode);
    logData("Operatorcode : %d", trxTable.operatorCode);
    logData("TariffVersion : %d", trxTable.tariffVersion);
    logData("DeviceModeCode : %d", trxTable.deviceModeCode);
    logData("DeviceDateKey : %d", trxTable.deviceDateKey);
    logData("DeviceTime : %s", trxTable.txnDeviceTime);
    logData("HostStatus : %s", trxTable.hostStatus);
    logData("HostFailReason : %s", trxTable.hostReason);
    logData("Host Update Day : %d", trxTable.hostUpdateDays);

    logData("======================================================");
}

/**
 * Process all the pending abt transactions with abt server
 **/
void processAbtPendingTransactions()
{
    int pendingCount = getAbtTransactionStatusCount(STATUS_PENDING);
    logData("Total pending ABT transactions : %d", pendingCount);

    int nokCount = getAbtTransactionStatusCount(STATUS_NOK);
    logData("Total NOK ABT transactions : %d", nokCount);

    const char *query = "SELECT * FROM AbtTransactions WHERE "
                        "HostStatus IN('Pending', 'NOK') "
                        "order by RowId asc";
    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, 0) != SQLITE_OK)
    {
        logWarn("Failed to prepare read query !!!");
        return;
    }

    int totalCount = pendingCount + nokCount;
    AbtTransactionTable trxDataList[totalCount + 100];
    int index = 0;
    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        if (index > totalCount)
        {
            logError("Extra data found on getting abt transactions, ignoring and picked up next.");
            break;
        }
        AbtTransactionTable trxData = populateAbtTableData(statement);
        trxDataList[index] = trxData;
        index++;
    }
    sqlite3_finalize(statement);

    logData("Total transactions available : %d", totalCount);
    for (int i = 0; i < totalCount; i++)
    {
        printAbtTransactionRow(trxDataList[i]);
    }

    if (totalCount == 0)
    {
        logInfo("There are no abt transactions available to send to server");
    }
    else
    {
        logData("Total records to be sent to ABT: %d", totalCount);
        for (int index = 0; index < totalCount; index += appConfig.abtHostPushBatchCount)
        {
            int max = appConfig.abtHostPushBatchCount;
            int pending = (totalCount - index);
            if (pending < max)
                max = pending;

            logData("Sending to ABT Server from record %d with the count of %d", index, max);
            char *message = generateAbtTapRequest(trxDataList, index, max);
            char body[1024 * 24] = {0};
            strcpy(body, message);
            free(message);
            char responseMessage[1024 * 32] = {0};
            logData("Sending data to ABT for tap request");
            int retStatus = sendAbtHostRequest(body, appConfig.abtTapUrl, responseMessage);
            logData("Ret Status : %d", retStatus);
            logData("Response length from server : %d", strlen(responseMessage));

            if (retStatus == 0)
            {
                HttpResponseData httpResponseData = parseHttpResponse(responseMessage);

                if ((httpResponseData.code == 200 || httpResponseData.code == 201) && httpResponseData.messageLen != 0)
                {
                    logData("Message len : %d", httpResponseData.messageLen);
                    logData("Message received : %s", httpResponseData.message);
                    // Parse and save to db
                    int responseCount;
                    AbtTapResponse abtTapResponses[20];
                    parseAbtTapResponse(httpResponseData.message, &responseCount, abtTapResponses);
                    saveAbtResponseToDb(abtTapResponses, responseCount);
                }
                else
                {
                    logError("Http response error");
                    // Increment the host retry for all those failed
                    for (int i = index; i < (index + max); i++)
                    {
                        AbtTransactionTable abtTrxTable = trxDataList[i];
                        updateAbtHostRetry(abtTrxTable.transactionId);
                    }
                }
                free(httpResponseData.message);
            }
            else
            {
                logData("Invalid return status");
                // Increment the host retry for all those failed
                for (int i = index; i < (index + max); i++)
                {
                    AbtTransactionTable abtTrxTable = trxDataList[i];
                    updateAbtHostRetry(abtTrxTable.transactionId);
                }
            }

            memset(responseMessage, 0, sizeof(responseMessage));
            memset(body, 0, sizeof(body));
        }
    }
}

/**
 * To increment the host retry count
 */
void updateAbtHostRetry(const char transactionId[50])
{
    const char *query = "UPDATE AbtTransactions "
                        "SET HostRetry = HostRetry + 1 "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarn("Failed to prepare updateAbtHostRetry query !!!");
        return;
    }

    sqlite3_bind_text(statement, 1, transactionId, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarn("Failed to update record : %d", result);
        return;
    }
    logData("updateAbtHostRetry success");
    sqlite3_finalize(statement);
}

/**
 * To save the abt transaction response with the matching record in db
 */
void saveAbtResponseToDb(AbtTapResponse abtResponses[], int count)
{
    logData("-------------------------------------------");
    for (int i = 0; i < count; i++)
    {
        logData("Processing abt response : %d", i);
        printAbtResponse(abtResponses[i]);
        int trxCount = getAbtTrxCount(abtResponses[i].tuid);
        if (trxCount == 1)
        {
            logData("Found 1 item as expected. Going to update");
            logData("Transaction id : %s", abtResponses[i].tuid);
            updateAbtTransactionStatus(abtResponses[i]);
        }
        logData("-------------------------------------------");
    }
}

/**
 * Update the offline transaction status
 **/
void updateAbtTransactionStatus(AbtTapResponse abtTapResponse)
{
    logData("Going to update the host status for : %s", abtTapResponse.tuid);

    const char *query = "UPDATE AbtTransactions "
                        "SET HostStatus = ?,  "
                        "HostFailReason = ?, "
                        "HostUpdateDays = ?, "
                        "HostRetry = HostRetry + 1 "
                        "WHERE TransactionId = ?";

    sqlite3_stmt *statement;

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarn("Failed to prepare updateAbtTransactionStatus query !!!");
        return;
    }

    int days = getDaysFromJan12020();

    sqlite3_bind_text(statement, 1, abtTapResponse.result, -1, SQLITE_STATIC); // Host Status
    sqlite3_bind_text(statement, 2, abtTapResponse.reason, -1, SQLITE_STATIC); // Host Reason
    sqlite3_bind_int(statement, 3, days);                                      // Host update Days
    sqlite3_bind_text(statement, 4, abtTapResponse.tuid, -1, SQLITE_STATIC);

    int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        logWarn("Failed to update record : %d", result);
        return;
    }
    logData("updateAbtTransactionStatus success");
    sqlite3_finalize(statement);
}

/**
 * To get the count of ABT transactions for a particular transaction id
 */
int getAbtTrxCount(char *transactionId)
{
    const char *query = "SELECT Count(*) FROM AbtTransactions WHERE "
                        "TransactionId = ? ";
    sqlite3_stmt *statement;
    logData("Going to get the count for %s", query);

    if (sqlite3_prepare_v2(sqlite3AbtDb, query, -1, &statement, NULL) != SQLITE_OK)
    {
        logWarn("Failed to prepare getTrxTableDataCount query !!!");
        return 0;
    }

    sqlite3_bind_text(statement, 1, transactionId, -1, SQLITE_STATIC);

    int count = 0;

    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        count = sqlite3_column_int64(statement, 0);
        break;
    }

    sqlite3_finalize(statement);
    logData("Count received : %d", count);
    logData("Transaction id : %s", transactionId);
    return count;
}

/**
 * To Print abt response
 */
void printAbtResponse(AbtTapResponse abtResponse)
{
    logData("Tuid : %s", abtResponse.tuid);
    logData("Result : %s", abtResponse.result);
}