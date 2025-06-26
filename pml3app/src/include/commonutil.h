#ifndef COMMONUTIL_H_
#define COMMONUTIL_H_

#include <stdbool.h>
#include <uuid/uuid.h>

#include "config.h"
#include "commandmanager.h"

/**
 * To handle the signals
 **/
void signalCallbackBandler(int signum);

/**
 * To read a file data
 */
unsigned char *readFile(const char fileName[]);

/**
 * to find the size of the file
 */
long int findSize(const char fileName[]);

/**
 * To get the device id
 **/
void getDeviceId(char *deviceId);

/**
 * To create the network interface file and copy it
 */
void createAndCopyNetworkInterfaceFile(struct message reqMessage);

/**
 * to create and copy resolve file
 */
void createAndCopyResolvFile(struct message reqMessage);

/**
 * Convert the Byte to Hex Value
 **/
void byteToHex(const unsigned char *data, size_t dataLen, char *output);

/**
 * Convert the amount to the 6 byte character value
 **/
void convertAmount(uint64_t amount, char *output);

/**
 * To check whether the minimum firmware is installed
 **/
bool isMinimumFirmwareInstalled(void);

/**
 * To convert hex string to byte data
 **/
int hexToByte(const char *hexData, unsigned char *output);

/**
 * Generate a unique GUID
 **/
void generateUUID(char uuidData[UUID_STR_LEN]);

/**
 * Update the transaction date and time with the current time
 **/
struct transactionData updateTransactionDateTime(struct transactionData trxData);

/**
 * To print the transaction data
 **/
void printCurrentTxnData(struct transactionData trxData);

/**
 * Generate the mask pan data
 */
const char *maskPan(const char *pan);

/**
 * Do the mutex lock for variable update in thread
 **/
void doLock();

/**
 * Release the mutex lock
 **/
void doUnLock();

/**
 * Convert the string to upper
 **/
void toUpper(char *data);

/**
 * Get the current seconds used for time measurement
 **/
long long getCurrentSeconds();

/**
 * Reset the transaction data after every txn
 **/
void resetTransactionData();

/**
 * To increment the transaction counter
 */
void incrementTransactionCounter();

/**
 * Remove the track 2 padding
 **/
char *removeTrack2FPadding(const char *track2, int len);

/**
 * to format the transaction date and time
 */
void formatTransactionTime(struct transactionData trxData, char trxDate[20]);

/**
 * Display sound in case of error
 **/
void makeBuzz();

/**
 * to make a beep sound
 */
void *makeBeep();

/**
 * To initiate a beep in thread
 */
void beepInThread();

/**
 * To generate a sha 256 of a string
 **/
void generateSha(char *string, char outputBuffer[65]);

/**
 * To generate a token for the pan number
 **/
void generatePanToken(const char *panNumber, const char *track2, char buffer[65]);

/**
 * Convert the string number to hex number
 * eg 123 to 313233
 **/
void string2hexString(const char *input, char *output);

/**
 * Display the light based on the config
 */
void displayLight(const char *stateName);

/**
 * To generate order id
 */
void generateOrderId();

/**
 * To delete a log file
 */
int deleteLogFile(struct message reqMessage);

/**
 * To generate the narration data
 **/
void generateNarrationData(char *stationId, char *acqTrxId,
                           char *acqUniqueTrxId, char amount[13], char *narration);

/**
 *
 **/
void pad0MultipleOf8(const char *input, char *ouput, int *outLen);

/**
 * To remove the ICC Track2 and Pan Tags
 * This will update the currentTxnData => iccData and iccDataLen
 */
void removeIccTags(uint8_t buffer[4096], size_t buffer_len);

/**
 * To get the days from Jan 1 2020
 */
int getDaysFromJan12020();

/**
 * To get the dates between
 */
int daysBetween(struct tm start, struct tm end);

/**
 * To populate the current time
 */
void populateCurrentTime(char timeData[20]);

/**
 * To get the seconds since start of the day in local time
 */
long long getSecondsSinceStartOfDay();

/**
 * to calculate hmac used by airtel for signature
 */
void calculate_hmac_sha256(const char *key, const char *data, char *hmac_hex);

/**
 * To remove the spaces
 */
void removeSpaces(char *message);

/**
 * To remove the trailing zeros from amount
 */
void trimAmount(char amount[13], char trimmedAmount[13]);

#endif