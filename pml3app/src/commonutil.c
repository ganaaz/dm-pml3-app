#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <uuid/uuid.h>
#include <libpay/tlv.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <openssl/hmac.h>

#include <feig/fetrm.h>
#include <feig/leds.h>
#include <feig/buzzer.h>

#include "include/commonutil.h"
#include "include/config.h"
#include "include/logutil.h"
#include "include/appcodes.h"
#include "include/commandmanager.h"

extern struct applicationData appData;
extern struct applicationConfig appConfig;
extern pthread_mutex_t lock;
extern struct transactionData currentTxnData;
extern struct serviceMetadata *selectedService;

mode_t allPerm = S_IRWXU | S_IRWXG | S_IRWXO; // 777 permissions

/**
 * To handle the signals
 **/
void signalCallbackBandler(int signum)
{
    logError("Caught signal %d\n", signum);
    displayLight(LED_ST_APP_EXITING);
    exit(signum);
}

/**
 * To read a file data
 */
unsigned char *readFile(const char fileName[])
{
    FILE *file = fopen(fileName, "rb");
    long int size = findSize(fileName);
    logData("Size of the file : %d", size);

    unsigned char *data = malloc(size);
    logData("Data allocated of size : %d", size);

    unsigned char k;
    int counter = 0;
    while (!feof(file))
    {
        fread(&k, sizeof(unsigned char), 1, file);
        data[counter] = k;
        counter++;
    }
    logData("Read Done, counter : %d", counter);
    return data;
}

/**
 * to find the size of the file
 */
long int findSize(const char fileName[])
{
    // opening the file in read mode
    FILE *fp = fopen(fileName, "r");

    // checking if the file exist or not
    if (fp == NULL)
    {
        logError("File Not Found!");
        return -1;
    }

    fseek(fp, 0L, SEEK_END);

    // calculating the size of the file
    long int res = ftell(fp);

    // closing the file
    fclose(fp);

    return res;
}

/**
 * Convert byte to hex
 **/
void byteToHex(const unsigned char *data, size_t dataLen, char *output)
{
    int j = 0;
    for (j = 0; j < dataLen; j++)
    {
        output[2 * j] = (data[j] >> 4) + 48;
        output[2 * j + 1] = (data[j] & 15) + 48;

        if (output[2 * j] > 57)
        {
            output[2 * j] += 7;
        }

        if (output[2 * j + 1] > 57)
        {
            output[2 * j + 1] += 7;
        }
    }

    output[2 * j] = '\0';
}

/**
 * Convert data to upper and update the same data
 **/
void toUpper(char *data)
{
    for (int i = 0; data[i] != '\0'; i++)
    {
        if (data[i] >= 'a' && data[i] <= 'z')
        {
            data[i] = data[i] - 32;
        }
    }
}

/**
 * To get the device id
 */
void getDeviceId(char *deviceId)
{
    uint32_t device_id;
    int rd = fetrm_get_devid(&device_id);
    logData("Device id result : %d", rd);
    logData("Device id : %lu", device_id);
    sprintf(deviceId, "%08" PRIX32, device_id);
    // sprintf(deviceId, "%s", "19117381634");
}

/**
 * To create the network interface file and copy it
 */
void createAndCopyNetworkInterfaceFile(struct message reqMessage)
{
    FILE *networkFile;
    networkFile = fopen("interfaces", "w+");
    bool isValid = false;
    logInfo("Generating ip interface file");
    if (strcmp(reqMessage.ipData.ipMode, IP_MODE_DYNAMIC) == 0)
    {
        logInfo("Generating file for dynamic ip");
        isValid = true;
        fprintf(networkFile, "auto lo\n");
        fprintf(networkFile, "iface lo inet loopback\n");
        fprintf(networkFile, "\n");
        fprintf(networkFile, "auto eth0\n");
        fprintf(networkFile, "iface eth0 inet manual\n");
        fprintf(networkFile, "	pre-up /sbin/udhcpc -b -p /var/run/udhcpc.eth0.pid -i eth0 -s /root/etc/network/default.script\n");
        fprintf(networkFile, "\n");
    }
    else if (strcmp(reqMessage.ipData.ipMode, IP_MODE_STATIC) == 0)
    {
        logInfo("Generating file for static ip");
        isValid = true;
        fprintf(networkFile, "auto lo\n");
        fprintf(networkFile, "iface lo inet loopback\n");
        fprintf(networkFile, "\n");
        fprintf(networkFile, "auto eth0\n");
        fprintf(networkFile, "iface eth0 inet static\n");
        fprintf(networkFile, " address ");
        fprintf(networkFile, reqMessage.ipData.ipAddress);
        fprintf(networkFile, "\n");
        fprintf(networkFile, " netmask ");
        fprintf(networkFile, reqMessage.ipData.netmask);
        fprintf(networkFile, "\n");
        fprintf(networkFile, " gateway ");
        fprintf(networkFile, reqMessage.ipData.gateway);
        fprintf(networkFile, "\n");
        fprintf(networkFile, "\n");
    }
    fclose(networkFile);

    if (isValid)
    {
        logInfo("Copying the interface file to /root/etc/network/interfaces");
        system("cp interfaces /root/etc/network/interfaces");
        chmod("/root/etc/network/interfaces", allPerm);
        logInfo("Permission updated to rwx all");
        logInfo("Copy Done");
    }
    else
    {
        logWarn("Invalid ip mode, nothing changed");
    }
}

/**
 * to create and copy resolve file
 */
void createAndCopyResolvFile(struct message reqMessage)
{
    if (strlen(reqMessage.ipData.dns) != 0 || strlen(reqMessage.ipData.searchDomain) != 0)
    {
        logInfo("DNS / Search domain data is available, so copying to resolve.conf");
        FILE *resolveFile;
        resolveFile = fopen("resolv.conf", "w+");
        if (strlen(reqMessage.ipData.searchDomain) != 0)
        {
            fprintf(resolveFile, "search ");
            fprintf(resolveFile, reqMessage.ipData.searchDomain);
            fprintf(resolveFile, "\n");
        }

        if (strlen(reqMessage.ipData.dns) != 0)
        {
            fprintf(resolveFile, "nameserver ");
            fprintf(resolveFile, reqMessage.ipData.dns);
            fprintf(resolveFile, "\n");
        }

        if (strlen(reqMessage.ipData.dns2) != 0)
        {
            fprintf(resolveFile, "nameserver ");
            fprintf(resolveFile, reqMessage.ipData.dns2);
            fprintf(resolveFile, "\n");
        }

        if (strlen(reqMessage.ipData.dns3) != 0)
        {
            fprintf(resolveFile, "nameserver ");
            fprintf(resolveFile, reqMessage.ipData.dns3);
            fprintf(resolveFile, "\n");
        }

        if (strlen(reqMessage.ipData.dns4) != 0)
        {
            fprintf(resolveFile, "nameserver ");
            fprintf(resolveFile, reqMessage.ipData.dns4);
            fprintf(resolveFile, "\n");
        }

        fprintf(resolveFile, "\n");
        fclose(resolveFile);

        logInfo("Copying the resolve.conf file to /root/etc/resolv.conf");
        system("cp resolv.conf /root/etc/resolv.conf");
        chmod("/root/etc/resolv.conf", allPerm);
        logInfo("Permission updated to rwx all");
        logInfo("Copy Done");
    }
    else
    {
        logWarn("First dns is empty or search domain is empty, nothing changed");
    }
}

/**
 * To get the days from Jan 1 2020
 */
int getDaysFromJan12020()
{
    struct tm start_date = {0};
    start_date.tm_year = 2020 - 1900; // Year since 1900
    start_date.tm_mon = 0;            // January (0-11)
    start_date.tm_mday = 1;           // Day of the month (1-31)

    // Get the current date
    time_t now = time(NULL);
    struct tm *current_date = localtime(&now);

    // Calculate the number of days between the two dates
    int days = daysBetween(start_date, *current_date);
    return days;
}

/**
 * To get the dates between
 */
int daysBetween(struct tm start, struct tm end)
{
    time_t start_time = mktime(&start);
    time_t end_time = mktime(&end);
    double seconds = difftime(end_time, start_time);
    int days = seconds / (60 * 60 * 24);
    return days;
}

/**
 * Get the current time in seconds
 **/
long long getCurrentSeconds()
{
    struct timeval te;
    gettimeofday(&te, NULL);                                         // get current time
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
    return milliseconds;
}

/**
 * To get the seconds since start of the day in local time
 */
long long getSecondsSinceStartOfDay()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    time_t now = te.tv_sec;
    struct tm *local_time = localtime(&now);
    long long seconds_since_start_of_day = local_time->tm_hour * 3600 + local_time->tm_min * 60 + local_time->tm_sec;
    return seconds_since_start_of_day;
}

/**
 * Convert the amount to hex string of 12 characters
 **/
void convertAmount(uint64_t amount, char *output)
{
    uint8_t amountAuthorised_9F02[6] = {0};
    libtlv_u64_to_bcd(amount, amountAuthorised_9F02, sizeof(amountAuthorised_9F02));
    sprintf(output, "%02X%02X%02X%02X%02X%02X",
            amountAuthorised_9F02[0], amountAuthorised_9F02[1],
            amountAuthorised_9F02[2], amountAuthorised_9F02[3],
            amountAuthorised_9F02[4], amountAuthorised_9F02[5]);
}

/**
 * Update the time and date in the transaction object
 **/
struct transactionData updateTransactionDateTime(struct transactionData trxData)
{
    time_t rawTime = time(NULL);
    struct tm *ptm = localtime(&rawTime);
    int iYear = ptm->tm_year - 100;
    logData("Year from time struct : %d", iYear);
    char year[5];
    snprintf(year, 5, "%d", 2000 + iYear);
    logData("Current Year : %s", year);
    strcpy(trxData.year, year);
    strftime(trxData.time, -1, "%H%M%S", ptm);
    logData("Transaction Time : %s", trxData.time);
    strftime(trxData.date, -1, "%m%d", ptm);
    logData("Transaction Date : %s", trxData.date);

    struct tm *ptmGMT = gmtime(&rawTime);
    strftime(trxData.gmtTime, sizeof(trxData.gmtTime), "%Y-%m-%d %H:%M:%S GMT", ptmGMT);
    strcat(trxData.gmtTime, "\0");
    logData("Transaction GMT Time : %s", trxData.gmtTime);

    return trxData;
}

/**
 * To remove the ICC Track2 and Pan Tags
 * This will update the currentTxnData => iccData and iccDataLen
 * This will also update label and effective date
 */
void removeIccTags(uint8_t buffer[4096], size_t buffer_len)
{
    char iccData[4096];
    memset(iccData, 0, 4096);
    strcpy(iccData, "");

    struct tlv *tlvData = NULL;
    tlv_parse(buffer, buffer_len, &tlvData);
    struct tlv *tlv_attr = NULL;

    for (tlv_attr = tlvData; tlv_attr; tlv_attr = tlv_get_next(tlv_attr))
    {
        uint8_t tag[TLV_MAX_TAG_LENGTH];
        size_t tag_sz = sizeof(tag);
        tlv_encode_identifier(tlv_attr, tag, &tag_sz);
        uint8_t value[256];
        size_t value_sz = sizeof(value);
        tlv_encode_value(tlv_attr, value, &value_sz);

        if (tag[0] == 0x57 || tag[0] == 0x5A)
        {
            continue;
        }

        char temp[5];
        byteToHex(tag, tag_sz, temp);
        strcat(iccData, temp);
        char temp2[5];
        sprintf(temp2, "%02X", value_sz);
        strcat(iccData, temp2);
        char temp3[256];
        byteToHex(value, value_sz, temp3);
        strcat(iccData, temp3);

        if (tag[0] == 0x5F && tag[1] == 0x25)
        {
            strcpy(currentTxnData.effectiveDate, temp3);
        }
    }

    strcpy(currentTxnData.iccData, iccData);
    logData("ICC DATA : %s", currentTxnData.iccData);
    currentTxnData.iccDataLen = strlen(iccData);
    tlv_free(tlvData);
    tlv_free(tlv_attr);
}

/**
 * Convert hex to byte data
 **/
int hexToByte(const char *hexData, unsigned char *output)
{
    size_t len = strlen(hexData);
    if (len % 2 != 0)
    {
        return -1;
    }

    size_t finalLen = len / 2;
    for (size_t inIdx = 0, outIdx = 0; outIdx < finalLen; inIdx += 2, outIdx++)
    {
        if ((hexData[inIdx] - 48) <= 9 && (hexData[inIdx + 1] - 48) <= 9)
        {
            goto convert;
        }
        else
        {
            if ((hexData[inIdx] - 65) <= 5 && (hexData[inIdx + 1] - 65) <= 5)
            {
                goto convert;
            }
            else
            {
                return -1;
            }
        }
    convert:
        output[outIdx] = (hexData[inIdx] % 32 + 9) % 25 * 16 + (hexData[inIdx + 1] % 32 + 9) % 25;
    }

    return 0;
}

/**
 * Check whether the minimum required firmware is installed
 **/
bool isMinimumFirmwareInstalled(void)
{
    char version[256] = {0};
    int rc = fetrm_get_firmware_string(version, sizeof(version));
    if (rc != 0)
    {
        fprintf(stderr, "Error getting firmware string\n");
        return false;
    }

    logInfo("Installed firmware: %s", version);

    uint8_t major = 0, minor = 0, feature = 0, rev = 0;
    rc = sscanf(version, "%*2s%" SCNx8 ".%" SCNx8 ".%" SCNx8 "-%*2s.%" SCNx8 "", &major, &minor, &feature, &rev);
    if (rc < 0)
    {
        fprintf(stderr, "Error getting version info from firmware string\n");
        return false;
    }

    uint32_t min_version = (0x02 << 24) + (0x01 << 16) + (0x00 << 8) + (0x32 << 0);
    uint32_t current_version = (major << 24) + (minor << 16) + (feature << 8) + (rev << 0);

    if (current_version < min_version)
    {
        fprintf(stderr, "Minimum firmware version xx02.01.00-xx.32-x-x required !\n");
        return false;
    }

    return true;
}

/**
 * Generate a unique UUID
 **/
void generateUUID(char uuidData[UUID_STR_LEN])
{
    uuid_t b;
    uuid_generate_time_safe(b);
    uuid_unparse_lower(b, uuidData);
}

/**
 * OrderId - 2022060921541300130810440488
    Break Up-year-2022(YYYY)
    date-0609(MMDD)
    time-215413(HHMMSS)
    invoiceNumber-001308
    tid-10440488
*/
void generateOrderId()
{
    logData("Txn counter : %ld", currentTxnData.txnCounter);
    char orderId[29];

    strcpy(orderId, currentTxnData.year);
    strcat(orderId, currentTxnData.date);
    strcat(orderId, currentTxnData.time);
    strcat(orderId, currentTxnData.stan);
    strcat(orderId, appConfig.terminalId);

    strcpy(currentTxnData.orderId, orderId);
    logData("Order Id Generated : %s", currentTxnData.orderId);
    logData("Order Id length : %d", strlen(currentTxnData.orderId));
}

/**
 * To delete a log file
 */
int deleteLogFile(struct message reqMessage)
{
    // check whether the file name is valid
    char *payTmfound = strstr(reqMessage.delFileData.fileName, "paytm.log");

    if (payTmfound != NULL)
    {
        logData("Paytm File name is valid : %s", reqMessage.delFileData.fileName);
        if (remove(reqMessage.delFileData.fileName) == 0)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }

    char *found = strstr(reqMessage.delFileData.fileName, "l3app.log");

    if (found == NULL)
    {
        return -2;
    }
    logData("Log File name is valid : %s", reqMessage.delFileData.fileName);
    if (remove(reqMessage.delFileData.fileName) == 0)
    {
        return 0;
    }

    return -1;
}

/**
 * Print the current inmemory transaction data for debug purpose
 **/
void printCurrentTxnData(struct transactionData trxData)
{
    logData("=========================================");

    logData("Current Transaction Data");
    logData("Unique Id : %s", trxData.transactionId);
    logData("Transaction Type : %s", trxData.trxType);
    logData("Transaction Bin : %02X", trxData.trxTypeBin);
    logData("Processing Code : %s", trxData.processingCode);
    logData("Counter : %d", trxData.txnCounter);
    logData("Stan : %s", trxData.stan);
    logData("Amount String : %s", trxData.sAmount);
    logData("Time : %s", trxData.time);
    logData("Date : %s", trxData.date);
    logData("Year : %s", trxData.year);
    logData("AID : %s", trxData.aid);
    logData("Masked PAN : %s", trxData.maskPan);
    logData("Token : %s", trxData.token);
    logData("Effective Date : %s", trxData.effectiveDate);
    logData("Service Id : %s", trxData.serviceId);
    logData("Service Index : %s", trxData.serviceIndex);
    logData("Service Data : %s", trxData.serviceData);
    logData("Service Control : %s", trxData.serviceControl);
    logData("Service Balance : %s", trxData.serviceBalance);
    logData("ICC Data : %s", trxData.iccData);
    logData("ICC Data Len : %d", trxData.iccDataLen);
    logData("KSN : %s", trxData.ksn);
    logData("Track 2 Enc : %s", trxData.track2Enc);
    logData("Pan Encypted : %s", trxData.panEncrypted);
    logData("Exp Date Encypted : %s", trxData.expDateEnc);
    logData("Mac Ksn : %s", trxData.macKsn);
    logData("Mac : %s", trxData.mac);
    // logData("Plain Pan : %s", trxData.plainPan); // TODO : Remove
    // logData("Plain Exp Date : %s", trxData.plainExpDate); // TODO : Remove
    // logData("Plain Track 2 : %s", trxData.plainTrack2); // TODO : Remove
    logData("Order id : %s", trxData.orderId);
    logData("Transaction Status : %s", trxData.txnStatus);
    logData("Is Balance Update : %s", trxData.isImmedOnline ? "true" : "false");
    logData("Is Service Creation : %s", trxData.isServiceCreation ? "true" : "false");
    logData("Update Amount : %s", trxData.updatedAmount);
    logData("Update Balance : %s", trxData.updatedBalance);
    logData("Host Response : %s", trxData.hostResponseCode);
    logData("Money Add Trx Type : %s", trxData.moneyAddTrxType);
    logData("Check Date : %s", trxData.checkDate);
    logData("Check Date Result : %s", trxData.checkDateResult);
    logData("Trx Issue Detail : %s", trxData.trxIssueDetail);
    logData("Is Card Present Sent : %s", trxData.cardPresentedSent ? "true" : "false");
    logData("Is Rupay Txn : %s", trxData.isRupayTxn ? "true" : "false");
    logData("Is Gate Open : %s", trxData.isGateOpen ? "true" : "false");
    logData("GMT Time : %s", trxData.gmtTime);

    logData("=========================================");
}

/**
 * Mask the pan and return the masked pan
 **/
const char *maskPan(const char *pan)
{
    int len = strlen(pan);
    char *maskedPan = (char *)malloc(len + 1);
    memset(maskedPan, '*', len);

    if (len < 5)
        return pan;

    // First 6 characters
    maskedPan[0] = pan[0];
    maskedPan[1] = pan[1];
    maskedPan[2] = pan[2];
    maskedPan[3] = pan[3];
    maskedPan[4] = pan[4];
    maskedPan[5] = pan[5];

    // Last 4 characters
    maskedPan[len - 1] = pan[len - 1];
    maskedPan[len - 2] = pan[len - 2];
    maskedPan[len - 3] = pan[len - 3];
    maskedPan[len - 4] = pan[len - 4];

    maskedPan[len] = '\0';

    return maskedPan;
}

/**
 * Do the mutex lock
 **/
void doLock()
{
    pthread_mutex_lock(&lock);
}

/**
 * Release the mutex lock
 **/
void doUnLock()
{
    pthread_mutex_unlock(&lock);
}

/**
 * Reset the transaction data after every txn
 **/
void resetTransactionData()
{
    logData("Resetting the transaction data");
    resetSecondTap();
    selectedService = NULL;

    memset(currentTxnData.transactionId, 0, sizeof(currentTxnData.transactionId));
    memset(currentTxnData.trxType, 0, sizeof(currentTxnData.trxType));
    currentTxnData.trxTypeBin = 0;
    memset(currentTxnData.processingCode, 0, sizeof(currentTxnData.processingCode));
    currentTxnData.txnCounter = 0;
    memset(currentTxnData.stan, 0, sizeof(currentTxnData.stan));
    currentTxnData.amount = 0;
    memset(currentTxnData.sAmount, 0, sizeof(currentTxnData.sAmount));
    memset(currentTxnData.time, 0, sizeof(currentTxnData.time));
    memset(currentTxnData.date, 0, sizeof(currentTxnData.date));
    memset(currentTxnData.year, 0, sizeof(currentTxnData.year));
    memset(currentTxnData.aid, 0, sizeof(currentTxnData.aid));
    memset(currentTxnData.maskPan, 0, sizeof(currentTxnData.maskPan));
    memset(currentTxnData.token, 0, sizeof(currentTxnData.token));
    memset(currentTxnData.effectiveDate, 0, sizeof(currentTxnData.effectiveDate));
    memset(currentTxnData.serviceId, 0, sizeof(currentTxnData.serviceId));
    memset(currentTxnData.serviceIndex, 0, sizeof(currentTxnData.serviceIndex));
    memset(currentTxnData.serviceData, 0, sizeof(currentTxnData.serviceData));
    memset(currentTxnData.serviceControl, 0, sizeof(currentTxnData.serviceControl));
    memset(currentTxnData.serviceBalance, 0, sizeof(currentTxnData.serviceBalance));
    memset(currentTxnData.iccData, 0, sizeof(currentTxnData.iccData));
    currentTxnData.iccDataLen = 0;
    memset(currentTxnData.ksn, 0, sizeof(currentTxnData.ksn));
    memset(currentTxnData.track2Enc, 0, sizeof(currentTxnData.track2Enc));
    memset(currentTxnData.panEncrypted, 0, sizeof(currentTxnData.panEncrypted));
    memset(currentTxnData.expDateEnc, 0, sizeof(currentTxnData.expDateEnc));
    memset(currentTxnData.macKsn, 0, sizeof(currentTxnData.macKsn));
    memset(currentTxnData.mac, 0, sizeof(currentTxnData.mac));
    memset(currentTxnData.plainPan, 0, sizeof(currentTxnData.plainPan));
    memset(currentTxnData.plainExpDate, 0, sizeof(currentTxnData.plainExpDate));
    memset(currentTxnData.plainTrack2, 0, sizeof(currentTxnData.plainTrack2));
    memset(currentTxnData.orderId, 0, sizeof(currentTxnData.orderId));
    memset(currentTxnData.txnStatus, 0, sizeof(currentTxnData.txnStatus));
    currentTxnData.isImmedOnline = false;
    currentTxnData.isServiceCreation = false;
    memset(currentTxnData.updatedAmount, 0, sizeof(currentTxnData.updatedAmount));
    memset(currentTxnData.updatedBalance, 0, sizeof(currentTxnData.updatedBalance));
    memset(currentTxnData.hostResponseCode, 0, sizeof(currentTxnData.hostResponseCode));
    memset(currentTxnData.moneyAddTrxType, 0, sizeof(currentTxnData.moneyAddTrxType));
    memset(currentTxnData.checkDate, 0, sizeof(currentTxnData.checkDate));
    memset(currentTxnData.trxIssueDetail, 0, sizeof(currentTxnData.trxIssueDetail));
    currentTxnData.checkDateResult = 0;
    currentTxnData.cardPresentedSent = false;
    currentTxnData.isRupayTxn = false;
    currentTxnData.isGateOpen = false;
    memset(currentTxnData.gmtTime, 0, sizeof(currentTxnData.gmtTime));

    logInfo("Transaction data reset successfully");
}

/**
 * To increment the transaction counter
 */
void incrementTransactionCounter()
{
    logData("Incrementing transaction counter. Current : %d", appData.transactionCounter);
    // Increment the next and save
    appData.transactionCounter++;

    if (appData.transactionCounter > MAX_TXN_COUNTER)
    {
        logData("Transaction counter is reset to 1 as the max is reached");
        appData.transactionCounter = 1;
    }

    writeAppData();
    logData("Updated one : %d", appData.transactionCounter);
}

/**
 * Format the date time for sending to client
 */
void formatTransactionTime(struct transactionData trxData, char trxDate[20])
{
    strcpy(trxDate, trxData.year);
    strcat(trxDate, "-");
    strncat(trxDate, &trxData.date[0], 1);
    strncat(trxDate, &trxData.date[1], 1);
    strcat(trxDate, "-");
    strncat(trxDate, &trxData.date[2], 1);
    strncat(trxDate, &trxData.date[3], 1);
    strcat(trxDate, " ");
    strncat(trxDate, &trxData.time[0], 1);
    strncat(trxDate, &trxData.time[1], 1);
    strcat(trxDate, ":");
    strncat(trxDate, &trxData.time[2], 1);
    strncat(trxDate, &trxData.time[3], 1);
    strcat(trxDate, ":");
    strncat(trxDate, &trxData.time[4], 1);
    strncat(trxDate, &trxData.time[5], 1);
}

/**
 * Remove the track 2 padding
 **/
char *removeTrack2FPadding(const char *track2, int len)
{
    // logData("Track 2 recevied : %s", track2);

    // count F padding in last
    int totalFPadding = 0;
    for (int i = len - 1; i >= 0; i--)
    {
        if (track2[i] == 'F')
        {
            totalFPadding++;
        }
        else
        {
            break;
        }
    }
    // logData("Total F padding : %d", totalFPadding);
    int size = len - totalFPadding;
    char *updatedTrack2 = malloc(size + 1);

    for (int i = 0; i < size; i++)
    {
        updatedTrack2[i] = track2[i];
    }

    updatedTrack2[size] = '\0';

    // logData("Updated Track 2 is : %s", updatedTrack2);
    return updatedTrack2;
}
/**
 * To display light based on the state
 */
void displayLight(const char *stateName)
{
    LEDDATA *ledData;
    bool found = false;

    for (int i = 0; i < appConfig.ledLength; i++)
    {
        if (strcmp(appConfig.ledDataList[i]->stateName, stateName) == 0)
        {
            ledData = appConfig.ledDataList[i];
            found = true;
        }
    }

    uint32_t ledValue = 0;

    leds_off(LEDS_LOGO | LEDS_GREEN0 | LEDS_GREEN1 | LEDS_GREEN2 | LEDS_GREEN3 | LEDS_YELLOW | LEDS_RED);

    if (found == false)
        return;

    if (strcmp(ledData->midLogo, "W") == 0)
        ledValue |= LEDS_LOGO;

    if (strcmp(ledData->led1, "G") == 0)
        ledValue |= LEDS_GREEN0;

    if (strcmp(ledData->led2, "G") == 0)
        ledValue |= LEDS_GREEN1;

    if (strcmp(ledData->led3, "G") == 0)
        ledValue |= LEDS_GREEN2;

    if (strcmp(ledData->led4, "G") == 0)
        ledValue |= LEDS_GREEN3;

    if (strcmp(ledData->led4, "R") == 0)
        ledValue |= LEDS_RED;

    leds_set(ledValue);
}

/**
 * Display sound and light in case of error
 **/
void makeBuzz()
{
    buzzer_beep(1000, 300);
}

/**
 * to make a beep sound
 */
void *makeBeep()
{
    buzzer_beep(1000, 300);
    pthread_exit(NULL);
    return NULL;
}

/**
 * To initiate a beep in thread
 */
void beepInThread()
{
    pthread_t beepThread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, 1);
    pthread_create(&beepThread, NULL, makeBeep, NULL);
    pthread_attr_destroy(&attr);
    pthread_detach(beepThread);
}

void generateSha(char *string, char outputBuffer[65])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, string, strlen(string));
    SHA256_Final(hash, &sha256);
    int i = 0;
    for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

void generatePanToken(const char *panNumber, const char *track2, char buffer[65])
{
    // doc : Hashing_SHA256_PAN_NCMC_Card.pdf
    // Identify the salt using service code and last 4 digits of pan
    char salt[8]; // last 4 digits of pan and 3 digits of service code

    // Identify the service code
    const char *found = strchr(track2, 'D');
    int index = (int)(found - track2) + 5;
    int k = 0;
    for (int i = index; i < index + 3; i++)
    {
        salt[k] = track2[i];
        k++;
    }

    for (int i = strlen(panNumber) - 4; i < strlen(panNumber); i++)
    {
        salt[k] = panNumber[i];
        k++;
    }
    salt[7] = '\0';
    // logData("Salt found : %s", salt);
    // logData("Salt length : %d", strlen(salt));

    char data[200];
    strcpy(data, "");
    strcat(data, salt);
    strcat(data, panNumber);
    strcat(data, "\0");
    // logData("Data for Sha %s", data);
    generateSha(data, buffer);
}

void string2hexString(const char *input, char *output)
{
    int loop = 0;
    int i = 0;

    while (input[loop] != '\0')
    {
        sprintf((char *)(output + i), "%02X", input[loop]);
        loop += 1;
        i += 2;
    }
    // insert NULL at the end of the output string
    output[i++] = '\0';
}

/**
 * This will pad 0 to input and return the ouput data and length
 * Logic
 * - Check the length of input if not even add 0
 **/
void pad0MultipleOf8(const char *input, char *output, int *outLen)
{
    logData("Input data : %s", input);
    int inputLen = strlen(input);
    logData("Input len : %d", inputLen);

    int evenAdd = 0;
    if (inputLen % 2 != 0)
    {
        evenAdd = 1;
    }
    logData("Now even add : %d", evenAdd);

    int bLen = (inputLen + evenAdd) / 2;
    logData("Byte length : %d", bLen);

    int bal = 0;
    if (bLen % 8 != 0)
    {
        bal = (8 - (bLen % 8)) * 2;
    }
    logData("Balance padding as per 8 byte len : %d", bal);
    *outLen = bal + evenAdd + inputLen;
    // output = malloc(*outLen + 1);
    int idx = 0;
    for (int i = 0; i < (bal + evenAdd); i++)
        output[idx++] = '0';

    for (int i = 0; i < inputLen; i++)
        output[idx++] = input[i];

    output[idx] = '\0';
    logData("Final padded length : %d", *outLen);
    logData("Final padded data : %s", output);
}

/**
 * To populate the current time
 */
void populateCurrentTime(char timeData[20])
{
    time_t timer;
    struct tm *tm_info;

    timer = time(NULL);
    tm_info = localtime(&timer);

    strftime(timeData, 26, "%Y-%m-%dT%H:%M:%S", tm_info);
}

/**
 * to calculate hmac used by airtel for signature
 */
void calculate_hmac_sha256(const char *key, const char *data, char *hmac_hex)
{
    unsigned char hmac_result[EVP_MAX_MD_SIZE]; // Raw HMAC output
    unsigned int hmac_length = 0;

    // Compute HMAC using SHA-256
    printf("Data for compute : %s", data);
    printf("Salt : %s", key);
    HMAC(EVP_sha256(), key, strlen(key), (unsigned char *)data, strlen(data), hmac_result, &hmac_length);

    // Convert binary HMAC to hexadecimal
    for (unsigned int i = 0; i < hmac_length; i++)
    {
        sprintf(&hmac_hex[i * 2], "%02x", hmac_result[i]);
    }

    hmac_hex[hmac_length * 2] = '\0';
}

/**
 * To remove the spaces
 */
void removeSpaces(char *message)
{
    int i = 0, j = 0;

    while (message[i])
    {
        if (message[i] != ' ')
        {
            message[j++] = message[i];
        }
        i++;
    }
    message[j] = '\0';
}

/**
 * To remove the trailing zeros from amount
 */
void trimAmount(char amount[13], char trimmedAmount[13])
{
    long value = strtol(amount, NULL, 10);
    sprintf(trimmedAmount, "%ld", value);
}
