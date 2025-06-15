
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <json-c/json.h>

#include <feig/fepkcs11.h>
#include <feig/ferkl.h>

#include "include/keymanager.h"
#include "include/config.h"
#include "include/commandmanager.h"
#include "include/commonutil.h"
#include "include/logutil.h"
#include "include/appcodes.h"
#include "include/pkcshelper.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

extern struct applicationConfig appConfig;
extern struct applicationData appData;
extern struct pkcs11 *crypto;

/**
 * Local log
 **/
static void log_cb(void *opaque, int pri, const char *msg)
{
    (void)opaque;
    logData("%d: %s\n", pri, msg);
}

/**
 * Error log
 **/
static void fatal(const char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);
}

/**
 * Do the login in Crypto
 **/
/*
static CK_SESSION_HANDLE crypto_token_login(void)
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    CK_RV rv = CKR_OK;

    rv = C_Initialize(NULL_PTR);

    if (rv != CKR_OK)
        fatal("C_Initialize() failed with rv 0x%08X!\n", (unsigned)rv);


    rv = C_OpenSession(FEPKCS11_APP3_TOKEN_SLOT_ID,
               CKF_RW_SESSION | CKF_SERIAL_SESSION,
               NULL, NULL, &hSession);

    if (rv != CKR_OK)
        fatal("C_OpenSession() failed with rv 0x%08X!\n", (unsigned)rv);


    rv = C_Login(hSession, CKU_USER, NULL_PTR, 0);

    if (rv != CKR_OK)
        fatal("C_Login() failed with rv 0x%08X!\n", (unsigned)rv);


    return hSession;
}
*/

/**
 * Check whether the key is present
 **/
static int is_key_present(CK_SESSION_HANDLE hSession, CK_OBJECT_CLASS ulClass,
                          CK_KEY_TYPE ulType, char *label)
{
    CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
    CK_ATTRIBUTE attrsKey[] = {
        {CKA_CLASS, &ulClass, sizeof(ulClass)},
        {CKA_KEY_TYPE, &ulType, sizeof(ulType)},
        {CKA_LABEL, label, strlen(label)}};
    CK_ULONG ulObjectCount = 0;
    CK_RV rv = CKR_OK;

    rv = C_FindObjectsInit(hSession, attrsKey, 3);

    if (rv != CKR_OK)
        fatal("C_FindObjectsInit() failed with rv 0x%08X!\n",
              (unsigned)rv);

    rv = C_FindObjects(hSession, &hKey, 1, &ulObjectCount);

    if (rv != CKR_OK)
        fatal("C_FindObjects() failed with rv 0x%08X!\n", (unsigned)rv);

    rv = C_FindObjectsFinal(hSession);

    if (rv != CKR_OK)
        fatal("C_FindObjectsFinal() failed with rv 0x%08X!\n",
              (unsigned)rv);

    return (int)ulObjectCount;
}

/**
 * Generate json key spec for the keys required as per KLD format
 **/
static const char *generateKeySpec(KEYDATA keyData, const char *dervType)
{
    json_object *jobj = json_object_new_object();

    // Root
    json_object *jDerivParams = json_object_new_object();
    json_object *jPkcs = json_object_new_object();

    json_object *jType = json_object_new_string(dervType);
    json_object *jSlot = json_object_new_int(keyData.slot);
    json_object *jMkVersion = json_object_new_int(keyData.mkVersion);
    json_object *jAstId = json_object_new_string(keyData.astId);

    json_object *jKeyId = json_object_new_string(keyData.pkcsId);
    json_object *jPkcsLabel = json_object_new_string(keyData.label);

    // Derviration params
    json_object_object_add(jDerivParams, "type", jType);
    json_object_object_add(jDerivParams, "slot", jSlot);
    json_object_object_add(jDerivParams, "mk_version", jMkVersion);

    if (strcmp(dervType, "None") == 0)
    {
        json_object_object_add(jDerivParams, "mk_label", json_object_new_string(keyData.label));
    }

    if (strcmp(dervType, "DUKPT") == 0)
    {
        json_object_object_add(jDerivParams, "key_set_identifier", json_object_new_string(keyData.keySetIdentifier));
    }

    json_object_object_add(jDerivParams, "astid", jAstId);

    // Add the derivation params
    json_object_object_add(jobj, "derivation_params", jDerivParams);

    // pkcs
    json_object_object_add(jPkcs, "id", jKeyId);
    json_object_object_add(jPkcs, "label", jPkcsLabel);

    json_object_object_add(jobj, "pkcs11_attrs", jPkcs);

    const char *jsonData = json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);

    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Search for the key spec and generate the missing keys
 **/
static char *generateMissingKeySpecs(CK_SESSION_HANDLE hSession)
{
    struct json_object *keyspecs = json_object_new_array();
    char *result = NULL;

    for (int i = 0; i < appConfig.keysLength; i++)
    {
        if (strcmp(appConfig.keyDataList[i]->type, KEY_TYPE_RUPAY_SERVICE) == 0)
        {
            if (!is_key_present(hSession, CKO_RUPAY_PRM_ACQ, CKK_AES,
                                appConfig.keyDataList[i]->label))
            {
                logInfo("Rupay Key : %s is not present and to be injected.",
                        appConfig.keyDataList[i]->label);
                const char *data = generateKeySpec(*appConfig.keyDataList[i], "None");
                logData("Generated Rupay Key spec : %s", data);
                json_object_array_add(keyspecs, json_tokener_parse(data));
            }
            else
            {
                logInfo("Rupay key : %s is already injected.", appConfig.keyDataList[i]->label);
            }
        }

        if (strcmp(appConfig.keyDataList[i]->type, "PAN_TOKEN") == 0)
        {
            if (!is_key_present(hSession, CKO_SECRET_KEY, CKK_GENERIC_SECRET,
                                appConfig.keyDataList[i]->label))
            {
                logInfo("Token Key : %s is not present and to be injected.",
                        appConfig.keyDataList[i]->label);
                const char *data = generateKeySpec(*appConfig.keyDataList[i], "None");
                logData("Generated Pan token Key spec : %s", data);
                json_object_array_add(keyspecs, json_tokener_parse(data));
            }
            else
            {
                logInfo("Token key : %s is already injected.", appConfig.keyDataList[i]->label);
            }
        }

        if (strcmp(appConfig.keyDataList[i]->type, KEY_TYPE_DUKPT_KEY) == 0)
        {
            if (!is_key_present(hSession, CKO_DUKPT_IKEY, CKK_DES2,
                                appConfig.keyDataList[i]->label))
            {
                logInfo("Dukpt Key : %s is not present and to be injected.",
                        appConfig.keyDataList[i]->label);
                const char *data = generateKeySpec(*appConfig.keyDataList[i], "DUKPT");
                logData("Generated Dukpt Key spec : %s", data);
                json_object_array_add(keyspecs, json_tokener_parse(data));
            }
            else
            {
                logInfo("Token key : %s is already injected.", appConfig.keyDataList[i]->label);
            }
        }
    }

    if (json_object_array_length(keyspecs) > 0)
    {
        result = strdup(json_object_to_json_string_ext(keyspecs, JSON_C_TO_STRING_PRETTY));
    }

    json_object_put(keyspecs);
    return result;
}

/**
 * Do the remote key loading
 **/
static int remote_key_loading(CK_SESSION_HANDLE hSession,
                              const char *kid_ip_str,
                              const char *keyspecs)
{
    struct in_addr kid_in_addr;
    int rc = 0;
    SSL *ssl = NULL;

    rc = inet_pton(AF_INET, kid_ip_str, &kid_in_addr);

    if (rc != 1)
    {
        fatal("inet_pton() failed to convert '%s' to a network "
              "address.\n",
              kid_ip_str);
        return EXIT_FAILURE;
    }

    rc = ferkl_init(log_cb, NULL);

    if (rc != FERKL_RC_OK)
    {
        fatal("ferkl_init() failed with rc %d!\n", rc);
        return EXIT_FAILURE;
    }

    rc = ferkl_connect(kid_in_addr, &ssl, hSession);

    if (rc != FERKL_RC_OK)
    {
        fatal("ferkl_connect() failed with rc %d to connect to KID!\n", rc);
        return EXIT_FAILURE;
    }

    rc = ferkl_import_keys_for_keyspecs(ssl, hSession, keyspecs);

    if (rc != FERKL_RC_OK)
    {
        fatal("ferkl_import_keys_for_keyspecs() failed with rc %d!\n", rc);
        return EXIT_FAILURE;
    }

    ferkl_disconnect(ssl);
    ferkl_term();

    return EXIT_SUCCESS;
}

/**
 * Logout of the crypto
 **/
/*
static void crypto_token_logout(CK_SESSION_HANDLE hSession)
{
    CK_RV rv = CKR_OK;

    rv = C_Logout(hSession);

    if (rv != CKR_OK)
        fprintf(stderr, "C_Logout() failed with rv 0x%08X!\n",
            (unsigned)rv);


    rv = C_CloseSession(hSession);

    if (rv != CKR_OK)
        fprintf(stderr, "C_CloseSession() failed with rv 0x%08X!\n",
            (unsigned)rv);


    rv = C_Finalize(NULL_PTR);

    if (rv != CKR_OK)
        fprintf(stderr, "C_Finalize() failed with rv 0x%08X!\n",
            (unsigned)rv);
}
*/

int destroyObject(CK_FUNCTION_LIST_PTR p11, CK_SESSION_HANDLE hSession, CK_OBJECT_CLASS keyClass, CK_KEY_TYPE keyType, char *label)
{
    CK_OBJECT_HANDLE hKey = CK_INVALID_HANDLE;
    CK_ATTRIBUTE attrs_key[] = {
        {CKA_CLASS, &keyClass, sizeof(keyClass)},
        {CKA_KEY_TYPE, &keyType, sizeof(keyType)},
        {CKA_LABEL, label, strlen(label)}};
    CK_ULONG ulObjectCount = 0;
    CK_RV rv = CKR_OK;

    rv = p11->C_FindObjectsInit(hSession, attrs_key, ARRAY_SIZE(attrs_key));
    if (rv != CKR_OK)
    {
        logData("%s() C_FindObjectsInit() failed! rv: 0x%08lX", __func__, rv);
        return -1;
    }

    rv = p11->C_FindObjects(hSession, &hKey, 1, &ulObjectCount);
    if (rv != CKR_OK)
    {
        logData("%s() C_FindObjects() failed! rv: 0x%08lX", __func__, rv);
        return -1;
    }

    rv = p11->C_FindObjectsFinal(hSession);
    if (rv != CKR_OK)
    {
        logData("%s() C_FindObjectsFinal() failed! rv: 0x%08lX", __func__, rv);
        return -1;
    }

    if (ulObjectCount)
    {
        rv = p11->C_DestroyObject(hSession, hKey);
        if (rv != CKR_OK)
        {
            logData("%s() C_DestroyObject() failed! rv: 0x%08lX", __func__, rv);
            return -1;
        }
        return 0;
    }
    else
    {
        logData("No key found");
        return -1;
    }

    return -1;
}

/**
 * Process key injection in loop with max from config file
 **/
void *processKeyInjection()
{
    // for (int i = 0; i < appConfig.maxKeyInjectionTry; i++)
    int i = 0;
    while (true)
    {
        i++;
        logInfo("Trying to inject key : %d", i);
        int result = doKeyInjection();
        if (result == EXIT_SUCCESS)
        {
            appData.isKeyInjectionSuccess = true;
            changeAppState(APP_STATUS_READY);
            displayLight(LED_ST_KEY_INJECT);
            break;
        }

        sleep(appConfig.keyInjectRetryDelaySec);

        if (i >= appConfig.maxKeyInjectionTry && appConfig.maxKeyInjectionTry != 0)
        {
            logInfo("Max Key Injection retry reached, exiting loop");
            break;
        }
    }

    logInfo("Key injection loop completed");
    return NULL;
}

/**
 * To destroy the key
 **/
int destroyKey(char *label, bool isBdk)
{
    if (isBdk)
    {
        CK_OBJECT_CLASS keyClass = CKO_DUKPT_IKEY;
        CK_KEY_TYPE keyType = CKK_DES2;
        return destroyObject(crypto->p11, crypto->hSession, keyClass, keyType, label);
    }
    else
    {
        CK_OBJECT_CLASS keyClass = CKO_RUPAY_PRM_ACQ;
        CK_KEY_TYPE keyType = CKK_AES;
        return destroyObject(crypto->p11, crypto->hSession, keyClass, keyType, label);
    }
}

/**
 * To check whether the key is present
 **/
int checkKeyPresent(char *label, bool isBdk)
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;

    hSession = crypto->hSession;
    if (hSession == CK_INVALID_HANDLE)
    {
        logError("Crypto invalid handle.");
        return EXIT_FAILURE;
    }

    if (isBdk == true)
    {
        if (!is_key_present(hSession, CKO_DUKPT_IKEY, CKK_DES2, label))
        {
            logData("BDK Key requested : %s is NOT present", label);
            return EXIT_FAILURE;
        }
        else
        {
            logData("BDK Key requested : %s is present", label);
            return EXIT_SUCCESS;
        }
    }
    else
    {
        if (!is_key_present(hSession, CKO_RUPAY_PRM_ACQ, CKK_AES, label))
        {
            logData("Rupay Key requested : %s is NOT present", label);
            return EXIT_FAILURE;
        }
        else
        {
            logData("Rupay Key requested : %s is present", label);
            return EXIT_SUCCESS;
        }
    }
}

/**
 * Do the key injection and get the result
 **/
int doKeyInjection()
{
    CK_SESSION_HANDLE hSession = CK_INVALID_HANDLE;
    char *keyspecs = NULL;
    int result = EXIT_FAILURE;

    // hSession = crypto_token_login();
    hSession = crypto->hSession;
    if (hSession == CK_INVALID_HANDLE)
    {
        logError("Crypto invalid handle.");
        return EXIT_FAILURE;
    }

    keyspecs = generateMissingKeySpecs(hSession);

    if (keyspecs)
    {
        logInfo("Going to contact key loading for injecting keys");
        result = remote_key_loading(hSession, appConfig.kldIP, keyspecs);
        free(keyspecs);
    }
    else
    {
        logInfo("All keys are already injected, nothing to be done.");
        result = EXIT_SUCCESS;
    }

    // crypto_token_logout(hSession);

    return result;
}
