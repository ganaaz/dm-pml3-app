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

#define VERSION "1.0.7"
#define RELEASE_DATE "29-Jun-2026"

struct applicationConfig appConfig;
struct applicationData appData;
bool isSecondTap;
sqlite3 *sqlite3Db;

extern _Atomic enum device_status DEVICE_STATUS;

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
        logInfoEx("Config", "", "Device status is online");

    if (DEVICE_STATUS == STATUS_OFFLINE)
        logInfoEx("Config", "", "Device status is offline");
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

    int result = initializeHostStaticData();
    return result;
}

/**
 * Initialize the db tables
 **/
void initTransactionTable()
{
    if (sqlite3_open("trxdata-pml3.db", &sqlite3Db) != SQLITE_OK)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(sqlite3Db));
        exit(-1);
    }

    doVaccumTrxDb();

    logDataEx("L3-App", "Config", "Transactions database opened successfully");

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
                                   "HostError TEXT, "
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
                                   "AirtelAckRefundId TEXT, "
                                   "AcqTransactionId TEXT, "
                                   "AcqUniqueTransactionId TEXT, "
                                   "MoneyAddRRN TEXT, "
                                   "MoneyAddTid TEXT "
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

    logInfoEx("Config", "", "Table created / avaialble success");
    if (errormsg)
    {
        free(errormsg);
    }
}

void doVaccumTrxDb()
{
    int rc = sqlite3_exec(sqlite3Db, "VACUUM;", 0, 0, 0);
    if (rc != SQLITE_OK)
    {
        logErrorEx("ABT", "Vaccum", "Error in VACUUM for database: %s", sqlite3_errmsg(sqlite3Db));
    }
    else
    {
        logInfoEx("Config", "", "VACUUM operation successful for NCRTC Trx Database.");
    }
}

/**
 * Print the current config info
 **/
void printConfig()
{
    logWarnEx("L3-App", "", "Version : %s", appConfig.version);
    logWarnEx("L3-App", "", "Release Date : %s", appConfig.releaseDate);

    logInfoEx("Config", "", "============================================================");
    logInfoEx("Config", "", "                     Version Info                           ");
    logInfoEx("Config", "", "");
    logInfoEx("Config", "", "Name : %s", appConfig.name);
    logInfoEx("Config", "", "Version : %s", appConfig.version);
    logInfoEx("Config", "", "Release Date : %s", appConfig.releaseDate);
    logInfoEx("Config", "", "Currency Code : %s", appConfig.currencyCode);
    logInfoEx("Config", "", "EMV Config File : %s", appConfig.emvConfigFile);
    logInfoEx("Config", "", "Terminal Id : %s", appConfig.terminalId);
    logInfoEx("Config", "", "Merchant Id : %s", appConfig.merchantId);
    logInfoEx("Config", "", "Client Id : %s", appConfig.clientId);
    logInfoEx("Config", "", "Client Name : %s", appConfig.clientName);
    logInfoEx("Config", "", "Purchase Limit : %s", appConfig.purchaseLimit);
    uint64_t pAmount = strtol(appConfig.purchaseLimit, NULL, 10);
    logDataEx("L3-App", "Config", "Parsed Purchase Limit = %llu.%02llu", pAmount / 100, pAmount % 100);
    logInfoEx("Config", "", "Money Add Limit : %s", appConfig.moneyAddLimit);
    uint64_t mAmount = strtol(appConfig.moneyAddLimit, NULL, 10);
    logDataEx("L3-App", "Config", "Parsed Money Add Limit = %llu.%02llu", mAmount / 100, mAmount % 100);
    logInfoEx("Config", "", "Host Version : %s", appConfig.hostVersion);
    logInfoEx("Config", "", "Host IP : %s", appConfig.hostIP);
    logInfoEx("Config", "", "Use ISO Host : %s", appConfig.useISOHost == true ? "true" : "false");
    logInfoEx("Config", "", "NII : %s", appConfig.nii);
    logInfoEx("Config", "", "TPDU : %s", appConfig.tpdu);
    logInfoEx("Config", "", "Host Port : %d", appConfig.hostPort);
    logInfoEx("Config", "", "Https Host Name : %s", appConfig.httpsHostName);
    logInfoEx("Config", "", "Offline Url : %s", appConfig.offlineUrl);
    logInfoEx("Config", "", "Service Creation : %s", appConfig.serviceCreationUrl);
    logInfoEx("Config", "", "Money Load Url : %s", appConfig.moneyLoadUrl);
    logInfoEx("Config", "", "Balance Update Url : %s", appConfig.balanceUpdateUrl);
    logInfoEx("Config", "", "Verify Terminal Url : %s", appConfig.verifyTerminalUrl);
    logInfoEx("Config", "", "Reversal Url : %s", appConfig.reversalUrl);
    logInfoEx("Config", "", "Host Txn Timeout : %d", appConfig.hostTxnTimeout);
    logInfoEx("Config", "", "Host Max Retry : %d", appConfig.hostMaxRetry);
    logInfoEx("Config", "", "Host Process Thread Time : %d", appConfig.hostProcessTimeInMinutes);
    logInfoEx("Config", "", "Reversal Process Check Time : %d", appConfig.reversalTimeInMinutes);
    logInfoEx("Config", "", "Write Card Wait (ms) : %d", appConfig.writeCardWaitTimeMs);
    logInfoEx("Config", "", "Max Transactions Before Device goes offline : %d", appConfig.maxOfflineTransactions);
    logInfoEx("Config", "", "Min Txn for Device to be online : %d", appConfig.minRequiredForOnline);
    logInfoEx("Config", "", "Force Key Injection : %s", appConfig.forceKeyInjection ? "true" : "false");
    logInfoEx("Config", "", "Max Key Injection Try : %d", appConfig.maxKeyInjectionTry);
    logInfoEx("Config", "", "Key Injection Retry Delay Sec : %d", appConfig.keyInjectRetryDelaySec);
    logInfoEx("Config", "", "KLD IP : %s", appConfig.kldIP);
    logInfoEx("Config", "", "Ignore zero value transaction : %d", appConfig.ignoreZeroValueTxn);
    logInfoEx("Config", "", "Beep on card found : %s", appConfig.beepOnCardFound == true ? "true" : "false");
    logInfoEx("Config", "", "Print Process outcome : %s", appConfig.printProcessOutcome == true ? "true" : "false");
    logInfoEx("Config", "", "Enable Apdu Log : %s", appConfig.enableApduLog == true ? "true" : "false");
    logInfoEx("Config", "", "Socket timeout : %d", appConfig.socketTimeout);
    logInfoEx("Config", "", "Auto read card : %s", appConfig.autoReadCard == true ? "true" : "false");

    logInfoEx("Config", "", "Device Code : %s", appConfig.deviceCode);
    logInfoEx("Config", "", "Equipment Type : %s", appConfig.equipmentType);
    logInfoEx("Config", "", "Equipment Code : %s", appConfig.equipmentCode);
    logInfoEx("Config", "", "Station Id : %s", appConfig.stationId);
    logInfoEx("Config", "", "Station Name : %s", appConfig.stationName);

    logInfoEx("Config", "", "Enable Abt : %s", appConfig.enableAbt == true ? "true" : "false");
    logInfoEx("Config", "", "Txn type code : %d", appConfig.txnTypeCode);
    logInfoEx("Config", "", "Device Type : %d", appConfig.deviceType);
    logInfoEx("Config", "", "Location Code : %d", appConfig.locationCode);
    logInfoEx("Config", "", "Operator Code : %d", appConfig.operatorCode);
    logInfoEx("Config", "", "Tariff Version : %d", appConfig.tariffVersion);
    logInfoEx("Config", "", "Device Mode Code : %d", appConfig.deviceModeCode);
    logInfoEx("Config", "", "Gate Open Wait time in ms : %d", appConfig.gateOpenWaitTimeInMs);

    logInfoEx("Config", "", "ABT Host IP : %s", appConfig.abtIP);
    logInfoEx("Config", "", "ABT Host Name : %s", appConfig.abtHostName);
    logInfoEx("Config", "", "ABT Host Port : %d", appConfig.abtPort);
    logInfoEx("Config", "", "ABT Tap Url : %d", appConfig.abtTapUrl);
    logInfoEx("Config", "", "ABT Host Process Wait Time in Minutes : %d", appConfig.abtHostProcessWaitTimeInMinutes);
    logInfoEx("Config", "", "ABT Retention Period in days : %d", appConfig.abtDataRetentionPeriodInDays);
    logInfoEx("Config", "", "ABT daily cleanup time : %s", appConfig.abtCleanupTimeHHMM);
    logInfoEx("Config", "", "ABT Host Push Batch Count : %d", appConfig.abtHostPushBatchCount);

    logInfoEx("Config", "", "Paytm Max Log Count : %d", appConfig.paytmLogCount);
    logInfoEx("Config", "", "Paytm Max Log Size : %d", appConfig.paytmMaxLogSizeKb);
    logInfoEx("Config", "", "Use EMV Config json : %s", appConfig.useConfigJson == true ? "true" : "false");

    logInfoEx("Config", "", "Use Airtel Host : %s", appConfig.useAirtelHost == true ? "true" : "false");
    logInfoEx("Config", "", "Airtel Host IP : %s", appConfig.airtelHostIP);
    logInfoEx("Config", "", "Airtel Host Port : %d", appConfig.airtelHostPort);
    logInfoEx("Config", "", "Airtel Https Host Name : %s", appConfig.airtelHttpsHostName);
    logInfoEx("Config", "", "Airtel Offline url : %s", appConfig.airtelOfflineUrl);
    logInfoEx("Config", "", "Airtel Balance Update url : %s", appConfig.airtelBalanceUpdateUrl);
    logInfoEx("Config", "", "Airtel Money Add url : %s", appConfig.airtelMoneyAddUrl);
    logInfoEx("Config", "", "Airtel Sign Salt : %s", appConfig.airtelSignSalt);
    logInfoEx("Config", "", "Latitude : %s", appConfig.latitude);
    logInfoEx("Config", "", "Longitude : %s", appConfig.longitude);

    logInfoEx("Config", "", "Total keys in config : %d", appConfig.keysLength);

    for (int i = 0; i < appConfig.keysLength; i++)
    {
        logDataEx("L3-App", "Config", "------------------------------------------------------------");
        logDataEx("L3-App", "Config", "Key : %d", (i + 1));
        logDataEx("L3-App", "Config", "Label : %s", appConfig.keyDataList[i]->label);
        logDataEx("L3-App", "Config", "MK Label : %s", appConfig.keyDataList[i]->mkLabel);
        logDataEx("L3-App", "Config", "Slot : %d", appConfig.keyDataList[i]->slot);
        logDataEx("L3-App", "Config", "MK Version : %d", appConfig.keyDataList[i]->mkVersion);
        logDataEx("L3-App", "Config", "AST Id : %s", appConfig.keyDataList[i]->astId);
        logDataEx("L3-App", "Config", "PKCS Id : %s", appConfig.keyDataList[i]->pkcsId);
        logDataEx("L3-App", "Config", "Type : %s", appConfig.keyDataList[i]->type);
        logDataEx("L3-App", "Config", "KeySet Identifier : %s", appConfig.keyDataList[i]->keySetIdentifier);
        logDataEx("L3-App", "Config", "IsMac : %d", appConfig.keyDataList[i]->isMac);
    }

    logInfoEx("Config", "", "");

    logInfoEx("Config", "", "Total LED config length : %d", appConfig.ledLength);
    for (int i = 0; i < appConfig.ledLength; i++)
    {
        logDataEx("L3-App", "Config", "------------------------------------------------------------");
        logDataEx("L3-App", "Config", "State Name : %s", appConfig.ledDataList[i]->stateName);
        logDataEx("L3-App", "Config", "Mid Logo : %s", appConfig.ledDataList[i]->midLogo);
        logDataEx("L3-App", "Config", "LED 1 : %s", appConfig.ledDataList[i]->led1);
        logDataEx("L3-App", "Config", "LED 2 : %s", appConfig.ledDataList[i]->led2);
        logDataEx("L3-App", "Config", "LED 3 : %s", appConfig.ledDataList[i]->led3);
        logDataEx("L3-App", "Config", "LED 4 : %s", appConfig.ledDataList[i]->led4);

        // displayLight(appConfig.ledDataList[i]->stateName);
        // sleep(5);
    }

    logInfoEx("Config", "", "");
    logDataEx("L3-App", "Config", "App Data Config values : ");
    logInfoEx("Config", "", "Transaction Counter : %ld", appData.transactionCounter);
    logInfoEx("Config", "", "Batch Counter : %ld", appData.batchCounter);

    logInfoEx("Config", "", "============================================================");
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
    logInfoEx("Config", "", "Going to read the file");
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
    safe_strcpy(appConfig.version, sizeof(appConfig.version), VERSION);
    safe_strcpy(appConfig.releaseDate, sizeof(appConfig.releaseDate), RELEASE_DATE);

    safe_strcpy(appConfig.name, sizeof(appConfig.name), (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_NAME)));
    safe_strcpy(appConfig.currencyCode, sizeof(appConfig.currencyCode),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_CURR_CODE)));
    safe_strcpy(appConfig.emvConfigFile, sizeof(appConfig.emvConfigFile),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_EMV_FILE)));
    safe_strcpy(appConfig.terminalId, sizeof(appConfig.terminalId),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_TERMINALID)));
    safe_strcpy(appConfig.merchantId, sizeof(appConfig.merchantId),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_MERCHANT_ID)));
    safe_strcpy(appConfig.clientId, sizeof(appConfig.clientId),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_CLIENT_ID)));
    safe_strcpy(appConfig.clientName, sizeof(appConfig.clientName),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_CLIENT_NAME)));
    safe_strcpy(appConfig.hostVersion, sizeof(appConfig.hostVersion),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_HOST_VERSION)));
    safe_strcpy(appConfig.nii, sizeof(appConfig.nii),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_NII)));
    safe_strcpy(appConfig.tpdu, sizeof(appConfig.tpdu),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_TPDU)));
    safe_strcpy(appConfig.hostIP, sizeof(appConfig.hostIP),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_HOST_IP)));
    safe_strcpy(appConfig.httpsHostName, sizeof(appConfig.httpsHostName),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_HTTPS_HOST_NAME)));
    safe_strcpy(appConfig.offlineUrl, sizeof(appConfig.offlineUrl),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_OFFLINE_URL)));
    safe_strcpy(appConfig.serviceCreationUrl, sizeof(appConfig.serviceCreationUrl),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_SERVRICE_CREATION_URL)));
    safe_strcpy(appConfig.balanceUpdateUrl, sizeof(appConfig.balanceUpdateUrl),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_BALANCE_UPDATE_URL)));
    safe_strcpy(appConfig.moneyLoadUrl, sizeof(appConfig.moneyLoadUrl),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_MONEY_LOAD_URL)));
    safe_strcpy(appConfig.verifyTerminalUrl, sizeof(appConfig.verifyTerminalUrl),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_VERIFY_TERMINAL_URL)));
    safe_strcpy(appConfig.reversalUrl, sizeof(appConfig.reversalUrl),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_REVERSAL_URL)));
    safe_strcpy(appConfig.kldIP, sizeof(appConfig.kldIP),
                (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_KLD_IP)));
    appConfig.hostPort = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_HOST_PORT));
    appConfig.hostTxnTimeout = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_HOST_TXN_TIMEOUT));
    appConfig.writeCardWaitTimeMs = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_WRITE_CARD_WAIT));
    appConfig.hostMaxRetry = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_HOST_MAX_RETRY));
    appConfig.hostProcessTimeInMinutes = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_HOST_PROCESS_TIMEOUT));
    appConfig.maxKeyInjectionTry = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_MAX_KEY_INJECTION_TRY));
    appConfig.keyInjectRetryDelaySec = json_object_get_int(json_object_object_get(jObject, CONFIG_KEY_KEYINJECT_RETRY_DELAY_SEC));

    if (json_object_object_get(jObject, CONFIG_KEY_PURCHASE_LIMIT) != NULL)
    {
        safe_strcpy(appConfig.purchaseLimit, sizeof(appConfig.purchaseLimit),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_PURCHASE_LIMIT)));
    }
    else
    {
        safe_strcpy(appConfig.purchaseLimit, sizeof(appConfig.purchaseLimit), "20000");
    }
    if (json_object_object_get(jObject, CONFIG_KEY_MONEY_ADD_LIMIT) != NULL)
    {
        safe_strcpy(appConfig.moneyAddLimit, sizeof(appConfig.moneyAddLimit),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_MONEY_ADD_LIMIT)));
    }
    else
    {
        safe_strcpy(appConfig.moneyAddLimit, sizeof(appConfig.moneyAddLimit), "200000");
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

    if (json_object_object_get(jObject, CONFIG_KEY_USE_ISO_HOST) != NULL)
    {
        json_bool jUseIsoHost = json_object_get_boolean(json_object_object_get(jObject, CONFIG_KEY_USE_ISO_HOST));
        if (jUseIsoHost == TRUE)
            appConfig.useISOHost = true;
    }
    else
    {
        appConfig.useISOHost = true;
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
        safe_strcpy(appConfig.deviceCode, sizeof(appConfig.deviceCode),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_DEVICE_CODE)));
    }
    else
    {
        safe_strcpy(appConfig.deviceCode, sizeof(appConfig.deviceCode), "1234567890");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_EQUIPMENT_TYPE) != NULL)
    {
        safe_strcpy(appConfig.equipmentType, sizeof(appConfig.equipmentType),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_EQUIPMENT_TYPE)));
    }
    else
    {
        safe_strcpy(appConfig.equipmentType, sizeof(appConfig.equipmentType), "eqType");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_EQUIPMENT_CODE) != NULL)
    {
        safe_strcpy(appConfig.equipmentCode, sizeof(appConfig.equipmentCode),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_EQUIPMENT_CODE)));
    }
    else
    {
        safe_strcpy(appConfig.equipmentCode, sizeof(appConfig.equipmentCode), "eqCode");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_STATION_ID) != NULL)
    {
        safe_strcpy(appConfig.stationId, sizeof(appConfig.stationId),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_STATION_ID)));
    }
    else
    {
        safe_strcpy(appConfig.stationId, sizeof(appConfig.stationId), "123");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_STATION_NAME) != NULL)
    {
        safe_strcpy(appConfig.stationName, sizeof(appConfig.stationName),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_STATION_NAME)));
    }
    else
    {
        safe_strcpy(appConfig.stationName, sizeof(appConfig.stationName), "Station-Name-123");
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
        appConfig.useConfigJson = true;
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
        safe_strcpy(appConfig.abtHostName, sizeof(appConfig.abtHostName),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_NAME)));
    }
    else
    {
        safe_strcpy(appConfig.abtHostName, sizeof(appConfig.abtHostName), "dev-abt-etpass.datamatics.com");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_IP) != NULL)
    {
        safe_strcpy(appConfig.abtIP, sizeof(appConfig.abtIP),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ABT_HOST_IP)));
    }
    else
    {
        safe_strcpy(appConfig.abtIP, sizeof(appConfig.abtIP), "10.0.0.20");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ABT_TAP_URL) != NULL)
    {
        safe_strcpy(appConfig.abtTapUrl, sizeof(appConfig.abtTapUrl),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ABT_TAP_URL)));
    }
    else
    {
        safe_strcpy(appConfig.abtTapUrl, sizeof(appConfig.abtTapUrl), "/api/taps_receiver");
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
        safe_strcpy(appConfig.abtCleanupTimeHHMM, sizeof(appConfig.abtCleanupTimeHHMM),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ABT_CLEANUP_TIME)));
    }
    else
    {
        safe_strcpy(appConfig.abtCleanupTimeHHMM, sizeof(appConfig.abtCleanupTimeHHMM), "22:00");
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
        safe_strcpy(appConfig.airtelHostIP, sizeof(appConfig.airtelHostIP),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HOST_IP)));
    }
    else
    {
        safe_strcpy(appConfig.airtelHostIP, sizeof(appConfig.airtelHostIP), "182.79.196.24");
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
        safe_strcpy(appConfig.airtelHttpsHostName, sizeof(appConfig.airtelHttpsHostName),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME)));
    }
    else
    {
        safe_strcpy(appConfig.airtelHttpsHostName, sizeof(appConfig.airtelHttpsHostName), "apbsit.airtelbank.com");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_ARITEL_OFFLINE_URL) != NULL)
    {
        safe_strcpy(appConfig.airtelOfflineUrl, sizeof(appConfig.airtelOfflineUrl),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_ARITEL_OFFLINE_URL)));
    }
    else
    {
        safe_strcpy(appConfig.airtelOfflineUrl, sizeof(appConfig.airtelOfflineUrl),
                    "/payments-sit/transit/acq/transactions/batch-offline-sale");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL) != NULL)
    {
        safe_strcpy(appConfig.airtelBalanceUpdateUrl, sizeof(appConfig.airtelBalanceUpdateUrl),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL)));
    }
    else
    {
        safe_strcpy(appConfig.airtelBalanceUpdateUrl, sizeof(appConfig.airtelBalanceUpdateUrl),
                    "/payments-sit/transit/acq/transactions/balance-update");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL) != NULL)
    {
        safe_strcpy(appConfig.airtelHealthCheckUrl, sizeof(appConfig.airtelHealthCheckUrl),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL)));
    }
    else
    {
        safe_strcpy(appConfig.airtelHealthCheckUrl, sizeof(appConfig.airtelHealthCheckUrl),
                    "/payments-sit/transit/acq/terminals/health");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL) != NULL)
    {
        safe_strcpy(appConfig.airtelVerifyTerminalUrl, sizeof(appConfig.airtelVerifyTerminalUrl),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL)));
    }
    else
    {
        safe_strcpy(appConfig.airtelVerifyTerminalUrl, sizeof(appConfig.airtelVerifyTerminalUrl),
                    "/payments-sit/transit/acq/terminals/verify");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_REVERSAL_URL) != NULL)
    {
        safe_strcpy(appConfig.airtelReversalUrl, sizeof(appConfig.airtelReversalUrl),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_REVERSAL_URL)));
    }
    else
    {
        safe_strcpy(appConfig.airtelReversalUrl, sizeof(appConfig.airtelReversalUrl),
                    "/payments-sit/transit/acq/transactions/acknowledgement");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_MONEY_ADD_URL) != NULL)
    {
        safe_strcpy(appConfig.airtelMoneyAddUrl, sizeof(appConfig.airtelMoneyAddUrl),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_MONEY_ADD_URL)));
    }
    else
    {
        safe_strcpy(appConfig.airtelMoneyAddUrl, sizeof(appConfig.airtelMoneyAddUrl),
                    "/payments-sit/transit/acq/transactions/money-add");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL) != NULL)
    {
        safe_strcpy(appConfig.airtelServiceCreationUrl, sizeof(appConfig.airtelServiceCreationUrl),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL)));
    }
    else
    {
        safe_strcpy(appConfig.airtelServiceCreationUrl, sizeof(appConfig.airtelServiceCreationUrl),
                    "/payments-sit/transit/acq/transactions/service-creation");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_AIRTEL_SIGN_SALT) != NULL)
    {
        safe_strcpy(appConfig.airtelSignSalt, sizeof(appConfig.airtelSignSalt),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_AIRTEL_SIGN_SALT)));
    }
    else
    {
        safe_strcpy(appConfig.airtelSignSalt, sizeof(appConfig.airtelSignSalt), "secretSalt128");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_LATITUDE) != NULL)
    {
        safe_strcpy(appConfig.latitude, sizeof(appConfig.latitude),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_LATITUDE)));
    }
    else
    {
        safe_strcpy(appConfig.latitude, sizeof(appConfig.latitude), "37");
    }

    if (json_object_object_get(jObject, CONFIG_KEY_LONGITUDE) != NULL)
    {
        safe_strcpy(appConfig.longitude, sizeof(appConfig.longitude),
                    (char *)json_object_get_string(json_object_object_get(jObject, CONFIG_KEY_LONGITUDE)));
    }
    else
    {
        safe_strcpy(appConfig.longitude, sizeof(appConfig.longitude), "69");
    }

    readAndUpdateKeys(jObject);
    readAndUpdateLedConfigs(jObject);

    json_object_put(jObject); // Clear the json memory
    free(buffer);

    logInfoEx("Config", "", "Config file read and updated successfully.");

    saveConfig();
}

/**
 * Load the application data config that has the counters
 **/
void loadAppDataConfig()
{
    logInfoEx("Config", "", "Going to read the app data file");
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

    logInfoEx("Config", "", "Data file read and updated successfully.");
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
    logInfoEx("Config", "", "App Data is successfully saved with new counters");
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
    json_object_object_add(jobj, CONFIG_KEY_NII, json_object_new_string(appConfig.nii));
    json_object_object_add(jobj, CONFIG_KEY_TPDU, json_object_new_string(appConfig.tpdu));
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
    json_object_object_add(jobj, CONFIG_KEY_USE_ISO_HOST, json_object_new_boolean(appConfig.useISOHost));
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
        json_object_object_add(jKeyData, CONFIG_KEY_KEY_MKLABEL, json_object_new_string(keyData->mkLabel));
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
    logInfoEx("Config", "", "Config is successfully saved");

    if (strlen(appConfig.terminalId) == 0 || strlen(appConfig.merchantId) == 0)
    {
        logInfoEx("Config", "", "Terminal id or Merchant id is empty");
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
    logInfoEx("Config", "", "Going to parse the received config : %s", data);
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
            logDataEx("L3-App", "Config", "Updated Write Wait Time : %d", writeWaitTime);
            doLock();
            appConfig.writeCardWaitTimeMs = writeWaitTime;
            appData.writeCardWaitTimeMs = writeWaitTime;
            doUnLock();
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_KLD_IP) != NULL)
        {
            safe_strcpy(appConfig.kldIP, sizeof(appConfig.kldIP),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_KLD_IP)));
            logDataEx("L3-App", "Config", "Updated KLD IP : %s", appConfig.kldIP);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_MAX_KEY_INJECTION_TRY) != NULL)
        {
            int maxKeyInjTry = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_MAX_KEY_INJECTION_TRY));
            logDataEx("L3-App", "Config", "Updated Max Key Injection Try : %d", maxKeyInjTry);
            appConfig.maxKeyInjectionTry = maxKeyInjTry;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_KEYINJECT_RETRY_DELAY_SEC) != NULL)
        {
            int keyInjectDelay = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_KEYINJECT_RETRY_DELAY_SEC));
            logDataEx("L3-App", "Config", "Updated Key Injection Retry Delay : %d", keyInjectDelay);
            appConfig.keyInjectRetryDelaySec = keyInjectDelay;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_PURCHASE_LIMIT) != NULL)
        {
            safe_strcpy(appConfig.purchaseLimit, sizeof(appConfig.purchaseLimit),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_PURCHASE_LIMIT)));
            logDataEx("L3-App", "Config", "Updated Purchase Limit : %s", appConfig.purchaseLimit);
            appConfig.purchaseLimitAmount = strtol(appConfig.purchaseLimit, NULL, 10);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_MONEY_ADD_LIMIT) != NULL)
        {
            safe_strcpy(appConfig.moneyAddLimit, sizeof(appConfig.moneyAddLimit),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_MONEY_ADD_LIMIT)));
            logDataEx("L3-App", "Config", "Updated Money Add Limit : %s", appConfig.moneyAddLimit);
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
            logDataEx("L3-App", "Config", "Updating timeout to %d", timeout);
            appConfig.socketTimeout = timeout;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_CODE) != NULL)
        {
            safe_strcpy(appConfig.deviceCode, sizeof(appConfig.deviceCode),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_CODE)));
            logDataEx("L3-App", "Config", "Updated Device Code : %s", appConfig.deviceCode);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_EQUIPMENT_TYPE) != NULL)
        {
            safe_strcpy(appConfig.equipmentType, sizeof(appConfig.equipmentType),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_EQUIPMENT_TYPE)));
            logDataEx("L3-App", "Config", "Updated equipment type : %s", appConfig.equipmentType);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_EQUIPMENT_CODE) != NULL)
        {
            safe_strcpy(appConfig.equipmentCode, sizeof(appConfig.equipmentCode),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_EQUIPMENT_CODE)));
            logDataEx("L3-App", "Config", "Updated Equipment Code : %s", appConfig.equipmentCode);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_STATION_ID) != NULL)
        {
            safe_strcpy(appConfig.stationId, sizeof(appConfig.stationId),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_STATION_ID)));
            logDataEx("L3-App", "Config", "Updated Station Id : %s", appConfig.stationId);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_STATION_NAME) != NULL)
        {
            safe_strcpy(appConfig.stationName, sizeof(appConfig.stationName),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_STATION_NAME)));
            logDataEx("L3-App", "Config", "Updated Station Name : %s", appConfig.stationName);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_PAYTM_LOG_COUNT) != NULL)
        {
            int logCount = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_PAYTM_LOG_COUNT));
            logDataEx("L3-App", "Config", "Updating Paytm log count to %d", logCount);
            appConfig.paytmLogCount = logCount;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_PAYTM_LOG_SIZE) != NULL)
        {
            int logSize = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_PAYTM_LOG_SIZE));
            logDataEx("L3-App", "Config", "Updating Paytm log size to %d", logSize);
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
        else
        {
            appConfig.useConfigJson = true;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_USE_ISO_HOST) != NULL)
        {
            json_bool useIsoHost = json_object_get_boolean(json_object_object_get(jGeneral, CONFIG_KEY_USE_ISO_HOST));

            if (useIsoHost == TRUE)
                appConfig.useISOHost = true;
            else
                appConfig.useISOHost = false;
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
            logDataEx("L3-App", "Config", "Updating txnTypeCode to %d", val);
            appConfig.txnTypeCode = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_TYPE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_TYPE));
            logDataEx("L3-App", "Config", "Updating deviceType to %d", val);
            appConfig.deviceType = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_LOCATION_CODE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_LOCATION_CODE));
            logDataEx("L3-App", "Config", "Updating locationCode to %d", val);
            appConfig.locationCode = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_OPERATOR_CODE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_OPERATOR_CODE));
            logDataEx("L3-App", "Config", "Updating operatorCode to %d", val);
            appConfig.operatorCode = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_TARIFF_VER) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_TARIFF_VER));
            logDataEx("L3-App", "Config", "Updating tariffVersion to %d", val);
            appConfig.tariffVersion = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_MODE_CODE) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_DEVICE_MODE_CODE));
            logDataEx("L3-App", "Config", "Updating deviceModeCode to %d", val);
            appConfig.deviceModeCode = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_GATE_OPEN_WAIT) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_GATE_OPEN_WAIT));
            logDataEx("L3-App", "Config", "Updating gateOpenWaitTimeInMs to %d", val);
            appConfig.gateOpenWaitTimeInMs = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_PORT) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_PORT));
            logDataEx("L3-App", "Config", "Updating abtPort to %d", val);
            appConfig.abtPort = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_IP) != NULL)
        {
            safe_strcpy(appConfig.abtIP, sizeof(appConfig.abtIP),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_IP)));
            logDataEx("L3-App", "Config", "Updated abtIP : %s", appConfig.abtIP);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_NAME) != NULL)
        {
            safe_strcpy(appConfig.abtHostName, sizeof(appConfig.abtHostName),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_NAME)));
            logDataEx("L3-App", "Config", "Updated abtHostName : %s", appConfig.abtHostName);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_TAP_URL) != NULL)
        {
            safe_strcpy(appConfig.abtTapUrl, sizeof(appConfig.abtTapUrl),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ABT_TAP_URL)));
            logDataEx("L3-App", "Config", "Updated abtTapUrl : %s", appConfig.abtTapUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_WAIT_TIME) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_WAIT_TIME));
            logDataEx("L3-App", "Config", "Updating abtHostProcessWaitTimeInMinutes to %d", val);
            appConfig.abtHostProcessWaitTimeInMinutes = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_RETENTION_DAYS) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_ABT_RETENTION_DAYS));
            logDataEx("L3-App", "Config", "Updating abtDataRetentionPeriodInDays to %d", val);
            appConfig.abtDataRetentionPeriodInDays = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_CLEANUP_TIME) != NULL)
        {
            safe_strcpy(appConfig.abtCleanupTimeHHMM, sizeof(appConfig.abtCleanupTimeHHMM),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ABT_CLEANUP_TIME)));
            logDataEx("L3-App", "Config", "Updated abtCleanupTimeHHMM : %s", appConfig.abtCleanupTimeHHMM);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_PUSH_BATCH_COUNT) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_ABT_HOST_PUSH_BATCH_COUNT));
            logDataEx("L3-App", "Config", "Updating abtHostPushBatchCount to %d", val);
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
            safe_strcpy(appConfig.airtelHostIP, sizeof(appConfig.airtelHostIP),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HOST_IP)));
            logDataEx("L3-App", "Config", "Updated airtelHostIP : %s", appConfig.airtelHostIP);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HOST_PORT) != NULL)
        {
            int val = json_object_get_int(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HOST_PORT));
            logDataEx("L3-App", "Config", "Updating airtelHostPort to %d", val);
            appConfig.airtelHostPort = val;
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME) != NULL)
        {
            safe_strcpy(appConfig.airtelHttpsHostName, sizeof(appConfig.airtelHttpsHostName),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HTTPS_HOST_NAME)));
            logDataEx("L3-App", "Config", "Updated airtelHttpsHostName : %s", appConfig.airtelHttpsHostName);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_ARITEL_OFFLINE_URL) != NULL)
        {
            safe_strcpy(appConfig.airtelOfflineUrl, sizeof(appConfig.airtelOfflineUrl),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_ARITEL_OFFLINE_URL)));
            logDataEx("L3-App", "Config", "Updated airtelOfflineUrl : %s", appConfig.airtelOfflineUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL) != NULL)
        {
            safe_strcpy(appConfig.airtelBalanceUpdateUrl, sizeof(appConfig.airtelBalanceUpdateUrl),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL)));
            logDataEx("L3-App", "Config", "Updated airtelBalanceUpdateUrl : %s", appConfig.airtelBalanceUpdateUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL) != NULL)
        {
            safe_strcpy(appConfig.airtelHealthCheckUrl, sizeof(appConfig.airtelHealthCheckUrl),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_HEALTH_CHECK_URL)));
            logDataEx("L3-App", "Config", "Updated airtelHealthCheckUrl : %s", appConfig.airtelHealthCheckUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL) != NULL)
        {
            safe_strcpy(appConfig.airtelVerifyTerminalUrl, sizeof(appConfig.airtelVerifyTerminalUrl),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_VERIFY_TERMINAL_URL)));
            logDataEx("L3-App", "Config", "Updated airtelVerifyTerminalUrl : %s", appConfig.airtelVerifyTerminalUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_BALANCE_UPDATE_URL) != NULL)
        {
            safe_strcpy(appConfig.airtelReversalUrl, sizeof(appConfig.airtelReversalUrl),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_REVERSAL_URL)));
            logDataEx("L3-App", "Config", "Updated airtelReversalUrl : %s", appConfig.airtelReversalUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_MONEY_ADD_URL) != NULL)
        {
            safe_strcpy(appConfig.airtelMoneyAddUrl, sizeof(appConfig.airtelMoneyAddUrl),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_MONEY_ADD_URL)));
            logDataEx("L3-App", "Config", "Updated airtelMoneyAddUrl : %s", appConfig.airtelMoneyAddUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL) != NULL)
        {
            safe_strcpy(appConfig.airtelServiceCreationUrl, sizeof(appConfig.airtelServiceCreationUrl),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_SERVICE_CREATION_URL)));
            logDataEx("L3-App", "Config", "Updated airtelServiceCreationUrl : %s", appConfig.airtelServiceCreationUrl);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_SIGN_SALT) != NULL)
        {
            safe_strcpy(appConfig.airtelSignSalt, sizeof(appConfig.airtelSignSalt),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_AIRTEL_SIGN_SALT)));
            logDataEx("L3-App", "Config", "Updated airtelSignSalt : %s", appConfig.airtelSignSalt);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_LATITUDE) != NULL)
        {
            safe_strcpy(appConfig.latitude, sizeof(appConfig.latitude),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_LATITUDE)));
            logDataEx("L3-App", "Config", "Updated latitude : %s", appConfig.latitude);
        }

        if (json_object_object_get(jGeneral, CONFIG_KEY_LONGITUDE) != NULL)
        {
            safe_strcpy(appConfig.longitude, sizeof(appConfig.longitude),
                        (char *)json_object_get_string(json_object_object_get(jGeneral, CONFIG_KEY_LONGITUDE)));
            logDataEx("L3-App", "Config", "Updated longitude : %s", appConfig.latitude);
        }
    }

    json_object *jPsp = json_object_object_get(jConfig, CONFIG_KEY_PSP);
    if (jPsp != NULL)
    {
        if (json_object_object_get(jPsp, CONFIG_KEY_PORT) != NULL)
        {
            int port = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_PORT));
            logDataEx("L3-App", "Config", "Updated Host Port : %d", port);
            appConfig.hostPort = port;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_TXNTIMEOUT) != NULL)
        {
            int txnTimeOut = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_TXNTIMEOUT));
            logDataEx("L3-App", "Config", "Updated Host txnTimeOut : %d", txnTimeOut);
            appConfig.hostTxnTimeout = txnTimeOut;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_IP) != NULL)
        {
            safe_strcpy(appConfig.hostIP, sizeof(appConfig.hostIP),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_IP)));
            logDataEx("L3-App", "Config", "Updated Host IP : %s", appConfig.hostIP);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_NII) != NULL)
        {
            safe_strcpy(appConfig.nii, sizeof(appConfig.nii),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_NII)));
            logDataEx("L3-App", "Config", "Updated NII : %s", appConfig.nii);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_TPDU) != NULL)
        {
            safe_strcpy(appConfig.tpdu, sizeof(appConfig.tpdu),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_TPDU)));
            logDataEx("L3-App", "Config", "Updated TPDU : %s", appConfig.tpdu);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_HTTPS_HOST_NAME) != NULL)
        {
            safe_strcpy(appConfig.httpsHostName, sizeof(appConfig.httpsHostName),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_HTTPS_HOST_NAME)));
            logDataEx("L3-App", "Config", "Updated Https Host Name : %s", appConfig.httpsHostName);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_OFFLINE_URL) != NULL)
        {
            safe_strcpy(appConfig.offlineUrl, sizeof(appConfig.offlineUrl),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_OFFLINE_URL)));
            logDataEx("L3-App", "Config", "Updated Offline Url : %s", appConfig.offlineUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_SERVRICE_CREATION_URL) != NULL)
        {
            safe_strcpy(appConfig.serviceCreationUrl, sizeof(appConfig.serviceCreationUrl),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_SERVRICE_CREATION_URL)));
            logDataEx("L3-App", "Config", "Updated Service Creation Url : %s", appConfig.serviceCreationUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MONEY_LOAD_URL) != NULL)
        {
            safe_strcpy(appConfig.moneyLoadUrl, sizeof(appConfig.moneyLoadUrl),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_MONEY_LOAD_URL)));
            logDataEx("L3-App", "Config", "Updated Money Load Url : %s", appConfig.moneyLoadUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_BALANCE_UPDATE_URL) != NULL)
        {
            safe_strcpy(appConfig.balanceUpdateUrl, sizeof(appConfig.balanceUpdateUrl),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_BALANCE_UPDATE_URL)));
            logDataEx("L3-App", "Config", "Updated Balance Update Url : %s", appConfig.balanceUpdateUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_VERIFY_TERMINAL_URL) != NULL)
        {
            safe_strcpy(appConfig.verifyTerminalUrl, sizeof(appConfig.verifyTerminalUrl),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_VERIFY_TERMINAL_URL)));
            logDataEx("L3-App", "Config", "Updated Verify Terminal Url : %s", appConfig.verifyTerminalUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_REVERSAL_URL) != NULL)
        {
            safe_strcpy(appConfig.reversalUrl, sizeof(appConfig.reversalUrl),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_REVERSAL_URL)));
            logDataEx("L3-App", "Config", "Updated Reversal Url : %s", appConfig.reversalUrl);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_TERMINALID) != NULL)
        {
            safe_strcpy(appConfig.terminalId, sizeof(appConfig.terminalId),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_TERMINALID)));
            logDataEx("L3-App", "Config", "Updated Terminal Id : %s", appConfig.terminalId);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MERCHANT_ID) != NULL)
        {
            safe_strcpy(appConfig.merchantId, sizeof(appConfig.merchantId),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_MERCHANT_ID)));
            logDataEx("L3-App", "Config", "Updated Merchant Id : %s", appConfig.merchantId);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_CLIENT_ID) != NULL)
        {
            safe_strcpy(appConfig.clientId, sizeof(appConfig.clientId),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_CLIENT_ID)));
            logDataEx("L3-App", "Config", "Updated Client Id : %s", appConfig.clientId);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_CLIENT_NAME) != NULL)
        {
            safe_strcpy(appConfig.clientName, sizeof(appConfig.clientName),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_CLIENT_NAME)));
            logDataEx("L3-App", "Config", "Updated Client Name : %s", appConfig.clientName);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_HOST_VERSION) != NULL)
        {
            safe_strcpy(appConfig.hostVersion, sizeof(appConfig.hostVersion),
                        (char *)json_object_get_string(json_object_object_get(jPsp, CONFIG_KEY_HOST_VERSION)));
            logDataEx("L3-App", "Config", "Updated Host Version : %s", appConfig.hostVersion);
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_HOST_PROCESS_TIMEOUT) != NULL)
        {
            int timeOut = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_HOST_PROCESS_TIMEOUT));
            logDataEx("L3-App", "Config", "Updated Host Process Timeout : %d", timeOut);
            appConfig.hostProcessTimeInMinutes = timeOut;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_REVERSAL_PROCESS_TIMEOUT) != NULL)
        {
            int timeOut = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_REVERSAL_PROCESS_TIMEOUT));
            logDataEx("L3-App", "Config", "Updated Reversal Process Timeout : %d", timeOut);
            appConfig.reversalTimeInMinutes = timeOut;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MAX_RETRY) != NULL)
        {
            int maxRetry = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_MAX_RETRY));
            logDataEx("L3-App", "Config", "Updated Host Max Retry : %d", maxRetry);
            appConfig.hostMaxRetry = maxRetry;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MAX_OFFLINE_TXN) != NULL)
        {
            int maxOffline = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_MAX_OFFLINE_TXN));
            logDataEx("L3-App", "Config", "Updated maxOfflineTransactions : %d", maxOffline);
            appConfig.maxOfflineTransactions = maxOffline;
        }

        if (json_object_object_get(jPsp, CONFIG_KEY_MIN_REQUIRED_ONLINE) != NULL)
        {
            int minRequired = json_object_get_int(json_object_object_get(jPsp, CONFIG_KEY_MIN_REQUIRED_ONLINE));
            logDataEx("L3-App", "Config", "Updated minRequiredForOnline : %d", minRequired);
            appConfig.minRequiredForOnline = minRequired;
        }

        initializeHostStaticData();
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
        logDataEx("L3-App", "Config", "Length of keys : %d", keyLen);
        appConfig.keysLength = keyLen;

        appConfig.keyDataList = malloc(keyLen * sizeof(KEYDATA));

        json_object *keyValue;
        for (int i = 0; i < keyLen; i++)
        {
            appConfig.keyDataList[i] = malloc(sizeof(KEYDATA));
            keyValue = json_object_array_get_idx(keys, i);

            const char *label = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_LABEL));
            safe_strcpy(appConfig.keyDataList[i]->label, sizeof(appConfig.keyDataList[i]->label), label);

            if (json_object_object_get(keyValue, CONFIG_KEY_KEY_MKLABEL) != NULL)
            {
                const char *mkLabel = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_MKLABEL));
                safe_strcpy(appConfig.keyDataList[i]->mkLabel, sizeof(appConfig.keyDataList[i]->mkLabel), mkLabel);
            }
            else
            {
                strcpy(appConfig.keyDataList[i]->mkLabel, "");
            }

            int slot = json_object_get_int(json_object_object_get(keyValue, CONFIG_KEY_KEY_SLOT));
            appConfig.keyDataList[i]->slot = slot;

            int mkVersion = json_object_get_int(json_object_object_get(keyValue, CONFIG_KEY_KEY_MKVERSION));
            appConfig.keyDataList[i]->mkVersion = mkVersion;

            const char *astId = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_AST_ID));
            safe_strcpy(appConfig.keyDataList[i]->astId, sizeof(appConfig.keyDataList[i]->astId), astId);

            const char *pkcsId = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_PKCS_ID));
            safe_strcpy(appConfig.keyDataList[i]->pkcsId, sizeof(appConfig.keyDataList[i]->pkcsId), pkcsId);

            const char *type = json_object_get_string(json_object_object_get(keyValue, CONFIG_KEY_KEY_TYPE));
            safe_strcpy(appConfig.keyDataList[i]->type, sizeof(appConfig.keyDataList[i]->type), type);

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
                safe_strcpy(appConfig.keyDataList[i]->keySetIdentifier, sizeof(appConfig.keyDataList[i]->keySetIdentifier), keySet);
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
        logDataEx("L3-App", "Config", "Length of Led Configs : %d", ledLen);
        appConfig.ledLength = ledLen;

        appConfig.ledDataList = malloc(ledLen * sizeof(LEDDATA));

        json_object *ledConfig;
        json_object *ledValues;
        json_object *ledData;

        for (int i = 0; i < 13; i++)
        {
            appConfig.ledDataList[i] = malloc(sizeof(LEDDATA));
            safe_strcpy(appConfig.ledDataList[i]->stateName, sizeof(appConfig.ledDataList[i]->stateName), stateNames[i]);

            // Get the data eg { "waiting_key_injection": ["N", "N", "N", "N", "R"] },
            ledConfig = json_object_array_get_idx(ledList, i);

            // Get the values of led
            ledValues = json_object_object_get(ledConfig, stateNames[i]);

            ledData = json_object_array_get_idx(ledValues, 0);
            const char *m1 = json_object_get_string(ledData);
            safe_strcpy(appConfig.ledDataList[i]->midLogo, sizeof(appConfig.ledDataList[i]->midLogo), m1);

            ledData = json_object_array_get_idx(ledValues, 1);
            const char *l1 = json_object_get_string(ledData);
            safe_strcpy(appConfig.ledDataList[i]->led1, sizeof(appConfig.ledDataList[i]->led1), l1);

            ledData = json_object_array_get_idx(ledValues, 2);
            const char *l2 = json_object_get_string(ledData);
            safe_strcpy(appConfig.ledDataList[i]->led2, sizeof(appConfig.ledDataList[i]->led2), l2);

            ledData = json_object_array_get_idx(ledValues, 3);
            const char *l3 = json_object_get_string(ledData);
            safe_strcpy(appConfig.ledDataList[i]->led3, sizeof(appConfig.ledDataList[i]->led3), l3);

            ledData = json_object_array_get_idx(ledValues, 4);
            const char *l4 = json_object_get_string(ledData);
            safe_strcpy(appConfig.ledDataList[i]->led4, sizeof(appConfig.ledDataList[i]->led4), l4);
        }
    }
}
