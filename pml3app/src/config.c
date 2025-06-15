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
#include "ISO/ISO8583_interface.h"
#include "ISO/log.h"
#include "ISO/utils.h"
#include "include/abtinittable.h"

#define VERSION "1.0.0"
#define RELEASE_DATE "16-Jun-2025"

struct applicationConfig appConfig;
struct applicationData appData;
bool isSecondTap;
sqlite3 *sqlite3Db;

extern enum device_status DEVICE_STATUS;

/**
 * Reset the second tap, so normal txn can be done
 **/
void resetSecondTap()
{
    isSecondTap = false;
}

/**
 * Mark as second tap for the ongoing txn
 **/
void setSecondTap()
{
    isSecondTap = true;
}

/**
 * Get whether its a second tap
 */
bool checkIsInSecondTap()
{
    return isSecondTap;
}

/**
 * To print the device status
 **/
void printDeviceStatus()
{
    if (DEVICE_STATUS == STATUS_ONLINE)
        logInfo("Device status is online");

    if (DEVICE_STATUS == STATUS_OFFLINE)
        logInfo("Device status is offline");
}

/**
 * Initialization the configuration file, host, db
 **/
int initConfig()
{
    loadAppConfig();
    loadAppDataConfig();

    // Initialize app data
    appData.currCodeBin = strtol(appConfig.currencyCode, NULL, 10);
    hexToByte(appConfig.currencyCode, appData.currencyCode);
    memset(appData.serviceBalanceLimit, 0, sizeof(appData.serviceBalanceLimit));
    appData.trxTypeBin = 0x00;
    appData.PRMAcqKeyIndex[0] = 0x01; // TODO : Not Required
    appData.searchTimeout = 10;
    appData.writeCardWaitTimeMs = appConfig.writeCardWaitTimeMs;

    initTransactionTable();
    initAbtTrxTable();

    return 0;
}

/**
 * Initialize the db tables
 **/
void initTransactionTable()
{
    if (sqlite3_open("pml3trxdata.db", &sqlite3Db) != SQLITE_OK)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(sqlite3Db));
        exit(-1);
    }

    int rc = sqlite3_exec(sqlite3Db, "VACUUM;", 0, 0, 0);
    if (rc != SQLITE_OK)
    {
        logError("Error in VACUUM for database: %s", sqlite3_errmsg(sqlite3Db));
    }
    else
    {
        logData("VACUUM operation successful.");
    }

    logData("Transactions database opened successfully");

    const char *createTableQuery = "CREATE TABLE IF NOT EXISTS Transactions("
                                   "ID INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                                   "TransactionId TEXT UNIQUE NOT NULL,"
                                   "TrxType TEXT NOT NULL, "
                                   "TrxBin TEXT NOT NULL, "
                                   "ProcessingCode TEXT, "
                                   "TrxCounter INT NOT NULL,"
                                   "Stan TEXT NOT NULL, "
                                   "Batch INT NOT NULL,"
                                   "Amount TEXT NOT NULL,"
                                   "Time TEXT NOT NULL,"
                                   "Date TEXT NOT NULL,"
                                   "Year TEXT NOT NULL,"
                                   "AID TEXT NOT NULL,"
                                   "MaskedPan TEXT NOT NULL,"
                                   "Token TEXT, "
                                   "EffectiveDate TEXT NOT NULL,"
                                   "ServiceId TEXT,"
                                   "ServiceIndex TEXT,"
                                   "ServiceData TEXT,"
                                   "ServiceControl TEXT,"
                                   "ServiceBalance TEXT,"
                                   "ICCData TEXT NOT NULL,"
                                   "ICCDataLen INT NOT NULL,"
                                   "KSN TEXT, "
                                   "PanEncrypted TEXT, "
                                   "Track2Enc TEXT, "
                                   "ExpDateEnc TEXT NULL, "
                                   "MacKsn TEXT NULL, "
                                   "Mac TEXT NULL, "
                                   "OrderId TEXT, "
                                   "TxnStatus TEXT NOT NULL,"
                                   "HostStatus TEXT, "
                                   "UpdateAmount TEXT, "
                                   "UpdatedBalance TEXT, "
                                   "TerminalId TEXT, "
                                   "MerchantId TEXT, "
                                   "HostRetry INT NOT NULL,"
                                   "RRN TEXT,"
                                   "AuthCode TEXT,"
                                   "HostResponseTimeStamp TEXT,"
                                   "HostResultStatus TEXT,"
                                   "HostResultMessage TEXT,"
                                   "HostResultCode TEXT,"
                                   "HostResultCodeId TEXT,"
                                   "HostIccData TEXT,"
                                   "HostError INT, "
                                   "ReversalStatus TEXT, "
                                   "MoneyAddTrxType TEXT, "
                                   "AcquirementId TEXT, "
                                   "ReversalResponseCode TEXT, "
                                   "ReversalRRN TEXT, "
                                   "ReversalAuthCode TEXT, "
                                   "ReversalManualCleared INT, "
                                   "ReversalMac TEXT, "
                                   "ReversalKsn TEXT, "
                                   "EchoMac TEXT, "
                                   "EchoKsn TEXT, "
                                   "ReversalInputResponseCode TEXT, "
                                   "HostErrorCategory TEXT, "
                                   "AirtelTxnStatus INT, "
                                   "AirtelTxnId TEXT, "
                                   "AirtelRequestData TEXT, "
                                   "AirtelResponseData TEXT, "
                                   "AirtelResponseCode TEXT, "
                                   "AirtelResponseDesc TEXT, "
                                   "AirtelSwitchResponseCode TEXT, "
                                   "AirtelSwitchTerminalId TEXT, "
                                   "AirtelSwichMerchantId TEXT, "
                                   "AirtelAckTxnType TEXT, "
                                   "AirtelAckPaymentMode TEXT, "
                                   "AirtelAckRefundId TEXT "
                                   ");";

    char *errormsg = 0;

    if (sqlite3_exec(sqlite3Db, createTableQuery, NULL, NULL, &errormsg) != SQLITE_OK)
    {
        fprintf(stderr, "Can't execute: %s\n", sqlite3_errmsg(sqlite3Db));
        exit(-1);
    }

    const char *indexQuery = "CREATE UNIQUE INDEX IF NOT EXISTS Txn_id_index ON Transactions(TransactionId);";

    if (sqlite3_exec(sqlite3Db, indexQuery, NULL, NULL, &errormsg) != SQLITE_OK)
    {
        fprintf(stderr, "Can't execute: %s\n", sqlite3_errmsg(sqlite3Db));
        exit(-1);
    }

    logInfo("Table created / avaialble success");
    if (errormsg)
    {
        free(errormsg);
    }
}

/**
 * Print the current config info
 **/
void printConfig()
{
    logWarn("Version : %s", appConfig.version);
    logWarn("Release Date : %s", appConfig.releaseDate);

    logInfo("============================================================");
    logInfo("                     Version Info                           ");
    logInfo("");
    logInfo("Name : %s", appConfig.name);
    logInfo("Version : %s", appConfig.version);
    logInfo("Release Date : %s", appConfig.releaseDate);
    logInfo("Currency Code : %s", appConfig.currencyCode);
    logInfo("EMV Config File : %s", appConfig.emvConfigFile);
    logInfo("Terminal Id : %s", appConfig.terminalId);
    logInfo("Merchant Id : %s", appConfig.merchantId);
    logInfo("Client Id : %s", appConfig.clientId);
    logInfo("Client Name : %s", appConfig.clientName);
    logInfo("Purchase Limit : %s", appConfig.purchaseLimit);
    uint64_t pAmount = strtol(appConfig.purchaseLimit, NULL, 10);
    logData("Parsed Purchase Limit = %llu.%02llu", pAmount / 100, pAmount % 100);
    logInfo("Money Add Limit : %s", appConfig.moneyAddLimit);
    uint64_t mAmount = strtol(appConfig.moneyAddLimit, NULL, 10);
    logData("Parsed Money Add Limit = %llu.%02llu", mAmount / 100, mAmount % 100);
    logInfo("Host Version : %s", appConfig.hostVersion);
    logInfo("Host IP : %s", appConfig.hostIP);
    logInfo("Host Port : %d", appConfig.hostPort);
    logInfo("Https Host Name : %s", appConfig.httpsHostName);
    logInfo("Offline Url : %s", appConfig.offlineUrl);
    logInfo("Service Creation : %s", appConfig.serviceCreationUrl);
    logInfo("Money Load Url : %s", appConfig.moneyLoadUrl);
    logInfo("Balance Update Url : %s", appConfig.balanceUpdateUrl);
    logInfo("Verify Terminal Url : %s", appConfig.verifyTerminalUrl);
    logInfo("Reversal Url : %s", appConfig.reversalUrl);
    logInfo("Host Txn Timeout : %d", appConfig.hostTxnTimeout);
    logInfo("Host Max Retry : %d", appConfig.hostMaxRetry);
    logInfo("Host Process Thread Time : %d", appConfig.hostProcessTimeInMinutes);
    logInfo("Reversal Process Check Time : %d", appConfig.reversalTimeInMinutes);
    logInfo("Write Card Wait (ms) : %d", appConfig.writeCardWaitTimeMs);
    logInfo("Max Transactions Before Device goes offline : %d", appConfig.maxOfflineTransactions);
    logInfo("Min Txn for Device to be online : %d", appConfig.minRequiredForOnline);
    logInfo("Force Key Injection : %s", appConfig.forceKeyInjection ? "true" : "false");
    logInfo("Max Key Injection Try : %d", appConfig.maxKeyInjectionTry);
    logInfo("Key Injection Retry Delay Sec : %d", appConfig.keyInjectRetryDelaySec);
    logInfo("KLD IP : %s", appConfig.kldIP);
    logInfo("Ignore zero value transaction : %d", appConfig.ignoreZeroValueTxn);
    logInfo("Beep on card found : %s", appConfig.beepOnCardFound == true ? "true" : "false");
    logInfo("Print Process outcome : %s", appConfig.printProcessOutcome == true ? "true" : "false");
    logInfo("Enable Apdu Log : %s", appConfig.enableApduLog == true ? "true" : "false");
    logInfo("Socket timeout : %d", appConfig.socketTimeout);
    logInfo("Auto read card : %s", appConfig.autoReadCard == true ? "true" : "false");

    logInfo("Device Code : %s", appConfig.deviceCode);
    logInfo("Equipment Type : %s", appConfig.equipmentType);
    logInfo("Equipment Code : %s", appConfig.equipmentCode);
    logInfo("Station Id : %s", appConfig.stationId);
    logInfo("Station Name : %s", appConfig.stationName);

    logInfo("Enable Abt : %s", appConfig.enableAbt == true ? "true" : "false");
    logInfo("Txn type code : %d", appConfig.txnTypeCode);
    logInfo("Device Type : %d", appConfig.deviceType);
    logInfo("Location Code : %d", appConfig.locationCode);
    logInfo("Operator Code : %d", appConfig.operatorCode);
    logInfo("Tariff Version : %d", appConfig.tariffVersion);
    logInfo("Device Mode Code : %d", appConfig.deviceModeCode);
    logInfo("Gate Open Wait time in ms : %d", appConfig.gateOpenWaitTimeInMs);

    logInfo("ABT Host IP : %s", appConfig.abtIP);
    logInfo("ABT Host Name : %s", appConfig.abtHostName);
    logInfo("ABT Host Port : %d", appConfig.abtPort);
    logInfo("ABT Tap Url : %d", appConfig.abtTapUrl);
    logInfo("ABT Host Process Wait Time in Minutes : %d", appConfig.abtHostProcessWaitTimeInMinutes);
    logInfo("ABT Retention Period in days : %d", appConfig.abtDataRetentionPeriodInDays);
    logInfo("ABT daily cleanup time : %s", appConfig.abtCleanupTimeHHMM);
    logInfo("ABT Host Push Batch Count : %d", appConfig.abtHostPushBatchCount);

    logInfo("Paytm Max Log Count : %d", appConfig.paytmLogCount);
    logInfo("Paytm Max Log Size : %d", appConfig.paytmMaxLogSizeKb);
    logInfo("Use EMV Config json : %s", appConfig.useConfigJson == true ? "true" : "false");

    logInfo("Use Airtel Host : %s", appConfig.useAirtelHost == true ? "true" : "false");
    logInfo("Airtel Host IP : %s", appConfig.airtelHostIP);
    logInfo("Airtel Host Port : %d", appConfig.airtelHostPort);
    logInfo("Airtel Https Host Name : %s", appConfig.airtelHttpsHostName);
    logInfo("Airtel Offline url : %s", appConfig.airtelOfflineUrl);
    logInfo("Airtel Balance Update url : %s", appConfig.airtelBalanceUpdateUrl);
    logInfo("Airtel Money Add url : %s", appConfig.airtelMoneyAddUrl);
    logInfo("Airtel Sign Salt : %s", appConfig.airtelSignSalt);
    logInfo("Latitude : %s", appConfig.latitude);
    logInfo("Longitude : %s", appConfig.longitude);

    logInfo("Total keys in config : %d", appConfig.keysLength);

    for (int i = 0; i < appConfig.keysLength; i++)
    {
        logData("------------------------------------------------------------");
        logData("Key : %d", (i + 1));
        logData("Label : %s", appConfig.keyDataList[i]->label);
        logData("Slot : %d", appConfig.keyDataList[i]->slot);
        logData("MK Version : %d", appConfig.keyDataList[i]->mkVersion);
        logData("AST Id : %s", appConfig.keyDataList[i]->astId);
        logData("PKCS Id : %s", appConfig.keyDataList[i]->pkcsId);
        logData("Type : %s", appConfig.keyDataList[i]->type);
        logData("KeySet Identifier : %s", appConfig.keyDataList[i]->keySetIdentifier);
        logData("IsMac : %d", appConfig.keyDataList[i]->isMac);
    }

    logInfo("");

    logInfo("Total LED config length : %d", appConfig.ledLength);
    for (int i = 0; i < appConfig.ledLength; i++)
    {
        logData("------------------------------------------------------------");
        logData("State Name : %s", appConfig.ledDataList[i]->stateName);
        logData("Mid Logo : %s", appConfig.ledDataList[i]->midLogo);
        logData("LED 1 : %s", appConfig.ledDataList[i]->led1);
        logData("LED 2 : %s", appConfig.ledDataList[i]->led2);
        logData("LED 3 : %s", appConfig.ledDataList[i]->led3);
        logData("LED 4 : %s", appConfig.ledDataList[i]->led4);

        // displayLight(appConfig.ledDataList[i]->stateName);
        // sleep(5);
    }

    logInfo("");
    logData("App Data Config values : ");
    logInfo("Transaction Counter : %ld", appData.transactionCounter);
    logInfo("Batch Counter : %ld", appData.batchCounter);

    logInfo("============================================================");
}

KEYDATA *getDukptKey()
{
    for (int i = 0; i < appConfig.keysLength; i++)
    {
        if (strcmp(appConfig.keyDataList[i]->type, "DUKPT_KEY") == 0 &&
            appConfig.keyDataList[i]->isMac == false)
        {
            return appConfig.keyDataList[i];
        }
    }

    return NULL;
}

KEYDATA *getMacKey()
{
    for (int i = 0; i < appConfig.keysLength; i++)
    {
        if (strcmp(appConfig.keyDataList[i]->type, "DUKPT_KEY") == 0 &&
            appConfig.keyDataList[i]->isMac == true)
        {
            return appConfig.keyDataList[i];
        }
    }

    return NULL;
}

/**
 * Get the status as a string value
 **/
char *getStatusString()
{
    if (appData.status == APP_STATUS_INITIALIZE)
    {
        return (char *)"Initializing";
    }

    if (appData.status == APP_STATUS_READY)
    {
        return (char *)"Ready";
    }

    if (appData.status == APP_STATUS_ERROR)
    {
        return (char *)"Error";
    }

    if (appData.status == APP_STATUS_AWAIT_CARD)
    {
        return (char *)"Await Card";
    }

    if (appData.status == APP_STATUS_STOPPING_SEARCH)
    {
        return (char *)"Stopping Search";
    }

    if (appData.status == APP_STATUS_KEY_MISSING)
    {
        return (char *)"Key Missing";
    }

    if (appData.status == APP_STATUS_TID_MID_EMPTY)
    {
        return (char *)"TID or MID is Empty";
    }

    if (appData.status == APP_STATUS_CARD_PRESENTED)
    {
        return (char *)"Card Presented";
    }

    if (appData.status == APP_STATUS_ABT_CARD_PRSENTED)
    {
        return (char *)"ABT Card Presented";
    }

    if (appData.status == APP_STATUS_RECALC_MAC)
    {
        return (char *)"Recalculate Mac";
    }

    return (char *)"Unknown";
}

/**
 * Load the app config from the file
 **/
void loadAppConfig()
{
    logInfo("Going to read the file");
    char *buffer;
    long length;
    FILE *file = fopen("config/app_config.json", "r");
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer = (char *)malloc(length);
    if (buffer)
    {
        fread(buffer, 1, length, file);
    }
    fclose(file);

    json_object *jObject = json_tokener_parse(buffer);

    // Fixed in code
    strcpy(appConfig.version, VERSION);
    strcpy(appConfig.releaseDate, RELEASE_DATE);

    strcpy(appConfig.name, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_NAME)));
    strcpy(appConfig.currencyCode, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_CURR_CODE)));
    strcpy(appConfig.emvConfigFile, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_EMV_FILE)));
    strcpy(appConfig.terminalId, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_TERMINALID)));
    strcpy(appConfig.merchantId, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_MERCHANT_ID)));
    strcpy(appConfig.clientId, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_CLIENT_ID)));
    strcpy(appConfig.clientName, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_CLIENT_NAME)));
    strcpy(appConfig.hostVersion, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_HOST_VERSION)));
    strcpy(appConfig.hostIP, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_HOST_IP)));
    strcpy(appConfig.httpsHostName, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_HTTPS_HOST_NAME)));
    strcpy(appConfig.offlineUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_OFFLINE_URL)));
    strcpy(appConfig.serviceCreationUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_SERVRICE_CREATION_URL)));
    strcpy(appConfig.balanceUpdateUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_BALANCE_UPDATE_URL)));
    strcpy(appConfig.moneyLoadUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_MONEY_LOAD_URL)));
    strcpy(appConfig.verifyTerminalUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_VERIFY_TERMINAL_URL)));
    strcpy(appConfig.reversalUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_REVERSAL_URL)));
    strcpy(appConfig.kldIP, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_KLD_IP)));
    appConfig.hostPort = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_HOST_PORT));
    appConfig.hostTxnTimeout = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_HOST_TXN_TIMEOUT));
    appConfig.writeCardWaitTimeMs = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_WRITE_CARD_WAIT));
    appConfig.hostMaxRetry = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_HOST_MAX_RETRY));
    appConfig.hostProcessTimeInMinutes = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_HOST_PROCESS_TIMEOUT));
    appConfig.maxKeyInjectionTry = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_MAX_KEY_INJECTION_TRY));
    appConfig.keyInjectRetryDelaySec = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_KEYINJECT_RETRY_DELAY_SEC));

    if (json_object_object_get(jObject, CONFIG_KEY_PURCHASE_LIMIT) != NULL)
    {
        strcpy(appConfig.purchaseLimit, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_PURCHASE_LIMIT)));
    }
    else
    {
        strcpy(appConfig.purchaseLimit, "20000");
    }
    if (json_object_object_get(jObject, CONFIG_KEY_MONEY_ADD_LIMIT) != NULL)
    {
        strcpy(appConfig.moneyAddLimit, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_MONEY_ADD_LIMIT)));
    }
    else
    {
        strcpy(appConfig.moneyAddLimit, "200000");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_REVERSAL_PROCESS_TIMEOUT) != NULL)
    {
        appConfig.reversalTimeInMinutes = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_REVERSAL_PROCESS_TIMEOUT));
    }
    else
    {
        appConfig.reversalTimeInMinutes = 100;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_MAX_OFFLINE_TXN) != NULL)
    {
        appConfig.maxOfflineTransactions = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_MAX_OFFLINE_TXN));
    }
    else
    {
        appConfig.maxOfflineTransactions = 100;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_MIN_REQUIRED_ONLINE) != NULL)
    {
        appConfig.minRequiredForOnline = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_MIN_REQUIRED_ONLINE));
    }
    else
    {
        appConfig.minRequiredForOnline = 80;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_PRINT_PROCESS_OUTCOME) != NULL)
    {
        appConfig.printProcessOutcome = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_PRINT_PROCESS_OUTCOME));
    }
    else
    {
        appConfig.printProcessOutcome = false;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ENABLE_APDU_LOG) != NULL)
    {
        appConfig.enableApduLog = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_ENABLE_APDU_LOG));
    }
    else
    {
        appConfig.enableApduLog = true;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_SOCKET_TIMEOUT) != NULL)
    {
        appConfig.socketTimeout = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_SOCKET_TIMEOUT));
    }
    else
    {
        appConfig.socketTimeout = -1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AUTO_READ_CARD) != NULL)
    {
        appConfig.autoReadCard = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_AUTO_READ_CARD));
    }
    else
    {
        appConfig.autoReadCard = true;
    }

    appConfig.purchaseLimitAmount = strtol(appConfig.purchaseLimit, NULL, 10);
    appConfig.moneyAddLimitAmount = strtol(appConfig.moneyAddLimit, NULL, 10);

    json_bool zeroValueTxn = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_ZERO_VALUE_TXN));

    if (zeroValueTxn == TRUE)
        appConfig.ignoreZeroValueTxn = true;
    else
        appConfig.ignoreZeroValueTxn = false;

    json_bool beepCardFound = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_BEEP_ON_CARD_FOUND));

    if (beepCardFound == TRUE)
        appConfig.beepOnCardFound = true;
    else
        appConfig.beepOnCardFound = false;

    json_bool keyInjection = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_FORCE_KEY_INJECTION));

    if (keyInjection == TRUE)
        appConfig.forceKeyInjection = true;
    else
        appConfig.forceKeyInjection = false;

    json_bool timingEnabled = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_TIMINIG_ENABLED));
    if (timingEnabled == TRUE)
        appConfig.isTimingEnabled = true;
    else
        appConfig.isTimingEnabled = false;

    json_bool printTiming = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_PRINT_DETAIL_TIMING_LOGS));
    if (printTiming == TRUE)
        appConfig.isPrintDetailTimingLogs = true;
    else
        appConfig.isPrintDetailTimingLogs = false;

    if (json_object_object_get(jObject, CONFIG_KEY_DEVICE_CODE) != NULL)
    {
        strcpy(appConfig.deviceCode, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_DEVICE_CODE)));
    }
    else
    {
        strcpy(appConfig.deviceCode, "1234567890");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_EQUIPMENT_TYPE) != NULL)
    {
        strcpy(appConfig.equipmentType, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_EQUIPMENT_TYPE)));
    }
    else
    {
        strcpy(appConfig.equipmentType, "eqType");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_EQUIPMENT_CODE) != NULL)
    {
        strcpy(appConfig.equipmentCode, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_EQUIPMENT_CODE)));
    }
    else
    {
        strcpy(appConfig.equipmentCode, "eqCode");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_STATION_ID) != NULL)
    {
        strcpy(appConfig.stationId, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_STATION_ID)));
    }
    else
    {
        strcpy(appConfig.stationId, "123");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_STATION_NAME) != NULL)
    {
        strcpy(appConfig.stationName, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_STATION_NAME)));
    }
    else
    {
        strcpy(appConfig.stationName, "Station-Name-123");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_PAYTM_LOG_COUNT) != NULL)
    {
        appConfig.paytmLogCount = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_PAYTM_LOG_COUNT));
    }
    else
    {
        appConfig.paytmLogCount = 2;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_PAYTM_LOG_SIZE) != NULL)
    {
        appConfig.paytmMaxLogSizeKb = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_PAYTM_LOG_SIZE));
    }
    else
    {
        appConfig.paytmMaxLogSizeKb = 100;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_USE_CONFIG_JSON) != NULL)
    {
        json_bool useConfigJson = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_USE_CONFIG_JSON));
        if (useConfigJson == TRUE)
            appConfig.useConfigJson = true;
    }
    else
    {
        appConfig.useConfigJson = false;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ENABLE_ABT) != NULL)
    {
        json_bool jEnableAbt = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_ENABLE_ABT));
        if (jEnableAbt == TRUE)
            appConfig.enableAbt = true;
    }
    else
    {
        appConfig.enableAbt = false;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_TXN_TYPE_CODE) != NULL)
    {
        appConfig.txnTypeCode = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_TXN_TYPE_CODE));
    }
    else
    {
        appConfig.txnTypeCode = 1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_DEVICE_TYPE) != NULL)
    {
        appConfig.deviceType = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_DEVICE_TYPE));
    }
    else
    {
        appConfig.deviceType = 1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_LOCATION_CODE) != NULL)
    {
        appConfig.locationCode = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_LOCATION_CODE));
    }
    else
    {
        appConfig.locationCode = 1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_OPERATOR_CODE) != NULL)
    {
        appConfig.operatorCode = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_OPERATOR_CODE));
    }
    else
    {
        appConfig.operatorCode = 1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_TARIFF_VER) != NULL)
    {
        appConfig.tariffVersion = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_TARIFF_VER));
    }
    else
    {
        appConfig.tariffVersion = 1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_DEVICE_MODE_CODE) != NULL)
    {
        appConfig.deviceModeCode = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_DEVICE_MODE_CODE));
    }
    else
    {
        appConfig.deviceModeCode = 1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_GATE_OPEN_WAIT) != NULL)
    {
        appConfig.gateOpenWaitTimeInMs = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_GATE_OPEN_WAIT));
    }
    else
    {
        appConfig.gateOpenWaitTimeInMs = 500;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_PORT) != NULL)
    {
        appConfig.abtPort = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_PORT));
    }
    else
    {
        appConfig.abtPort = 443;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_NAME) != NULL)
    {
        strcpy(appConfig.abtHostName, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_NAME)));
    }
    else
    {
        strcpy(appConfig.abtHostName, "dev-abt-etpass.datamatics.com");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_IP) != NULL)
    {
        strcpy(appConfig.abtIP, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_IP)));
    }
    else
    {
        strcpy(appConfig.abtIP, "10.0.0.20");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_TAP_URL) != NULL)
    {
        strcpy(appConfig.abtTapUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ABT_TAP_URL)));
    }
    else
    {
        strcpy(appConfig.abtTapUrl, "/api/taps_receiver");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_WAIT_TIME) != NULL)
    {
        appConfig.abtHostProcessWaitTimeInMinutes = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_WAIT_TIME));
    }
    else
    {
        appConfig.abtHostProcessWaitTimeInMinutes = 1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_RETENTION_DAYS) != NULL)
    {
        appConfig.abtDataRetentionPeriodInDays = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_ABT_RETENTION_DAYS));
    }
    else
    {
        appConfig.abtDataRetentionPeriodInDays = 1;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_CLEANUP_TIME) != NULL)
    {
        strcpy(appConfig.abtCleanupTimeHHMM, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ABT_CLEANUP_TIME)));
    }
    else
    {
        strcpy(appConfig.abtCleanupTimeHHMM, "22:00");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_PUSH_BATCH_COUNT) != NULL)
    {
        appConfig.abtHostPushBatchCount = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_PUSH_BATCH_COUNT));
    }
    else
    {
        appConfig.abtHostPushBatchCount = 10;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_USE_AIRTEL_HOST) != NULL)
    {
        json_bool jUseAirtelHost = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_USE_AIRTEL_HOST));
        if (jUseAirtelHost == TRUE)
            appConfig.useAirtelHost = true;
    }
    else
    {
        appConfig.useAirtelHost = false;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HOST_IP) != NULL)
    {
        strcpy(appConfig.airtelHostIP, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HOST_IP)));
    }
    else
    {
        strcpy(appConfig.airtelHostIP, "182.79.196.24");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HOST_PORT) != NULL)
    {
        appConfig.airtelHostPort = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HOST_PORT));
    }
    else
    {
        appConfig.airtelHostPort = 443;
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME) != NULL)
    {
        strcpy(appConfig.airtelHttpsHostName, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME)));
    }
    else
    {
        strcpy(appConfig.airtelHttpsHostName, "apbsit.airtelbank.com");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ARITEL_OFFLINE_URL) != NULL)
    {
        strcpy(appConfig.airtelOfflineUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ARITEL_OFFLINE_URL)));
    }
    else
    {
        strcpy(appConfig.airtelOfflineUrl, "/payments-sit/transit/acq/transactions/batch-offline-sale");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL) != NULL)
    {
        strcpy(appConfig.airtelBalanceUpdateUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL)));
    }
    else
    {
        strcpy(appConfig.airtelBalanceUpdateUrl, "/payments-sit/transit/acq/transactions/balance-update");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL) != NULL)
    {
        strcpy(appConfig.airtelHealthCheckUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL)));
    }
    else
    {
        strcpy(appConfig.airtelHealthCheckUrl, "/payments-sit/transit/acq/terminals/health");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL) != NULL)
    {
        strcpy(appConfig.airtelVerifyTerminalUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL)));
    }
    else
    {
        strcpy(appConfig.airtelVerifyTerminalUrl, "/payments-sit/transit/acq/terminals/verify");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_REVERSAL_URL) != NULL)
    {
        strcpy(appConfig.airtelReversalUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_REVERSAL_URL)));
    }
    else
    {
        strcpy(appConfig.airtelReversalUrl, "/payments-sit/transit/acq/transactions/acknowledgement");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_MONEY_ADD_URL) != NULL)
    {
        strcpy(appConfig.airtelMoneyAddUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_MONEY_ADD_URL)));
    }
    else
    {
        strcpy(appConfig.airtelMoneyAddUrl, "/payments-sit/transit/acq/transactions/money-add");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL) != NULL)
    {
        strcpy(appConfig.airtelServiceCreationUrl, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL)));
    }
    else
    {
        strcpy(appConfig.airtelServiceCreationUrl, "/payments-sit/transit/acq/transactions/service-creation");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_SIGN_SALT) != NULL)
    {
        strcpy(appConfig.airtelSignSalt, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_SIGN_SALT)));
    }
    else
    {
        strcpy(appConfig.airtelSignSalt, "secretSalt128");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_LATITUDE) != NULL)
    {
        strcpy(appConfig.latitude, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_LATITUDE)));
    }
    else
    {
        strcpy(appConfig.latitude, "37");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_LONGITUDE) != NULL)
    {
        strcpy(appConfig.longitude, (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_LONGITUDE)));
    }
    else
    {
        strcpy(appConfig.longitude, "69");
    }

    readAndUpdateKeys(jObject);
    readAndUpdateLedConfigs(jObject);

    json_object_put(jObject); // Clear the json memory
    free(buffer);

    logInfo("Config file read and updated successfully.");

    saveConfig();
}

/**
 * Load the application data config that has the counters
 **/
void loadAppDataConfig()
{
    logInfo("Going to read the app data file");
    char *buffer;
    long length;
    FILE *file = fopen("config/app_data.json", "r");
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer = (char *)malloc(length);
    if (buffer)
    {
        fread(buffer, 1, length, file);
    }
    fclose(file);

    json_object *jObject = json_tokener_parse(buffer);

    appData.transactionCounter = json_object_get_int64(json_object_object_get(jObject, CONFIG_KEY_TXN_COUNTER));
    appData.batchCounter = json_object_get_int64(json_object_object_get(jObject, CONFIG_KEY_BATCH_COUNTER));

    json_object_put(jObject); // Clear the json memory
    free(buffer);

    logInfo("Data file read and updated successfully.");
}

/**
 * Write the application data that has counters after every change
 **/
void writeAppData()
{
    json_object *jobj = json_object_new_object();

    json_object *jTxnCounter = json_object_new_int64(appData.transactionCounter);
    json_object *jBatchCounter = json_object_new_int64(appData.batchCounter);

    json_object_object_add(jobj, CONFIG_KEY_TXN_COUNTER, jTxnCounter);
    json_object_object_add(jobj, CONFIG_KEY_BATCH_COUNTER, jBatchCounter);

    const char *jsonData = json_object_to_json_string(jobj);

    FILE *outputFile = fopen("config/app_data.json", "w");
    fprintf(outputFile, "%s\n", jsonData);
    fclose(outputFile);

    json_object_put(jobj);
    logInfo("App Data is successfully saved with new counters");
}

/**
 * Save the config back to the json file
 */
void saveConfig()
{
    json_object *jobj = json_object_new_object();

    json_object *jName = json_object_new_string(appConfig.name);
    json_object *jCurrCode = json_object_new_string(appConfig.currencyCode);
    json_object *jEmvConfig = json_object_new_string(appConfig.emvConfigFile);

    json_object *jTerminalId = json_object_new_string(appConfig.terminalId);
    json_object *jMerchantId = json_object_new_string(appConfig.merchantId);
    json_object *jClientId = json_object_new_string(appConfig.clientId);
    json_object *jClientName = json_object_new_string(appConfig.clientName);
    json_object *jHostVersion = json_object_new_string(appConfig.hostVersion);
    json_object *jHostIP = json_object_new_string(appConfig.hostIP);
    json_object *jHttpsHostName = json_object_new_string(appConfig.httpsHostName);
    json_object *jKldIP = json_object_new_string(appConfig.kldIP);
    json_object *jHostPort = json_object_new_int(appConfig.hostPort);
    json_object *jHostTxnTimeOut = json_object_new_int(appConfig.hostTxnTimeout);

    json_object *jWriteCardWait = json_object_new_int(appConfig.writeCardWaitTimeMs);

    json_object *jHostMaxRetry = json_object_new_int(appConfig.hostMaxRetry);
    json_object *jHostProcessTime = json_object_new_int(appConfig.hostProcessTimeInMinutes);
    json_object *jReversalTime = json_object_new_int(appConfig.reversalTimeInMinutes);
    json_object *jMaxKeyInjectionTry = json_object_new_int(appConfig.maxKeyInjectionTry);
    json_object *jKeyInjectionRetryDelay = json_object_new_int(appConfig.keyInjectRetryDelaySec);
    json_object *jForceKeyInjection = json_object_new_boolean(appConfig.forceKeyInjection);

    json_object *jMaxOfflineTrx = json_object_new_int(appConfig.maxOfflineTransactions);
    json_object *jMinRequiredForOnline = json_object_new_int(appConfig.minRequiredForOnline);

    json_object *jTimingEnabled = json_object_new_boolean(appConfig.isTimingEnabled);
    json_object *jPrintTimingLogs = json_object_new_boolean(appConfig.isPrintDetailTimingLogs);
    json_object *jIgnoreZeroValue = json_object_new_boolean(appConfig.ignoreZeroValueTxn);
    json_object *jBeepOnCardFound = json_object_new_boolean(appConfig.beepOnCardFound);
    json_object *jUseConfigJson = json_object_new_boolean(appConfig.useConfigJson);
    json_object *jEnableAbt = json_object_new_boolean(appConfig.enableAbt);

    // Save top level data
    json_object_object_add(jobj, CONFIG_KEY_NAME, jName);
    json_object_object_add(jobj, CONFIG_KEY_CURR_CODE, jCurrCode);
    json_object_object_add(jobj, CONFIG_KEY_EMV_FILE, jEmvConfig);
    json_object_object_add(jobj, CONFIG_KEY_TERMINALID, jTerminalId);
    json_object_object_add(jobj, CONFIG_KEY_MERCHANT_ID, jMerchantId);
    json_object_object_add(jobj, CONFIG_KEY_CLIENT_ID, jClientId);
    json_object_object_add(jobj, CONFIG_KEY_PURCHASE_LIMIT, json_object_new_string(appConfig.purchaseLimit));
    json_object_object_add(jobj, CONFIG_KEY_MONEY_ADD_LIMIT, json_object_new_string(appConfig.moneyAddLimit));
    json_object_object_add(jobj, CONFIG_KEY_CLIENT_NAME, jClientName);
    json_object_object_add(jobj, CONFIG_KEY_HOST_VERSION, jHostVersion);
    json_object_object_add(jobj, CONFIG_KEY_HOST_IP, jHostIP);
    json_object_object_add(jobj, CONFIG_KEY_HOST_PORT, jHostPort);
    json_object_object_add(jobj, CONFIG_KEY_HTTPS_HOST_NAME, jHttpsHostName);
    json_object_object_add(jobj, CONFIG_KEY_OFFLINE_URL, json_object_new_string(appConfig.offlineUrl));
    json_object_object_add(jobj, CONFIG_KEY_SERVRICE_CREATION_URL, json_object_new_string(appConfig.serviceCreationUrl));
    json_object_object_add(jobj, CONFIG_KEY_MONEY_LOAD_URL, json_object_new_string(appConfig.moneyLoadUrl));
    json_object_object_add(jobj, CONFIG_KEY_BALANCE_UPDATE_URL, json_object_new_string(appConfig.balanceUpdateUrl));
    json_object_object_add(jobj, CONFIG_KEY_VERIFY_TERMINAL_URL, json_object_new_string(appConfig.verifyTerminalUrl));
    json_object_object_add(jobj, CONFIG_KEY_REVERSAL_URL, json_object_new_string(appConfig.reversalUrl));
    json_object_object_add(jobj, CONFIG_KEY_HOST_TXN_TIMEOUT, jHostTxnTimeOut);
    json_object_object_add(jobj, CONFIG_KEY_WRITE_CARD_WAIT, jWriteCardWait);
    json_object_object_add(jobj, CONFIG_KEY_HOST_MAX_RETRY, jHostMaxRetry);
    json_object_object_add(jobj, CONFIG_KEY_HOST_PROCESS_TIMEOUT, jHostProcessTime);
    json_object_object_add(jobj, CONFIG_KEY_REVERSAL_PROCESS_TIMEOUT, jReversalTime);
    json_object_object_add(jobj, CONFIG_KEY_MAX_OFFLINE_TXN, jMaxOfflineTrx);
    json_object_object_add(jobj, CONFIG_KEY_MIN_REQUIRED_ONLINE, jMinRequiredForOnline);
    json_object_object_add(jobj, CONFIG_KEY_KLD_IP, jKldIP);
    json_object_object_add(jobj, CONFIG_KEY_MAX_KEY_INJECTION_TRY, jMaxKeyInjectionTry);
    json_object_object_add(jobj, CONFIG_KEY_FORCE_KEY_INJECTION, jForceKeyInjection);
    json_object_object_add(jobj, CONFIG_KEY_TIMINIG_ENABLED, jTimingEnabled);
    json_object_object_add(jobj, CONFIG_KEY_PRINT_DETAIL_TIMING_LOGS, jPrintTimingLogs);
    json_object_object_add(jobj, CONFIG_KEY_KEYINJECT_RETRY_DELAY_SEC, jKeyInjectionRetryDelay);
    json_object_object_add(jobj, CONFIG_KEY_ZERO_VALUE_TXN, jIgnoreZeroValue);
    json_object_object_add(jobj, CONFIG_KEY_BEEP_ON_CARD_FOUND, jBeepOnCardFound);
    json_object_object_add(jobj, CONFIG_KEY_PRINT_PROCESS_OUTCOME, json_object_new_boolean(appConfig.printProcessOutcome));
    json_object_object_add(jobj, CONFIG_KEY_ENABLE_APDU_LOG, json_object_new_boolean(appConfig.enableApduLog));
    json_object_object_add(jobj, CONFIG_KEY_SOCKET_TIMEOUT, json_object_new_int(appConfig.socketTimeout));
    json_object_object_add(jobj, CONFIG_KEY_AUTO_READ_CARD, json_object_new_boolean(appConfig.autoReadCard));
    json_object_object_add(jobj, CONFIG_KEY_DEVICE_CODE, json_object_new_string(appConfig.deviceCode));
    json_object_object_add(jobj, CONFIG_KEY_EQUIPMENT_TYPE, json_object_new_string(appConfig.equipmentType));
    json_object_object_add(jobj, CONFIG_KEY_EQUIPMENT_CODE, json_object_new_string(appConfig.equipmentCode));
    json_object_object_add(jobj, CONFIG_KEY_STATION_ID, json_object_new_string(appConfig.stationId));
    json_object_object_add(jobj, CONFIG_KEY_STATION_NAME, json_object_new_string(appConfig.stationName));
    json_object_object_add(jobj, CONFIG_KEY_PAYTM_LOG_COUNT, json_object_new_int(appConfig.paytmLogCount));
    json_object_object_add(jobj, CONFIG_KEY_PAYTM_LOG_SIZE, json_object_new_int(appConfig.paytmMaxLogSizeKb));
    json_object_object_add(jobj, CONFIG_KEY_USE_CONFIG_JSON, jUseConfigJson);
    json_object_object_add(jobj, CONFIG_KEY_ENABLE_ABT, jEnableAbt);
    json_object_object_add(jobj, CONFIG_KEY_TXN_TYPE_CODE, json_object_new_int(appConfig.txnTypeCode));
    json_object_object_add(jobj, CONFIG_KEY_DEVICE_TYPE, json_object_new_int(appConfig.deviceType));
    json_object_object_add(jobj, CONFIG_KEY_LOCATION_CODE, json_object_new_int(appConfig.locationCode));
    json_object_object_add(jobj, CONFIG_KEY_OPERATOR_CODE, json_object_new_int(appConfig.operatorCode));
    json_object_object_add(jobj, CONFIG_KEY_TARIFF_VER, json_object_new_int(appConfig.tariffVersion));
    json_object_object_add(jobj, CONFIG_KEY_DEVICE_MODE_CODE, json_object_new_int(appConfig.deviceModeCode));
    json_object_object_add(jobj, CONFIG_KEY_GATE_OPEN_WAIT, json_object_new_int(appConfig.gateOpenWaitTimeInMs));
    json_object_object_add(jobj, CONFIG_KEY_ABT_HOST_IP, json_object_new_string(appConfig.abtIP));
    json_object_object_add(jobj, CONFIG_KEY_ABT_HOST_NAME, json_object_new_string(appConfig.abtHostName));
    json_object_object_add(jobj, CONFIG_KEY_ABT_TAP_URL, json_object_new_string(appConfig.abtTapUrl));
    json_object_object_add(jobj, CONFIG_KEY_ABT_HOST_PORT, json_object_new_int(appConfig.abtPort));
    json_object_object_add(jobj, CONFIG_KEY_ABT_HOST_WAIT_TIME, json_object_new_int(appConfig.abtHostProcessWaitTimeInMinutes));
    json_object_object_add(jobj, CONFIG_KEY_ABT_RETENTION_DAYS, json_object_new_int(appConfig.abtDataRetentionPeriodInDays));
    json_object_object_add(jobj, CONFIG_KEY_ABT_CLEANUP_TIME, json_object_new_string(appConfig.abtCleanupTimeHHMM));
    json_object_object_add(jobj, CONFIG_KEY_ABT_HOST_PUSH_BATCH_COUNT, json_object_new_int(appConfig.abtHostPushBatchCount));
    json_object_object_add(jobj, CONFIG_KEY_USE_AIRTEL_HOST, json_object_new_boolean(appConfig.useAirtelHost));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_HOST_IP, json_object_new_string(appConfig.airtelHostIP));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_HOST_PORT, json_object_new_int(appConfig.airtelHostPort));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME, json_object_new_string(appConfig.airtelHttpsHostName));
    json_object_object_add(jobj, CONFIG_KEY_ARITEL_OFFLINE_URL, json_object_new_string(appConfig.airtelOfflineUrl));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL, json_object_new_string(appConfig.airtelBalanceUpdateUrl));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_MONEY_ADD_URL, json_object_new_string(appConfig.airtelMoneyAddUrl));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL, json_object_new_string(appConfig.airtelServiceCreationUrl));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL, json_object_new_string(appConfig.airtelVerifyTerminalUrl));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL, json_object_new_string(appConfig.airtelHealthCheckUrl));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_REVERSAL_URL, json_object_new_string(appConfig.airtelReversalUrl));
    json_object_object_add(jobj, CONFIG_KEY_AIRTEL_SIGN_SALT, json_object_new_string(appConfig.airtelSignSalt));
    json_object_object_add(jobj, CONFIG_KEY_LATITUDE, json_object_new_string(appConfig.latitude));
    json_object_object_add(jobj, CONFIG_KEY_LONGITUDE, json_object_new_string(appConfig.longitude));

    // Write the keys
    json_object *jKeyArrayObject = json_object_new_array();

    for (int i = 0; i < appConfig.keysLength; i++)
    {
        KEYDATA *keyData = appConfig.keyDataList[i];
        json_object *jKeyData = json_object_new_object();

        json_object_object_add(jKeyData, CONFIG_KEY_KEY_LABEL, json_object_new_string(keyData->label));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_SLOT, json_object_new_int(keyData->slot));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_MKVERSION, json_object_new_int(keyData->mkVersion));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_AST_ID, json_object_new_string(keyData->astId));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_PKCS_ID, json_object_new_string(keyData->pkcsId));
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_TYPE, json_object_new_string(keyData->type));

        if (strcmp(keyData->type, KEY_TYPE_DUKPT_KEY) == 0)
        {
            json_object_object_add(jKeyData, CONFIG_KEY_KEY_KEY_SET_IDENTIFIER, json_object_new_string(keyData->keySetIdentifier));
            json_object_object_add(jKeyData, CONFIG_KEY_IS_MAC, json_object_new_boolean(keyData->isMac));
        }

        json_object_array_add(jKeyArrayObject, jKeyData);
    }
    json_object_object_add(jobj, CONFIG_KEY_KEYS, jKeyArrayObject);

    // Write the LED
    json_object *jLedConfigArrayObject = json_object_new_array();

    for (int i = 0; i < appConfig.ledLength; i++)
    {
        LEDDATA *ledData = appConfig.ledDataList[i];
        json_object *jLedData = json_object_new_object();
        json_object *jLedDataList = json_object_new_array();

        json_object_array_add(jLedDataList, json_object_new_string(ledData->midLogo));
        json_object_array_add(jLedDataList, json_object_new_string(ledData->led1));
        json_object_array_add(jLedDataList, json_object_new_string(ledData->led2));
        json_object_array_add(jLedDataList, json_object_new_string(ledData->led3));
        json_object_array_add(jLedDataList, json_object_new_string(ledData->led4));

        json_object_object_add(jLedData, ledData->stateName, jLedDataList);
        json_object_array_add(jLedConfigArrayObject, jLedData);
    }
    json_object_object_add(jobj, CONFIG_KEY_LED_CONFIGS, jLedConfigArrayObject);

    const char *jsonData = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);

    FILE *outputFile = fopen("config/app_config.json", "w");
    fprintf(outputFile, "%s\n", jsonData);
    fclose(outputFile);

    json_object_put(jobj);
    logInfo("Config is successfully saved");

    if (strlen(appConfig.terminalId) == 0 || strlen(appConfig.merchantId) == 0)
    {
        logInfo("Terminal id or Merchant id is empty");
        changeAppState(APP_STATUS_TID_MID_EMPTY);
    }
    else
    {
        if (appData.isKeyInjectionSuccess)
            changeAppState(APP_STATUS_READY);
        else
            changeAppState(APP_STATUS_KEY_MISSING);
    }
}

/**
 * Parse the received update config and update the local app configuration and file
 **/
int parseConfigAndUpdate(const char *data)
{
    logInfo("Going to parse the received config : %s", data);
    json_object *jObject = json_tokener_parse(data);

    json_object *jConfig = json_object_object_get(jObject, CONFIG_KEY);

    if (jConfig == NULL)
        return CONFIG_NOT_AVAILABLE;

    json_object *jGeneral = json_object_object_get(jConfig, CONFIG_KEY_GENERAL);

    if (jGeneral != NULL)
    {
        if (json_object_object_get(jGeneral, CONFIG_KEY_WAIT_WRITE_CARD) != NULL)
        {
            int writeWaitTime = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_WAIT_WRITE_CARD));
            logData("Updated Write Wait Time : %d", writeWaitTime);
            doLock();
            appConfig.writeCardWaitTimeMs = writeWaitTime;
            appData.writeCardWaitTimeMs = writeWaitTime;
            doUnLock();
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_KLD_IP) != NULL)
        {
            strcpy(appConfig.kldIP, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_KLD_IP)));
            logData("Updated KLD IP : %s", appConfig.kldIP);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_MAX_KEY_INJECTION_TRY) != NULL)
        {
            int maxKeyInjTry = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_MAX_KEY_INJECTION_TRY));
            logData("Updated Max Key Injection Try : %d", maxKeyInjTry);
            appConfig.maxKeyInjectionTry = maxKeyInjTry;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_KEYINJECT_RETRY_DELAY_SEC) != NULL)
        {
            int keyInjectDelay = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_KEYINJECT_RETRY_DELAY_SEC));
            logData("Updated Key Injection Retry Delay : %d", keyInjectDelay);
            appConfig.keyInjectRetryDelaySec = keyInjectDelay;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_PURCHASE_LIMIT) != NULL)
        {
            strcpy(appConfig.purchaseLimit, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_PURCHASE_LIMIT)));
            logData("Updated Purchase Limit : %s", appConfig.purchaseLimit);
            appConfig.purchaseLimitAmount = strtol(appConfig.purchaseLimit, NULL, 10);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_MONEY_ADD_LIMIT) != NULL)
        {
            strcpy(appConfig.moneyAddLimit, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_MONEY_ADD_LIMIT)));
            logData("Updated Money Add Limit : %s", appConfig.moneyAddLimit);
            appConfig.moneyAddLimitAmount = strtol(appConfig.moneyAddLimit, NULL, 10);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ZERO_VALUE_TXN) != NULL)
        {
            json_bool zeroValueTxn = json_object_get_boolean(json_object_object_get(jGeneral, CONFIG_KEY_ZERO_VALUE_TXN));

            if (zeroValueTxn == TRUE)
                appConfig.ignoreZeroValueTxn = true;
            else
                appConfig.ignoreZeroValueTxn = false;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_BEEP_ON_CARD_FOUND) != NULL)
        {
            json_bool beepCardFound = json_object_get_boolean(json_object_object_get(jGeneral, CONFIG_KEY_BEEP_ON_CARD_FOUND));

            if (beepCardFound == TRUE)
                appConfig.beepOnCardFound = true;
            else
                appConfig.beepOnCardFound = false;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ENABLE_APDU_LOG) != NULL)
        {
            json_bool enableApdu = json_object_get_boolean(json_object_object_get(jGeneral, CONFIG_KEY_ENABLE_APDU_LOG));

            if (enableApdu == TRUE)
                appConfig.enableApduLog = true;
            else
                appConfig.enableApduLog = false;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AUTO_READ_CARD) != NULL)
        {
            json_bool autoReadCard = json_object_get_boolean(json_object_object_get(jGeneral, CONFIG_KEY_AUTO_READ_CARD));

            if (autoReadCard == TRUE)
                appConfig.autoReadCard = true;
            else
                appConfig.autoReadCard = false;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_SOCKET_TIMEOUT) != NULL)
        {
            int timeout = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_SOCKET_TIMEOUT));
            logData("Updating timeout to %d", timeout);
            appConfig.socketTimeout = timeout;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_CODE) != NULL)
        {
            strcpy(appConfig.deviceCode, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_CODE)));
            logData("Updated Device Code : %s", appConfig.deviceCode);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_EQUIPMENT_TYPE) != NULL)
        {
            strcpy(appConfig.equipmentType, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_EQUIPMENT_TYPE)));
            logData("Updated equipment type : %s", appConfig.equipmentType);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_EQUIPMENT_CODE) != NULL)
        {
            strcpy(appConfig.equipmentCode, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_EQUIPMENT_CODE)));
            logData("Updated Equipment Code : %s", appConfig.equipmentCode);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_STATION_ID) != NULL)
        {
            strcpy(appConfig.stationId, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_STATION_ID)));
            logData("Updated Station Id : %s", appConfig.stationId);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_STATION_NAME) != NULL)
        {
            strcpy(appConfig.stationName, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_STATION_NAME)));
            logData("Updated Station Name : %s", appConfig.stationName);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_PAYTM_LOG_COUNT) != NULL)
        {
            int logCount = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_PAYTM_LOG_COUNT));
            logData("Updating Paytm log count to %d", logCount);
            appConfig.paytmLogCount = logCount;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_PAYTM_LOG_SIZE) != NULL)
        {
            int logSize = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_PAYTM_LOG_SIZE));
            logData("Updating Paytm log size to %d", logSize);
            appConfig.paytmMaxLogSizeKb = logSize;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_USE_CONFIG_JSON) != NULL)
        {
            json_bool useConfigJson = json_object_get_boolean(json_object_object_get(jGeneral, CONFIG_KEY_USE_CONFIG_JSON));

            if (useConfigJson == TRUE)
                appConfig.useConfigJson = true;
            else
                appConfig.useConfigJson = false;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ENABLE_ABT) != NULL)
        {
            json_bool enableAbt = json_object_get_boolean(json_object_object_get(jGeneral, CONFIG_KEY_ENABLE_ABT));

            if (enableAbt == TRUE)
                appConfig.enableAbt = true;
            else
                appConfig.enableAbt = false;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_TXN_TYPE_CODE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_TXN_TYPE_CODE));
            logData("Updating txnTypeCode to %d", val);
            appConfig.txnTypeCode = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_TYPE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_TYPE));
            logData("Updating deviceType to %d", val);
            appConfig.deviceType = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_LOCATION_CODE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_LOCATION_CODE));
            logData("Updating locationCode to %d", val);
            appConfig.locationCode = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_OPERATOR_CODE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_OPERATOR_CODE));
            logData("Updating operatorCode to %d", val);
            appConfig.operatorCode = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_TARIFF_VER) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_TARIFF_VER));
            logData("Updating tariffVersion to %d", val);
            appConfig.tariffVersion = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_MODE_CODE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_MODE_CODE));
            logData("Updating deviceModeCode to %d", val);
            appConfig.deviceModeCode = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_GATE_OPEN_WAIT) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_GATE_OPEN_WAIT));
            logData("Updating gateOpenWaitTimeInMs to %d", val);
            appConfig.gateOpenWaitTimeInMs = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_PORT) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_PORT));
            logData("Updating abtPort to %d", val);
            appConfig.abtPort = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_IP) != NULL)
        {
            strcpy(appConfig.abtIP, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_IP)));
            logData("Updated abtIP : %s", appConfig.abtIP);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_NAME) != NULL)
        {
            strcpy(appConfig.abtHostName, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_NAME)));
            logData("Updated abtHostName : %s", appConfig.abtHostName);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_TAP_URL) != NULL)
        {
            strcpy(appConfig.abtTapUrl, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ABT_TAP_URL)));
            logData("Updated abtTapUrl : %s", appConfig.abtTapUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_WAIT_TIME) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_WAIT_TIME));
            logData("Updating abtHostProcessWaitTimeInMinutes to %d", val);
            appConfig.abtHostProcessWaitTimeInMinutes = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_RETENTION_DAYS) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_ABT_RETENTION_DAYS));
            logData("Updating abtDataRetentionPeriodInDays to %d", val);
            appConfig.abtDataRetentionPeriodInDays = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_CLEANUP_TIME) != NULL)
        {
            strcpy(appConfig.abtCleanupTimeHHMM, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ABT_CLEANUP_TIME)));
            logData("Updated abtCleanupTimeHHMM : %s", appConfig.abtCleanupTimeHHMM);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_PUSH_BATCH_COUNT) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_PUSH_BATCH_COUNT));
            logData("Updating abtHostPushBatchCount to %d", val);
            appConfig.abtHostPushBatchCount = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_USE_AIRTEL_HOST) != NULL)
        {
            json_bool useAirtel = json_object_get_boolean(json_object_object_get(jGeneral, CONFIG_KEY_USE_AIRTEL_HOST));

            if (useAirtel == TRUE)
                appConfig.useAirtelHost = true;
            else
                appConfig.useAirtelHost = false;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HOST_IP) != NULL)
        {
            strcpy(appConfig.airtelHostIP, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HOST_IP)));
            logData("Updated airtelHostIP : %s", appConfig.airtelHostIP);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HOST_PORT) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HOST_PORT));
            logData("Updating airtelHostPort to %d", val);
            appConfig.airtelHostPort = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME) != NULL)
        {
            strcpy(appConfig.airtelHttpsHostName, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME)));
            logData("Updated airtelHttpsHostName : %s", appConfig.airtelHttpsHostName);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ARITEL_OFFLINE_URL) != NULL)
        {
            strcpy(appConfig.airtelOfflineUrl, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ARITEL_OFFLINE_URL)));
            logData("Updated airtelOfflineUrl : %s", appConfig.airtelOfflineUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL) != NULL)
        {
            strcpy(appConfig.airtelBalanceUpdateUrl, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL)));
            logData("Updated airtelBalanceUpdateUrl : %s", appConfig.airtelBalanceUpdateUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL) != NULL)
        {
            strcpy(appConfig.airtelHealthCheckUrl, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL)));
            logData("Updated airtelHealthCheckUrl : %s", appConfig.airtelHealthCheckUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL) != NULL)
        {
            strcpy(appConfig.airtelVerifyTerminalUrl, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL)));
            logData("Updated airtelVerifyTerminalUrl : %s", appConfig.airtelVerifyTerminalUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL) != NULL)
        {
            strcpy(appConfig.airtelReversalUrl, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_REVERSAL_URL)));
            logData("Updated airtelReversalUrl : %s", appConfig.airtelReversalUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_MONEY_ADD_URL) != NULL)
        {
            strcpy(appConfig.airtelMoneyAddUrl, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_MONEY_ADD_URL)));
            logData("Updated airtelMoneyAddUrl : %s", appConfig.airtelMoneyAddUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL) != NULL)
        {
            strcpy(appConfig.airtelServiceCreationUrl, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL)));
            logData("Updated airtelServiceCreationUrl : %s", appConfig.airtelServiceCreationUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_SIGN_SALT) != NULL)
        {
            strcpy(appConfig.airtelSignSalt, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_SIGN_SALT)));
            logData("Updated airtelSignSalt : %s", appConfig.airtelSignSalt);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_LATITUDE) != NULL)
        {
            strcpy(appConfig.latitude, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_LATITUDE)));
            logData("Updated latitude : %s", appConfig.latitude);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_LONGITUDE) != NULL)
        {
            strcpy(appConfig.longitude, (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_LONGITUDE)));
            logData("Updated longitude : %s", appConfig.latitude);
        }
    }

    json_object *jPsp = json_object_object_get(jConfig, CONFIG_KEY_PSP);
    if (jPsp != NULL)
    {
        if (json_object_object_get(jPsp, CONFIG_KEY_PORT) != NULL)
        {
            int port = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_PORT));
            logData("Updated Host Port : %d", port);
            appConfig.hostPort = port;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_TXNTIMEOUT) != NULL)
        {
            int txnTimeOut = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_TXNTIMEOUT));
            logData("Updated Host txnTimeOut : %d", txnTimeOut);
            appConfig.hostTxnTimeout = txnTimeOut;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_IP) != NULL)
        {
            strcpy(appConfig.hostIP, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_IP)));
            logData("Updated Host IP : %s", appConfig.hostIP);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_HTTPS_HOST_NAME) != NULL)
        {
            strcpy(appConfig.httpsHostName, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_HTTPS_HOST_NAME)));
            logData("Updated Https Host Name : %s", appConfig.httpsHostName);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_OFFLINE_URL) != NULL)
        {
            strcpy(appConfig.offlineUrl, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_OFFLINE_URL)));
            logData("Updated Offline Url : %s", appConfig.offlineUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_SERVRICE_CREATION_URL) != NULL)
        {
            strcpy(appConfig.serviceCreationUrl, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_SERVRICE_CREATION_URL)));
            logData("Updated Service Creation Url : %s", appConfig.serviceCreationUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MONEY_LOAD_URL) != NULL)
        {
            strcpy(appConfig.moneyLoadUrl, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_MONEY_LOAD_URL)));
            logData("Updated Money Load Url : %s", appConfig.moneyLoadUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_BALANCE_UPDATE_URL) != NULL)
        {
            strcpy(appConfig.balanceUpdateUrl, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_BALANCE_UPDATE_URL)));
            logData("Updated Balance Update Url : %s", appConfig.balanceUpdateUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_VERIFY_TERMINAL_URL) != NULL)
        {
            strcpy(appConfig.verifyTerminalUrl, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_VERIFY_TERMINAL_URL)));
            logData("Updated Verify Terminal Url : %s", appConfig.verifyTerminalUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_REVERSAL_URL) != NULL)
        {
            strcpy(appConfig.reversalUrl, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_REVERSAL_URL)));
            logData("Updated Reversal Url : %s", appConfig.reversalUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_TERMINALID) != NULL)
        {
            strcpy(appConfig.terminalId, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_TERMINALID)));
            logData("Updated Terminal Id : %s", appConfig.terminalId);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MERCHANT_ID) != NULL)
        {
            strcpy(appConfig.merchantId, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_MERCHANT_ID)));
            logData("Updated Merchant Id : %s", appConfig.merchantId);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_CLIENT_ID) != NULL)
        {
            strcpy(appConfig.clientId, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_CLIENT_ID)));
            logData("Updated Client Id : %s", appConfig.clientId);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_CLIENT_NAME) != NULL)
        {
            strcpy(appConfig.clientName, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_CLIENT_NAME)));
            logData("Updated Client Name : %s", appConfig.clientName);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_HOST_VERSION) != NULL)
        {
            strcpy(appConfig.hostVersion, (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_HOST_VERSION)));
            logData("Updated Host Version : %s", appConfig.hostVersion);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_HOST_PROCESS_TIMEOUT) != NULL)
        {
            int timeOut = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_HOST_PROCESS_TIMEOUT));
            logData("Updated Host Process Timeout : %d", timeOut);
            appConfig.hostProcessTimeInMinutes = timeOut;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_REVERSAL_PROCESS_TIMEOUT) != NULL)
        {
            int timeOut = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_REVERSAL_PROCESS_TIMEOUT));
            logData("Updated Reversal Process Timeout : %d", timeOut);
            appConfig.reversalTimeInMinutes = timeOut;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MAX_RETRY) != NULL)
        {
            int maxRetry = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_MAX_RETRY));
            logData("Updated Host Max Retry : %d", maxRetry);
            appConfig.hostMaxRetry = maxRetry;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MAX_OFFLINE_TXN) != NULL)
        {
            int maxOffline = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_MAX_OFFLINE_TXN));
            logData("Updated maxOfflineTransactions : %d", maxOffline);
            appConfig.maxOfflineTransactions = maxOffline;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MIN_REQUIRED_ONLINE) != NULL)
        {
            int minRequired = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_MIN_REQUIRED_ONLINE));
            logData("Updated minRequiredForOnline : %d", minRequired);
            appConfig.minRequiredForOnline = minRequired;
        }
    }

    readAndUpdateKeys(jConfig);
    readAndUpdateLedConfigs(jConfig);

    json_object_put(jObject); // Clear the json memory
    saveConfig();

    return CONFIG_WRITE_SUCCESS;
}

void readAndUpdateKeys(json_object *jConfig)
{
    if (json_object_object_get(jConfig, CONFIG_KEY_KEYS) != NULL)
    {
        json_object *keys = json_object_object_get(jConfig, CONFIG_KEY_KEYS);
        int keyLen = json_object_array_length(keys);
        logData("Length of keys : %d", keyLen);
        appConfig.keysLength = keyLen;

        appConfig.keyDataList = malloc(keyLen * sizeof(KEYDATA));

        json_object *keyValue;
        for (int i = 0; i < keyLen; i++)
        {
            appConfig.keyDataList[i] = malloc(sizeof(KEYDATA));
            keyValue = json_object_array_get_idx(keys, i);

            const char *label = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_LABEL));
            strcpy(appConfig.keyDataList[i]->label, label);

            int slot = json_object_get_int(json_object_object_get(keyValue, CONFIG_KEY_KEY_SLOT));
            appConfig.keyDataList[i]->slot = slot;

            int mkVersion = json_object_get_int(json_object_object_get(keyValue, CONFIG_KEY_KEY_MKVERSION));
            appConfig.keyDataList[i]->mkVersion = mkVersion;

            const char *astId = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_AST_ID));
            strcpy(appConfig.keyDataList[i]->astId, astId);

            const char *pkcsId = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_PKCS_ID));
            strcpy(appConfig.keyDataList[i]->pkcsId, pkcsId);

            const char *type = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_TYPE));
            strcpy(appConfig.keyDataList[i]->type, type);

            if (json_object_object_get(keyValue, CONFIG_KEY_IS_MAC) != NULL &&
                strcmp(type, KEY_TYPE_DUKPT_KEY) == 0)
            {
                json_bool keyIsMac = json_object_get_boolean(json_object_object_get(keyValue, CONFIG_KEY_IS_MAC));
                if (keyIsMac == TRUE)
                    appConfig.keyDataList[i]->isMac = true;
                else
                    appConfig.keyDataList[i]->isMac = false;
            }

            if (json_object_object_get(keyValue, CONFIG_KEY_KEY_KEY_SET_IDENTIFIER) != NULL &&
                strcmp(type, KEY_TYPE_DUKPT_KEY) == 0)
            {
                const char *keySet = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_KEY_SET_IDENTIFIER));
                strcpy(appConfig.keyDataList[i]->keySetIdentifier, keySet);
            }
            else
            {
                appConfig.keyDataList[i]->keySetIdentifier[0] = '\0';
            }
        }
    }
}

void readAndUpdateLedConfigs(json_object *jConfig)
{
    // Load the led data
    char stateNames[13][100] = {
        LED_ST_WAITING_KEY_INJECTION,
        LED_ST_KEY_INJECT,
        LED_ST_AWAITING_CARD_SUCCESS,
        LED_ST_AWAITING_CARD_FAILURE,
        LED_ST_CARD_READ_FAILED,
        LED_ST_CARD_PRESENTED,
        LED_ST_WRITE_SUCCESS,
        LED_ST_WRITE_FAILED,
        LED_ST_CARD_PROCESSED_SUCCESS,
        LED_ST_CARD_PROCESSED_FAILURE,
        LED_ST_CARD_PROCESSED_MSG_SENT,
        LED_ST_APP_STARTED,
        LED_ST_APP_EXITING};
    if (json_object_object_get(jConfig, CONFIG_KEY_LED_CONFIGS) != NULL)
    {
        json_object *ledList = json_object_object_get(jConfig, CONFIG_KEY_LED_CONFIGS);

        int ledLen = json_object_array_length(ledList);
        logData("Length of Led Configs : %d", ledLen);
        appConfig.ledLength = ledLen;

        appConfig.ledDataList = malloc(ledLen * sizeof(LEDDATA));

        json_object *ledConfig;
        json_object *ledValues;
        json_object *ledData;

        for (int i = 0; i < 13; i++)
        {
            appConfig.ledDataList[i] = malloc(sizeof(LEDDATA));
            strcpy(appConfig.ledDataList[i]->stateName, stateNames[i]);

            // Get the data eg { "waiting_key_injection": ["N", "N", "N", "N", "R"] },
            ledConfig = json_object_array_get_idx(ledList, i);

            // Get the values of led
            ledValues = json_object_object_get(ledConfig, stateNames[i]);

            ledData = json_object_array_get_idx(ledValues, 0);
            const char *m1 = json_object_get_string(ledData);
            strcpy(appConfig.ledDataList[i]->midLogo, m1);

            ledData = json_object_array_get_idx(ledValues, 1);
            const char *l1 = json_object_get_string(ledData);
            strcpy(appConfig.ledDataList[i]->led1, l1);

            ledData = json_object_array_get_idx(ledValues, 2);
            const char *l2 = json_object_get_string(ledData);
            strcpy(appConfig.ledDataList[i]->led2, l2);

            ledData = json_object_array_get_idx(ledValues, 3);
            const char *l3 = json_object_get_string(ledData);
            strcpy(appConfig.ledDataList[i]->led3, l3);

            ledData = json_object_array_get_idx(ledValues, 4);
            const char *l4 = json_object_get_string(ledData);
            strcpy(appConfig.ledDataList[i]->led4, l4);
        }
    }
}
