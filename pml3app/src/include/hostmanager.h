#ifndef HOSTMANAGER_H
#define HOSTMANAGER_H

#include "dboperations.h"
#include "../ISO/ISO8583_interface.h"

/**
 * Initialize the static data for the host communication
 **/
int initializeHostStaticData();

/**
 * Initiate the offline transaction from thread
 **/
// void handleHostOfflineTransactions(size_t timer_id, void * user_data);
void *handleHostOfflineTransactions();

/**
 * To verify whether we need to do reversal and do it
 */
void verifyAndDoReversal();

/**
 * Start the reversal process with sleep time
 */
void *startReversalThread();

/**
 * Perform the balance update or service creation with host
 **/
void performHostUpdate(struct transactionData trxData, long batchCounter,
                       uint8_t *response, size_t *response_len, enum host_trx_type hostTrxType);

/**
 * Perform the balance update or service creation with airtel host
 **/
void performAirtelHostUpdate(struct transactionData trxData, long batchCounter,
                             uint8_t *response, size_t *response_len, enum host_trx_type hostTrxType);

/**
 * Perform reversal
 **/
void performReversal(TransactionTable trxTable);

/**
 *  To initiate a verify terminal and get the response
 */
char *performVerifyTerminal(const char tid[50], const char mid[50]);

/**
 *  To initiate a verify terminal and get the response for Airtel
 */
char *performAirtelVerifyTerminal();

/**
 *  To initiate a health check and get the response for Airtel
 */
char *performAirtelHealthCheck();

/**
 * Get the  host error as string
 **/
const char *getHostErrorString(ISO8583_ERROR_CODES errorCode);

///// old codes

/**
 * Process the balance update response received
 **/
// void doBalanceUpdateHostResponseInDb(BALANCE_UPDATE_RESPONSE bal_update_resp,
//                 const char* transactionId, int hostResult);

/**
 * Invoke the reversal
 **/
// void performReversal(const char* transactionId, bool isTimeOut, const char* responseCode);

#endif