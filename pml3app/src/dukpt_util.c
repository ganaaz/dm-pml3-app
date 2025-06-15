/**
 * FEIG ELECTRONIC DUKPT Demo
 *
 * Copyright (C) 2016-2020 FEIG ELECTRONIC GmbH
 *
 * This software is the confidential and proprietary information of
 * FEIG ELECTRONIC GmbH ("Confidential Information"). You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered
 * into with FEIG ELECTRONIC GmbH.
 */

/*
 * This demo program looks up the DUKPT initial key with id 0xCC01 and label
 * "DUKPT_IK" in application 0's Cryptographic Token and executes three
 * transaction key derivations and data encryption operations.
 *
 * Build as follows:
 *
 * arm-linux-gcc -Wall -Werror dukpt-demo.c -o dukpt-demo -lfepkcs11 -lcrypto
 * fesign -k 00a0 -i dukpt-demo -p 648219
 *
 * Application Slot Tenant lock-in (with AST-ID 00A0):
 * pkcs11-test-import-slot-appcerts \
 *		/etc/ssl/certs/astid-code-signing-certificate-pack.pem -f 0 -t 0
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <feig/fepkcs11.h>
#include <termios.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "include/dukpt-util.h"
#include "include/commonutil.h"
#include "include/config.h"
#include "include/logutil.h"
#include "include/pkcshelper.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

CK_OBJECT_HANDLE hTxnKey = CK_INVALID_HANDLE;
CK_OBJECT_HANDLE hDataKey = CK_INVALID_HANDLE;

extern struct applicationConfig appConfig;
extern struct pkcs11 *crypto;
extern struct transactionData currentTxnData;

static CK_OBJECT_HANDLE get_dukpt_ikey(CK_SESSION_HANDLE hSession, char *label)
{
    CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
    CK_OBJECT_CLASS dukptClass = CKO_DUKPT_IKEY;
    CK_KEY_TYPE dukptKeyType = CKK_DES2;
    CK_ATTRIBUTE attrs_dukpt_key[] = {
        {CKA_CLASS, &dukptClass, sizeof(dukptClass)},
        {CKA_KEY_TYPE, &dukptKeyType, sizeof(dukptKeyType)},
        {CKA_LABEL, label, strlen(label)}};
    CK_ULONG ulObjectCount = 0;
    CK_RV rv = CKR_OK;

    rv = C_FindObjectsInit(hSession, attrs_dukpt_key,
                           ARRAY_SIZE(attrs_dukpt_key));
    assert(rv == CKR_OK);

    rv = C_FindObjects(hSession, &hKey, 1, &ulObjectCount);
    assert(rv == CKR_OK);

    rv = C_FindObjectsFinal(hSession);
    assert(rv == CKR_OK);

    return hKey;
}

static unsigned char *get_key_serial_number(CK_SESSION_HANDLE hSession,
                                            CK_OBJECT_HANDLE hIKey, unsigned char ksn[10])
{
    CK_ATTRIBUTE ksn_template[] = {
        {CKA_DUKPT_KEY_SERIAL_NUMBER, ksn, 10}};
    CK_RV rv = CKR_OK;

    rv = C_GetAttributeValue(hSession, hIKey, ksn_template,
                             ARRAY_SIZE(ksn_template));
    assert(rv == CKR_OK);

    return ksn;
}

static unsigned char *get_key_check_value(CK_SESSION_HANDLE hSession,
                                          CK_OBJECT_HANDLE hIKey, unsigned char kcv[3])
{
    CK_ATTRIBUTE kcv_template[] = {
        {CKA_CHECK_VALUE, kcv, 3}};
    CK_RV rv = CKR_OK;

    rv = C_GetAttributeValue(hSession, hIKey, kcv_template,
                             ARRAY_SIZE(kcv_template));
    assert(rv == CKR_OK);

    return kcv;
}

static CK_OBJECT_HANDLE get_transaction_key(CK_SESSION_HANDLE hSession,
                                            CK_OBJECT_HANDLE hIKey)
{
    CK_RV rv = CKR_OK;
    CK_OBJECT_HANDLE hTxnKey = CK_INVALID_HANDLE;
    CK_MECHANISM mechanism = {
        CKM_KEY_DERIVATION_DUKPT_TRANSACTION_KEY, NULL_PTR, 0};
    CK_BBOOL ckTrue = CK_TRUE;
    CK_BBOOL ckFalse = CK_FALSE;
    CK_ATTRIBUTE template[] = {
        {CKA_TOKEN, &ckFalse, sizeof(ckFalse)},
        {CKA_DERIVE, &ckTrue, sizeof(ckTrue)}};

    rv = C_DeriveKey(hSession, &mechanism, hIKey, template,
                     ARRAY_SIZE(template), &hTxnKey);
    assert(rv == CKR_OK);

    return hTxnKey;
}

static CK_OBJECT_HANDLE get_mac_key(CK_SESSION_HANDLE hSession,
                                    CK_OBJECT_HANDLE hTxnKey)
{
    CK_RV rv = CKR_OK;
    CK_OBJECT_HANDLE hMacKey = CK_INVALID_HANDLE;
    CK_MECHANISM mechanism = {
        CKM_KEY_DERIVATION_DUKPT_MSG_AUTHENTICATION_REQUEST, NULL_PTR, 0};
    CK_BBOOL ckTrue = CK_TRUE;
    CK_BBOOL ckFalse = CK_FALSE;
    CK_ATTRIBUTE template[] = {
        {CKA_TOKEN, &ckFalse, sizeof(ckFalse)},
        {CKA_SIGN, &ckTrue, sizeof(ckTrue)}};

    rv = C_DeriveKey(hSession, &mechanism, hTxnKey, template,
                     ARRAY_SIZE(template), &hMacKey);
    assert(rv == CKR_OK);

    return hMacKey;
}

static CK_OBJECT_HANDLE get_data_key(CK_SESSION_HANDLE hSession,
                                     CK_OBJECT_HANDLE hTxnKey)
{
    CK_RV rv = CKR_OK;
    CK_OBJECT_HANDLE hDataKey = CK_INVALID_HANDLE;
    CK_MECHANISM mechanism = {
        CKM_KEY_DERIVATION_DUKPT_DATA_ENCRYPTION_REQUEST, NULL_PTR, 0};
    CK_BBOOL ckTrue = CK_TRUE;
    CK_BBOOL ckFalse = CK_FALSE;
    CK_ATTRIBUTE template[] = {
        {CKA_TOKEN, &ckFalse, sizeof(ckFalse)},
        {CKA_ENCRYPT, &ckTrue, sizeof(ckTrue)}};

    rv = C_DeriveKey(hSession, &mechanism, hTxnKey, template,
                     ARRAY_SIZE(template), &hDataKey);
    assert(rv == CKR_OK);

    return hDataKey;
}

void dukpt_mac(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hIKey,
               void *in, size_t in_len, void *out, size_t *out_len)
{
    CK_OBJECT_HANDLE hTxnKey = get_transaction_key(hSession, hIKey);
    CK_OBJECT_HANDLE hMacKey = get_mac_key(hSession, hTxnKey);
    CK_MECHANISM mechanism = {CKM_DES3_ISO_9797_1_M1_ALG_3_MAC, NULL, 0};
    CK_ULONG ulOutLen = (CK_ULONG)(*out_len);
    CK_RV rv = CKR_OK;

    rv = C_SignInit(hSession, &mechanism, hMacKey);
    assert(rv == CKR_OK);

    rv = C_Sign(hSession, (CK_BYTE_PTR)in, (CK_ULONG)in_len,
                (CK_BYTE_PTR)out, &ulOutLen);
    assert(rv == CKR_OK);
    assert(ulOutLen == 8);

    *out_len = (size_t)ulOutLen;

    C_DestroyObject(hSession, hMacKey);
    C_DestroyObject(hSession, hTxnKey);
}

void dukpt_encrypt(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hIKey,
                   void *in, size_t in_len, void *out, size_t *out_len,
                   bool finalized, bool getDataKey)
{
    if (getDataKey)
    {
        hTxnKey = get_transaction_key(hSession, hIKey);
        hDataKey = get_data_key(hSession, hTxnKey);
    }
    CK_BYTE iv[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CK_MECHANISM mech_des3_cbc = {CKM_DES3_CBC, &iv, sizeof(iv)};
    CK_ULONG ulOutLen = (CK_ULONG)(*out_len);
    CK_RV rv = CKR_OK;
    size_t padded_len = (in_len + 7) & ~0x7u;
    unsigned char padded_in[padded_len];

    logData("Padded len : %d", padded_len);
    logData("out len: %d", *out_len);

    assert(*out_len >= padded_len);

    memset(padded_in, 0, sizeof(padded_in));
    memcpy(padded_in, in, in_len);

    rv = C_EncryptInit(hSession, &mech_des3_cbc, hDataKey);
    assert(rv == CKR_OK);

    rv = C_Encrypt(hSession, padded_in, padded_len, out, &ulOutLen);
    assert(rv == CKR_OK);

    *out_len = (size_t)ulOutLen;

    if (finalized)
    {
        C_DestroyObject(hSession, hDataKey);
        C_DestroyObject(hSession, hTxnKey);
    }
}

/**
 * To generate the Mac for the input data as per the PayTM requirement
 * this will update the current txn data
 **/
void generateMac(const char *macInput)
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hIKey = CK_INVALID_HANDLE;
    unsigned char kcv[3];
    unsigned char ksn[10];
    KEYDATA *key = getMacKey();
    unsigned char buffer[128];
    size_t len = sizeof(buffer);

    hSession = crypto->hSession;
    int macLen = strlen(macInput);
    char hexMac[macLen * 2 + 1];

    for (int i = 0; i < macLen; i++)
    {
        sprintf(hexMac + i * 2, "%02X", macInput[i]);
    }
    // printf("Mac Hex Data : %s\n", hexMac);

    unsigned char byteMac[macLen];
    hexToByte(hexMac, byteMac);

    logData("Key Label for MAC : %s", key->label);
    hIKey = get_dukpt_ikey(hSession, key->label);
    get_key_serial_number(hSession, hIKey, ksn);
    get_key_check_value(hSession, hIKey, kcv);

    byteToHex(ksn, 10, currentTxnData.macKsn);
    logData("Mac KSN Generated : %s", currentTxnData.macKsn);

    len = sizeof(buffer);
    dukpt_mac(hSession, hIKey, byteMac, sizeof(byteMac), buffer, &len);

    byteToHex(buffer, len, currentTxnData.mac);
    logData("Generated Mac : %s", currentTxnData.mac);
    logData("Generated Mac length : %d", strlen(currentTxnData.mac));
}

/**
 * To generate the Mac for the input data as per the PayTM requirement
 * and return the KSN and data used for reversal and echo
 **/
void generateMacReversalEcho(const char *macInput, char ksnData[21], char macData[17])
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hIKey = CK_INVALID_HANDLE;
    unsigned char kcv[3];
    unsigned char ksn[10];
    KEYDATA *key = getMacKey();
    unsigned char buffer[128];
    size_t len = sizeof(buffer);

    hSession = crypto->hSession;
    int macLen = strlen(macInput);
    char hexMac[macLen * 2 + 1];

    for (int i = 0; i < macLen; i++)
    {
        sprintf(hexMac + i * 2, "%02X", macInput[i]);
    }
    // printf("Mac Hex Data : %s\n", hexMac);

    unsigned char byteMac[macLen];
    hexToByte(hexMac, byteMac);

    logData("Key Label for MAC : %s", key->label);
    hIKey = get_dukpt_ikey(hSession, key->label);
    get_key_serial_number(hSession, hIKey, ksn);
    get_key_check_value(hSession, hIKey, kcv);

    byteToHex(ksn, 10, ksnData);
    logData("Mac KSN Generated : %s", ksnData);

    len = sizeof(buffer);
    dukpt_mac(hSession, hIKey, byteMac, sizeof(byteMac), buffer, &len);

    byteToHex(buffer, len, macData);
    logData("Generated Mac : %s", macData);
    logData("Generated Mac length : %d", strlen(macData));
}

/**
 * To encrypt the pan and exp date available in current txn data and pouplate the ksn
 * in the same current txn data object, used for offline sale
 */
void encryptPanExpDate()
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hIKey = CK_INVALID_HANDLE;
    unsigned char kcv[3];
    unsigned char ksn[10];
    KEYDATA *key = getDukptKey();
    unsigned char buffer[128 * 10];
    size_t len = sizeof(buffer);

    hSession = crypto->hSession;

    logData("Going to retrieve key from device for label : %s", key->label);
    hIKey = get_dukpt_ikey(hSession, key->label);
    if (hIKey == CK_INVALID_HANDLE)
    {
        printf("No DUKPT Initial Key found (label '%s', id %02hX).\n", key->label, 0);
        return;
    }
    logData("Dukpt key retrieved success");

    // Generate the KSN and KCV
    get_key_serial_number(hSession, hIKey, ksn);
    get_key_check_value(hSession, hIKey, kcv);

    byteToHex(ksn, 10, currentTxnData.ksn);
    logData("KSN Generated : %s", currentTxnData.ksn);

    // logData("KSN : %s", bin2hex(hex, get_key_serial_number(hSession, hIKey, ksn), 10));
    // logData("KCV(IKEY) : %s", bin2hex(hex, get_key_check_value(hSession, hIKey, kcv), sizeof(kcv)));

    // Perform encryption of PAN
    // Do the padding for Pan as per Airtel requirement
    char paddedPan[96]; // cant be more than 96
    int paddedPanLen;
    pad0MultipleOf8(currentTxnData.plainPan, paddedPan, &paddedPanLen);

    // Airtel doc ask to make it as 24 so add 0
    if (paddedPanLen == 16)
    {
        char tempPaddedPan[96];
        strcpy(tempPaddedPan, "00000000");
        strcat(tempPaddedPan, paddedPan);
        strcpy(paddedPan, tempPaddedPan);
        paddedPanLen += 8;
    }

    // logData("Padded pan data : %s", paddedPan); // TODO : Remove
    // logData("Padded pan length : %d", paddedPanLen);

    // Generate byte data of the hex pan
    unsigned char panByte[paddedPanLen];
    // hexToByte(paddedPan, panByte);
    for (size_t i = 0; i < paddedPanLen; ++i)
    {
        panByte[i] = (unsigned char)paddedPan[i];
    }

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, panByte, paddedPanLen, buffer, &len, FALSE, TRUE);
    byteToHex(buffer, len, currentTxnData.panEncrypted);
    logData("Encrypted pan : %s", currentTxnData.panEncrypted);
    logData("Encrypted pan length : %d", strlen(currentTxnData.panEncrypted));

    // Perform the card expiry encryption
    // Do the padding for expiry as per PayTM requirement
    char paddedExpiry[17]; // cant be more than 16
    int paddedExpLen;
    pad0MultipleOf8(currentTxnData.plainExpDate, paddedExpiry, &paddedExpLen);

    logData("Padded exp data : %s", paddedExpiry); // TODO : Remove
    logData("Padded exp length : %d", paddedExpLen);

    // Generate byte data of the hex pan
    unsigned char expByte[paddedExpLen / 2];
    hexToByte(paddedExpiry, expByte);

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, expByte, paddedExpLen / 2, buffer, &len, TRUE, FALSE);
    byteToHex(buffer, len, currentTxnData.expDateEnc);
    logData("Encrypted exp date : %s", currentTxnData.expDateEnc);
    logData("Encrypted exp length : %d", strlen(currentTxnData.expDateEnc));
}

/**
 * To encrypt the pan, track2 and exp date available in current txn data and pouplate the ksn
 * in the same current txn data object, used for immediate online scenarios
 */
void encryptPanTrack2ExpDate()
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hIKey = CK_INVALID_HANDLE;
    unsigned char kcv[3];
    unsigned char ksn[10];
    KEYDATA *key = getDukptKey();
    unsigned char buffer[128 * 10];
    size_t len = sizeof(buffer);

    hSession = crypto->hSession;

    logData("Going to retrieve key from device for label : %s", key->label);
    hIKey = get_dukpt_ikey(hSession, key->label);
    if (hIKey == CK_INVALID_HANDLE)
    {
        printf("No DUKPT Initial Key found (label '%s', id %02hX).\n", key->label, 0);
        return;
    }
    logData("Dukpt key retrieved success");

    // Generate the KSN and KCV
    get_key_serial_number(hSession, hIKey, ksn);
    get_key_check_value(hSession, hIKey, kcv);

    byteToHex(ksn, 10, currentTxnData.ksn);
    logData("KSN Generated : %s", currentTxnData.ksn);

    // logData("KSN : %s", bin2hex(hex, get_key_serial_number(hSession, hIKey, ksn), 10));
    // logData("KCV(IKEY) : %s", bin2hex(hex, get_key_check_value(hSession, hIKey, kcv), sizeof(kcv)));

    // Perform encryption of Track2
    // Do the padding for data as per PayTM requirement
    char paddedTrack2[96]; // cant be more than 96
    int paddedTrack2Len;
    pad0MultipleOf8(currentTxnData.plainTrack2, paddedTrack2, &paddedTrack2Len);

    // logData("Padded track2 data : %s", paddedTrack2); // TODO : Remove
    // logData("Padded track2 length : %d", paddedTrack2Len);

    // Generate byte data of the hex track 2
    unsigned char track2Byte[paddedTrack2Len];
    // hexToByte(paddedTrack2, track2Byte);
    for (size_t i = 0; i < paddedTrack2Len; ++i)
    {
        track2Byte[i] = (unsigned char)paddedTrack2[i];
    }

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, track2Byte, paddedTrack2Len, buffer, &len, FALSE, TRUE);
    byteToHex(buffer, len, currentTxnData.track2Enc);
    logData("Encrypted track2 : %s", currentTxnData.track2Enc);
    logData("Encrypted track2 length : %d", strlen(currentTxnData.track2Enc));

    // Perform encryption of Pan
    // Do the padding for data as per Airtel requirement
    char paddedPan[96]; // cant be more than 96
    int paddedPanLen;
    pad0MultipleOf8(currentTxnData.plainPan, paddedPan, &paddedPanLen);

    // Airtel doc ask to make it as 24 so add 0
    if (paddedPanLen == 16)
    {
        char tempPaddedPan[96];
        strcpy(tempPaddedPan, "00000000");
        strcat(tempPaddedPan, paddedPan);
        strcpy(paddedPan, tempPaddedPan);
        paddedPanLen += 8;
    }

    logData("Padded pan data : %s", paddedPan); // TODO : Remove
    logData("Padded pan length : %d", paddedPanLen);

    // Generate byte data of the hex pan
    unsigned char panByte[paddedPanLen];
    // hexToByte(paddedPan, panByte);
    for (size_t i = 0; i < paddedPanLen; ++i)
    {
        panByte[i] = (unsigned char)paddedPan[i];
    }

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, panByte, paddedPanLen, buffer, &len, FALSE, FALSE);
    byteToHex(buffer, len, currentTxnData.panEncrypted);
    logData("Encrypted pan : %s", currentTxnData.panEncrypted);
    logData("Encrypted pan length : %d", strlen(currentTxnData.panEncrypted));

    // Perform the card expiry encryption
    // Do the padding for expiry as per PayTM requirement
    char paddedExpiry[17]; // cant be more than 16
    int paddedExpLen;

    char expDateYYMM[6];
    strncpy(expDateYYMM, currentTxnData.plainExpDate, 4);
    expDateYYMM[5] = '\0';

    pad0MultipleOf8(expDateYYMM, paddedExpiry, &paddedExpLen);

    // logData("Exp date used for padding : %s and len is : %d", expDateYYMM, strlen(expDateYYMM));
    // logData("Padded exp data : %s", paddedExpiry); // TODO : Remove
    // logData("Padded exp length : %d", paddedExpLen);

    // Generate byte data of the hex pan
    unsigned char expByte[paddedExpLen / 2];
    hexToByte(paddedExpiry, expByte);

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, expByte, paddedExpLen / 2, buffer, &len, TRUE, FALSE);
    byteToHex(buffer, len, currentTxnData.expDateEnc);
    logData("Encrypted exp date : %s", currentTxnData.expDateEnc);
    logData("Encrypted pan length : %d", strlen(currentTxnData.expDateEnc));
}

/**
 * For offline scenario for paytm, not used now
 */
void encryptPanExpDatePayTm()
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hIKey = CK_INVALID_HANDLE;
    unsigned char kcv[3];
    unsigned char ksn[10];
    KEYDATA *key = getDukptKey();
    unsigned char buffer[128 * 10];
    size_t len = sizeof(buffer);

    hSession = crypto->hSession;

    logData("Going to retrieve key from device for label : %s", key->label);
    hIKey = get_dukpt_ikey(hSession, key->label);
    if (hIKey == CK_INVALID_HANDLE)
    {
        printf("No DUKPT Initial Key found (label '%s', id %02hX).\n", key->label, 0);
        return;
    }
    logData("Dukpt key retrieved success");

    // Generate the KSN and KCV
    get_key_serial_number(hSession, hIKey, ksn);
    get_key_check_value(hSession, hIKey, kcv);

    byteToHex(ksn, 10, currentTxnData.ksn);
    logData("KSN Generated : %s", currentTxnData.ksn);

    // logData("KSN : %s", bin2hex(hex, get_key_serial_number(hSession, hIKey, ksn), 10));
    // logData("KCV(IKEY) : %s", bin2hex(hex, get_key_check_value(hSession, hIKey, kcv), sizeof(kcv)));

    // Perform encryption of PAN
    // Do the padding for Pan as per PayTM requirement
    char paddedPan[96]; // cant be more than 96
    int paddedPanLen;
    pad0MultipleOf8(currentTxnData.plainPan, paddedPan, &paddedPanLen);

    logData("Padded pan data : %s", paddedPan); // TODO : Remove
    logData("Padded pan length : %d", paddedPanLen);

    // Generate byte data of the hex pan
    unsigned char panByte[paddedPanLen / 2];
    hexToByte(paddedPan, panByte);

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, panByte, paddedPanLen / 2, buffer, &len, FALSE, TRUE);
    byteToHex(buffer, len, currentTxnData.panEncrypted);
    logData("Encrypted pan : %s", currentTxnData.panEncrypted);
    logData("Encrypted pan length : %d", strlen(currentTxnData.panEncrypted));

    // Perform the card expiry encryption
    // Do the padding for expiry as per PayTM requirement
    char paddedExpiry[17]; // cant be more than 16
    int paddedExpLen;
    pad0MultipleOf8(currentTxnData.plainExpDate, paddedExpiry, &paddedExpLen);

    logData("Padded exp data : %s", paddedExpiry); // TODO : Remove
    logData("Padded exp length : %d", paddedExpLen);

    // Generate byte data of the hex pan
    unsigned char expByte[paddedExpLen / 2];
    hexToByte(paddedExpiry, expByte);

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, expByte, paddedExpLen / 2, buffer, &len, TRUE, FALSE);
    byteToHex(buffer, len, currentTxnData.expDateEnc);
    logData("Encrypted exp date : %s", currentTxnData.expDateEnc);
    logData("Encrypted pan length : %d", strlen(currentTxnData.expDateEnc));
}

/**
 * For online scenario for paytm not used now
 */
void encryptPanTrack2ExpDatePayTm()
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hIKey = CK_INVALID_HANDLE;
    unsigned char kcv[3];
    unsigned char ksn[10];
    KEYDATA *key = getDukptKey();
    unsigned char buffer[128 * 10];
    size_t len = sizeof(buffer);

    hSession = crypto->hSession;

    logData("Going to retrieve key from device for label : %s", key->label);
    hIKey = get_dukpt_ikey(hSession, key->label);
    if (hIKey == CK_INVALID_HANDLE)
    {
        printf("No DUKPT Initial Key found (label '%s', id %02hX).\n", key->label, 0);
        return;
    }
    logData("Dukpt key retrieved success");

    // Generate the KSN and KCV
    get_key_serial_number(hSession, hIKey, ksn);
    get_key_check_value(hSession, hIKey, kcv);

    byteToHex(ksn, 10, currentTxnData.ksn);
    logData("KSN Generated : %s", currentTxnData.ksn);

    // logData("KSN : %s", bin2hex(hex, get_key_serial_number(hSession, hIKey, ksn), 10));
    // logData("KCV(IKEY) : %s", bin2hex(hex, get_key_check_value(hSession, hIKey, kcv), sizeof(kcv)));

    // Perform encryption of Track2
    // Do the padding for data as per PayTM requirement
    char paddedTrack2[96]; // cant be more than 96
    int paddedTrack2Len;
    pad0MultipleOf8(currentTxnData.plainTrack2, paddedTrack2, &paddedTrack2Len);

    // logData("Padded track2 data : %s", paddedTrack2); // TODO : Remove
    // logData("Padded track2 length : %d", paddedTrack2Len);

    // Generate byte data of the hex track 2
    unsigned char track2Byte[paddedTrack2Len / 2];
    hexToByte(paddedTrack2, track2Byte);

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, track2Byte, paddedTrack2Len / 2, buffer, &len, FALSE, TRUE);
    byteToHex(buffer, len, currentTxnData.track2Enc);
    logData("Encrypted track2 : %s", currentTxnData.track2Enc);
    logData("Encrypted track2 length : %d", strlen(currentTxnData.track2Enc));

    // Perform encryption of Pan
    // Do the padding for data as per PayTM requirement
    char paddedPan[96]; // cant be more than 96
    int paddedPanLen;
    pad0MultipleOf8(currentTxnData.plainPan, paddedPan, &paddedPanLen);

    // logData("Padded pan data : %s", paddedPan); // TODO : Remove
    // logData("Padded pan length : %d", paddedPanLen);

    // Generate byte data of the hex pan
    unsigned char panByte[paddedPanLen / 2];
    hexToByte(paddedPan, panByte);

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, panByte, paddedPanLen / 2, buffer, &len, FALSE, FALSE);
    byteToHex(buffer, len, currentTxnData.panEncrypted);
    logData("Encrypted pan : %s", currentTxnData.track2Enc);
    logData("Encrypted pan length : %d", strlen(currentTxnData.track2Enc));

    // Perform the card expiry encryption
    // Do the padding for expiry as per PayTM requirement
    char paddedExpiry[17]; // cant be more than 16
    int paddedExpLen;

    char expDateYYMM[6];
    strncpy(expDateYYMM, currentTxnData.plainExpDate, 4);
    expDateYYMM[5] = '\0';

    pad0MultipleOf8(expDateYYMM, paddedExpiry, &paddedExpLen);

    // logData("Exp date used for padding : %s and len is : %d", expDateYYMM, strlen(expDateYYMM));
    // logData("Padded exp data : %s", paddedExpiry); // TODO : Remove
    // logData("Padded exp length : %d", paddedExpLen);

    // Generate byte data of the hex pan
    unsigned char expByte[paddedExpLen / 2];
    hexToByte(paddedExpiry, expByte);

    // Do the encrypt
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, expByte, paddedExpLen / 2, buffer, &len, TRUE, FALSE);
    byteToHex(buffer, len, currentTxnData.expDateEnc);
    logData("Encrypted exp date : %s", currentTxnData.expDateEnc);
    logData("Encrypted pan length : %d", strlen(currentTxnData.expDateEnc));
}

/*
int main1(int argc, char **argv)
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hIKey = CK_INVALID_HANDLE;
    unsigned char ksn[10];
    unsigned char kcv[3];
    char label[] = "DUKPT", hex[256];
    uint16_t id = 0xCC01;
    uint8_t slot = 0;
    char track2[] = ";4111111111111111=151220100000?";
    char track2ctls[] = "4111111111111111D15122010000000F";
    unsigned char icc[] = {
        0x5A, 0x08, 0x41, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x5F, 0x24, 0x03, 0x15, 0x12, 0x31,
        0x57, 0x0F, 0x41, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                0xD1, 0x51, 0x22, 0x01, 0x00, 0x00, 0x0F
    };
    unsigned char buffer[128];
    size_t len = sizeof(buffer);

    if (argc == 1) {
        slot = FEPKCS11_APP0_TOKEN_SLOT_ID;
    } else if (argc == 2) {
        slot = atoi(argv[1]);
        if (slot < FEPKCS11_APP0_TOKEN_SLOT_ID || slot > FEPKCS11_APP7_TOKEN_SLOT_ID) {
            printf("Usage:	%s <SLOT ID 1-8>\n", argv[0]);
            return EXIT_FAILURE;
        }
    } else if (argc > 2) {
        redirect_stdio(argv[2]);
    }

    crypto_token_login(&hSession, slot);

    hIKey = get_dukpt_ikey(hSession, label, id);
    if (hIKey == CK_INVALID_HANDLE) {
        printf("No DUKPT Initial Key found (label '%s', id %02hX).\n", label, id);
        goto done;
    }

    printf("Example 1: Contact Magstripe\n");
    printf("KSN       : %s\n", bin2hex(hex, get_key_serial_number(
                       hSession, hIKey, ksn), sizeof(ksn)));
    printf("KCV(IKEY) : %s\n", bin2hex(hex, get_key_check_value(
                       hSession, hIKey, kcv), sizeof(kcv)));
    printf("Plaintext : %s\n", bin2hex(hex, track2, strlen(track2)));
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, track2, strlen(track2), buffer, &len);
    printf("CipherText: %s\n\n", bin2hex(hex, buffer, len));

    printf("Example 2: Contactless Magstripe\n");
    printf("KSN       : %s\n", bin2hex(hex, get_key_serial_number(
                       hSession, hIKey, ksn), sizeof(ksn)));
    printf("Plaintext : %s\n", bin2hex(hex, track2ctls,
                               strlen(track2ctls)));
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, track2ctls, strlen(track2ctls), buffer,
                                      &len);
    printf("CipherText: %s\n\n", bin2hex(hex, buffer, len));

    printf("Example 3: ICC (Contact and Contactless)\n");
    printf("KSN       : %s\n", bin2hex(hex, get_key_serial_number(
                       hSession, hIKey, ksn), sizeof(ksn)));
    printf("Plaintext : %s\n", bin2hex(hex, icc, sizeof(icc)));
    len = sizeof(buffer);
    dukpt_encrypt(hSession, hIKey, icc, sizeof(icc), buffer, &len);
    printf("CipherText: %s\n", bin2hex(hex, buffer, len));

done:
    crypto_token_logout(hSession);

    return EXIT_SUCCESS;
}*/
