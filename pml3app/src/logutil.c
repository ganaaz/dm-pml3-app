#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <inttypes.h>
#include <log4c.h>
#include <log4c/appender.h>
#include <log4c/layout.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libpay/tlv.h>
#include <feig/emvco_tags.h>
#include <feig/feig_tags.h>
#include <feig/feig_e2ee_tags.h>
#include <feig/feig_trace_tags.h>
#include <feig/emvco_ep.h>

#include "include/commonutil.h"
#include "include/logutil.h"
#include "include/config.h"
#include "include/tlvhelper.h"

pthread_t logThread;
pthread_mutex_t logLock;
extern struct applicationConfig appConfig;
extern log4c_category_t *logCategory;
extern struct applicationData appData;

static const char *message_layout_format(const log4c_layout_t *layout, const log4c_logging_event_t *event)
{
    static char buf[4096];
    snprintf(buf, sizeof(buf), "%s\n", event->evt_msg);
    return buf;
}

static const log4c_layout_type_t message_layout_type = {
    "message",
    message_layout_format};

void registerMessageLayout(void)
{
    log4c_layout_type_set(&message_layout_type);
}

static int plain_stdout_open(log4c_appender_t *appender) { return 0; }
static int plain_stdout_close(log4c_appender_t *appender) { return 0; }

static int plain_stdout_append(log4c_appender_t *appender, const log4c_logging_event_t *event)
{
    return fputs(log4c_layout_format(log4c_appender_get_layout(appender), event), stdout);
}

static const log4c_appender_type_t plain_stdout_appender_type = {
    "plain_stdout",
    plain_stdout_open,
    plain_stdout_append,
    plain_stdout_close};

void registerPlainStdoutAppender(void)
{
    log4c_appender_type_set(&plain_stdout_appender_type);
}

char **logTimeMessageList;
int logTimeMessageCount = 0;
int paytmLogIndex;

/**
 * To initialize the time log data list
 */
void initTimeLogData()
{
    logTimeMessageCount = 0;
    if (appConfig.isPrintDetailTimingLogs == false ||
        appConfig.isDebugEnabled == false)
    {
        return;
    }

    logTimeMessageList = malloc(2000 * sizeof(char *));
}

/**
 * to clear the time log data
 */
void clearTimeLogData()
{
    for (int i = 0; i < logTimeMessageCount; i++)
        free(logTimeMessageList[i]);

    free(logTimeMessageList);
}

/**
 * To print the time log data
 */
void printTimeLogData()
{
    for (int i = 0; i < logTimeMessageCount; i++)
    {
        printf(logTimeMessageList[i]);
        printf("\n");
    }
}

/**
 * To load the paytm index from the file
 */
void loadPayTmIndex()
{
    FILE *fp;
    fp = fopen("paytm_index", "r");

    if (fp == NULL)
    {
        logDataEx("L3-App", "", "paytm_index file does not exists, creating index as 1");
        paytmLogIndex = 1;
        fp = fopen("paytm_index", "w");
        fprintf(fp, "%d", paytmLogIndex);
        fclose(fp);
    }
    else
    {
        logDataEx("L3-App", "", "paytm_index file exists so reading it");
        int count;
        fscanf(fp, "%d", &count);
        fclose(fp);
        paytmLogIndex = count;
        logDataEx("L3-App", "", "Index read : %d", paytmLogIndex);
    }
}

/**
 * To write the paytm log index
 */
void writePayTmLogIndex(int index)
{
    FILE *fp;
    fp = fopen("paytm_index", "w");
    fprintf(fp, "%d", index);
    fclose(fp);
}

/**
 * To get the size of the file
 */
long getFileSize(char fileName[50])
{
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL)
    {
        logDataEx("L3-App", "", "File doesnt exists");
        return 0;
    }

    fseek(fp, 0, SEEK_END);

    long fileSize = ftell(fp);
    fclose(fp);
    logDataEx("L3-App", "", "Size of the file : %s is %d", fileName, fileSize);
    return fileSize;
}

/**
 * To check whether the file exists
 */
bool isFileExists(char fileName[50])
{
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL)
    {
        return false;
    }

    fclose(fp);
    return true;
}

/**
 * To get the log file name based on the rotation logic
 */
void getHostLogFileName(char *fileName)
{
    char currFileName[50] = {0};
    char index[3] = {0};
    sprintf(index, "%d", paytmLogIndex);
    logDataEx("L3-App", "", "Index : %s", index);

    safe_strcpy(currFileName, sizeof(currFileName), "host.log.");
    safe_strcat(currFileName, sizeof(currFileName), index);

    logDataEx("L3-App", "", "Generated file name : %s", currFileName);

    bool exists = isFileExists(currFileName);

    // Check the file exists
    if (exists == false)
    {
        safe_strcpy(fileName, 50, currFileName);
        logDataEx("L3-App", "", "Log file doesnot exists, so using that");
        return;
    }

    // Check the size of the file
    long fileSize = getFileSize(currFileName);
    long maxSize = appConfig.paytmMaxLogSizeKb * 1024;

    if (fileSize < maxSize)
    {
        logDataEx("L3-App", "", "File size under the limit, so using that");
        safe_strcpy(fileName, 50, currFileName);
        return;
    }

    logDataEx("L3-App", "", "File size is more than the max, so increasing the index");
    paytmLogIndex++;
    logDataEx("L3-App", "", "Now the host log index : %d", paytmLogIndex);

    if (paytmLogIndex <= appConfig.paytmLogCount)
    {
        logDataEx("L3-App", "", "Log index is less than the max count");
    }
    else
    {
        logDataEx("L3-App", "", "Reset to the 1 as the max count is reached");
        paytmLogIndex = 1;
    }

    char newFileName[50] = {0};
    char newIndex[3] = {0};
    sprintf(newIndex, "%d", paytmLogIndex);
    logDataEx("L3-App", "", "newIndex : %s", newIndex);

    safe_strcpy(newFileName, 50, "paytm.log.");
    safe_strcat(newFileName, 50, newIndex);

    logDataEx("L3-App", "", "Generated new file name : %s", newFileName);

    // Reset the file if present to empty
    FILE *fp;
    fp = fopen(newFileName, "w");
    if (fp != NULL)
    {
        fprintf(fp, "%s", "");
        fclose(fp);
    }
    logDataEx("L3-App", "", "File %s is reset", newFileName);
    long size = getFileSize(newFileName);
    logDataEx("L3-App", "", "Size is : %d", size);
    safe_strcpy(fileName, 50, newFileName);
}

/**
 * To log the host request to file
 */
void logHostData(const char *message)
{
    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    char fileName[50];
    getHostLogFileName(fileName);
    logDataEx("L3-App", "", "File used : %s", fileName);

    FILE *fp;
    fp = fopen(fileName, "a+");

    if (fp == NULL)
    {
        logErrorEx("L3-App", "HostData", "Unable to open the file for host data write");
        return;
    }

    fprintf(fp, "%s\n", message);
    fclose(fp);
}

/**
 * Log to log4c logger.
 * Formats message as:
 *   DateTime|Identifier|IP|LoggingLevel|Module|SubModule|Status|AppVersion|Message|AdditionalInfo
 **/
void logToLog4c(int priority, const char *module, const char *subModule, const char *fmt, va_list args)
{
    if (!log4c_category_is_priority_enabled(logCategory, priority))
        return;

    static char cachedIp[64] = {0};
    static char cachedDeviceId[128] = {0};
    static bool cacheInitialized = false;

    if (!cacheInitialized)
    {
        getLocalIP(cachedIp, sizeof(cachedIp));
        getDeviceId(cachedDeviceId);
        cacheInitialized = true;
    }

    char userMessage[2048];
    vsnprintf(userMessage, sizeof(userMessage), fmt, args);

    char dateTime[32];
    time_t timer = time(NULL);
    struct tm tm_buf;
    struct tm *tm_info = localtime_r(&timer, &tm_buf);
    strftime(dateTime, sizeof(dateTime), "%d-%m-%Y %H:%M:%S", tm_info);

    const char *levelStr;
    switch (priority)
    {
    case LOG4C_PRIORITY_INFO:
        levelStr = "INFO";
        break;
    case LOG4C_PRIORITY_WARN:
        levelStr = "WARN";
        break;
    case LOG4C_PRIORITY_ERROR:
        levelStr = "ERROR";
        break;
    default:
        levelStr = "DEBUG";
        break;
    }

    char *status = getStatusString(appData.status);

    char fullMessage[4096];
    snprintf(fullMessage, sizeof(fullMessage),
             "%s|%s|%s|%s|%s|%s|%s|%s|%s|",
             dateTime,
             cachedDeviceId,
             cachedIp,
             levelStr,
             module ? module : "",
             subModule ? subModule : "",
             status,
             appConfig.version,
             userMessage);

    pthread_mutex_lock(&logLock);
    log4c_category_log(logCategory, priority, "%s", fullMessage);
    pthread_mutex_unlock(&logLock);
}

/**
 * Log info data
 **/
void logInfo(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logToLog4c(LOG4C_PRIORITY_INFO, "", "", fmt, args);
    va_end(args);
}

void logInfoEx(const char *module, const char *subModule, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logToLog4c(LOG4C_PRIORITY_INFO, module, subModule, fmt, args);
    va_end(args);
}

/**
 * Log error data
 **/
void logError(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logToLog4c(LOG4C_PRIORITY_ERROR, "", "", fmt, args);
    va_end(args);
}

void logErrorEx(const char *module, const char *subModule, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logToLog4c(LOG4C_PRIORITY_ERROR, module, subModule, fmt, args);
    va_end(args);
}

void *printMessage(void *message)
{
    printf((char *)message);
    printf("\n");

    return (void *)NULL;
}

void logTimeWarnData(const char *fmt, ...)
{
    if (appConfig.isPrintDetailTimingLogs == false ||
        appConfig.isDebugEnabled == false)
    {
        return;
    }

    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(message, fmt, args);
    va_end(args);

    logTimeMessageList[logTimeMessageCount] = malloc(1030);
    sprintf(logTimeMessageList[logTimeMessageCount], "%s", message);
    logTimeMessageCount++;
}

/**
 * Log warn data
 **/
void logWarn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logToLog4c(LOG4C_PRIORITY_WARN, "", "", fmt, args);
    va_end(args);
}

void logWarnEx(const char *module, const char *subModule, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logToLog4c(LOG4C_PRIORITY_WARN, module, subModule, fmt, args);
    va_end(args);
}

/**
 * Log debug data
 **/
void logData(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logToLog4c(LOG4C_PRIORITY_DEBUG, "", "", fmt, args);
    va_end(args);
}

void logDataEx(const char *module, const char *subModule, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logToLog4c(LOG4C_PRIORITY_DEBUG, module, subModule, fmt, args);
    va_end(args);
}

/**
 * Get formatted date time
 **/
void getDateTime(char *buffer)
{
    time_t timer;
    struct tm *tm_info;

    timer = time(NULL);
    tm_info = localtime(&timer);

    strftime(buffer, 26, "[%Y-%m-%d %H:%M:%S] ", tm_info);
}

/**
 * Log hex data string
 **/
void logHexData(const char *title, const void *data, int len)
{
    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    // char buffer[26];
    // getDateTime(buffer);
    // printf("%s%s : ", buffer, title);
    printf("%s", title);
    const unsigned char *p = (const unsigned char *)data;
    int i = 0;

    for (; i < len; ++i)
    {
        printf("%02X ", *p++);
    }

    printf("\n");
}

/**
 * Log hex data string
 **/
void logHexDataNoSpace(const char *title, const void *data, int len)
{
    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    // char buffer[26];
    // getDateTime(buffer);
    // printf("%s%s : ", buffer, title);
    printf("%s", title);
    const unsigned char *p = (const unsigned char *)data;
    int i = 0;

    for (; i < len; ++i)
    {
        printf("%02X", *p++);
    }

    printf("\n");
}

/**
 * Print service qualifier
 **/
void printServiceQualifier(uint8_t *buffer, size_t buffer_len)
{
    if (0 != buffer_len % 5)
    {
        logDataEx("L3-App", "", "Invalid Service Qualifier format!");
        return;
    }

    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    uint8_t *p = buffer;
    size_t i = 0;
    for (i = 0; i < buffer_len / 5; i++)
    {
        logDataEx("L3-App", "", "  Service Qualifier: %02X%02X%02X%02X%02X", p[0], p[1], p[2], p[3], p[4]);
        logDataEx("L3-App", "", "    Priority 0x%02X: ", p[0] & 0xFE);
        if (0x01 == (0x01 & p[0]))
        {
            logDataEx("L3-App", "", "Cardholder Confirmation is NOT needed");
        }
        else
        {
            logDataEx("L3-App", "", "Cardholder Confirmation is needed");
        }
        logDataEx("L3-App", "", "    Service ID: %02X%02X", p[1], p[2]);
        printServiceManagementInfo(&p[3], 2);
        p += 5;
    }
    return;
}

/**
 * Print service management info
 **/
void printServiceManagementInfo(uint8_t *buffer, size_t buffer_len)
{
    if (2 != buffer_len)
    {
        logDataEx("L3-App", "", "Invalid Service Management Info format!");
        return;
    }

    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    uint8_t *p = buffer;

    logDataEx("L3-App", "", "      Service Management Info: %02X%02X", p[0], p[1]);
    /* Byte 1 */
    logDataEx("L3-App", "", "        Service Type: ");
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "       Permanent Service");
    }
    else
    {
        logDataEx("L3-App", "", "       Temporary Service");
    }

    logDataEx("L3-App", "", "        Service Updata Type: ");
    if (0x00 == (0x60 & *p))
    {
        logDataEx("L3-App", "", "Service specific data");
    }
    else if (0x20 == (0x60 & *p))
    {
        logDataEx("L3-App", "", "Service Key Plant");
    }
    else if (0x40 == (0x60 & *p))
    {
        logDataEx("L3-App", "", "Service Balance Limit Update");
    }
    else
    {
        logDataEx("L3-App", "", "Undefined");
    }

    logDataEx("L3-App", "", "        Service activation: ");
    if (0x10 == (0x10 & *p))
    {
        logDataEx("L3-App", "", " Active");
    }
    else
    {
        logDataEx("L3-App", "", " Inactive");
    }
    logDataEx("L3-App", "", "        Service state: ");
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "      Blocked");
    }
    else
    {
        logDataEx("L3-App", "", "      Operational");
    }
    logDataEx("L3-App", "", "        Wallet: ");
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "             Inactive");
    }
    else
    {
        logDataEx("L3-App", "", "             Active");
    }
    logDataEx("L3-App", "", "        Wallet linked to: ");
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "   Card Global Balance");
    }
    else
    {
        logDataEx("L3-App", "", "   Service Local Balance");
    }

    p++;
    /* Byte 2 */
    logDataEx("L3-App", "", "        Currency Code: ");
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "      Kilometres (0x0999)");
    }
    else
    {
        logDataEx("L3-App", "", "      Indian Rupee (0x0356)");
    }
    return;
}

/**
 * Print service control data
 **/
void printServiceControl(uint8_t *buffer, size_t buffer_len)
{
    if (2 != buffer_len)
    {
        logDataEx("L3-App", "", "Invalid Service Control format!");
        return;
    }

    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    uint8_t *p = buffer;

    logDataEx("L3-App", "", "    Service Control: %02X%02X", p[0], p[1]);
    /* Byte 1 */
    logDataEx("L3-App", "", "      Service Type: ");
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "      Permanent Service");
    }
    else
    {
        logDataEx("L3-App", "", "      Temporary Service");
    }

    logDataEx("L3-App", "", "      Service activation: ");
    if (0x20 == (0x20 & *p))
    {
        logDataEx("L3-App", "", "Active");
    }
    else
    {
        logDataEx("L3-App", "", "Inactive");
    }
    logDataEx("L3-App", "", "      Service state: ");
    if (0x10 == (0x10 & *p))
    {
        logDataEx("L3-App", "", "     Blocked");
    }
    else
    {
        logDataEx("L3-App", "", "     Operational");
    }
    logDataEx("L3-App", "", "      Wallet: ");
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "            Active");
    }
    else
    {
        logDataEx("L3-App", "", "            Inactive");
    }
    logDataEx("L3-App", "", "      Wallet linked to: ");
    if (0x04 == (0x04 & *p))
    {
        logDataEx("L3-App", "", "  Card Global Balance");
    }
    else
    {
        logDataEx("L3-App", "", "  Service Local Balance");
    }

    p++;
    /* Byte 2 */
    logDataEx("L3-App", "", "      Currency Code: ");
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "     Kilometres (0x0999)");
    }
    else
    {
        logDataEx("L3-App", "", "     Indian Rupee (0x0356)");
    }
    return;
}

/**
 * Print service folder
 **/
void printServiceFolder(uint8_t *buffer, size_t buffer_len)
{
    if (0 != buffer_len % 4)
    {
        logDataEx("L3-App", "", "Invalid Service Folder format!");
        return;
    }

    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    uint8_t *p = buffer;
    size_t i = 0;
    for (i = 0; i < buffer_len / 4; i++)
    {
        if (p[0] == 0x00 && p[1] == 0x00)
        {
            /* Indicates No Service specified
             * (i.e. not a Servicebased transaction)
             */
        }
        else if ((p[0] == 0xFF && p[1] == 0xFF) ||
                 (p[0] == 0x00 && p[1] > 0x01 && p[1] <= 0x0F))
        {
            logDataEx("L3-App", "", "  Service Folder %zu: %02X%02X%02X%02X", i, p[0], p[1], p[2], p[3]);
            logDataEx("L3-App", "", "    Service ID: %02X%02X --> RFU", p[0], p[1]);
            printServiceControl(&p[2], 2);
        }
        else if (p[0] == 0xFF && p[1] == 0xFE)
        {
            logDataEx("L3-App", "", "  Service Folder %zu: %02X%02X%02X%02X", i, p[0], p[1], p[2], p[3]);
            logDataEx("L3-App", "", "    Service ID: %02X%02X --> Indicates Global offline balance is to be updated",
                      p[0], p[1]);
            printServiceControl(&p[2], 2);
        }
        else
        {
            logDataEx("L3-App", "", "  Service Folder %zu: %02X%02X%02X%02X", i, p[0], p[1], p[2], p[3]);
            logDataEx("L3-App", "", "    Service ID: %02X%02X", p[0], p[1]);
            printServiceControl(&p[2], 2);
        }
        p += 4;
    }
    return;
}

/**
 * Print service directory
 **/
void printServiceDirectory(uint8_t *buffer, size_t buffer_len)
{
    if (buffer_len < 17 || 0 != (buffer_len - 17) % 4)
    {
        logDataEx("L3-App", "", "Invalid Service Directory format!");
        return;
    }

    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    uint8_t *p = buffer;
    logDataEx("L3-App", "", "  Service Version (SVER): %02X", *p++);
    logDataEx("L3-App", "", "  RFU: %02X", *p++);
    logDataEx("L3-App", "", "  Service Label (SLBL) (PAN+PSN): %02X%02X%02X%02X%02X%02X%02X%02X%02X %02X",
              p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
    p += 10;
    logDataEx("L3-App", "", "  Service Limit (SLIM): %02X", *p++);
    logDataEx("L3-App", "", "  Service Counter (SCON): %02X", *p++);
    logDataEx("L3-App", "", "  Should be 0x0: %01X", ((p[0] & 0xF0) >> 4));
    logDataEx("L3-App", "", "  Service SFI: %02X", (*p++ & 0x0F));
    logDataEx("L3-App", "", "  DF50 - Service Permanent Counter (SCPER): %02X", *p++);
    logDataEx("L3-App", "", "  DF12 - Service Permanent Limit(SPLIM): %02X", *p++);

    printServiceFolder(p, buffer_len - 17);
    return;
}

/**
 * Print service related data
 **/
void printServiceRelatedData(uint8_t *buffer, size_t buffer_len)
{
    if (128 != buffer_len)
    {
        logDataEx("L3-App", "", "Invalid Service Related Data length!");
        return;
    }

    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    uint8_t *p = buffer;

    if (p[1] == 0x00 && p[2] == 0x00)
    {
        logDataEx("L3-App", "", "Empty Service Record(%02X)", p[0]);
        return;
    }

    logDataEx("L3-App", "", "Service Record:");
    logDataEx("L3-App", "", "    Service Index:       %02X", p[0]);
    logDataEx("L3-App", "", "    Service ID:          %02X%02X", p[1], p[2]);
    printServiceControl(&p[3], 2);
    logDataEx("L3-App", "", "    KCV:                 %02X%02X%02X", p[5], p[6], p[7]);
    logDataEx("L3-App", "", "    PRMacq key index:    %02X", p[8]);
    logDataEx("L3-App", "", "    RFU:                 %02X%02X%02X%02X", p[9], p[10], p[11], p[12]);
    logDataEx("L3-App", "", "    Last service update ATC: %02X%02X", p[13], p[14]);
    logDataEx("L3-App", "", "    Last service update date & time: %02X%02X%02X%02X%02X%02X", p[15], p[16], p[17], p[18], p[19], p[20]);
    logDataEx("L3-App", "", "    Service ATC:         %02X%02X", p[21], p[22]);
    logDataEx("L3-App", "", "    Service balance:     %02X%02X%02X%02X%02X%02X", p[23], p[24], p[25], p[26], p[27], p[28]);
    logDataEx("L3-App", "", "    Service Currency:    %02X%02X", p[29], p[30]);
    logDataEx("L3-App", "", "    Service Data Length: %02X", p[31]);
    logHexData("    Selected Related Data", &p[32], p[31]);
}

/**
 * Print TLV Data
 **/
int printTlvData(uint8_t *data, size_t data_len)
{
    struct tlv *tlv = NULL;
    int rc = 0;

    if (appConfig.isDebugEnabled != 1)
    {
        return 0;
    }

    rc = tlv_parse(data, data_len, &tlv);
    if (rc != TLV_RC_OK)
    {
        logDataEx("L3-App", "", "tlv_parse() failed with rc = %d", rc);
        return -1;
    }

    do
    {
        uint8_t buffer[4096];
        char line[4096];
        size_t size;
        int i, j, len = 0;
        int depth = tlv_get_depth(tlv);

        for (i = 0, j = 0; i < depth; i++, j += 2)
            len += snprintf(&line[len], sizeof(line) - len, "  ");
        size = sizeof(buffer);
        tlv_encode_identifier(tlv, buffer, &size);
        for (i = 0; i < (int)size; i++, j += 2)
            len += snprintf(&line[len], sizeof(line) - len, "%02X",
                            buffer[i]);
        len += snprintf(&line[len], sizeof(line) - len, " ");
        size = sizeof(buffer);
        tlv_encode_length(tlv, buffer, &size);
        for (i = 0; i < (int)size; i++, j += 2)
            len += snprintf(&line[len], sizeof(line) - len, "%02X",
                            buffer[i]);
        len += snprintf(&line[len], sizeof(line) - len, " ");
        size = sizeof(buffer);
        tlv_encode_value(tlv, buffer, &size);
        for (i = 0; i < (int)size; i++, j += 2)
            len += snprintf(&line[len], sizeof(line) - len, "%02X",
                            buffer[i]);
        /*len += */ snprintf(&line[len], sizeof(line) - len, "\n");

        logDataEx("L3-App", "", "%s", line);

        tlv = tlv_iterate(tlv);
    } while (tlv);

    if (tlv)
        tlv_free(tlv);

    return TLV_RC_OK;
}

void print_service_management_info(uint8_t *buffer, size_t buffer_len)
{
    if (2 != buffer_len)
    {
        logDataEx("L3-App", "", "Invalid Service Management Info format!");
        return;
    }
    uint8_t *p = buffer;

    /* Byte 1 */
    logDataEx("L3-App", "", "\t      Service Type: ");
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "Permanent Service");
    }
    else
    {
        logDataEx("L3-App", "", "Temporary Service");
    }
    logDataEx("L3-App", "", "\t      Service operation: ");
    if (0x60 == (0x60 & *p))
    {
        logDataEx("L3-App", "", "Undefined");
    }
    else if (0x40 == (0x40 & *p))
    {
        logDataEx("L3-App", "", "Service Balance Limit Update");
    }
    else if (0x20 == (0x20 & *p))
    {
        logDataEx("L3-App", "", "Service Key Plant");
    }
    else
    {
        logDataEx("L3-App", "", "Service specific data");
    }
    logDataEx("L3-App", "", "\t      Service activation: ");
    if (0x10 == (0x10 & *p))
    {
        logDataEx("L3-App", "", "Active");
    }
    else
    {
        logDataEx("L3-App", "", "Inactive");
    }
    logDataEx("L3-App", "", "\t      Service blockation: ");
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "Blocked");
    }
    else
    {
        logDataEx("L3-App", "", "Unblocked");
    }
    logDataEx("L3-App", "", "\t      Wallet: ");
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "Inactive");
    }
    else
    {
        logDataEx("L3-App", "", "Active");
    }
    logDataEx("L3-App", "", "\t      Wallet linked to: ");
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "Card Global Balance");
    }
    else
    {
        logDataEx("L3-App", "", "Service Local Balance");
    }

    p++;
    /* Byte 2 */
    logDataEx("L3-App", "", "\t      Currency Code: ");
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "Kilometres (0x0999)");
    }
    else
    {
        logDataEx("L3-App", "", "Indian Rupee (0x0356)");
    }
    return;
}

void print_cvr(uint8_t *buffer, size_t buffer_len)
{
    if (8 != buffer_len)
    {
        logDataEx("L3-App", "", "Invalid Card Verification result format!");
        return;
    }
    uint8_t *p = buffer;
    /*
     * CVR
     * Card Verification results
     */
    /* Byte 1 */
    logDataEx("L3-App", "", "\t      Application Cryptogram Type Returned in Second GENERATE AC: ");
    if (0xC0 == (0xC0 & *p))
    {
        logDataEx("L3-App", "", "RFU");
    }
    else if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "Second GENERATE AC not requested");
    }
    else if (0x40 == (0x40 & *p))
    {
        logDataEx("L3-App", "", "TC");
    }
    else
    {
        logDataEx("L3-App", "", "AAC");
    }
    logDataEx("L3-App", "", "\t      Application Cryptogram Type Returned in First GENERATE AC: ");
    if (0x30 == (0x30 & *p))
    {
        logDataEx("L3-App", "", "RFU");
    }
    else if (0x20 == (0x20 & *p))
    {
        logDataEx("L3-App", "", "ARQC");
    }
    else if (0x10 == (0x10 & *p))
    {
        logDataEx("L3-App", "", "TC");
    }
    else
    {
        logDataEx("L3-App", "", "AAC");
    }
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "\t      CDA performed");
    }
    if (0x04 == (0x04 & *p))
    {
        logDataEx("L3-App", "", "\t      qDDA signature returned");
    }
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "\t      Issuer Authentication NOT performed");
    }
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "\t      Issuer Authentication failed");
    }

    p++;
    /* Byte 2 */
    logDataEx("L3-App", "", "\t      PIN Try Counter: 0x%02X", (0xF0 & *p) >> 4);
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "\t      Offline PIN verification performed");
    }
    if (0x04 == (0x04 & *p))
    {
        logDataEx("L3-App", "", "\t      Offline PIN verification performed and PIN was NOT successfully verified");
    }
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "\t      PIN Try Limit has been exceeded");
    }
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "\t      Last Online Transaction Not Completed");
    }

    p++;
    /* Byte 3 */
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "\t      Issuer Authentication failed in previous transaction");
    }
    if (0x40 == (0x40 & *p))
    {
        logDataEx("L3-App", "", "\t      Issuer Authentication was NOT performed after online authorisation in previous transaction");
    }
    if (0x20 == (0x20 & *p))
    {
        logDataEx("L3-App", "", "\t      Go online on next transaction");
    }
    if (0x10 == (0x10 & *p))
    {
        logDataEx("L3-App", "", "\t      Balance Limit Exceeded");
    }
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "\t      Match found in additional check table");
    }
    if (0x04 == (0x04 & *p))
    {
        logDataEx("L3-App", "", "\t      Application blocked by issuer via CSU processing");
    }
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "\t      Void original and current transaction data mismatch");
    }
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "\t      Was unable to go online in previous transaction");
    }

    p++;
    /* Byte 4 */
    logDataEx("L3-App", "", "\t      Script Counter (Number of Successfully Processed Issuer Script Commands): 0x%02X", (0xF0 & *p) >> 4);
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "\t      Issuer Script command processing failed in previous transaction");
    }
    if (0x04 == (0x04 & *p))
    {
        logDataEx("L3-App", "", "\t      Offline Data Authentication failed");
    }
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "\t      RFU");
    }
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "\t      Was unable to go online");
    }

    p++;
    /* Byte 5 */
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "\t      Transaction type not supported");
    }
    if (0x40 == (0x40 & *p))
    {
        logDataEx("L3-App", "", "\t      RFU");
    }
    if (0x20 == (0x20 & *p))
    {
        logDataEx("L3-App", "", "\t      Transaction offline accepted");
    }
    if (0x10 == (0x10 & *p))
    {
        logDataEx("L3-App", "", "\t      RFU");
    }
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "\t      Service Key planted");
    }
    if (0x04 == (0x04 & *p))
    {
        logDataEx("L3-App", "", "\t      Service Balance received from Issuer");
    }
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "\t      Void original transaction not found");
    }
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "\t      Void original transaction expired");
    }

    p++;
    /* Byte 6 */
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "\t      Requested service not allowed for card product");
    }
    if (0x40 == (0x40 & *p))
    {
        logDataEx("L3-App", "", "\t      New Service Creation");
    }
    if (0x20 == (0x20 & *p))
    {
        logDataEx("L3-App", "", "\t      Service reallocation");
    }
    if (0x10 == (0x10 & *p))
    {
        logDataEx("L3-App", "", "\t      Transaction Initiated with Service");
    }
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "\t      Service update failed");
    }
    if (0x04 == (0x04 & *p))
    {
        logDataEx("L3-App", "", "\t      Service updated successfully");
    }
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "\t      Service created with global wallet initiated in first generate AC");
    }
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "\t      Service Process skipped");
    }

    p++;
    /* Byte 7 */
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "\t      Offline Balance is Insufficient");
    }
    if (0x40 == (0x40 & *p))
    {
        logDataEx("L3-App", "", "\t      CVM Limit Exceeded");
    }
    if (0x20 == (0x20 & *p))
    {
        logDataEx("L3-App", "", "\t      Invalid Void Transaction");
    }
    logDataEx("L3-App", "", "\t      RFU: 0x%02X\n", (0x1F & *p));

    p++;
    /* Byte 8 */
    if (0x80 == (0x80 & *p))
    {
        logDataEx("L3-App", "", "\t      Service Signature Failed");
    }
    if (0x40 == (0x40 & *p))
    {
        logDataEx("L3-App", "", "\t      Service Summary Failed");
    }
    if (0x20 == (0x20 & *p))
    {
        logDataEx("L3-App", "", "\t      Critical Service Update received");
    }
    if (0x10 == (0x10 & *p))
    {
        logDataEx("L3-App", "", "\t      Offline Spending Limit exceeded (Amount)");
    }
    if (0x08 == (0x08 & *p))
    {
        logDataEx("L3-App", "", "\t      Offline Txn. Limit exceeded (Counter)");
    }
    if (0x04 == (0x04 & *p))
    {
        logDataEx("L3-App", "", "\t      RFU");
    }
    if (0x02 == (0x02 & *p))
    {
        logDataEx("L3-App", "", "\t      Transaction date Format (YYMMDD) is incorrect");
    }
    if (0x01 == (0x01 & *p))
    {
        logDataEx("L3-App", "", "\t      Transaction time Format (HHmmss) is incorrect");
    }

    return;
}

void print_iad(uint8_t *buffer, size_t buffer_len)
{
    if (32 != buffer_len)
    {
        logDataEx("L3-App", "", "Invalid Issuer Application Data format!");
        return;
    }
    size_t i = 0;
    uint8_t *p = buffer;

    logDataEx("L3-App", "", "\tIssuer Application Data: ");
    for (i = 0; i < buffer_len; i++)
    {
        logDataEx("L3-App", "", "%02X", buffer[i]);
    }
    logDataEx("L3-App", "", "");
    /*
     * Byte 1: Length Indicator
     * Length of EMVco defined data in IAD. Set to ‘0F’
     */
    logDataEx("L3-App", "", "\t  Length Indicator: %02X", *p);

    p++;
    /*
     * Byte 2: CAII
     * Common Application Implementation Indicator
     */
    logDataEx("L3-App", "", "\t  CAII: %02X", *p);
    logDataEx("L3-App", "", "\t    IAD Format: ");
    if (0xA0 == (0xA0 & *p))
    {
        logDataEx("L3-App", "", "CCD Version 4.1");
    }
    else if (0xB0 == (0xB0 & *p))
    {
        logDataEx("L3-App", "", "RuPay Version");
    }
    else
    {
        logDataEx("L3-App", "", "Unknown");
    }
    logDataEx("L3-App", "", "\t    Cryptogram Version: ");
    if (0x05 == (0x05 & *p))
    {
        logDataEx("L3-App", "", "Triple DES");
    }
    else if (0x06 == (0x06 & *p))
    {
        logDataEx("L3-App", "", "AES");
    }
    else
    {
        logDataEx("L3-App", "", "Unknown");
    }

    p++;
    /*
     * Byte 3: DKI
     * Derivation Key Index
     */
    logDataEx("L3-App", "", "\t  DKI: %02X", *p);

    p++;
    /*
     * Byte 4 - 11: CVR
     * Card Verification results
     */
    logDataEx("L3-App", "", "\t  CVR: %02X%02X%02X%02X%02X%02X%02X%02X", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    print_cvr(p, 8);

    p += 8;
    /*
     * Byte 12 - 25: Counters
     * Counters
     */
    logDataEx("L3-App", "", "\t  Counters:");
    logDataEx("L3-App", "", "\t    Global balance: %02X%02X%02X%02X%02X%02X", p[0], p[1], p[2], p[3], p[4], p[5]);
    logDataEx("L3-App", "", "\t    Service Balance current transaction: %02X%02X%02X%02X%02X%02X", p[6], p[7], p[8], p[9], p[10], p[11]);
    logDataEx("L3-App", "", "\t    RFU: %02X%02X", p[12], p[13]);

    p += 14;
    /*
     * Byte 26 - 32: IDD
     * Issuer Discretionary Data
     */
    logDataEx("L3-App", "", "\t  IDD:");
    logDataEx("L3-App", "", "\t    Service ID selected in current transaction: %02X%02X", p[0], p[1]);
    logDataEx("L3-App", "", "\t    Service Management Info: %02X%02X", p[2], p[3]);
    print_service_management_info(&p[2], 2);
    logDataEx("L3-App", "", "\t    Service Compatibility Version (from Application Control): %02X", p[4]);
    logDataEx("L3-App", "", "\t    Host Compatibility Version: %02X", p[5]);
    logDataEx("L3-App", "", "\t    RFU: %02X", p[6]);

    return;
}

static void print_data_record(uint8_t *data_record, size_t data_record_len)
{
    int rc = TLV_RC_OK;
    struct tlv *tlv_data_record = NULL;
    struct tlv *tlv_obj = NULL;
    uint8_t buffer[4 * 1024];
    size_t buffer_len = sizeof(buffer);

    rc = tlv_parse(data_record, data_record_len, &tlv_data_record);
    if (TLV_RC_OK != rc)
    {
        logDataEx("L3-App", "", "%s: data record format is corrupted", __func__);
        return;
    }
    logDataEx("L3-App", "", "\t############# Issuer Application Data (IAD) ##############");
    /* Issuer Application Data (IAD) */
    tlv_obj = tlv_find(tlv_data_record, EMV_ID_ISSUER_APPLICATION_DATA);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    print_iad(buffer, buffer_len);
    logDataEx("L3-App", "", "\t##########################################################");

    tlv_free(tlv_data_record);
    return;
}

static void print_discretionary_data(uint8_t *data_record, size_t data_record_len)
{
    logDataEx("L3-App", "", "Discretionary record to be printed");
}

void processOutcome(const void *outcome, size_t outcomeLen)
{
    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    if (appConfig.printProcessOutcome == false)
    {
        return;
    }

    int rc = TLV_RC_OK;

    struct tlv *tlv_outcome = NULL;
    struct tlv *tlv_e2ee_outcome = NULL;
    struct tlv *tlv_obj = NULL;
    char hex_buffer[4 * 1024];
    uint8_t buffer[4 * 1024];
    size_t buffer_len = sizeof(buffer);

    if ((NULL == outcome) || (0 == outcomeLen))
    {
        // printf("%s: outcome is invalid\n", __func__);
        return;
    }

    rc = tlv_parse(outcome, outcomeLen, &tlv_outcome);
    if (TLV_RC_OK != rc)
    {
        logDataEx("L3-App", "", "%s: outcome format is corrupted", __func__);
        return;
    }

    /*
    FILE *fp;
    fp=fopen("outcome.txt", "a");
    if(fp != NULL) {
        for (size_t i = 0; i < outcomeLen; i++) {
            fprintf(fp, "%02X", ((uint8_t *)outcome)[i]);
        }
        fclose(fp);
    }*/

    printf("Outcome: ");
    for (size_t i = 0; i < outcomeLen; i++)
    {
        printf("%02X", ((uint8_t *)outcome)[i]);
    }
    printf("\n");

    printf("\t############# OUTCOME ##############\n");
    printf("Outcome: \t\t%s\n", getOutcomeStatus(tlv_outcome));
    /* Entry Point Start Point */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_EP_START_POINT);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    printf("\tStart:\t\t\t");
    switch (buffer[0])
    {
    case emvco_start_na:
        printf("N/A\n");
        break;
    case emvco_start_a:
        printf("A\n");
        break;
    case emvco_start_b:
        printf("B\n");
        break;
    case emvco_start_c:
        printf("C\n");
        break;
    case emvco_start_d:
        printf("D\n");
        break;
    default:
        printf("UNKNOWN\n");
        break;
    }

    /* Online Response Data */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome),
                       FEIG_ID_OUTCOME_ONLINE_RESPONSE_DATA);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);

    printf("\tOnline Response Data:\t");
    switch (buffer[0])
    {
    case emvco_ord_na:
        printf("N/A\n");
        break;
    case emvco_ord_emv_data:
        printf("EMV Data\n");
        break;
    case emvco_ord_any:
        printf("EMV Any\n");
        break;
    default:
        printf("UNKNOWN\n");
        break;
    }
    /* Receipt */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_RECEIPT);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    printf("\tReceipt:\t");
    if (buffer[0] > 0)
        printf("\tPresent\n");
    else
        printf("\tN/A\n");

    /* CVM */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_CVM);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    printf("\tCVM:\t\t\t");
    switch (buffer[0])
    {
    case emvco_cvm_na:
        printf("N/A\n");
        break;
    case emvco_cvm_online_pin:
        printf("Online Pin\n");
        break;
    case emvco_cvm_confirmation_code_verified:
        printf("Confirmation Code verified\n");
        break;
    case emvco_cvm_obtain_signature:
        printf("Obtain signature\n");
        break;
    case emvco_cvm_no_cvm:
        printf("No CVM\n");
        break;
    default:
        printf("UNKNOWN\n");
        break;
    }

    /* UI Request on Outcome ignored */

    /* UI Request on Restart ignored */

    /* Data Record */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_DATA_RECORD);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    printf("\tData record:");
    if (buffer_len > 0)
    {
        printf("\t\tPresent\n");
        printf("\t\t\t\t");
        printf("%s\n", libtlv_bin_to_hex(buffer, buffer_len, hex_buffer));
        print_data_record(buffer, buffer_len);
    }
    else
    {
        printf("\t\tN/A\n");
    }

    /* Discretionary Data */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_DISCRETIONARY_DATA);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    printf("\tDiscretionary Data:");
    if (buffer_len > 0)
    {
        printf("\tPresent\n");
        printf("\t\t\t\t");
        printf("%s\n", libtlv_bin_to_hex(buffer, buffer_len, hex_buffer));
        print_discretionary_data(buffer, buffer_len);
    }
    else
    {
        printf("\tN/A\n");
    }

    /* Alternate Interface Preference */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome),
                       FEIG_ID_OUTCOME_ALTERNATE_INTERFACE_PREFERENCE);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    printf("\tAlternative interface:\t");
    switch (buffer[0])
    {
    case emvco_aip_na:
        printf("N/A\n");
        break;
    case emvco_aip_contact_chip:
        printf("Contact Chip\n");
        break;
    case emvco_aip_magstripe:
        printf("Magstripe\n");
        break;
    case emvco_aip_both:
        printf("Both - Contact Chip + Magstripe\n");
        break;
    default:
        printf("UNKNOWN\n");
        break;
    }

    /* Field Off Hold Time */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome),
                       FEIG_ID_OUTCOME_FIELD_OFF_REQUEST_HOLD_TIME);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    printf("\tField Off Hold Time:\t");
    switch (buffer[0])
    {
    case emvco_field_off_request_na:
        printf("N/A\n");
        break;
    default:
        printf("%d\n", buffer[0]);
        break;
    }

    tlv_e2ee_outcome = tlv_find(tlv_get_child(tlv_outcome),
                                FEIG_ID_E2EE_OUTCOME_DATA);

    if (tlv_e2ee_outcome)
    {
        printf("\t------------ E2EE Data -------------\n");
        /* PAN */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           EMV_ID_APPLICATION_PAN);
        buffer_len = sizeof(buffer);
        tlv_encode_value(tlv_obj, buffer, &buffer_len);
        printf("\tPAN:");
        if (buffer_len > 0)
        {
            printf("\t\t\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }
        else
        {
            printf("\t\t\tN/A\n");
        }

        /* Track 2 Equivalent Data */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           EMV_ID_TRACK2_EQUIVALENT_DATA);
        buffer_len = sizeof(buffer);
        tlv_encode_value(tlv_obj, buffer, &buffer_len);
        printf("\tTrack 2 Equivalent Data:");
        if (buffer_len > 0)
        {
            printf("\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }
        else
        {
            printf("\t\t\tN/A\n");
        }

        /* E2EE List of encrypted tags */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_E2EE_ENCRYPTED_TAG_LIST);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tE2EE List of encrypted tags:\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

        /* E2EE List of removed tags */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_E2EE_REMOVED_TAG_LIST);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tE2EE List of removed tags:\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

        /* E2EE Encrypted Data */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_E2EE_ENCRYPTED_DATA);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tE2EE Encrypted Data:\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

        /* DUKPT KSN */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_E2EE_DUKPT_KSN);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tDUKPT KSN:\t\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

#ifdef FEIG_ID_POSEIDON_KEY_INDEX
        /* Future USE */

        /* POSEIDON Key Index */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_POSEIDON_KEY_INDEX);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tPOSEIDON Key Index:\t\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

        /* POSEIDON Key Set Number */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_POSEIDON_KEY_SET_NUMBER);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tPOSEIDON Key Set Number:\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

        /* POSEIDON Security Algorithm ID */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_POSEIDON_SECURITY_ALGORITHM_ID);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tPOSEIDON Security Algorithm ID:\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

        /* POSEIDON Key Version */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_POSEIDON_KEY_VERSION);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tPOSEIDON Key Version:\t\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

        /* POSEIDON Encrypted Data */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_POSEIDON_ENCRYPTED_DATA);
        while (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tPOSEIDON Encrypted:\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));

            tlv_obj = tlv_find(tlv_get_next(tlv_obj),
                               FEIG_ID_POSEIDON_ENCRYPTED_DATA);
        };
#endif
#ifdef FEIG_ID_E2EE_TRANSARMOR_PAN
        /* TRANSARMOR Encrypted PAN */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_E2EE_TRANSARMOR_PAN);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tTRANSARMOR Encrypted PAN:\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }

        /* TRANSARMOR Encrypted Track2 */
        tlv_obj = tlv_find(tlv_get_child(tlv_e2ee_outcome),
                           FEIG_ID_E2EE_TRANSARMOR_TRACK2);
        if (tlv_obj)
        {
            buffer_len = sizeof(buffer);
            tlv_encode_value(tlv_obj, buffer, &buffer_len);
            printf("\tTRANSARMOR Encrypted Track2:\t%s\n",
                   libtlv_bin_to_hex(buffer, buffer_len,
                                     hex_buffer));
        }
#endif
        printf("\t------------------------------------\n\n");
    }

    /* Removal Timeout */
    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_REMOVAL_TIMEOUT);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);
    printf("\tRemoval Timeout:\t%d\n", buffer[0]);
    printf("\t####################################\n\n");

    tlv_free(tlv_obj);
    tlv_free(tlv_e2ee_outcome);
    tlv_free(tlv_outcome);
}

int generateLog(const void *outcome, size_t outcome_len, const char *fileName, int trxCounter)
{
    /*
    if (appConfig.isDebugEnabled != 1)
    {
        return 0;
    }*/

    int rc = TLV_RC_OK;
    struct tlv *tlv_outcome = NULL;
    struct tlv *tlv_obj = NULL;
    struct tlv *tlv_obj_trace_apdu = NULL;
    uint8_t buffer[4 * 1024];
    size_t buffer_len = sizeof(buffer);
    char log_file_name[80];
    FILE *fp;

    if ((NULL == outcome) || (0 == outcome_len))
    {
        // printf("%s: outcome is invalid\n", __func__);
        return TLV_RC_INVALID_ARG;
    }

    sprintf(log_file_name, "logs/%s.txt", fileName);
    umask(0);
    int fd = open(log_file_name, O_CREAT | O_WRONLY | O_APPEND, 0666);
    fp = (fd < 0) ? NULL : fdopen(fd, "a");
    if (NULL == fp)
    {
        if (fd >= 0)
            close(fd);
        printf("%s: unable to create logfile\n", __func__);
        return TLV_RC_IO_ERROR;
    }

    // Parse DER-TLV and create tlv object
    rc = tlv_parse(outcome, outcome_len, &tlv_outcome);
    if (TLV_RC_OK != rc)
    {
        printf("%s: outcome format is corrupted, ret_code : [%d]\n", __func__, rc);
        if (NULL != tlv_outcome)
        {
            tlv_free(tlv_outcome);
        }
        fclose(fp);
        return rc;
    }

    // Check for constructed TLV
    if (tlv_is_constructed(tlv_outcome) != 1)
    {
        printf("%s: outcome is not constructed TLV\n", __func__);
        tlv_free(tlv_outcome);
        fclose(fp);
        return TLV_RC_IO_ERROR;
    }

    // Find FEIG_ID_TRACE_APDU [FFFEF010] from the tlv object
    tlv_obj_trace_apdu = tlv_deep_find(tlv_get_child(tlv_outcome), FEIG_ID_TRACE_APDU);
    while (NULL != tlv_obj_trace_apdu)
    {
        // Find FEIG_ID_TRACE_C_APDU [DFFEF010] Command from the tlv object
        tlv_obj = tlv_deep_find(tlv_get_child(tlv_obj_trace_apdu), FEIG_ID_TRACE_C_APDU);
        if (NULL == tlv_obj)
        {
            printf("%s : unable to find the tag FEIG_ID_TRACE_C_APDU [DFFEF010]", __func__);
            rc = TLV_RC_INVALID_ARG;
            break;
        }

        // Get the value of FEIG_ID_TRACE_C_APDU [DFFEF010] Command from tlv object node
        buffer_len = sizeof(buffer);
        memset(buffer, 0x00, sizeof(buffer));
        rc = tlv_encode_value(tlv_obj, buffer, &buffer_len);
        if (TLV_RC_OK != rc)
        {
            printf("%s : unable to encode value for tag FEIG_ID_TRACE_C_APDU [DFFEF010] ret_code : [%d]", __func__, rc);
            break;
        }

        // Write IFD value into log file
        fprintf(fp, "IFD : ");
        for (int i = 0; i < buffer_len; i++)
        {
            fprintf(fp, "%02X ", ((uint8_t *)buffer)[i]);
        }

        // Find FEIG_ID_TRACE_R_APDU [DFFEF011] Response from the tlv object
        tlv_obj = tlv_deep_find(tlv_get_next(tlv_obj), FEIG_ID_TRACE_R_APDU);
        if (NULL == tlv_obj)
        {
            printf("%s : unable to find the tag FEIG_ID_TRACE_R_APDU [DFFEF011]", __func__);
            rc = TLV_RC_INVALID_ARG;
            break;
        }

        // Get the value of FEIG_ID_TRACE_R_APDU [DFFEF011] Response from tlv object node
        buffer_len = sizeof(buffer);
        memset(buffer, 0x00, sizeof(buffer));
        rc = tlv_encode_value(tlv_obj, buffer, &buffer_len);
        if (TLV_RC_OK != rc)
        {
            printf("%s : unable to encode value for tag FEIG_ID_TRACE_R_APDU [DFFEF011], ret_code : [%d]\n", __func__, rc);
            break;
        }

        // Write ICC value into log file
        fprintf(fp, "\nICC : ");
        for (int i = 0; i < buffer_len; i++)
        {
            fprintf(fp, "%02X ", ((uint8_t *)buffer)[i]);
        }

        // Timing DFFEF002
        // Find FEIG_ID_TRACE_TIME_BEFORE [DFFEF002] Response from the tlv object
        tlv_obj = tlv_deep_find(tlv_get_next(tlv_obj), FEIG_ID_TRACE_TIME_BEFORE);
        if (NULL == tlv_obj)
        {
            printf("%s : unable to find the tag FEIG_ID_TRACE_TIME_BEFORE [DFFEF002]", __func__);
            rc = TLV_RC_INVALID_ARG;
            break;
        }

        // Get the value of FEIG_ID_TRACE_TIME_BEFORE [DFFEF002] Response from tlv object node
        buffer_len = sizeof(buffer);
        memset(buffer, 0x00, sizeof(buffer));
        rc = tlv_encode_value(tlv_obj, buffer, &buffer_len);
        if (TLV_RC_OK != rc)
        {
            printf("%s : unable to encode value for tag FEIG_ID_TRACE_TIME_BEFORE [DFFEF002], ret_code : [%d]\n", __func__, rc);
            break;
        }

        // Write Timing value into log file
        fprintf(fp, "\nTiming 1 : ");
        for (int i = 0; i < buffer_len; i++)
        {
            fprintf(fp, "%02X", ((uint8_t *)buffer)[i]);
        }

        // Timing DFFEF003
        // Find FEIG_ID_TRACE_TIME_AFTER [DFFEF003] Response from the tlv object
        tlv_obj = tlv_deep_find(tlv_get_next(tlv_obj), FEIG_ID_TRACE_TIME_AFTER);
        if (NULL == tlv_obj)
        {
            printf("%s : unable to find the tag FEIG_ID_TRACE_TIME_AFTER [DFFEF003]", __func__);
            rc = TLV_RC_INVALID_ARG;
            break;
        }

        // Get the value of FEIG_ID_TRACE_TIME_AFTER [DFFEF003] Response from tlv object node
        buffer_len = sizeof(buffer);
        memset(buffer, 0x00, sizeof(buffer));
        rc = tlv_encode_value(tlv_obj, buffer, &buffer_len);
        if (TLV_RC_OK != rc)
        {
            printf("%s : unable to encode value for tag FEIG_ID_TRACE_TIME_AFTER [DFFEF003], ret_code : [%d]\n", __func__, rc);
            break;
        }

        // Write Timing value into log file
        fprintf(fp, "\nTiming 2 : ");
        for (int i = 0; i < buffer_len; i++)
        {
            fprintf(fp, "%02X", ((uint8_t *)buffer)[i]);
        }

        // Find next FEIG_ID_TRACE_APDU [FFFEF010] from the tlv object
        fprintf(fp, "\n\n");
        tlv_obj_trace_apdu = tlv_deep_find(tlv_get_next(tlv_obj_trace_apdu), FEIG_ID_TRACE_APDU);
    }

    // Clean up
    tlv_free(tlv_outcome);
    fclose(fp);

    return TLV_RC_OK;
}
