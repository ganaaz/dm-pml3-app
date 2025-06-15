#ifndef JHOSTINTERFACE_H
#define JHOSTINTERFACE_H

#include "../include/config.h"
#include "../include/dboperations.h"

/**
 * Generate the request for verify terminal
 */
char *generateVerifyTerminalRequest(struct transactionData trxData, const char tid[50]);

/**
 * Generate offline sale request
 **/
char *generateOfflineSaleRequest(TransactionTable trxDataList[], int start, int trxCount);

/**
 * Generate balance update request
 **/
char *generateBalanceUpdateRequest(struct transactionData trxData);

/**
 * Generate Money add request
 **/
char *generateMoneyAddRequest(struct transactionData trxData);

/**
 * Generate the service creation request
 **/
char *generateServiceCreationRequest(struct transactionData trxData);

/**
 * Generate Reversal request
 **/
char *generateReversalRequest(TransactionTable trxTable);

/**
 * To parse the response to object
 **/
HostResponse parseHostResponseData(const char *data);

/**
 * To parse the offline sale respinse
 **/
void parseofflineSaleResponse(const char *data, int *count, OfflineSaleResponse offlineSaleResponses[]);

/**
 * Parsing of the reversal host response
 **/
ReversalResponse parseReversalHostResponseData(const char *data);

/**
 * Parsing of the verify terminal response
 */
VerifyTerminalResponse parseVerifyTerminalResponse(const char *data);

/**
 * Generate mac
 * MAC String Sequence processingCode
 * stan + tid + serialNo + vendorName + extendInfo
 **/
void generateMacVerifyTerminal(struct transactionData trxData, const char tid[50]);

/**
 * Generate mac
 * 6. Offline Sale
    MAC String Sequence =
    processingCode + amount + stan + expiryDate(O) + time + date + year +
    invoiceNumber + posConditionCode + posEntryMode + primaryAccountNr(O) + tid + mid + iccData (O) +
    authorizationCode(O) + extendInfo-values(O)
 */
void generateMacOfflineSale(struct transactionData trxData);

/**
 * Generate mac
 * MAC String Sequence processingCode
 * + amount + stan + expiryDate(O) + time + date + year + invoiceNumber +
 * pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + encryptedTrack2 +
 * iccData (O) + tid + mid + extendInfo + txnType(O)
 **/
void generateMacBalanceUpdate(struct transactionData trxData);

/**
 * Generate mac
 * MAC String Sequence processingCode
 * + amount + stan + expiryDate(O) + time + date + year + invoiceNumber +
 * pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + encryptedTrack2 +
 * iccData (O) + tid + mid + extendInfo + txnType(O)
 **/
void generateMacServiceCreation(struct transactionData trxData);

/**
 * Generate mac
 * MAC String Sequence money add
 * processingCode + amount + stan + expiryDate(O) + time + date + year +
 * invoiceNumber + pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + track2(O) +
 * iccData (O) + tid + mid + extendInfo-values(O) + TxnType
 **/
void generateMacMoneyAdd(struct transactionData trxData);

/**
 * Generate mac
 * tid + mid + stan + time + date + year + invoiceNumber + ( if primaryAccountNr present )[
 * posConditionCode + posEntryMode + primaryAccountNr ](O) + extendInfo(O)
 **/
TransactionTable generateMacReversal(TransactionTable trxTable, const char de39[3]);

/**
 * Generate mac for echo
 * tid + mid + stan + time + date + year + invoiceNumber + txnMode + txnStatus + txnType +
 * acquirementId(O) + orderId(O) + extendInfo(O)
 **/
TransactionTable generateMacEcho(TransactionTable trxTable, const char de39[3]);

#endif