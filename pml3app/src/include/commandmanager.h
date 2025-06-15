#ifndef COMMANDMANAGER_H
#define COMMANDMANAGER_H

/**
 * Search Card structure, that is received from the Client
 **/
struct searchcard
{
    char mode[20];
    char trxtype[20];
    int timeout;
    int amountAvailable; // whether the amount is provided at start
    char amount[13];     // Used for Balance Update and amount at start
    char serviceId[10];  // used for service creation
    int isServiceBlock;
    bool isCheckDateAvailable; // if true then perform date check
    char checkDate[11];        // For date checking
};

/**
 * Write card structure that is received from the client
 **/
struct writecard
{
    char amount[13];
    char serviceData[200];
};

/**
 * Fetch data structure that is received from client
 **/
struct fetchData
{
    int fetchid;
    int maxrecords;
    char mode[20];
};

struct abtFetchData
{
    int skipRecords;
    int maxrecords;
    char mode[20];
};

struct moneyAddData
{
    char trxMode[20];
    char sourceTxnId[50];
};

/**
 * For changing the time
 **/
struct timeData
{
    char time[17];
};

/**
 * For verify terminal data
 */
struct verifyTerminal
{
    char tid[50];
    char mid[50];
};

struct keyData
{
    char label[100];
    bool isBdk;
};

struct changeIpData
{
    char ipMode[100];
    char dns[100];
    char dns2[100];
    char dns3[100];
    char dns4[100];
    char searchDomain[100];
    char ipAddress[20];
    char netmask[20];
    char gateway[20];
};

struct changeLogMode
{
    char logMode[20];
};

struct downloadFile
{
    char fileName[200];
};

struct deleteFile
{
    char fileName[200];
};

struct gateOpen
{
    bool status;
};

/**
 * Base message structure that is received from client
 **/
struct message
{
    char cmd[20];
    struct searchcard sCard;
    struct writecard wCard;
    struct fetchData fData;
    struct timeData tData;
    struct moneyAddData mData;
    struct verifyTerminal vData;
    struct changeIpData ipData;
    struct changeLogMode logData;
    struct keyData kData;
    struct downloadFile dData;
    struct deleteFile delFileData;
    struct gateOpen gateOpenData;
    struct abtFetchData abtFetch;
};

/**
 * To parse the message and convert the json to the structure
 **/
struct message parseMessage(const char *data);

/**
 * Handle the message received from client and generate response
 **/
char *handleClientMessage(const char *data);

/**
 * To handle the search card
 **/
char *handleSearchCard(struct message reqMessage);

/**
 * Perform the stop of the search card
 **/
void stopCardSearch();

/**
 * Generate the get config message
 **/
char *buildGetConfigMessage();

/**
 * Perform the reboot
 **/
int performReboot();

/**
 * Change the app state and do the notification to client
 **/
void changeAppState(enum app_status status);

/**
 * To handle message for fetch commands
 */
char *handleClientFetchMessage(const char *data);

/**
 * To check whether its a valid txn mode
 */
bool isValidTrxMode(const char mode[20]);

#endif