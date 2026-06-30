
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpay/tlv.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stddef.h>

#include "include/abtmanager.h"
#include "include/logutil.h"
#include "include/commonutil.h"
#include "include/responsemanager.h"
#include "include/appcodes.h"
#include "include/abtdbmanager.h"

extern struct applicationConfig appConfig;
extern struct applicationData appData;
extern struct transactionData currentTxnData;

extern int isGateOpenAvailable;
extern bool gateOpenStatus;
extern pthread_mutex_t lockGateOpen;
extern volatile __sig_atomic_t shutdown_requested;

/**
 * To handle the completion of the ABT
 */
void handleAbtCompletion(const void *outcome, size_t outcomeLen)
{
    logDataEx("ABT", "", "Handling ABT Complete");
    printCurrentTxnData(currentTxnData);
    appData.status = APP_STATUS_ABT_CARD_PRSENTED;
    sendAbtCardPresented(currentTxnData);
    bool gateStatus = checkAndGetGateOpen();
    if (gateStatus)
        safe_strcpy(currentTxnData.txnStatus, sizeof(currentTxnData.txnStatus), STATUS_SUCCESS);
    else
        safe_strcpy(currentTxnData.txnStatus, sizeof(currentTxnData.txnStatus), STATUS_FAILURE);
    logDataEx("ABT", "", "Gate status received : %d", gateStatus);
    currentTxnData.isGateOpen = gateStatus;
    createAbtTransactionData(&currentTxnData);
}

/**
 * To check whether we get the gate open from validator
 */
bool checkAndGetGateOpen()
{
    int msec = 0, trigger = appConfig.gateOpenWaitTimeInMs; // ms
    clock_t before = clock();
    int gateOpenCommand = 0;

    do
    {
        clock_t difference = clock() - before;
        msec = difference * 1000 / CLOCKS_PER_SEC;

        pthread_mutex_lock(&lockGateOpen);
        gateOpenCommand = isGateOpenAvailable;
        pthread_mutex_unlock(&lockGateOpen);

        if (gateOpenCommand == 1)
        {
            displayLight(LED_ST_WRITE_SUCCESS);
            logInfoEx("ABT", "", "Gate Open command received, so breaking the waiting");
            break;
        }

    } while (msec < trigger);

    if (gateOpenCommand == 1)
    {
        logDataEx("ABT", "", "Gate open command received with status : %d", gateOpenStatus);
        return gateOpenStatus;
    }

    logDataEx("ABT", "", "Gate open status not received so return false");

    return false;
}

/**
 * Initiate the abt pending / nok transactions from a thread
 **/
void *handleAbtTransactions()
{
    logDataEx("ABT", "", "ABT pending / nok transaction thread triggered");
    logDataEx("ABT", "", "Host process interval : %d", appConfig.abtHostProcessWaitTimeInMinutes);
    int waitTime = appConfig.abtHostProcessWaitTimeInMinutes * 60;
    int counter = 1;
    while (1)
    {
        for (int i = 0; i < waitTime; i++)
        {
            if (shutdown_requested)
            {
                logDataEx("ABT", "", "Shutting down the abt thread");
                return NULL;
            }
            sleep(1);
        }

        logDataEx("ABT", "", "Initiating the abt pending transaction, counter : %d", counter);
        processAbtPendingTransactions();
        logDataEx("ABT", "", "Abt pending transaction process completed. Now sleeping for %d seconds.", waitTime);
        counter++;
    }
}

/**
 * To delete the ABT transactions at a specified time for house keeping
 * Only the OK Transactions will be deleted
 **/
void *houseKeepingAbtTransactions()
{
    logDataEx("ABT", "", "ABT House keeping thread triggered");
    while (1)
    {
        if (shutdown_requested)
        {
            logErrorEx("ABT", "HouseKeeping", "Shutting down ABT house keeping thread");
            return NULL;
        }

        int target_hour, target_minute;
        sscanf(appConfig.abtCleanupTimeHHMM, "%d:%d", &target_hour, &target_minute);
        time_t t = time(NULL);
        struct tm *current_time = localtime(&t);
        int current_hour = current_time->tm_hour;
        int current_minute = current_time->tm_min;
        if (current_hour == target_hour && current_minute == target_minute)
        {
            logDataEx("ABT", "", "Initiating the abt house keeping transaction");
            deleteAbtTransactions();
            logDataEx("ABT", "", "Abt house keeping transaction process completed.");
            // Sleep for 23 hours in 1-second intervals
            for (int i = 0; i < 23 * 60 * 60; i++)
            {
                if (shutdown_requested)
                {
                    logErrorEx("ABT", "HouseKeeping", "Shutting down ABT house keeping thread during sleep");
                    return NULL;
                }
                sleep(1);
            }
        }

        // Short poll sleep — also interruptible
        for (int i = 0; i < 2; i++)
        {
            if (shutdown_requested)
            {
                logErrorEx("ABT", "HouseKeeping", "Shutting down ABT house keeping thread");
                return NULL;
            }
            sleep(1);
        }
    }
}
