#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include <json-c/json.h>

/**
 * Data structure for storing the key info from config
 **/
typedef struct KEY_DATA
{
    char label[100];
    int slot;
    int mkVersion;
    char astId[100];
    char pkcsId[100];
    char type[50];
    char keySetIdentifier[50];
    bool isMac;

} KEYDATA;

typedef struct LED_DATA
{
    char stateName[100];
    char midLogo[2];
    char led1[2];
    char led2[2];
    char led3[2];
    char led4[2];

} LEDDATA;

enum device_status
{
    STATUS_ONLINE = 0,
    STATUS_OFFLINE = 1
};

/**
 * Config structure that is mapped to the  app_config.json
 **/
struct applicationConfig
{
    char name[30];
    char version[10];
    char releaseDate[15];
    int isDebugEnabled; // Used for local debug printf statements
    char currencyCode[6];
    char emvConfigFile[50];
    char PRMAcqKeyIndex[2];
    char terminalId[9];
    char merchantId[25];
    char clientId[30];
    char clientName[50];
    char hostVersion[10];
    char nii[4];
    char tpdu[11];
    char hostIP[100];
    int hostPort;
    char httpsHostName[100];
    bool useISOHost;
    char offlineUrl[100];
    char serviceCreationUrl[100];
    char balanceUpdateUrl[100];
    char moneyLoadUrl[100];
    char verifyTerminalUrl[100];
    char reversalUrl[100];
    int hostTxnTimeout;
    int searchTimeout;
    int writeCardWaitTimeMs;
    int hostMaxRetry;
    int hostProcessTimeInMinutes;
    int reversalTimeInMinutes;
    int maxOfflineTransactions;
    int minRequiredForOnline;
    int maxKeyInjectionTry;
    int keyInjectRetryDelaySec;
    bool forceKeyInjection;
    char kldIP[16];
    bool ignoreZeroValueTxn;
    bool beepOnCardFound;
    char purchaseLimit[13];
    char moneyAddLimit[13];
    bool printProcessOutcome;
    bool enableApduLog;
    int socketTimeout;
    bool autoReadCard;
    char deviceCode[50];
    char equipmentType[50];
    char equipmentCode[50];
    char stationId[20];
    char stationName[300];
    int paytmLogCount;
    int paytmMaxLogSizeKb;
    bool useConfigJson;
    int txnTypeCode;
    int deviceType;
    int locationCode;
    int operatorCode;
    int tariffVersion;
    int deviceModeCode;
    int gateOpenWaitTimeInMs;
    bool enableAbt;
    char abtIP[100];
    char abtHostName[100];
    int abtPort;
    char abtTapUrl[100];
    int abtHostProcessWaitTimeInMinutes;
    int abtDataRetentionPeriodInDays;
    char abtCleanupTimeHHMM[50];
    int abtHostPushBatchCount;
    bool useAirtelHost;
    char airtelHostIP[100];
    int airtelHostPort;
    char airtelHttpsHostName[100];
    char airtelOfflineUrl[200];
    char airtelBalanceUpdateUrl[200];
    char airtelVerifyTerminalUrl[200];
    char airtelReversalUrl[200];
    char airtelMoneyAddUrl[200];
    char airtelServiceCreationUrl[200];
    char airtelHealthCheckUrl[200];
    char airtelSignSalt[100];
    char latitude[50];
    char longitude[50];

    uint64_t purchaseLimitAmount; // Converted value
    uint64_t moneyAddLimitAmount; // Converted value

    // uint64_t contactlessLimit; // Dyamically loaded from EMV
    // uint64_t cvmLimit; // Dyamically loaded from EMV
    // uint64_t floorLimit; // dynamically loaded from EMV

    bool isTimingEnabled;
    bool isPrintDetailTimingLogs;

    int keysLength;
    KEYDATA **keyDataList;

    int ledLength;
    LEDDATA **ledDataList;
};

/**
 * Transaction data structure to hold the transaction information
 * This data is reset on every transaction, this is in memory object
 **/
struct transactionData
{
    char transactionId[38];
    char trxType[50];
    uint8_t trxTypeBin;
    char processingCode[7];
    long txnCounter;
    char stan[7];
    uint64_t amount;
    char sAmount[13];
    char time[7];
    char date[5];
    char year[5];
    char aid[20];
    char maskPan[21];
    char token[65];
    char effectiveDate[11];
    char serviceId[5];
    char serviceIndex[3];
    char serviceData[193];
    char serviceControl[5];
    char serviceBalance[13];
    char iccData[1024 * 4];
    int iccDataLen;
    char ksn[21];
    char track2Enc[97];
    char panEncrypted[49];
    char expDateEnc[17];
    char macKsn[21];
    char mac[17];
    char plainPan[22];
    char plainExpDate[8];
    char plainTrack2[40];
    char orderId[30];
    char txnStatus[20];
    bool isImmedOnline;
    bool isServiceCreation;
    char updatedAmount[13];
    char updatedBalance[13];
    char hostResponseCode[3]; // This is host result code in trx table
    char moneyAddTrxType[20];
    char sourceTxnId[50];
    char checkDate[11];
    int checkDateResult;
    char trxIssueDetail[100];
    bool cardPresentedSent;
    bool isRupayTxn; // If true its a rupay service based or else its ABT
    bool isGateOpen;
    char gmtTime[100];

    char acqTransactionId[13];
    char acqUniqueTransactionId[21];
};

/**
 * Application Status that are maintained and returned to the client
 **/
enum app_status
{
    APP_STATUS_INITIALIZE = 0,
    APP_STATUS_READY = 1,
    APP_STATUS_AWAIT_CARD = 2,
    APP_STATUS_CARD_PRESENTED = 3,
    APP_STATUS_STOPPING_SEARCH = 4,
    APP_STATUS_KEY_MISSING = 5,
    APP_STATUS_TID_MID_EMPTY = 6,
    APP_STATUS_RECALC_MAC = 7,
    APP_STATUS_ABT_CARD_PRSENTED = 8,
    APP_STATUS_ERROR = -1
};

enum fetch_mode
{
    FETCH_MODE_SUCCESS = 0,
    FETCH_MODE_FAILURE = 1,
    FETCH_MODE_PENDING = 2
};

enum host_trx_type
{
    BALANCE_UPDATE = 0,
    SERVICE_CREATION = 1,
    MONEY_ADD = 2,
    OFFLINE_SALE = 3
};

/**
 * Different search mode that are available
 **/
enum search_mode
{
    SEARCH_MODE_SINGLE = 0,
    SEARCH_MODE_LOOP = 1
};

/**
 * In memory application data
 **/
struct applicationData
{
    enum app_status status;
    enum search_mode searchMode;
    uint16_t currCodeBin;
    uint8_t currencyCode[2];
    uint8_t serviceBalanceLimit[6];
    bool isKeyInjectionSuccess;

    uint8_t trxTypeBin;
    uint8_t PRMAcqKeyIndex[1];
    uint64_t amountAuthorizedBin;
    int amountKnownAtStart;
    int searchTimeout;
    int writeCardWaitTimeMs;
    char trxType[50];
    char moneyAddTrxType[20];
    char sourceTxnId[50];
    char createServiceId[10];
    int isServiceBlock;
    bool isCheckDateAvailable; // if true then perform date check
    char checkDate[11];        // For date checking

    long transactionCounter; // This is read from file at start
    long batchCounter;       // This is read from file at start
};

/**
 * To initialize the configuration data
 **/
int initConfig();

/**
 * Print the current loaded config
 **/
void printConfig();

/**
 * To print the device status
 **/
void printDeviceStatus();

/**
 * Reset the data of second tap, so that normal transaction can be performed
 **/
void resetSecondTap();

/**
 * Set that next transaction is going to be a second tap
 **/
void setSecondTap();

/**
 * Get whether its a second tap
 */
bool checkIsInSecondTap();

/**
 * Get the status a string data
 **/
char *getStatusString();

/**
 * Initializes the transaction table
 **/
void initTransactionTable();

/**
 * Load the app config from the configuration file
 **/
void loadAppConfig();

/**
 * load the app data config that has the counters
 **/
void loadAppDataConfig();

/**
 * Save the updated config in file system
 **/
void saveConfig();

/**
 * Write the app data back to the file system
 **/
void writeAppData();

/**
 * Parse the json config data that is requested and update in the local config data
 **/
int parseConfigAndUpdate(const char *data);

/**
 * To get the dukpt key that is present in the config file
 **/
KEYDATA *getDukptKey();

/**
 * To read and update led config data from json
 **/
void readAndUpdateLedConfigs(json_object *jObject);

/*
 * To read and update keys data from json
 **/
void readAndUpdateKeys(json_object *jObject);

/**
 * Get the mac key
 **/
KEYDATA *getMacKey();

#endif