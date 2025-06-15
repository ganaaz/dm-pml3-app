#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <feig/fetrm.h>

#include "include/pkcshelper.h"
#include "include/logutil.h"

/**
 * Load module
 **/
static void *C_LoadModule(const char *soname, CK_FUNCTION_LIST_PTR_PTR functions)
{
    CK_RV (*get_function_list)(CK_FUNCTION_LIST_PTR_PTR) = NULL;
    void *handle = NULL;
    CK_RV rv = CKR_OK;

    handle = dlopen(soname, RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
        fprintf(stderr, "%s(): dlopen(%s) failed: %s\n",
                __func__, soname, dlerror());
        return NULL;
    }

    /*
     * The C99 standard leaves casting from "void *" to a function
     * pointer undefined. The assignment used below is the POSIX.1-2003
     * (Technical Corrigendum 1) workaround; see the Rationale for the
     * POSIX specification of dlsym().
     */
    *(void **)(&get_function_list) = dlsym(handle, "C_GetFunctionList");

    if (!get_function_list)
    {
        fprintf(stderr, "%s(): dlsym() failed: %s\n", __func__,
                dlerror());
        dlclose(handle);
        return NULL;
    }

    rv = get_function_list(functions);
    if (rv != CKR_OK)
    {
        fprintf(stderr, "%s(): get_function_list() failed: rv 0x%08x\n",
                __func__, (unsigned)rv);
        dlclose(handle);
        return NULL;
    }

    return handle;
}

/**
 * unload crypto module
 **/
static void C_UnloadModule(void *handle)
{
    /* Only close and unload the shared object if
     * we are not profiling or using memcheck
     *
     * Unloading results in some strange errors while using valgrind
     */
#ifndef PROFILING
    dlclose(handle);
#endif
}

/**
 * Cleanup pkcs
 **/
void pkcs11_free(struct pkcs11 **ctx_ptr)
{
    if (NULL == ctx_ptr)
    {
        return;
    }
    struct pkcs11 *ctx = *ctx_ptr;
    if (NULL == ctx)
    {
        return;
    }

    if (ctx->hSession != CK_INVALID_HANDLE)
    {
        CK_RV rv = CKR_OK;

        rv = ctx->p11->C_Logout(ctx->hSession);
        if (CKR_OK != rv)
        {
            fprintf(stderr, "%s(): C_Logout() failed: rv 0x%08lx\n",
                    __func__, rv);
        }

        rv = ctx->p11->C_CloseSession(ctx->hSession);
        if (CKR_OK != rv)
        {
            fprintf(stderr, "%s(): C_CloseSession() failed: rv 0x%08lx\n",
                    __func__, rv);
        }

        rv = ctx->p11->C_Finalize(NULL_PTR);
        if (rv != CKR_OK)
        {
            fprintf(stderr, "C_Finalize failed (rv 0x%08lx)!\n", rv);
        }
    }

    if (ctx->p11_handle)
    {
        C_UnloadModule(ctx->p11_handle);
    }
    free(ctx);
    ctx = NULL;
}

/**
 * Create new pkcs
 **/
struct pkcs11 *pkcs11_new(const char *p11Libname, CK_SLOT_ID *slotID)
{
    CK_SLOT_ID slot_id;
    CK_RV rv = CKR_OK;
    struct pkcs11 *ctx = NULL;
    ctx = (struct pkcs11 *)malloc(sizeof(struct pkcs11));
    if (NULL == ctx)
    {
        fprintf(stderr, "%s[%d]: %s()\n", __FILE__, __LINE__, __func__);
        return NULL;
    }

    ctx->p11_handle = C_LoadModule(p11Libname, &ctx->p11);
    if (!ctx->p11_handle)
    {
        fprintf(stderr, "%s[%d]: %s()\n", __FILE__, __LINE__, __func__);
        pkcs11_free(&ctx);
        return NULL;
    }

    if (NULL != slotID)
    {
        slot_id = *slotID;
    }
    else
    {
        // FEPKCS11_FIRMWARE_TOKEN_SLOT_ID
        /* Get current slot ID */
        int rc = fetrm_get_slot_id_by_uid(&slot_id);
        if (0 != rc)
        {
            // fprintf(stderr, "%s: %s() [%d]: fetrm_get_slot_id_by_uid() failed! rc: %d",
            //					__FILE__, __func__, __LINE__, rc);
            // return NULL;
            //  Use Slot ID 0x00
            slot_id = FEPKCS11_FIRMWARE_TOKEN_SLOT_ID;
        }
    }

    rv = ctx->p11->C_Initialize(NULL_PTR);
    if ((rv != CKR_OK) && (rv != CKR_CRYPTOKI_ALREADY_INITIALIZED))
    {
        fprintf(stderr, "C_Initialize() failed (rv 0x%08lx)!\n", rv);
        return NULL;
    }

    rv = ctx->p11->C_OpenSession(slot_id, CKF_RW_SESSION | CKF_SERIAL_SESSION, NULL, NULL, &ctx->hSession);
    if (ctx->hSession == CK_INVALID_HANDLE)
    {
        fprintf(stderr, "%s[%d]: %s() rv = %lX; slot_id = 0x%08lx\n", __FILE__, __LINE__, __func__, rv, slot_id);
        pkcs11_free(&ctx);
        return NULL;
    }

    rv = ctx->p11->C_Login(ctx->hSession, CKU_USER, NULL_PTR, 0);
    if (CKR_OK != rv)
    {
        fprintf(stderr, "%s[%d]: %s()\n", __FILE__, __LINE__, __func__);
        pkcs11_free(&ctx);
        return NULL;
    }
    return ctx;
}
