#ifndef RESPONSEMANAGER_H_
#define RESPONSEMANAGER_H_

#include <json-c/json.h>

#include "config.h"
#include "dboperations.h"

/**
 * Build the response message
 **/
char *buildResponseMessage(const char *status, int errorCode);

/**
 * Build the status response message with command
 **/
char *buildCommandResponseMessage(const char *command, const char *status, int errorCode);

/**
 * To send the device offline message, this is after the transaction is done
 * to send to the client directly
 **/
void sendDeviceOfflineMessage();

/**
 * To send a set time response
 */
char *buildSetTimeResponse(const char *status);

/**
 * To send the file in socket
 */
void sendFileToSocket(const char fileName[]);

/**
 * To send a message when there is no reversal
 */
char *buildNoReversalMessage();

/**
 * Build the config message
 **/
char *buildGetConfigMessage();

/**
 * Build key present message
 **/
char *buildKeyPresentMessage(int result, char *label, bool isBdk);

/**
 * Build destroy key message
 **/
char *buildDestroyKeyMessage(int result, char *label, bool isBdk);

/**
 * Build pending offline summary message
 **/
char *buildPendingOfflineSummaryMessage();

/**
 * Build verify terminal response message
 **/
char *buildVerifyTerminalResponseMessage(VerifyTerminalResponse response, int midStatus);

/**
 * Transaction processed message, card processed message
 **/
void sendTransactionProcessedMessage(struct transactionData trxData, const char *outcomeStatus);

/**
 * Balance message, Card presented message
 **/
void sendBalanceMessage(struct transactionData trxData);

/**
 * Send ABT cart presented
 */
void sendAbtCardPresented(struct transactionData trxData);

/**
 * Build ABT Summary message
 **/
char *buildAbtTrxSummaryMessage();

/**
 * Version message
 **/
char *buildVersionMessage();

/**
 * Build get device id message
 **/
char *buildDeviceIdMessage();

/**
 * To send the card removed message
 **/
void sendCardRemovedMessage();

/**
 * To send a delete file response
 */
char *buildDeleteFileResponse(const char *message, int code);

/**
 * Build the firmware version message
 **/
char *buildFirmwareVersionMessage();

/**
 * Build the product order number message
 **/
char *buildGetProductOrderNumberMessage();

/**
 * Build verify terminal response message for airtel
 **/
char *buildAirtelVerifyTerminalResponseMessage(AirtelVerifyTerminalResponse response, int status);

/**
 * Build health check response message for airtel
 **/
char *buildAirtelHealthCheckResponseMessage(AirtelHealthCheckResponse response, int status);

#endif