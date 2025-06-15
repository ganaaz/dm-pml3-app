#ifndef JAIRTELHOSTINTERFACE_H
#define JAIRTELHOSTINTERFACE_H

#include "../include/config.h"
#include "../include/dboperations.h"

/**
 * Generate offline sale request
 **/
char *generateAirtelOfflineSaleRequest(TransactionTable trxDataList[], int start, int trxCount);

/**
 * To parse the offline sale response for Airtel
 **/
void parseAirtelOfflineSaleResponse(const char *data, int *count, OfflineSaleResponse offlineSaleResponses[]);

/**
 * Common parsing for Balance update, service creation, money add for Airtel
 **/
AirtelHostResponse parseAirtelHostResponseData(const char *data);

// /**
//  * Generate the request for verify terminal
//  */
// char *generateVerifyTerminalRequest(struct transactionData trxData, const char tid[50]);

/**
 * Generate balance update request
 **/
char *generateAirtelBalanceUpdateRequest(struct transactionData trxData);

/**
 * Generate Money add request
 **/
char *generateAirtelMoneyAddRequest(struct transactionData trxData);

/**
 * Generate the service creation request
 **/
char *generateAirtelServiceCreationRequest(struct transactionData trxData);

/**
 * Airtel reversal request
 */
char *generateAirtelReversalRequest(TransactionTable trxTable);

/**
 * Verify terminal request for Airtel
 */
char *generateAirtelVerifyTerminalRequest();

/**
 * Parse verify terminal response for airtel
 **/
AirtelVerifyTerminalResponse parseAirtelHostVerifyTerminalResponse(const char *data);

/**
 * Health check request for Airtel
 */
char *generateAirtelHealthCheckRequest();

/**
 * Parse health check response for airtel
 **/
AirtelHealthCheckResponse parseAirtelHostHealthCheckResponse(const char *data);

// /**
//  * Generate mac
//  * MAC String Sequence processingCode
//  * stan + tid + serialNo + vendorName + extendInfo
//  **/
// void generateMacVerifyTerminal(struct transactionData trxData, const char tid[50]);

/**
 * Generate mac
 * 6. Offline Sale
    MAC String Sequence =
    processingCode + amount + stan + expiryDate(O) + time + date + year +
    invoiceNumber + posConditionCode + posEntryMode + primaryAccountNr(O) + tid + mid + iccData (O) +
    authorizationCode(O) + extendInfo-values(O)
 */
// void generateAirtelMacOfflineSale(struct transactionData trxData);

/**
 * Generate mac
 * MAC String Sequence processingCode
 * + amount + stan + expiryDate(O) + time + date + year + invoiceNumber +
 * pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + encryptedTrack2 +
 * iccData (O) + tid + mid + extendInfo + txnType(O)
 **/
// void generateAirtelMacBalanceUpdate(struct transactionData trxData);

// /**
//  * Generate mac
//  * MAC String Sequence processingCode
//  * + amount + stan + expiryDate(O) + time + date + year + invoiceNumber +
//  * pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + encryptedTrack2 +
//  * iccData (O) + tid + mid + extendInfo + txnType(O)
//  **/
// void generateMacServiceCreation(struct transactionData trxData);

// /**
//  * Generate mac
//  * MAC String Sequence money add
//  * processingCode + amount + stan + expiryDate(O) + time + date + year +
//  * invoiceNumber + pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + track2(O) +
//  * iccData (O) + tid + mid + extendInfo-values(O) + TxnType
//  **/
// void generateMacMoneyAdd(struct transactionData trxData);

// /**
//  * Generate mac
//  * tid + mid + stan + time + date + year + invoiceNumber + ( if primaryAccountNr present )[
//  * posConditionCode + posEntryMode + primaryAccountNr ](O) + extendInfo(O)
//  **/
// TransactionTable generateMacReversal(TransactionTable trxTable, const char de39[3]);

// /**
//  * Generate mac for echo
//  * tid + mid + stan + time + date + year + invoiceNumber + txnMode + txnStatus + txnType +
//  * acquirementId(O) + orderId(O) + extendInfo(O)
//  **/
// TransactionTable generateMacEcho(TransactionTable trxTable, const char de39[3]);

#endif