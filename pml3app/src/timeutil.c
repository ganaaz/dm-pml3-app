#include <feig/fepkcs11.h>
#include <feig/fetrm.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "include/timeutil.h"
#include "include/logutil.h"
#include "include/pkcshelper.h"

#define ARRAY_SIZE(array) \
    (sizeof(array) / sizeof(*array))

extern struct pkcs11 *crypto;

static void get_system_clock(CK_SESSION_HANDLE hSession,
                             CK_OBJECT_HANDLE_PTR phClock)
{
    CK_OBJECT_CLASS hwFeatureClass = CKO_HW_FEATURE;
    CK_HW_FEATURE_TYPE typeClock = CKH_CLOCK;
    uint16_t id = FEPKCS11_SYSTEM_CLOCK_ID;
    CK_ATTRIBUTE clockTemplate[] = {
        {CKA_CLASS, &hwFeatureClass, sizeof(hwFeatureClass)},
        {CKA_HW_FEATURE_TYPE, &typeClock, sizeof(typeClock)},
        {CKA_ID, &id, sizeof(id)}};
    CK_ULONG ulFound = 0;
    CK_RV rv = CKR_OK;

    *phClock = CK_INVALID_HANDLE;

    rv = C_FindObjectsInit(hSession, clockTemplate,
                           ARRAY_SIZE(clockTemplate));
    if (rv != CKR_OK)
    {
        logError("%s(): C_FindObjectsInit failed. rv 0x%08x",
                 __func__, (unsigned)rv);
        return;
    }

    rv = C_FindObjects(hSession, phClock, 1, &ulFound);
    C_FindObjectsFinal(hSession);
    if (rv != CKR_OK)
    {
        logError("%s(): C_FindObjects failed. rv 0x%08x",
                 __func__, (unsigned)rv);
        return;
    }

    if (ulFound != 1)
    {
        logError("%s(): No Clock object found! ulFound %u",
                 __func__, (unsigned)ulFound);
        return;
    }
}

CK_RV get_utc(CK_SESSION_HANDLE hSession, char utc[17])
{
    CK_OBJECT_HANDLE hClock = CK_INVALID_HANDLE;
    CK_ATTRIBUTE clockValue = {CKA_VALUE, utc, 16};
    CK_RV rv = CKR_OK;

    strcpy(utc, "0000000000000000");

    get_system_clock(hSession, &hClock);

    if (hClock == CK_INVALID_HANDLE)
        return CKR_GENERAL_ERROR;

    rv = C_GetAttributeValue(hSession, hClock, &clockValue, 1);
    if (rv != CKR_OK)
        logError("%s(): C_GetAttributeValue failed. rv 0x%08x",
                 __func__, (unsigned)rv);
    return rv;
}

CK_RV set_utc(CK_SESSION_HANDLE hSession, char utc[17])
{
    CK_OBJECT_HANDLE hClock = CK_INVALID_HANDLE;
    CK_ATTRIBUTE clockValue = {CKA_VALUE, utc, 16};
    CK_RV rv = CKR_OK;

    utc[14] = '0';
    utc[15] = '0';
    utc[16] = '\0';

    get_system_clock(hSession, &hClock);
    if (hClock == CK_INVALID_HANDLE)
        return CKR_GENERAL_ERROR;

    rv = C_SetAttributeValue(hSession, hClock, &clockValue, 1);
    if (rv != CKR_OK)
        logError("%s(): C_SetAttributeValue failed. rv 0x%08x",
                 __func__, (unsigned)rv);
    return rv;
}

/**
 * To set the reader time
 **/
int setReaderTime(char utc[17])
{
    logData("Going to change the reader time to : %s", utc);
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_RV rv = CKR_OK;
    // int rc = 0;
    // CK_SLOT_ID slot_id;
    // char set_time = 0;

    hSession = crypto->hSession;

    // rv = C_Initialize(NULL_PTR);
    // if (rv != CKR_OK)
    //	goto done;

    // rc =  fetrm_get_slot_id_by_uid(&slot_id);
    // if (rc != 0)
    //	goto done;

    /*
    rv = C_OpenSession(slot_id, CKF_RW_SESSION | CKF_SERIAL_SESSION, NULL, NULL, &hSession);
    if (rv != CKR_OK)
        goto done;

    rv = C_Login(hSession, CKU_USER, NULL_PTR, 0);
    if (rv != CKR_OK)
        goto done;
    logData("Session opened and initialized, going to change");
    */

    /* Set the time in UTC in format: YYYYMMDDhhmmss00 */
    rv = set_utc(hSession, utc);
    rv = get_utc(hSession, utc);
    logData("Time changed, now the reader time is : %s", utc);

    // done:
    // C_Logout(hSession);
    // C_CloseSession(hSession);
    // C_Finalize(NULL_PTR);

    if (rv != CKR_OK)
    {
        logError("Error : Failed with rv 0x%08lX", (unsigned long)rv);
        return -1;
    }
    return 0;
}
