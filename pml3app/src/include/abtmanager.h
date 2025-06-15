#ifndef ABTMANAGER_H_
#define ABTMANAGER_H_

#include <stddef.h>
#include "abtdbmanager.h"

/**
 * To handle the completion of the ABT
 */
void handleAbtCompletion(const void *outcome, size_t outcomeLen);

/**
 * To check whether we get the gate open from validator
 */
bool checkAndGetGateOpen();

/**
 * To print ABT transaction row
 */
void printAbtTransactionRow(AbtTransactionTable trxTable);

/**
 * Process all the pending abt transactions with abt server
 **/
void processAbtPendingTransactions();

#endif