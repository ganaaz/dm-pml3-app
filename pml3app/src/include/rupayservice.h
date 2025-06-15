#ifndef RUPAY_SERVICE_H
#define RUPAY_SERVICE_H

#include <stdlib.h>

#include <libpay/tlv.h>

/**
 * Service update flags
 **/
enum service_flag_t
{
    service_flag_service_update = 0,
    service_flag_service_balance_limit_update = 1,
    service_flag_service_key_plant = 2,
    service_flag_service_global_offline_balance_update = 3,
};

/**
 * Update service type info
 **/
void updateServiceType(uint8_t trx_type,
                       uint8_t currency_code[2],
                       uint8_t PRMacq_key_index[1],
                       uint8_t service_balance_limit[6]);

/*
FEIG_C13_ID_SERVICE_SIGNAL_REQ_FS
This data exchange callback is only executed, if the FCI from the SELECT AID contains a Service Directory (DF07).
The request data contain the
•	Application PAN (5A)
•	Application PAN Sequence Number (5F34)
•	DF Name (84)
The response data should be empty.
*/
int c13_service_signal_req_fs_handling(struct tlv *tlv_req, struct tlv **tlv_reply);

/*
FEIG_C13_ID_SERVICE_SIGNAL_REQ_PRE_GPO
This data exchange callback is only executed,
if the PDOL (9F38) contains the Service ID (DF16).
The request data contain the Service Directory (DF07),
if present in the FCI of the selected AID as well as
the Terminal Service Qualifier (FEIG_C13_ID_TERMINAL_SERVICE_QUALIFIER),
if configured.
The response data should contain the shortlisted Service ID (DF16).
*/
int c13_service_signal_req_pre_gpo_handling(struct tlv *tlv_req, struct tlv **tlv_reply);

/*
FEIG_C13_ID_SERVICE_SIGNAL_REQ_POST_GPO
This data exchange callback is only executed,
if the GPO Response contains the Service Related Data (DF33) and
the PDOL related data contains a Service ID (DF16) != 0000 in the GPO request.
The request data contain
•	Service Related Data (DF33)
•	Service Data Format (DF44)
The response data should be empty.
*/
int c13_service_signal_req_post_gpo_handling(struct tlv *tlv_req, struct tlv **tlv_reply);

/*
FEIG_C13_ID_SERVICE_SIGNAL_REQ_RR
This data exchange callback is only executed,
if the PDOL related data contains a Service ID (DF16) != 0000 in the GPO request.
The request data contain
•	CA Public Key Index (8F)
•	DF Name (84)
The response data should be empty.
*/
int c13_service_signal_req_rr_handling(struct tlv *tlv_req, struct tlv **tlv_reply);

/**
 * Data exchange for GAC
 **/
int c13_service_signal_req_gac_handling(struct fetpf *client,
                                        struct tlv *tlv_req, struct tlv **tlv_reply);

/**
 * Data exchange for Read Record
 **/
int c13_service_signal_req_rr_handling(struct tlv *tlv_req, struct tlv **tlv_reply);

/**
 * Data exchange for Post GPO
 **/
int c13_service_signal_req_post_gpo_handling(struct tlv *tlv_req, struct tlv **tlv_reply);

/**
 * Reset the stored candidate list
 **/
void resetServiceCandidateList();

/**
 * Add a service to the list
 **/
void addCandidate(uint8_t service_qualifier[5], uint8_t service_control[2]);

/**
 * Print the service ids
 **/
void printCandidateList();

/**
 * Sort the data
 **/
int sortFunction(const void *a, const void *b);

/**
 * Sort the stored service
 **/
void sortCandidateList();

/**
 * Check its a permananent service
 **/
bool isPermanentService(const uint8_t smi[2]);

/**
 * Check its a service data update
 **/
bool isServiceDataUpdate(const uint8_t smi[2]);

/**
 * check its a legacy one
 **/
bool isLegacy(const uint8_t avn[2]);

/**
 * Check its a balance limit update
 **/
bool isServiceBalanceLimitUpdate(const uint8_t smi[2]);

/**
 * Check its a service creation
 **/
bool isServiceCreation(const uint8_t smi[2]);

/**
 * Send the message to client
 **/
void handleServiceData();

/**
 * Read the extra service records
 **/
void readServiceRecords(struct fetpf *client, uint8_t service_limit);

/**
 * Build reply to the call back
 **/
int buildReply(const uint8_t managementInfo[2],
               const uint8_t *data, size_t data_len,
               const uint8_t summary[8],
               const uint8_t signature[8],
               const uint8_t amount_authorized[6],
               const uint8_t amount_other[6],
               struct tlv **tlv_reply);

/**
 * Build the decline reply
 **/
int buildDeclineReply(
    const uint8_t amount_authorized[6],
    struct tlv **tlv_reply);

/**
 * Check is space available
 **/
bool isServiceSpaceAvailable(const uint8_t smi[2], const uint8_t serviceAvailabilityInfo_DF03[1]);

#endif