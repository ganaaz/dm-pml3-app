#ifndef PKCSHELPER_H
#define PKCSHELPER_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <feig/fepkcs11.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

    /**
     * To store pkcs info
     **/
    struct pkcs11
    {
        void *p11_handle;
        CK_FUNCTION_LIST_PTR p11;
        CK_SESSION_HANDLE hSession;
    };

    /**
     * Clean
     **/
    void pkcs11_free(struct pkcs11 **ctx_ptr);

    /**
     * Create new
     **/
    struct pkcs11 *pkcs11_new(const char *p11Libname, CK_SLOT_ID *slotID);

#ifdef __cplusplus
}
#endif

#endif /* ndef HELPER_HPP */