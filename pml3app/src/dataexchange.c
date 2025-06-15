#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <feig/emvco_ep.h>
#include <feig/feig_tags.h>
#include <feig/fetpf.h>

#include "include/config.h"
#include "include/logutil.h"
#include "include/dataexchange.h"
#include "include/rupayservice.h"
#include "include/commonutil.h"

extern struct applicationConfig appConfig;
extern struct transactionData currentTxnData;

/**
 * Handle the data exchange from the kernel, process the actions and return the value
 * back to the kernel
 **/
int handleDataExchange(struct fetpf *client, const void *request, size_t request_len,
                       void *reply, size_t *reply_len)
{
    int rc = EMVCO_RC_OK;
    struct tlv *tlv_req = NULL;
    struct tlv *tlv_reply = NULL;
    uint8_t *reply_data = NULL;
    struct tlv *tlv_reply_data = NULL;
    struct tlv *tlv_attr = NULL;

    logHexData("Rupay request data", request, request_len);
    printTlvData((uint8_t *)request, request_len);

    if (request && request_len > 0)
    {
        int tlv_rc = tlv_parse(request, request_len, &tlv_req);
        if (tlv_rc != TLV_RC_OK)
        {
            logData("tlv_parse() failed with %d\n", tlv_rc);
            rc = EMVCO_RC_FAIL;
            goto done;
        }

        for (tlv_attr = tlv_req; tlv_attr; tlv_attr = tlv_get_next(tlv_attr))
        {
            uint8_t tag[TLV_MAX_TAG_LENGTH];
            size_t tag_sz = sizeof(tag);
            tlv_rc = tlv_encode_identifier(tlv_attr, tag, &tag_sz);
            if (tlv_rc != TLV_RC_OK)
            {
                logData("tlv_encode_identifier() failed with %d\n", tlv_rc);
                continue;
            }

            logData("++++++++++++++++++++++++++++++++\n");
            logData(" RuPay Data Exchange Signal\n");
            logData("++++++++++++++++++++++++++++++++\n");

            if (0 == libtlv_compare_ids(FEIG_C13_ID_SERVICE_SIGNAL_REQ_FS, tag))
            {
                logData("Service Signal after SELECT");
                // Simple
                rc = c13_service_signal_req_fs_handling(tlv_req, &tlv_reply_data);
            }
            else if (0 == libtlv_compare_ids(FEIG_C13_ID_SERVICE_SIGNAL_REQ_PRE_GPO, tag))
            {

                logData("Service Signal before GPO");
                rc = c13_service_signal_req_pre_gpo_handling(tlv_req, &tlv_reply_data);

                logData("Pre GPO Result : %d", rc);
                if (rc == EMVCO_RC_FAIL)
                {
                    goto done;
                }
            }
            else if (0 == libtlv_compare_ids(FEIG_C13_ID_SERVICE_SIGNAL_REQ_POST_GPO, tag))
            {
                logData("Service Signal after GPO");
                // Simple
                rc = c13_service_signal_req_post_gpo_handling(tlv_req, &tlv_reply_data);
            }
            else if (0 == libtlv_compare_ids(FEIG_C13_ID_SERVICE_SIGNAL_REQ_RR, tag))
            {
                logData("Service Signal after Read Records");
                // Simple
                rc = c13_service_signal_req_rr_handling(tlv_req, &tlv_reply_data);
            }
            else if (0 == libtlv_compare_ids(FEIG_C13_ID_SERVICE_SIGNAL_REQ_GAC, tag))
            {
                logData("Service Signal before Gen AC");
                size_t value_sz = 0;
                tlv_rc = tlv_encode_value(tlv_attr, NULL, &value_sz);
                if (tlv_rc != TLV_RC_OK)
                {
                    logData("tlv_encode_value() failed with %d\n", tlv_rc);
                    rc = EMVCO_RC_FAIL;
                    goto done;
                }
                else if (value_sz == 0)
                {
                    logData("tlv_encode_value() returnd value_sz of 0\n");
                    rc = EMVCO_RC_FAIL;
                    goto done;
                }

                uint8_t value[value_sz];
                tlv_rc = tlv_encode_value(tlv_attr, value, &value_sz);
                if (tlv_rc != TLV_RC_OK)
                {
                    logData("tlv_encode_value() failed with %d\n", tlv_rc);
                    rc = EMVCO_RC_FAIL;
                    goto done;
                }

                struct tlv *tlv_request_data = NULL;
                tlv_rc = tlv_parse(value, value_sz, &tlv_request_data);
                if (tlv_rc != TLV_RC_OK)
                {
                    logData("tlv_parse() failed with %d\n", tlv_rc);
                    rc = EMVCO_RC_FAIL;
                    goto done;
                }

                rc = c13_service_signal_req_gac_handling(client, tlv_request_data, &tlv_reply_data);

                if (tlv_request_data)
                    tlv_free(tlv_request_data);
            }
            else
            {
                logData("UNKNOWN\n");
            }

            logData("RuPay Service signal handling returned with %d\n", rc);

            if (reply_len)
            {
                // logData("Reply available size: %d\n", *reply_len);
                if (tlv_reply_data)
                {
                    size_t reply_data_sz = 0;
                    tlv_rc = tlv_encode(tlv_reply_data, NULL, &reply_data_sz);
                    if (tlv_rc != TLV_RC_OK)
                    {
                        logData("[%d]: tlv_encode() failed with %d\n", __LINE__, tlv_rc);
                        rc = EMVCO_RC_FAIL;
                        goto done;
                    }
                    else if (reply_data_sz == 0)
                    {
                        logData("[%d]: reply_data_sz is 0\n", __LINE__);
                        rc = EMVCO_RC_FAIL;
                        goto done;
                    }
                    reply_data = (uint8_t *)malloc(reply_data_sz);
                    if (!reply_data)
                    {
                        logData("[%d]: malloc() failed\n", __LINE__);
                        rc = EMVCO_RC_FAIL;
                        goto done;
                    }
                    tlv_rc = tlv_encode(tlv_reply_data, reply_data, &reply_data_sz);
                    if (tlv_rc != TLV_RC_OK)
                    {
                        logData("[%d]: tlv_encode() failed with %d\n", __LINE__, tlv_rc);
                        rc = EMVCO_RC_FAIL;
                        goto done;
                    }

                    tlv_reply = tlv_new(FEIG_C13_ID_DATA_EXCHANGE_RESPONSE, reply_data_sz, reply_data);
                    tlv_rc = tlv_encode(tlv_reply, reply, reply_len);
                    if (tlv_rc != TLV_RC_OK)
                    {
                        logData("tlv_encode() failed with %d\n", tlv_rc);
                        rc = EMVCO_RC_FAIL;
                        goto done;
                    }
                }
                else
                {
                    *reply_len = 0;
                }
                if (appConfig.isDebugEnabled == 1)
                {
                    if (*reply_len > 0)
                    {
                        printf("Reply len: %zu\n", *reply_len);
                        size_t i = 0;
                        printf("Reply: ");
                        for (i = 0; i < *reply_len; i++)
                        {
                            printf("%02X", ((uint8_t *)reply)[i]);
                        }
                        printf("\n");
                        printf("++++++++++++++++++++++++++++++++\n\n");
                    }
                }
            }
        }
    }

done:
    if (tlv_attr)
        tlv_free(tlv_attr);

    if (tlv_reply_data)
        tlv_free(tlv_reply_data);

    if (tlv_req)
        tlv_free(tlv_req);

    if (tlv_reply)
        tlv_free(tlv_reply);

    if (reply_data)
        free(reply_data);

    return rc;
}
