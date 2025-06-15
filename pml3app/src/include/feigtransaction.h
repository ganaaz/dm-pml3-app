#ifndef FEIGTRANSACTION_H_
#define FEIGTRANSACTION_H_

#include <stdlib.h>

/**
 * Read the emv config file and populate the configuration that is required
 * by Feig
 **/
int readEmvConfig(const char *path, void **config, size_t *config_len);

/**
 * Perform the transaction
 **/
void *processTransaction(void *arg);

/**
 * Generate pre process data based on the config
 **/
int generatePreProcessData(void **preData, size_t *preDataLen);

/**
 * Update amount, trx type, trx bin, processing code of
 * current transaction
 **/
void populateTrxDetails(uint64_t amount_authorized_bin);

/**
 * On cancel call back
 **/
int callBackSurrender(struct fetpf *client);

/**
 * Call back for track2 where we get the track 2 data or pan token here
 **/
void callBacktrack2(struct fetpf *client, const void *track2, size_t track_len);

/**
 * For on line call back request to be handled used for balance update
 **/
int callBackOnlineRequest(struct fetpf *client,
                          const void *outcome, size_t outcome_len,
                          void *response, size_t *response_len,
                          void *out_data, size_t *out_data_len);

/**
 * To handle the online request to the host
 **/
int handle_online_request(const void *outcome, size_t outcome_len,
                          uint8_t *online_response, size_t *online_response_len);

/**
 * Data exchange event from kernel where all the activities are handled
 * Basedd on the request reply is generated and returned
 **/
int callBackDataExchange(struct fetpf *client,
                         const void *source, size_t source_len,
                         const void *request, size_t request_len,
                         void *reply, size_t *reply_len);

int closed_loop_card_found(struct fetpf *client,
                           uint64_t tech, const void *in_data, size_t in_data_len,
                           void *out_data, size_t *out_data_len);

int callBackFinalOutcome(struct fetpf *client, const void *outcome,
                         size_t outcome_len, const void *online_response, size_t online_response_len,
                         void *out_data, size_t *out_data_len);

/**
 * Read the limit from config file and populate the inmemory data
 * for validation when the receive the amount
 **/
// void readAndStoreLimit(void *config, size_t configLen);

/**
 * Read the CVM limit from config file and populate the inmemory data
 * for validation when the receive the amount
 **/
// void readAndStoreCVMLimit(void *config, size_t configLen);

/**
 * Read the floor limit from the config file and used for validation
 * once the write amount is received
 **/
// void readAndStoreFloorLimit(void *config, size_t configLen);

#endif