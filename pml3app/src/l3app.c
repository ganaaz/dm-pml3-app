#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <log4c.h>
#include <time.h>
#include <signal.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

#include <feig/sdk.h>
#include <feig/fetpf.h>
#include <feig/leds.h>
#include <feig/buzzer.h>
#include <feig/fetrm.h>

#include "include/commonutil.h"
#include "include/config.h"
#include "include/pkcshelper.h"
#include "include/rupayservice.h"
#include "include/feigtransaction.h"
#include "include/feiginit.h"
#include "include/socketmanager.h"
#include "include/datasocketmanager.h"
#include "include/serialmanager.h"
#include "include/logutil.h"
#include "include/aztimer.h"
#include "include/hostmanager.h"
#include "include/commandmanager.h"
#include "include/appcodes.h"
#include "include/keymanager.h"
#include "include/emvconfig.h"
#include "JHost/jhost_interface.h"
#include "JHost/jhostutil.h"
#include "include/tlvhelper.h"
#include "include/test.h"
#include "include/abtdbmanager.h"

#if (CVEND_SDK_VERSION < 2010000)
#error "Please use firmware and SDK >= 02.01.00"
#endif

pthread_mutex_t lock;
pthread_mutex_t lockFeigTrx;
pthread_mutex_t lockRupayService;
pthread_mutex_t lockGateOpen;
pthread_t transactionThread;
pthread_t fetchDataThread;
pthread_t usbMessageThread;
pthread_t keyInjectionThread;
pthread_t hostOfflineThread;
pthread_t abtHostThread;
pthread_t abtHouseKeepingThread;
pthread_t reversalThread;

log4c_category_t *logCategory = NULL;
struct fetpf *fetpf = NULL;
int logPriority;
int activePendingTxnCount = 0;
enum device_status DEVICE_STATUS;

extern struct applicationData appData;
extern struct applicationConfig appConfig;
extern int IS_SERIAL_CONNECTED;
extern struct pkcs11 *crypto;

/**
 * Main entry point
 *      - Load Config
 *      - Check for Pending Reversal
 *      - Initialize reader and emv config
 *      - Inject Keys in a thread
 *      - Start the offline host process threaad
 *      - Start the main socket to listen for commands
 **/
int main(int argc, char **argv)
{
    leds_init();
    buzzer_init();

    ERR_load_BIO_strings();
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    IS_SERIAL_CONNECTED = -1;

    // Default to start with key missing
    appData.status = APP_STATUS_KEY_MISSING;

    resetSecondTap();

    if (log4c_init())
    {
        printf("Log4c init Failed.....\n");
        return EXIT_FAILURE;
    }

    logCategory = log4c_category_get("l3.app.file");
    logPriority = log4c_category_get_priority(logCategory);
    time_t t;
    time(&t);
    logInfo("Starting L3 Transit Application (PML3) : %s", ctime(&t));
    logInfo("Log4c initialized successfully and with priority %d", logPriority);

    appConfig.isDebugEnabled = 0;
    if (logPriority == LOG4C_PRIORITY_DEBUG)
    {
        appConfig.isDebugEnabled = 1;
    }

    signal(SIGINT, signalCallbackBandler);
    signal(SIGKILL, signalCallbackBandler);
    signal(SIGTERM, signalCallbackBandler);

    if (!isMinimumFirmwareInstalled())
    {
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        logError("Lock Init Failed");
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&lockFeigTrx, NULL) != 0)
    {
        logError("Lock Init lockFeigTrx Failed");
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&lockRupayService, NULL) != 0)
    {
        logError("Lock Init lockRupayService Failed");
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&lockGateOpen, NULL) != 0)
    {
        logError("Lock Init lockGateOpen Failed");
        return EXIT_FAILURE;
    }

    loadPayTmIndex();
    int result = initConfig();
    if (result != 0)
    {
        logError("Initialization failed.");
        return EXIT_FAILURE;
    }

    initTimeLogData();
    printConfig();

    displayLight(LED_ST_APP_STARTED);
    appData.status = APP_STATUS_INITIALIZE;
    checkRecordCount();

    activePendingTxnCount = getActivePendingTransactions();
    int failureTxn = getActivePendingHostErrorCategoryTransactions(HOST_ERROR_CATEGORY_FAILED);
    int timeoutTxn = getActivePendingHostErrorCategoryTransactions(HOST_ERROR_CATEGORY_TIMEOUT);
    int pendingTxn = activePendingTxnCount - (failureTxn + timeoutTxn);

    logInfo("Active Pending Transactions : %d", activePendingTxnCount);
    logInfo("Pending with Failure : %d", failureTxn);
    logInfo("Pending with Timeout : %d", timeoutTxn);
    logInfo("Pending with not yet sent : %d", pendingTxn);

    if (activePendingTxnCount == -1)
    {
        logError("Unable to get the active pending transactions.");
        return EXIT_FAILURE;
    }

    if (activePendingTxnCount > appConfig.maxOfflineTransactions)
        DEVICE_STATUS = STATUS_OFFLINE;
    else
        DEVICE_STATUS = STATUS_ONLINE;

    printDeviceStatus();

    int rc = initializeFeigReader(&fetpf);

    if (rc != 0)
    {
        logError("Feig initialization failed.");
        return EXIT_FAILURE;
    }

    appData.isKeyInjectionSuccess = false;
    if (appConfig.forceKeyInjection)
    {
        logWarn("Starting key injection process as a thread in background");
        displayLight(LED_ST_WAITING_KEY_INJECTION);
        pthread_create(&keyInjectionThread, NULL, processKeyInjection, NULL);
    }
    else
    {
        logWarn("Key injection skipped as per config, so making key injection as success");
        appData.isKeyInjectionSuccess = true;
    }

    if (appConfig.useConfigJson)
    {
        logInfo("Going to read the emv config json file and parse it");
        struct tlv *tlvConfig = parseEmvConfigFile();
        void *dataConfig = NULL;
        size_t dataConfigLen = 0;
        serializeTlv(tlvConfig, &dataConfig, &dataConfigLen);
        logInfo("File read and parsed");

        logInfo("Going to configure Feig");
        rc = fetpf_ep_configure(fetpf, dataConfig, dataConfigLen);
        if (rc != FETPF_RC_OK)
        {
            changeAppState(APP_STATUS_READY);
            logError("fetpf_ep_configure failed (rc: %d)\n", rc);
            return EXIT_FAILURE;
        }
        logInfo("fetpf_ep_configure success");
        free(dataConfig);
        free(tlvConfig);
    }
    else
    {
        logInfo("Going to read config file : %s, and configure kernel.", appConfig.emvConfigFile);
        size_t configLen;
        void *config = NULL;
        rc = readEmvConfig(appConfig.emvConfigFile, &config, &configLen);
        if (rc)
        {
            changeAppState(APP_STATUS_READY);
            logError("config failed (rc: %d)\n", rc);
            return EXIT_FAILURE;
        }
        logInfo("EMV Config read successfully.");

        logInfo("Going to configure Feig");
        rc = fetpf_ep_configure(fetpf, config, configLen);
        if (rc != FETPF_RC_OK)
        {
            changeAppState(APP_STATUS_READY);
            logError("fetpf_ep_configure failed (rc: %d)\n", rc);
            return EXIT_FAILURE;
        }
        logInfo("fetpf_ep_configure success");
        free(config);
    }

    changeAppState(APP_STATUS_READY);
    if (!appData.isKeyInjectionSuccess)
        changeAppState(APP_STATUS_KEY_MISSING);

    if (strlen(appConfig.terminalId) == 0 || strlen(appConfig.merchantId) == 0)
    {
        logInfo("Terminal id or Merchant id is empty");
        changeAppState(APP_STATUS_TID_MID_EMPTY);
    }

    // Create the transaction thread
    pthread_create(&transactionThread, NULL, processTransaction, NULL);
    pthread_create(&hostOfflineThread, NULL, handleHostOfflineTransactions, NULL);
    pthread_create(&reversalThread, NULL, startReversalThread, NULL);
    pthread_create(&abtHostThread, NULL, handleAbtTransactions, NULL);
    pthread_create(&abtHouseKeepingThread, NULL, houseKeepingAbtTransactions, NULL);

    // Create the data socket server for fetch info
    pthread_create(&fetchDataThread, NULL, createAndListenForFetchData, NULL);

    // Create the serial usb server for messages
    pthread_create(&usbMessageThread, NULL, createAndListenForUSB, NULL);

    // Run the main socket
    createAndListenServer();

    finalizeTimer();
    log4c_fini();

    return 0;
}