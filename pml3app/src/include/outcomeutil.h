#ifndef OUTCOMEUTIL_H_
#define OUTCOMEUTIL_H_

/**
 * Handle the transaction completion message
 **/
void handleTransactionCompletion(const void *outcome, size_t outcomeLen);

/**
 * Get the updated balance
 **/
char *getUpdatedBalance(struct tlv *tlv_outcome);

/**
 * Handle the completion when the card is not presented again and do reversal
 */
void handleSecondTapNotPresented();

/**
 * Populate card expirty in current txn data
 **/
void populateCardExpiry(uint8_t *buffer, size_t buffer_len);

/**
 * to perform the date checking with the expiry date
 */
void performCheckDate();

#endif