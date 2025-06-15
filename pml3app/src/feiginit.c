#include <stdio.h>
#include <errno.h>
#include <sys/fcntl.h>

#include <feig/fetrm.h>
#include <feig/fetpf.h>

#include "include/config.h"
#include "include/commonutil.h"
#include "include/pkcshelper.h"
#include "include/rupayservice.h"
#include "include/logutil.h"
#include "include/commandmanager.h"

CK_SLOT_ID slot_id;
struct pkcs11 *crypto;

extern struct applicationData appData;

/**
 * Do the initialization of the feig reader
 **/
int initializeFeigReader(struct fetpf **fetpfClient)
{
    int rc;
    int fd_feclr = -1;
    struct fetpf *fetpf;

    rc = fetrm_get_slot_id_by_uid(&slot_id);
    if (0 != rc)
    {
        logError("%s: %s() [%d]: fetrm_get_slot_id_by_uid() failed! rc: %d errno: %d",
                 __FILE__, __func__, __LINE__, rc, errno);
        return EXIT_FAILURE;
    }

    crypto = pkcs11_new("libfepkcs11.so", &slot_id);
    if (NULL == crypto)
    {
        return EXIT_FAILURE;
    }

    fetpf = fetpf_new(slot_id);
    if (!fetpf)
    {
        logError("fetpf_new() failed\n");
        return EXIT_FAILURE;
    }

    fd_feclr = open("/dev/feclr0", O_RDWR | O_CLOEXEC);
    if (fd_feclr < 0)
    {
        logError("feclr open failed: %m\n");
        return EXIT_FAILURE;
    }
    fetpf_set_user_data(fetpf, (void *)fd_feclr);

    // register ep client to fetpfd
    rc = fetpf_register_ep_client(fetpf);
    if (rc != FETPF_RC_OK)
    {
        logError("fetpf_register_ep_client failed (rc: %d)\n", rc);
        return EXIT_FAILURE;
    }

    // register all kernel
    rc = fetpf_ep_register_kernel(fetpf, "libemvco_c2.so", (uint8_t *)"\x02", 1);
    if (rc != FETPF_RC_OK)
    {
        logError("%s: fetpf_ep_register_kernel failed (rc %d)!\n",
                 __func__, rc);
        return EXIT_FAILURE;
    }

    rc = fetpf_ep_register_kernel(fetpf, "libemvco_c3.so", (uint8_t *)"\x03", 1);
    if (rc != FETPF_RC_OK)
    {
        logError("%s: fetpf_ep_register_kernel failed (rc %d)!\n",
                 __func__, rc);
        return EXIT_FAILURE;
    }

    rc = fetpf_ep_register_kernel(fetpf, "libemvco_c4.so", (uint8_t *)"\x04", 1);
    if (rc != FETPF_RC_OK)
    {
        logError("%s: fetpf_ep_register_kernel failed (rc %d)!\n",
                 __func__, rc);
        return EXIT_FAILURE;
    }

    rc = fetpf_ep_register_kernel(fetpf, "libemvco_c6.so", (uint8_t *)"\x06", 1);
    if (rc != FETPF_RC_OK)
    {
        logError("%s: fetpf_ep_register_kernel failed (rc %d)!\n",
                 __func__, rc);
        return EXIT_FAILURE;
    }

    // Rupay Kernel
    rc = fetpf_ep_register_kernel(fetpf, "libemvco_c13.so", (uint8_t *)"\x0D", 1);
    if (rc != FETPF_RC_OK)
    {
        logError("%s: fetpf_ep_register_kernel failed (rc %d)!\n",
                 __func__, rc);
        return EXIT_FAILURE;
    }

    *fetpfClient = fetpf;
    logInfo("Feig reader is successfully initialized.");

    return 0;
}