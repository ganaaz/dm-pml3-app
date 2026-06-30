#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <log4c.h>
#include <time.h>
#include <signal.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <unistd.h>

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
#include "include/hostmanager.h"
#include "include/commandmanager.h"
#include "include/appcodes.h"
#include "include/keymanager.h"
#include "include/emvconfig.h"
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
pthread_t mainAppThread;

log4c_category_t *logCategory = NULL;
struct fetpf *fetpf = NULL;
int logPriority;
_Atomic int activePendingTxnCount = 0;
_Atomic enum device_status DEVICE_STATUS;

extern struct applicationData appData;
extern struct applicationConfig appConfig;
extern int IS_SERIAL_CONNECTED;
extern struct pkcs11 *crypto;
extern volatile __sig_atomic_t shutdown_requested;

/**
 * Main entry point
 *      - Load Config
 *      - Check for Pending Reversal
 *      - Initialize reader and emv config
 *      - Inject Keys in a thread
 *      - Start the offline host process threaad
 *      - Start the main socket to listen for commands
 **/
int main(void)
{
    leds_init();
    buzzer_init();

    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                         OPENSSL_INIT_LOAD_CRYPTO_STRINGS |
                         OPENSSL_INIT_ADD_ALL_CIPHERS |
                         OPENSSL_INIT_ADD_ALL_DIGESTS,
                     NULL);

    IS_SERIAL_CONNECTED = -1;

    // Default to start with key missing
    appData.status = APP_STATUS_KEY_MISSING;

    resetSecondTap();

    registerMessageLayout();
    registerPlainStdoutAppender();
    if (log4c_init())
    {
        printf("Log4c init Failed.....\n");
        return EXIT_FAILURE;
    }

    logCategory = log4c_category_get("l3.app.file");
    logPriority = log4c_category_get_priority(logCategory);
    time_t t;
    time(&t);
    char timeBuf[26];
    ctime_r(&t, timeBuf);
    timeBuf[strcspn(timeBuf, "\n")] = '\0';
    logInfoEx("L3-App", "", "Starting L3 Transit Application (PML3) : %s", timeBuf);
    logInfoEx("L3-App", "", "Log4c initialized successfully and with priority %d", logPriority);

    appConfig.isDebugEnabled = 0;
    if (logPriority == LOG4C_PRIORITY_DEBUG)
    {
        appConfig.isDebugEnabled = 1;
    }

    signal(SIGINT, signalCallbackHandler);
    signal(SIGTERM, signalCallbackHandler);

    if (!isMinimumFirmwareInstalled())
    {
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Lock Init Failed");
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&lockFeigTrx, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Lock Init lockFeigTrx Failed");
        pthread_mutex_destroy(&lock);
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&lockRupayService, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Lock Init lockRupayService Failed");
        pthread_mutex_destroy(&lockFeigTrx);
        pthread_mutex_destroy(&lock);
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&lockGateOpen, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Lock Init lockGateOpen Failed");
        pthread_mutex_destroy(&lockRupayService);
        pthread_mutex_destroy(&lockFeigTrx);
        pthread_mutex_destroy(&lock);
        return EXIT_FAILURE;
    }

    loadPayTmIndex();
    int result = initConfig();
    logDataEx("L3-App", "", "Result :: %d", result);
    if (result != 0)
    {
        logErrorEx("L3-App", "", "Initialization failed.");
        return EXIT_FAILURE;
    }
    logDataEx("L3-App", "", "init config done");

    initTimeLogData();
    printConfig();

    displayLight(LED_ST_APP_STARTED);
    appData.status = APP_STATUS_INITIALIZE;
    checkRecordCount();

    /* bitmask tracking which service threads were successfully created */
    int started = 0;

    activePendingTxnCount = getActivePendingTransactions();
    if (activePendingTxnCount == -1)
    {
        logErrorEx("L3-App", "", "Unable to get the active pending transactions.");
        goto cleanup;
    }

    int failureTxn = getActivePendingHostErrorCategoryTransactions(HOST_ERROR_CATEGORY_FAILED);
    int timeoutTxn = getActivePendingHostErrorCategoryTransactions(HOST_ERROR_CATEGORY_TIMEOUT);
    int pendingTxn = activePendingTxnCount - (failureTxn + timeoutTxn);

    logInfoEx("L3-App", "", "Active Pending Transactions : %d", activePendingTxnCount);
    logInfoEx("L3-App", "", "Pending with Failure : %d", failureTxn);
    logInfoEx("L3-App", "", "Pending with Timeout : %d", timeoutTxn);
    logInfoEx("L3-App", "", "Pending with not yet sent : %d", pendingTxn);

    if (activePendingTxnCount > appConfig.maxOfflineTransactions)
        DEVICE_STATUS = STATUS_OFFLINE;
    else
        DEVICE_STATUS = STATUS_ONLINE;

    printDeviceStatus();

    int rc = initializeFeigReader(&fetpf);

    if (rc != 0)
    {
        logErrorEx("L3-App", "", "Feig initialization failed.");
        goto cleanup;
    }

#define TH_KEY_INJECTION (1 << 8)

    appData.isKeyInjectionSuccess = false;
    if (appConfig.forceKeyInjection)
    {
        logWarnEx("L3-App", "", "Starting key injection process as a thread in background");
        displayLight(LED_ST_WAITING_KEY_INJECTION);
        if (pthread_create(&keyInjectionThread, NULL, processKeyInjection, NULL) != 0)
        {
            logErrorEx("L3-App", "", "Failed to create keyInjectionThread: %s", strerror(errno));
            goto cleanup;
        }
        started |= TH_KEY_INJECTION;
    }
    else
    {
        logWarnEx("L3-App", "", "Key injection skipped as per config, so making key injection as success");
        appData.isKeyInjectionSuccess = true;
    }

    if (appConfig.useConfigJson)
    {
        logInfoEx("L3-App", "", "Going to read the emv config json file and parse it");
        struct tlv *tlvConfig = parseEmvConfigFile();
        void *dataConfig = NULL;
        size_t dataConfigLen = 0;
        serializeTlv(tlvConfig, &dataConfig, &dataConfigLen);
        logInfoEx("L3-App", "", "File read and parsed");

        logInfoEx("L3-App", "", "Going to configure Feig");
        rc = fetpf_ep_configure(fetpf, dataConfig, dataConfigLen);
        if (rc != FETPF_RC_OK)
        {
            changeAppState(APP_STATUS_READY);
            logErrorEx("L3-App", "", "fetpf_ep_configure failed (rc: %d)\n", rc);
            free(dataConfig);
            tlv_free(tlvConfig);
            goto cleanup;
        }
        logInfoEx("L3-App", "", "fetpf_ep_configure success");
        free(dataConfig);
        tlv_free(tlvConfig);
    }
    else
    {
        logInfoEx("L3-App", "", "Going to read config file : %s, and configure kernel.", appConfig.emvConfigFile);
        size_t configLen;
        void *config = NULL;
        rc = readEmvConfig(appConfig.emvConfigFile, &config, &configLen);
        if (rc)
        {
            changeAppState(APP_STATUS_READY);
            logErrorEx("L3-App", "", "config failed (rc: %d)\n", rc);
            goto cleanup;
        }
        logInfoEx("L3-App", "", "EMV Config read successfully.");

        logInfoEx("L3-App", "", "Going to configure Feig");
        rc = fetpf_ep_configure(fetpf, config, configLen);
        if (rc != FETPF_RC_OK)
        {
            changeAppState(APP_STATUS_READY);
            logErrorEx("L3-App", "", "fetpf_ep_configure failed (rc: %d)\n", rc);
            free(config);
            goto cleanup;
        }
        logInfoEx("L3-App", "", "fetpf_ep_configure success");
        free(config);
    }

    changeAppState(APP_STATUS_READY);
    if (!appData.isKeyInjectionSuccess)
        changeAppState(APP_STATUS_KEY_MISSING);

    if (strlen(appConfig.terminalId) == 0 || strlen(appConfig.merchantId) == 0)
    {
        logInfoEx("L3-App", "", "Terminal id or Merchant id is empty");
        changeAppState(APP_STATUS_TID_MID_EMPTY);
    }

#define TH_TRANSACTION (1 << 0)
#define TH_HOST_OFFLINE (1 << 1)
#define TH_REVERSAL (1 << 2)
#define TH_ABT_HOST (1 << 3)
#define TH_ABT_HOUSEKEEP (1 << 4)
#define TH_FETCH_DATA (1 << 5)
#define TH_USB_MESSAGE (1 << 6)
#define TH_MAIN_APP (1 << 7)

    if (pthread_create(&transactionThread, NULL, processTransaction, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Failed to create transactionThread: %s", strerror(errno));
        goto cleanup;
    }
    started |= TH_TRANSACTION;

    if (pthread_create(&hostOfflineThread, NULL, handleHostOfflineTransactions, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Failed to create hostOfflineThread: %s", strerror(errno));
        goto cleanup;
    }
    started |= TH_HOST_OFFLINE;

    if (pthread_create(&reversalThread, NULL, startReversalThread, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Failed to create reversalThread: %s", strerror(errno));
        goto cleanup;
    }
    started |= TH_REVERSAL;

    if (pthread_create(&abtHostThread, NULL, handleAbtTransactions, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Failed to create abtHostThread: %s", strerror(errno));
        goto cleanup;
    }
    started |= TH_ABT_HOST;

    if (pthread_create(&abtHouseKeepingThread, NULL, houseKeepingAbtTransactions, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Failed to create abtHouseKeepingThread: %s", strerror(errno));
        goto cleanup;
    }
    started |= TH_ABT_HOUSEKEEP;

    if (pthread_create(&fetchDataThread, NULL, createAndListenForFetchData, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Failed to create fetchDataThread: %s", strerror(errno));
        goto cleanup;
    }
    started |= TH_FETCH_DATA;

    if (pthread_create(&usbMessageThread, NULL, createAndListenForUSB, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Failed to create usbMessageThread: %s", strerror(errno));
        goto cleanup;
    }
    started |= TH_USB_MESSAGE;

    if (pthread_create(&mainAppThread, NULL, createAndListenServer, NULL) != 0)
    {
        logErrorEx("L3-App", "", "Failed to create mainAppThread: %s", strerror(errno));
        goto cleanup;
    }
    started |= TH_MAIN_APP;

    // char deviceId[10];
    // getDeviceId(deviceId);
    // logInfoEx("L3-App", "", "Device Id : %s", deviceId);

    // char ip[16];
    // getLocalIP(ip, sizeof(ip));
    // logInfoEx("L3-App", "", "Local IP : %s", ip);

    while (!shutdown_requested)
    {
        sleep(1);
    }

    logErrorEx("L3-App", "", "Shutting down the application");

cleanup:
    if (started & TH_KEY_INJECTION)
    {
        pthread_cancel(keyInjectionThread);
        pthread_join(keyInjectionThread, NULL);
        logErrorEx("L3-App", "", "keyInjectionThread exited");
    }
    if (started & TH_HOST_OFFLINE)
    {
        pthread_cancel(hostOfflineThread);
        pthread_join(hostOfflineThread, NULL);
        logErrorEx("L3-App", "", "hostOfflineThread exited");
    }
    if (started & TH_REVERSAL)
    {
        pthread_cancel(reversalThread);
        pthread_join(reversalThread, NULL);
        logErrorEx("L3-App", "", "startReversalThread exited");
    }
    if (started & TH_ABT_HOST)
    {
        pthread_cancel(abtHostThread);
        pthread_join(abtHostThread, NULL);
        logErrorEx("L3-App", "", "abtHostThread exited");
    }
    if (started & TH_ABT_HOUSEKEEP)
    {
        pthread_cancel(abtHouseKeepingThread);
        pthread_join(abtHouseKeepingThread, NULL);
        logErrorEx("L3-App", "", "abtHouseKeepingThread exited");
    }
    if (started & TH_FETCH_DATA)
    {
        pthread_cancel(fetchDataThread);
        pthread_join(fetchDataThread, NULL);
        logErrorEx("L3-App", "", "fetchDataThread exited");
    }
    if (started & TH_USB_MESSAGE)
    {
        pthread_cancel(usbMessageThread);
        pthread_join(usbMessageThread, NULL);
        logErrorEx("L3-App", "", "usbMessageThread exited");
    }
    if (started & TH_MAIN_APP)
    {
        pthread_cancel(mainAppThread);
        pthread_join(mainAppThread, NULL);
        logErrorEx("L3-App", "", "server thread exited");
    }
    if (started & TH_TRANSACTION)
    {
        pthread_cancel(transactionThread);
        pthread_join(transactionThread, NULL);
        logErrorEx("L3-App", "", "transactionThread exited");
    }

    printDiskMemory();
    logErrorEx("L3-App", "", "Application shut down");
    log4c_fini();

    int expectedThreads = TH_TRANSACTION | TH_HOST_OFFLINE | TH_REVERSAL | TH_ABT_HOST |
                          TH_ABT_HOUSEKEEP | TH_FETCH_DATA | TH_USB_MESSAGE | TH_MAIN_APP;
    if (appConfig.forceKeyInjection)
        expectedThreads |= TH_KEY_INJECTION;

    return (started == expectedThreads) ? 0 : EXIT_FAILURE;
}