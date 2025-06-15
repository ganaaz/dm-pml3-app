#include <string.h>
#include <stdio.h>

#include "../include/logutil.h"
#include "../include/config.h"
#include "../include/dboperations.h"
#include "../include/dukpt-util.h"
#include "../include/commonutil.h"
#include "../include/appcodes.h"

extern struct applicationConfig appConfig;

/**
 * Generate mac
 * 6. Offline Sale
    MAC String Sequence =
    processingCode + amount + stan + expiryDate(O) + time + date + year +
    invoiceNumber + posConditionCode + posEntryMode + primaryAccountNr(O) + tid + mid + iccData (O) +
    authorizationCode(O) + extendInfo-values(O)
    This will update the mac in curent txn data
 */
void generateMacOfflineSale(struct transactionData trxData)
{
    logData("Generating mac data for offline sale");
    char macData[1024 * 5] = {0};
    strcat(macData, trxData.processingCode);
    strcat(macData, trxData.sAmount);
    strcat(macData, trxData.stan);

    // encrypted exp date
    strcat(macData, trxData.expDateEnc);

    strcat(macData, trxData.time);
    strcat(macData, trxData.date);
    strcat(macData, trxData.year);

    strcat(macData, trxData.stan);               // Invoice is same as Stan
    strcat(macData, HOST_POS_CONDITION_CODE_00); // POS Condition code is 00
    strcat(macData, HOST_POS_ENTRY_MODE);        // POS Entry mode is 071
    strcat(macData, trxData.panEncrypted);
    strcat(macData, appConfig.terminalId);
    strcat(macData, appConfig.merchantId);
    strcat(macData, trxData.iccData);
    // Auth code is not needed
    strcat(macData, trxData.orderId);      // Order id
    strcat(macData, appConfig.deviceCode); // Gate number
    strcat(macData, appConfig.stationId);
    strcat(macData, appConfig.stationName);
    strcat(macData, trxData.orderId); // Operator Order Id

    logData("Generated mac data : %s", macData);
    logData("Mac data len : %d", strlen(macData));

    // generate in dukpt util and update current txn data
    generateMac(macData);
}

/**
 * Generate mac
 * MAC String Sequence processingCode
 * + amount + stan + expiryDate(O) + time + date + year + invoiceNumber +
 * pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + encryptedTrack2 +
 * iccData (O) + tid + mid + extendInfo + txnType(O)
 **/
void generateMacBalanceUpdate(struct transactionData trxData)
{
    logData("Generating mac data for balance update");
    char macData[1024 * 5] = {0};
    strcat(macData, trxData.processingCode);
    strcat(macData, trxData.sAmount);
    strcat(macData, trxData.stan);

    // encrypted exp date
    strcat(macData, trxData.expDateEnc);

    strcat(macData, trxData.time);
    strcat(macData, trxData.date);
    strcat(macData, trxData.year);

    strcat(macData, trxData.stan);               // Invoice is same as Stan
    strcat(macData, HOST_POS_CONDITION_CODE_02); // POS Condition code is 02
    strcat(macData, HOST_POS_ENTRY_MODE);        // POS Entry mode is 071
    strcat(macData, trxData.track2Enc);
    strcat(macData, trxData.iccData);
    strcat(macData, appConfig.terminalId);
    strcat(macData, appConfig.merchantId);
    // Auth code is not needed
    strcat(macData, trxData.orderId);      // Order id
    strcat(macData, appConfig.deviceCode); // Gate number
    strcat(macData, appConfig.stationId);
    strcat(macData, appConfig.stationName);
    strcat(macData, trxData.orderId); // Operator Order Id

    logData("Generated mac data : %s", macData);
    logData("Mac data len : %d", strlen(macData));

    // generate in dukpt util and update current txn data
    generateMac(macData);
}

/**
 * Generate mac
 * MAC String Sequence processingCode
 * + amount + stan + expiryDate(O) + time + date + year + invoiceNumber +
 * pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + encryptedTrack2 +
 * iccData (O) + tid + mid + extendInfo + txnType(O)
 **/
void generateMacServiceCreation(struct transactionData trxData)
{
    logData("Generating mac data for service creation");
    char macData[1024 * 5] = {0};
    strcat(macData, trxData.processingCode);
    strcat(macData, trxData.sAmount);
    strcat(macData, trxData.stan);

    // encrypted exp date
    strcat(macData, trxData.expDateEnc);

    strcat(macData, trxData.time);
    strcat(macData, trxData.date);
    strcat(macData, trxData.year);

    strcat(macData, trxData.stan);               // Invoice is same as Stan
    strcat(macData, HOST_POS_CONDITION_CODE_02); // POS Condition code is 02
    strcat(macData, HOST_POS_ENTRY_MODE);        // POS Entry mode is 071
    strcat(macData, trxData.track2Enc);
    strcat(macData, trxData.iccData);
    strcat(macData, appConfig.terminalId);
    strcat(macData, appConfig.merchantId);
    // Auth code is not needed
    strcat(macData, trxData.orderId); // Order id

    logData("Generated mac data : %s", macData);
    logData("Mac data len : %d", strlen(macData));

    // generate in dukpt util and update current txn data
    generateMac(macData);
}

/**
 * Generate mac
 * MAC String Sequence money add
 * processingCode + amount + stan + expiryDate(O) + time + date + year +
 * invoiceNumber + pinBlock(O) + posConditionCode + posEntryMode + primaryAccountNr(O) + track2(O) +
 * iccData (O) + tid + mid + extendInfo-values(O) + TxnType
 **/
void generateMacMoneyAdd(struct transactionData trxData)
{
    logData("Generating mac data for money add");
    char macData[1024 * 5] = {0};
    strcat(macData, trxData.processingCode);
    strcat(macData, trxData.sAmount);
    strcat(macData, trxData.stan);

    // encrypted exp date
    strcat(macData, trxData.expDateEnc);

    strcat(macData, trxData.time);
    strcat(macData, trxData.date);
    strcat(macData, trxData.year);

    strcat(macData, trxData.stan);               // Invoice is same as Stan
    strcat(macData, HOST_POS_CONDITION_CODE_02); // POS Condition code is 02
    strcat(macData, HOST_POS_ENTRY_MODE);        // POS Entry mode is 071
    strcat(macData, trxData.track2Enc);
    strcat(macData, trxData.iccData);
    strcat(macData, appConfig.terminalId);
    strcat(macData, appConfig.merchantId);
    // Auth code is not needed
    strcat(macData, trxData.orderId);
    strcat(macData, trxData.moneyAddTrxType);
    strcat(macData, appConfig.deviceCode); // Gate number
    strcat(macData, appConfig.stationId);
    strcat(macData, appConfig.stationName);
    strcat(macData, trxData.orderId); // Operator Order Id

    logData("Generated mac data : %s", macData);
    logData("Mac data len : %d", strlen(macData));

    // generate in dukpt util and update current txn data
    generateMac(macData);
}

/**
 * Generate mac
 * MAC String Sequence processingCode
 * stan + tid + serialNo + vendorName + extendInfo
 **/
void generateMacVerifyTerminal(struct transactionData trxData, const char tid[50])
{
    logData("Generating mac data for verify terminal");
    char macData[1024 * 5] = {0};
    strcat(macData, trxData.stan);
    strcat(macData, tid);
    char deviceId[20];
    getDeviceId(deviceId);
    strcat(macData, deviceId);
    strcat(macData, appConfig.clientName);

    logData("Generated mac data : %s", macData);
    logData("Mac data len : %d", strlen(macData));

    // generate in dukpt util and update current txn data
    generateMac(macData);
}

/**
 * Generate mac for reversal
 * tid + mid + stan + time + date + year + invoiceNumber + ( if primaryAccountNr present )[
 * posConditionCode + posEntryMode + primaryAccountNr ](O) + extendInfo(O)
 **/
TransactionTable generateMacReversal(TransactionTable trxTable, const char de39[3])
{
    logData("Generating mac data for reversal");
    char macData[1024] = {0};
    strcat(macData, trxTable.terminalId);
    strcat(macData, trxTable.merchantId);
    strcat(macData, trxTable.stan);
    strcat(macData, trxTable.time);
    strcat(macData, trxTable.date);
    strcat(macData, trxTable.year);
    strcat(macData, trxTable.stan);              // Invoice
    strcat(macData, HOST_POS_CONDITION_CODE_02); // POS Condition code is 02
    strcat(macData, HOST_POS_ENTRY_MODE);        // POS Entry mode is 071
    strcat(macData, trxTable.panEncrypted);
    strcat(macData, de39);
    strcat(macData, trxTable.orderId);

    logData("Generated mac data for reversal : %s", macData);
    logData("Mac data len : %d", strlen(macData));

    // generate in dukpt util and update current txn data
    char ksn[21] = {0};
    char mac[17] = {0};
    generateMacReversalEcho(macData, ksn, mac);
    strcpy(trxTable.reversalKsn, ksn);
    strcpy(trxTable.reversalMac, mac);
    return trxTable;
}

/**
 * Generate mac for echo
 * tid + mid + stan + time + date + year + invoiceNumber + txnMode + txnStatus + txnType +
 * acquirementId(O) + orderId(O) + extendInfo(O)
 **/
TransactionTable generateMacEcho(TransactionTable trxTable, const char de39[3])
{
    logData("Generating mac data for echo");
    char macData[1024] = {0};
    strcat(macData, trxTable.terminalId);
    strcat(macData, trxTable.merchantId);
    strcat(macData, trxTable.stan);
    strcat(macData, trxTable.time);
    strcat(macData, trxTable.date);
    strcat(macData, trxTable.year);
    strcat(macData, trxTable.stan); // Invoice
    strcat(macData, TXNMODE_CARD);  // Txn Mode
    strcat(macData, HOST_TXN_FAIL); // Txn Status

    if (strcmp(trxTable.trxType, TRXTYPE_BALANCE_UPDATE) == 0)
    {
        strcat(macData, HOST_TXN_BALANCE_UPDATE); // Txn Type
    }

    if (strcmp(trxTable.trxType, TRXTYPE_MONEY_ADD) == 0)
    {
        strcat(macData, HOST_TXN_MONEY_ADD); // Txn Type
    }

    strcat(macData, trxTable.acquirementId); //
    strcat(macData, trxTable.orderId);
    strcat(macData, de39);

    logData("Generated mac data for echo : %s", macData);
    logData("Mac data len : %d", strlen(macData));

    // generate in dukpt util and update current txn data
    char ksn[21] = {0};
    char mac[17] = {0};
    generateMacReversalEcho(macData, ksn, mac);
    strcpy(trxTable.echoKsn, ksn);
    strcpy(trxTable.echoMac, mac);
    return trxTable;
}
