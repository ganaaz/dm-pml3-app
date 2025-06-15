#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <json-c/json.h>
#include <stdio.h>

#include "include/config.h"
#include "include/commonutil.h"
#include "include/sqlite3.h"
#include "include/logutil.h"
#include "include/errorcodes.h"
#include "include/dboperations.h"
#include "include/commandmanager.h"
#include "include/hostmanager.h"
#include "include/appcodes.h"
#include "include/abtinittable.h"

sqlite3 *sqlite3AbtDb;

/**
 * Initialize the db tables
 **/
void initAbtTrxTable()
{
    if (sqlite3_open("abttrxdata.db", &sqlite3AbtDb) != SQLITE_OK)
    {
        fprintf(stderr, "Can't open abttrxdata database: %s\n", sqlite3_errmsg(sqlite3AbtDb));
        exit(-1);
    }

    logInfo("ABT database opened successfully");
    int rc = sqlite3_exec(sqlite3AbtDb, "VACUUM;", 0, 0, 0);
    if (rc != SQLITE_OK)
    {
        printf("Error in VACUUM for abt db: %s\n", sqlite3_errmsg(sqlite3AbtDb));
    }
    else
    {
        printf("VACUUM operation successful for ABT DB.\n");
    }

    const char *createTableQuery = "CREATE TABLE IF NOT EXISTS AbtTransactions("
                                   "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                                   "TransactionId TEXT UNIQUE NOT NULL,"
                                   "TrxCounter INT NOT NULL,"
                                   "Stan TEXT NOT NULL, "
                                   "Batch INT NOT NULL,"
                                   "Amount TEXT NOT NULL,"
                                   "Time TEXT NOT NULL,"
                                   "Date TEXT NOT NULL,"
                                   "Year TEXT NOT NULL,"
                                   "MaskedPan TEXT NOT NULL,"
                                   "Token TEXT, "
                                   "ICCData TEXT NOT NULL,"
                                   "ICCDataLen INT NOT NULL,"
                                   "OrderId TEXT, "
                                   "TxnStatus INT NOT NULL,"
                                   "TerminalId TEXT, "
                                   "MerchantId TEXT, "
                                   "HostRetry INT NOT NULL,"
                                   "DeviceId TEXT, "
                                   "GateStatus TEXT, "
                                   "TrxTypeCode INT NOT NULL, "
                                   "DeviceType INT NOT NULL, "
                                   "DeviceCode TEXT, "
                                   "LocationCode INT NOT NULL, "
                                   "OperatorCode INT NOT NULL, "
                                   "TraiffVersion INT NOT NULL, "
                                   "DeviceModeCode INT NOT NULL, "
                                   "TxnDeviceDateKey INT NOT NULL, "
                                   "TxnDeviceTime TEXT NOT NULL, "
                                   "HostStatus TEXT NOT NULL, "
                                   "HostFailReason TEXT,  "
                                   "HostUpdateDays INT NOT NULL "
                                   ");";

    char *errormsg = 0;

    if (sqlite3_exec(sqlite3AbtDb, createTableQuery, NULL, NULL, &errormsg) != SQLITE_OK)
    {
        fprintf(stderr, "Can't execute: %s\n", sqlite3_errmsg(sqlite3AbtDb));
        exit(-1);
    }

    const char *indexQuery = "CREATE UNIQUE INDEX IF NOT EXISTS Abt_Txn_id_index ON AbtTransactions(TransactionId);";

    if (sqlite3_exec(sqlite3AbtDb, indexQuery, NULL, NULL, &errormsg) != SQLITE_OK)
    {
        fprintf(stderr, "Can't execute: %s\n", sqlite3_errmsg(sqlite3AbtDb));
        exit(-1);
    }

    logInfo("ABT Trx Table created / avaialble success");
    if (errormsg)
    {
        free(errormsg);
    }
}
