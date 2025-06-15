#ifndef TLVHELPER_H_
#define TLVHELPER_H_

#include <libpay/tlv.h>

/**
 * Build the preprocess TLV for the transaction
 **/
struct tlv *buildPreprocessTlv(uint64_t transact_type,
                               uint64_t amount,
                               uint64_t amount_other,
                               uint64_t currency,
                               uint64_t currency_exponent);

/**
 * Build the transaction TLV
 **/
struct tlv *buildTransactionTlv(void);

/**
 * Serialize the TLV
 **/
int serializeTlv(const struct tlv *tlv, void **data, size_t *data_len);

/**
 * Populate the time
 **/
int getCurrentTime(struct tm *tm);

/**
 * Get the outcome status string
 **/
const char *getOutcomeStatus(struct tlv *tlv_outcome);

#endif