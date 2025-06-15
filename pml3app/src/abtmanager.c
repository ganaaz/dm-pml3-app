
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

/**
 * To handle the completion of the ABT
 */
void handleAbtCompletion(const void *outcome, size_t outcomeLen)
{
    logData("Handling ABT Complete");
    printCurrentTxnData(currentTxnData);
    appData.status = APP_STATUS_ABT_CARD_PRSENTED;
    sendAbtCardPresented(currentTxnData);
    bool gateStatus = checkAndGetGateOpen();
    if (gateStatus)
        strcpy(currentTxnData.txnStatus, STATUS_SUCCESS);
    else
        strcpy(currentTxnData.txnStatus, STATUS_FAILURE);
    logData("Gate status received : %d", gateStatus);
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
            logInfo("Gate Open command received, so breaking the waiting");
            break;
        }

    } while (msec < trigger);

    if (gateOpenCommand == 1)
    {
        logData("Gate open command received with status : %d", gateOpenStatus);
        return gateOpenStatus;
    }

    logData("Gate open status not received so return false");

    return false;
}

/**
 * Initiate the abt pending / nok transactions from a thread
 **/
void *handleAbtTransactions()
{
    logData("ABT pending / nok transaction thread triggered");
    logData("Host process interval : %d", appConfig.abtHostProcessWaitTimeInMinutes);
    int waitTime = appConfig.abtHostProcessWaitTimeInMinutes * 60;
    int counter = 1;
    while (1)
    {
        if (counter != 1)
            sleep(waitTime);

        logData("Initiating the abt pending transaction, counter : %d", counter);
        processAbtPendingTransactions();
        logData("Abt pending transaction process completed. Now sleeping for %d seconds.", waitTime);
        counter++;
    }
}

/**
 * To delete the ABT transactions at a specified time for house keeping
 * Only the OK Transactions will be deleted
 **/
void *houseKeepingAbtTransactions()
{
    logData("ABT House keeping thread triggered");
    while (1)
    {
        int target_hour, target_minute;
        sscanf(appConfig.abtCleanupTimeHHMM, "%d:%d", &target_hour, &target_minute);
        time_t t = time(NULL);
        struct tm *current_time = localtime(&t);
        int current_hour = current_time->tm_hour;
        int current_minute = current_time->tm_min;
        if (current_hour == target_hour && current_minute == target_minute)
        {
            logData("Initiating the abt house keeping transaction");
            deleteAbtTransactions();
            logData("Abt house keeping transaction process completed.");
            sleep(23 * 60 * 60); // Sleep for 23 hours
        }
        sleep(2);
    }
}
