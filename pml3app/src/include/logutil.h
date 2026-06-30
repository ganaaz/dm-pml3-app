#ifndef LOGUTIL_H
#define LOGUTIL_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * To initialize the time log data list
 */
void initTimeLogData();

/**
 * To print the time log data
 */
void printTimeLogData();

/**
 * to clear the time log data
 */
void clearTimeLogData();

/**
 * To load the paytm index from the file
 */
void loadPayTmIndex();

/**
 * To check whether the file exists
 */
bool isFileExists(char fileName[50]);

/**
 * To write the paytm log index
 */
void writePayTmLogIndex(int index);

/*
 * To get the size of the file
 */
long getFileSize(char fileName[50]);

/**
 * To get the log file name based on the rotation logic
 */
void getHostLogFileName(char *fileName);

/**
 * To log the paytm request to file
 */
void logHostData(const char *message);

/**
 * log data to log4c — formats structured pipe-delimited message
 **/
void logToLog4c(int priority, const char *module, const char *subModule, const char *fmt, va_list args);

/**
 * Debug log (module/subModule default to "")
 **/
void logData(const char *fmt, ...);

/**
 * Info log (module/subModule default to "")
 **/
void logInfo(const char *fmt, ...);

/**
 * Error log (module/subModule default to "")
 **/
void logError(const char *fmt, ...);

/**
 * Warn log (module/subModule default to "")
 **/
void logWarn(const char *fmt, ...);

/**
 * Debug log with explicit module and subModule
 **/
void logDataEx(const char *module, const char *subModule, const char *fmt, ...);

/**
 * Info log with explicit module and subModule
 **/
void logInfoEx(const char *module, const char *subModule, const char *fmt, ...);

/**
 * Error log with explicit module and subModule
 **/
void logErrorEx(const char *module, const char *subModule, const char *fmt, ...);

/**
 * Warn log with explicit module and subModule
 **/
void logWarnEx(const char *module, const char *subModule, const char *fmt, ...);

/**
 * To log time data for measuring performance
 */
void logTimeWarnData(const char *fmt, ...);

/**
 * Log the data in hex
 **/
void logHexData(const char *title, const void *data, int len);

/**
 * Log the data in hex without space
 **/
void logHexDataNoSpace(const char *title, const void *data, int len);

/**
 * Print Service qualifier in readable format
 **/
void printServiceQualifier(uint8_t *buffer, size_t buffer_len);

/**
 * Print service management info
 **/
void printServiceManagementInfo(uint8_t *buffer, size_t buffer_len);

/**
 * Print service control
 **/
void printServiceControl(uint8_t *buffer, size_t buffer_len);

/**
 * Print service folder
 **/
void printServiceFolder(uint8_t *buffer, size_t buffer_len);

/**
 * Print service directory
 **/
void printServiceDirectory(uint8_t *buffer, size_t buffer_len);

/**
 * Print service related data
 **/
void printServiceRelatedData(uint8_t *buffer, size_t buffer_len);

/**
 * Print the TLV data
 **/
int printTlvData(uint8_t *data, size_t data_len);

/**
 * Get the date time in the buffer
 **/
void getDateTime(char *buffer);

/**
 * Process and display the outcome for debug purpose
 **/
void processOutcome(const void *outcome, size_t outcome_len);

/**
 * Generate the log required as per certification purpose
 **/
int generateLog(const void *outcome, size_t outcome_len, const char *prefixFileName, int trxCounter);

/**
 * Register "message" layout type with log4c — call before log4c_init()
 **/
void registerMessageLayout(void);

/**
 * Register "plain_stdout" appender type with log4c — call before log4c_init()
 **/
void registerPlainStdoutAppender(void);

#endif