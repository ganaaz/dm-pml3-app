#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <feig/emvco_tags.h>
#include <feig/emvco_tags.h>
#include <feig/feig_tags.h>
#include <feig/feig_e2ee_tags.h>
#include <feig/feig_trace_tags.h>
#include <feig/emvco_ep.h>

#include "include/config.h"
#include "include/tlvhelper.h"
#include "include/logutil.h"
#include "include/commonutil.h"

extern struct applicationData appData;

/**
 * Build the pre process TLV
 **/
struct tlv *buildPreprocessTlv(uint64_t transact_type,
                               uint64_t amount,
                               uint64_t amount_other,
                               uint64_t currency,
                               uint64_t currency_exponent)
{
    uint8_t transact_type_bcd[1];
    uint8_t amount_bcd[6];
    uint8_t amount_other_bcd[6];
    uint8_t currency_bcd[2];
    uint8_t currency_exp_bcd[1];
    struct tlv *tlv;
    struct tlv *tlv_tail;
    struct tlv *transaction_tlv;
    int rc;

    rc = libtlv_u64_to_bcd(transact_type, transact_type_bcd, sizeof(transact_type_bcd));
    assert(rc == TLV_RC_OK);
    rc = libtlv_u64_to_bcd(amount, amount_bcd, sizeof(amount_bcd));
    assert(rc == TLV_RC_OK);
    rc = libtlv_u64_to_bcd(amount_other, amount_other_bcd, sizeof(amount_other_bcd));
    assert(rc == TLV_RC_OK);
    rc = libtlv_u64_to_bcd(currency, currency_bcd, sizeof(currency_bcd));
    assert(rc == TLV_RC_OK);
    rc = libtlv_u64_to_bcd(currency_exponent, currency_exp_bcd, sizeof(currency_exp_bcd));
    assert(rc == TLV_RC_OK);
    (void)rc; /* prevent unused warning in -DNDEBUG case */

    tlv = tlv_new(EMV_ID_TRANSACTION_TYPE, sizeof(transact_type_bcd), &transact_type_bcd);
    transaction_tlv = tlv;
    tlv_tail = tlv;

    tlv = tlv_new(EMV_ID_AMOUNT_AUTHORIZED, sizeof(amount_bcd), amount_bcd);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    tlv = tlv_new(EMV_ID_AMOUNT_OTHER, sizeof(amount_other_bcd), amount_other_bcd);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    tlv = tlv_new(EMV_ID_TRANSACTION_CURRENCY_CODE, sizeof(currency_bcd), currency_bcd);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    tlv = tlv_new(EMV_ID_TRANSACTION_CURRENCY_EXPONENT, sizeof(currency_exp_bcd), &currency_exp_bcd);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    return transaction_tlv;
}

/**
 * Serialize the TLV
 **/
int serializeTlv(const struct tlv *tlv, void **data, size_t *data_len)
{
    int rc;
    void *d;
    size_t dl = 0;

    rc = tlv_encode(tlv, NULL, &dl);
    if (rc != TLV_RC_OK)
    {
        fprintf(stderr, "tlv_encode failed! (rc: %d) %d\n", rc, __LINE__);
        return -1;
    }
    d = malloc(dl);
    if (!d)
    {
        fprintf(stderr, "malloc failed!\n");
        return -1;
    }
    rc = tlv_encode(tlv, d, &dl);
    if (rc != TLV_RC_OK)
    {
        fprintf(stderr, "tlv_encode failed! (rc: %d) %d\n", rc, __LINE__);
        free(d);
        return -1;
    }
    *data = d;
    *data_len = dl;

    return 0;
}

/**
 * Get the current time
 **/
int getCurrentTime(struct tm *tm)
{
    time_t t;
    if (time(&t) < 0)
        return -1;
    if (!localtime_r(&t, tm))
        return -1;
    return 0;
}

/**
 * Build the transaction related TLV
 **/
struct tlv *buildTransactionTlv(void)
{
    uint8_t counter[4] = {0}; /* 4 bytes transaction counter */
    uint8_t date[3] = {0};
    uint8_t time[3] = {0};
    struct tlv *tlv;
    struct tlv *tlv_tail;
    struct tlv *transaction_tlv;
    struct tm timeinfo;
    int rc;
    uint64_t txnCounter = appData.transactionCounter;

    rc = libtlv_u64_to_bcd(txnCounter, counter, sizeof(counter));
    assert(rc == TLV_RC_OK);

    rc = getCurrentTime(&timeinfo);
    assert(!rc);

    libtlv_u64_to_bcd(timeinfo.tm_year % 100, &date[0], sizeof(date[0]));
    libtlv_u64_to_bcd(timeinfo.tm_mon + 1, &date[1], sizeof(date[1]));
    libtlv_u64_to_bcd(timeinfo.tm_mday, &date[2], sizeof(date[2]));

    libtlv_u64_to_bcd(timeinfo.tm_hour, &time[0], sizeof(time[0]));
    libtlv_u64_to_bcd(timeinfo.tm_min, &time[1], sizeof(time[1]));
    libtlv_u64_to_bcd(timeinfo.tm_sec, &time[2], sizeof(time[2]));

    tlv = tlv_new(EMV_ID_TRANSACTION_SEQUENCE_COUNTER, sizeof(counter), counter);
    transaction_tlv = tlv;
    tlv_tail = tlv;

    tlv = tlv_new(EMV_ID_TRANSACTION_DATE, sizeof(date), date);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    tlv = tlv_new(EMV_ID_TRANSACTION_TIME, sizeof(time), time);
    tlv_tail = tlv_insert_after(tlv_tail, tlv);

    return transaction_tlv;
}

/**
 * Get the outcome status string
 **/
const char *getOutcomeStatus(struct tlv *tlv_outcome)
{
    struct tlv *tlv_obj = NULL;
    uint8_t buffer[1024];
    size_t buffer_len = sizeof(buffer);

    tlv_obj = tlv_find(tlv_get_child(tlv_outcome), FEIG_ID_OUTCOME_STATUS);
    buffer_len = sizeof(buffer);
    tlv_encode_value(tlv_obj, buffer, &buffer_len);

    switch (buffer[0])
    {
    case emvco_out_na:
        return "N/A";
        break;
    case emvco_out_select_next:
        return "SELECT NEXT";
        break;
    case emvco_out_try_again:
        makeBuzz();
        return "TRY AGAIN";
        break;
    case emvco_out_approved:
        return "APPROVED";
        break;
    case emvco_out_declined:
        makeBuzz();
        return "DECLINED";
        break;
    case emvco_out_online_request:
        return "ONLINE REQUEST";
        break;
    case emvco_out_try_another_interface:
        makeBuzz();
        return "TRY ANOTHER INTERFACE";
        break;
    case emvco_out_end_application:
        makeBuzz();
        return "DECLINED";
        break;
    default:
        return "UNKNOWN";
        break;
    }

    return "UNKNOWN";
}
