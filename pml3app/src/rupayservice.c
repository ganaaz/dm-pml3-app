#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#include <libpay/tlv.h>
#include <feig/feclr.h>
#include <feig/fetpf.h>
#include <feig/fepkcs11.h>
#include <feig/emvco_ep.h>
#include <feig/emvco_tags.h>
#include <feig/feig_tags.h>
#include <feig/emvco_hal.h>
#include <feig/rupay_sbt.h>

#include "include/rupayservice.h"
#include "include/commonutil.h"
#include "include/logutil.h"
#include "include/config.h"
#include "include/pkcshelper.h"
#include "include/commandmanager.h"
#include "include/responsemanager.h"
#include "include/appcodes.h"
#include "include/dukpt-util.h"

#define DF32_LENGTH 4

#ifndef FEIG_C13_ID_TRANSACTION_CONTROL
#define FEIG_C13_ID_TRANSACTION_CONTROL "\xDF\xFE\xCD\x11" // will be defined in next SDK
#endif

/****************************** DF15 Service Management Info ******************************/
/* DF15 Byte 1 bit 8 */
#define DF15_SERVICE_MANAGEMENT_INFO_TEMPORARY_SERVICE 0x00
#define DF15_SERVICE_MANAGEMENT_INFO_PERMANENT_SERVICE 0x80

/* DF15 Byte 1 bit 7-6 */
#define DF15_SERVICE_MANAGEMENT_INFO_SERVICE_SPECIFIC_DATA 0x00
#define DF15_SERVICE_MANAGEMENT_INFO_SERVICE_KEY_PLANT 0x20
#define DF15_SERVICE_MANAGEMENT_INFO_SERVICE_BALANCE_LIMIT_UPDATE 0x40

/* DF15 Byte 1 bit 5 */
#define DF15_SERVICE_MANAGEMENT_INFO_ACTIVE 0x10
#define DF15_SERVICE_MANAGEMENT_INFO_INACTIVE 0x00

/* DF15 Byte 1 bit 4 */
#define DF15_SERVICE_MANAGEMENT_INFO_BLOCKED 0x08
#define DF15_SERVICE_MANAGEMENT_INFO_UNBLOCKED 0x00

/* DF15 Byte 1 bit 3 and Byte 2 bit 7-1 */
#define DF15_SERVICE_MANAGEMENT_INFO_RFU 0x00

/* DF15 Byte 1 bit 2 */
#define DF15_SERVICE_MANAGEMENT_INFO_WALLET_INACTIVE 0x02
#define DF15_SERVICE_MANAGEMENT_INFO_WALLET_ACTIVE 0x00

/* DF15 Byte 1 bit 1 */
#define DF15_SERVICE_MANAGEMENT_INFO_USE_SERVICE_LOCAL_BALANCE 0x00
#define DF15_SERVICE_MANAGEMENT_INFO_USE_CARD_GLOBAL_BALANCE 0x01

/* DF15 Byte 2 bit 8 */
#define DF15_SERVICE_MANAGEMENT_INFO_CURRENCY_CODE_KILOMETRES 0x80
#define DF15_SERVICE_MANAGEMENT_INFO_CURRENCY_CODE_INDIAN_RUPEE 0x00

/* DF03 Byte 1 bit 8 */
#define DF03_SERVICE_AVAILABILITY_INFO_PERMANENT_SERVICE_SPACE_AVAILABLE 0x80
#define DF03_SERVICE_AVAILABILITY_INFO_TEMPORARY_SERVICE_SPACE_AVAILABLE 0x40

// uint8_t PRMiss_rupay_testcard[16] = {0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13};
// uint8_t IIN_rupay_testcard[3] = {0x60, 0x83, 0x26};

extern CK_SLOT_ID slot_id;
extern struct pkcs11 *crypto;
extern bool isSecondTap;
extern struct applicationConfig appConfig;
extern struct applicationData appData;
extern struct transactionData currentTxnData;
extern int isWriteCardCommand;
extern char writeCardAmount[13];
extern char writeCardServiceData[193];
extern int isServiceDataAvailable;
extern long long deAzCompletedTime;
extern long long deEvent;
extern pthread_mutex_t lockRupayService;

struct service_trx_data
{
    uint8_t serviceLimit;
    uint8_t trxType;
    uint8_t balance[6];
    uint8_t PRMacqKeyIndex[1];
    uint8_t currencyCode[2];
};

struct service_trx_data serviceTrxData;

struct serviceMetadata
{
    uint8_t serviceQualifier[5];
    uint8_t serviceControl[2];
};
struct serviceMetadata *serviceCandidatelist[256] = {0};
size_t serviceCandidatelist_len = 0;
struct serviceMetadata *selectedService = NULL;

/*
static uint64_t bin2uint64(uint8_t *bin)
{
    uint64_t uint64 = 0;
    uint64 = ((uint64_t) bin[0]) << 56 |
             ((uint64_t) bin[1]) << 48 |
             ((uint64_t) bin[2]) << 40 |
             ((uint64_t) bin[3]) << 32 |
             ((uint64_t) bin[4]) << 24 |
             ((uint64_t) bin[5]) << 16 |
             ((uint64_t) bin[6]) << 8 |
             ((uint64_t) bin[7]);
    return uint64;
}

static void uint642bin(uint64_t uint64, uint8_t *bin)
{
    bin[0] = (uint8_t) (uint64 >> 56) & 0xFF;
    bin[1] = (uint8_t) (uint64 >> 48) & 0xFF;
    bin[2] = (uint8_t) (uint64 >> 40) & 0xFF;
    bin[3] = (uint8_t) (uint64 >> 32) & 0xFF;
    bin[4] = (uint8_t) (uint64 >> 24) & 0xFF;
    bin[5] = (uint8_t) (uint64 >> 16) & 0xFF;
    bin[6] = (uint8_t) (uint64 >> 8) & 0xFF;
    bin[7] = (uint8_t) (uint64 & 0xFF);
}
*/

/*
| Service ID      | Description                                                          |
|-----------------|----------------------------------------------------------------------|
| Hex FFFF        |  RFU                                                                 |
| Hex FFFE        | Indicates Global offline balance is to be updated                    |
| Hex 0010 – FFFD | Available for Service owner                                          |
| Hex 0001 - 000F | RFU                                                                  |
| Hex 0000        | Indicates No Service specified (i.e. not a Servicebased transaction) |
*/

void updateServiceType(uint8_t trx_type,
                       uint8_t currency_code[2],
                       uint8_t PRMacq_key_index[1],
                       uint8_t service_balance_limit[6])
{
    char tempData[33] = {0};
    memset(&serviceTrxData, 0x00, sizeof(serviceTrxData));
    serviceTrxData.trxType = trx_type;

    memcpy(serviceTrxData.currencyCode, currency_code, 2);
    if (serviceTrxData.trxType == 0x83)
    {
        memcpy(serviceTrxData.PRMacqKeyIndex, PRMacq_key_index, 1);
    }
    memcpy(serviceTrxData.balance, service_balance_limit, 6);

    logData("Selected service settings:");

    libtlv_bin_to_hex(&serviceTrxData.trxType, 1, tempData);
    logData("Transaction Type: %s", tempData);

    libtlv_bin_to_hex(serviceTrxData.currencyCode, 2, tempData);
    logData("Currency Code: %s", tempData);

    libtlv_bin_to_hex(serviceTrxData.balance, 6, tempData);

    if (serviceTrxData.trxType == 0x83)
    {
        libtlv_bin_to_hex(&serviceTrxData.PRMacqKeyIndex, 1, tempData);
        logData("PRMacq Key Index: %s", tempData);
    }

    serviceTrxData.serviceLimit = 0;
    logData("");
}

/*
FEIG_C13_ID_SERVICE_SIGNAL_REQ_FS
This data exchange callback is only executed, if the FCI from the SELECT AID contains a Service Directory (DF07).
The request data contain the
•	Application PAN (5A)
•	Application PAN Sequence Number (5F34)
•	DF Name (84)
The response data should be empty.
*/
int c13_service_signal_req_fs_handling(struct tlv *tlv_req, struct tlv **tlv_reply)
{
    int rc = EMVCO_RC_OK;
    if (appConfig.isTimingEnabled)
        logTimeWarnData("DE, File Handling Event");

    if (appConfig.isDebugEnabled == 1)
    {
        if (tlv_req)
        {
            uint8_t buf[256] = {0};
            size_t buf_len = sizeof(buf);
            tlv_encode_value(tlv_req, buf, &buf_len);
            logHexData("Signal FS TLV Request", buf, buf_len);
        }
    }

    long long eventEnd = getCurrentSeconds();
    if (appConfig.isTimingEnabled)
    {
        logTimeWarnData("DE, FS Handling Call back End : %lld", eventEnd);
        logTimeWarnData("DE, FS Handling Call back, time took (Az Code) : %lld", (eventEnd - deEvent));
    }
    deAzCompletedTime = eventEnd;
    return rc;
}

/*
FEIG_C13_ID_SERVICE_SIGNAL_REQ_PRE_GPO
This data exchange callback is only executed,
if the PDOL (9F38) contains the Service ID (DF16).
The request data contain the Service Directory (DF07), if present in the FCI of the selected AID
as well as the Terminal Service Qualifier (FEIG_C13_ID_TERMINAL_SERVICE_QUALIFIER), if con-figured.
The response data should contain the Service ID (DF16) which should be selected.
*/
int c13_service_signal_req_pre_gpo_handling(struct tlv *tlv_req, struct tlv **tlv_reply)
{
    if (appConfig.isTimingEnabled)
        logTimeWarnData("DE, GPO Handling");

    if (strcmp(currentTxnData.trxType, TRXTYPE_SERVICE_CREATE) == 0)
    {
        logData("Service creation requested with the service id : %s", appData.createServiceId);
        uint8_t newId[2];
        size_t dataLen = 2;
        libtlv_hex_to_bin(appData.createServiceId, newId, &dataLen);

        logData("Service Id 0 : %02x", newId[0]);
        logData("Service Id 1 : %02x", newId[1]);

        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, newId);

        // Service qualifier :
        // 01 (priority - 1 byte)
        // 1011 (service id - 2 byte)
        // B100 (DF 15 - Service management info) as per Table 16 - DF15 Service Management Info
        selectedService = (struct serviceMetadata *)malloc(sizeof(struct serviceMetadata));
        memset(selectedService, 0x00, sizeof(struct serviceMetadata));

        selectedService->serviceQualifier[0] = 0x01;
        selectedService->serviceQualifier[1] = newId[0];
        selectedService->serviceQualifier[2] = newId[1];

        // TODO : Can be taken from outside
        selectedService->serviceQualifier[3] = 0xB1;
        selectedService->serviceQualifier[4] = 0x00;

        if (appData.isServiceBlock == 1)
        {
            selectedService->serviceQualifier[3] = 0x99;
        }

        selectedService->serviceControl[0] = 0xAC;
        selectedService->serviceControl[1] = 0x00;

        return 0;
    }

    int rc = EMVCO_RC_OK;

    int rc_tlv;
    /*
     * Check Terminal Service Qualifier (FEIG_C13_ID_TERMINAL_SERVICE_QUALIFIER)
     * against Service Folders (DF32) listed in Service Directory (DF07)
     */
    uint8_t *service_qualifier_list = NULL;
    size_t service_qualifier_list_len = 0;
    uint8_t *service_card_list = NULL;
    size_t service_card_list_len = 0;
    struct tlv *tlv_data = NULL;
    struct tlv *tlv_service_directory = NULL;
    uint8_t service_directory[256] = {0};
    size_t service_directory_len = sizeof(service_directory);
    uint8_t no_service_id[2] = {0x00, 0x00};

    struct tlv *tlv_service_qualifier = NULL;

    // TODO :: Actual card presented

    if (isSecondTap)
    {
        // Use previously selected Service ID
        if (NULL != selectedService)
        {
            logHexData("Selected Service", &selectedService->serviceQualifier[1], 2);
            printServiceQualifier(selectedService->serviceQualifier, 5);
            printServiceControl(selectedService->serviceControl, 2);
        }

        if (NULL != selectedService)
        {
            *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, &selectedService->serviceQualifier[1]);
        }

        return EMVCO_RC_OK;
    }

    // Reset Service Candidate List and Service Selection
    resetServiceCandidateList();

    *tlv_reply = NULL;

    if (tlv_req)
    {
        uint8_t tag[16] = {0};
        size_t tag_len = 16;
        rc_tlv = tlv_encode_identifier(tlv_req, tag, &tag_len);
        if (0 != memcmp(tag, FEIG_C13_ID_SERVICE_SIGNAL_REQ_PRE_GPO, tag_len))
        {
            printf("Invalid tag!\n");
            return EMVCO_RC_FAIL;
        }
        uint8_t *buffer = NULL;
        size_t buffer_len = 0;
        rc_tlv = tlv_encode_value(tlv_req, buffer, &buffer_len);
        buffer = (uint8_t *)malloc(buffer_len);
        rc_tlv = tlv_encode_value(tlv_req, buffer, &buffer_len);
        rc_tlv = tlv_parse(buffer, buffer_len, &tlv_data);
        free(buffer);
    }

    if (NULL == tlv_data)
    {
        printf("No data available!\n");
        *tlv_reply = NULL;
        return EMVCO_RC_FAIL;
    }

    /*No Service handling if Transaction Type is with Cashback */
    if (serviceTrxData.trxType == 0x09)
    {
        printf("Purchase with Cash Back is not supported in a Service based transaction!\n");
        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, no_service_id);
        return EMVCO_RC_OK;
    }

    tlv_service_directory = tlv_find(tlv_data, C13_ID_SERVICE_DIRECTORY);
    if (NULL == tlv_service_directory)
    {
        /* Service directory is not available */
        printf("Service directory is not available!\n");
        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, no_service_id);
        return EMVCO_RC_OK;
    }

    rc_tlv = tlv_encode_value(tlv_service_directory, service_directory, &service_directory_len);
    if (rc_tlv != TLV_RC_OK ||
        0 != (service_directory_len - 17) % DF32_LENGTH ||
        service_directory_len < 17)
    {
        /* Get Service Directory value failed */
        printf("Get Service Directory value failed!\n");
        *tlv_reply = NULL;
        return EMVCO_RC_FAIL;
    }
    logData("Printing card service data");
    printServiceDirectory(service_directory, service_directory_len);
    logData("Card service data printed");

    /* Read Service Related Data of all available services */
    uint8_t serviceSFI = service_directory[14] & 0x0F;
    /* Service SFI should be 15 for Service Compartment */
    if (15 != serviceSFI)
    {
        printf("Unexpected Service SFI!\n");
        *tlv_reply = NULL;
        return EMVCO_RC_FAIL;
    }

    /* Store Service Limit for read of Service Related Data of all Services */
    serviceTrxData.serviceLimit = service_directory[12];
    service_card_list_len = service_directory_len - 17;
    service_card_list = service_directory + 17;

    // print_service_folder(service_candidate_list, service_candidate_list_len);
    // printServiceQualifier(service_card_list, service_card_list_len);

    tlv_service_qualifier = tlv_find(tlv_data, FEIG_C13_ID_TERMINAL_SERVICE_QUALIFIER);
    if (NULL == tlv_service_qualifier)
    {
        /* No Service Qualifier available */
        logData("No Service Qualifier available!\n");
        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, no_service_id);
        return EMVCO_RC_OK;
    }
    rc_tlv = tlv_encode_value(tlv_service_qualifier, NULL, &service_qualifier_list_len);
    service_qualifier_list = (uint8_t *)malloc(service_qualifier_list_len);
    rc_tlv = tlv_encode_value(tlv_service_qualifier, service_qualifier_list, &service_qualifier_list_len);
    // print_data("Service Qualifiers", service_qualifier_list, service_qualifier_list_len);
    printServiceQualifier(service_qualifier_list, service_qualifier_list_len);

    /*	RuPay.Terminal.Specification.2.0.pdf
        7 Service Shortlisting
        7.1 Overview
        Service Shortlisting is a conditional step carried out by the Terminal. This step is only
        required when a Service-based transaction is to be performed and the Terminal supports more
        than one service. In this step, the Terminal decides which Service ID to use for the
        transaction based upon rules determined by the Acquirer.
    */
    /* Number of card supported services */
    size_t num_card_services;
    num_card_services = service_card_list_len / 4;

    logData("Total card services : %d", num_card_services);

    /* Number of terminal supported services */
    size_t num_terminal_services;
    num_terminal_services = service_qualifier_list_len / 5;

    logData("Total terminal services : %d", num_terminal_services);

    if (num_terminal_services == 0)
    {
        /* Terminal supports no Service */
        /* Add supported Services to config with tag FEIG_C13_ID_TERMINAL_SERVICE_QUALIFIER */
        selectedService = NULL;
        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, no_service_id);
        return EMVCO_RC_OK;
    }

    /* Create list of terminal and card supported services (Candidate List) */
    for (size_t t = 0; t < num_terminal_services; t++)
    {
        bool serv_supported_by_card = false;
        bool is_candidate = false;
        uint8_t *current_term_serv = &service_qualifier_list[t * 5];
        uint8_t *current_card_serv = NULL;
        for (size_t c = 0; c < num_card_services; c++)
        {
            current_card_serv = &service_card_list[c * 4];

            logData("Comparing");
            logData("Card service : %02X %02X ", current_card_serv[0], current_card_serv[1]);
            logData("Terminal service : %02X %02X ", current_term_serv[1], current_term_serv[2]);

            /* Compare card and terminal service IDs */
            /* Terminal support service and Card support service? */
            if (0 == memcmp(&current_term_serv[1], current_card_serv, 2))
            {
                logData("Service supported");
                serv_supported_by_card = true;
                break;
            }
        }

        if (serv_supported_by_card)
        {
            /* Terminal and card support service and Transaction Type is not Service Creation */
            if (!isServiceCreation(&current_term_serv[3]) &&
                serviceTrxData.trxType != 0x83)
            {
                logData("adding to the candidate list");
                /* Only add as candidate if it is NOT a key plant */
                is_candidate = true;
            }
        }
        else
        {
            /* Only Terminal support service and Transaction Type is Service Creation */
            if (isServiceCreation(&current_term_serv[3]) && serviceTrxData.trxType == 0x83)
            {
                /* Only add as candidate if it is a key plant */
                is_candidate = true;
            }
        }
        if (is_candidate)
        {
            logData("Candidate list added : %02X %02X", current_term_serv[1], current_term_serv[2]);
            addCandidate(current_term_serv, &current_card_serv[2]);
        }
    }
    if (serviceCandidatelist_len < 1)
    {
        selectedService = NULL;
        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, no_service_id);
        logWarn("NO SERVICE AVAILABLE HERE, Transaction to be rejected");
        strcpy(currentTxnData.trxIssueDetail, "No Mutal Service Available");

        pthread_mutex_lock(&lockRupayService);
        // changeAppState(APP_STATUS_CARD_PRESENTED);
        appData.status = APP_STATUS_CARD_PRESENTED;
        pthread_mutex_unlock(&lockRupayService);

        handleServiceData();

        // Set Transaction Control to 0x01 to abort transaction -> End Application
        uint8_t transactionControl[1] = {0};
        transactionControl[0] = 0x01;
        *tlv_reply = tlv_new(FEIG_C13_ID_TRANSACTION_CONTROL, 1, transactionControl);
        return EMVCO_RC_OK;

        // uint8_t transactionCertificateType[1] = {0};
        // transactionCertificateType[0] = 0x00;
        //*tlv_reply = tlv_new(FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE, 1, transactionCertificateType);

        //*tlv_reply = NULL;
        //*tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, no_service_id);
        // return EMVCO_RC_FAIL;
        // return EMVCO_RC_FAIL;
    }

    // TODO : May be not required to sort it, just take the first one
    // Need to see whether the card holder selection is required here
    // Or else just take the first common one as per the service qualifier
    // set in terminal

    sortCandidateList();

    /* Select service with highest Priority */
    /* Check if Cardholder Confirmation needed */
    if ((serviceCandidatelist[0]->serviceQualifier[0] & 0x01) == 0x01)
    {
        selectedService = serviceCandidatelist[0];
    }
    else
    {
        logWarn("Manual selection requested, but not supported in this reader.");
        selectedService = NULL;
        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, no_service_id);
        return EMVCO_RC_OK;
        // We should not have this scenario as the service is auto selected
        /*
        while (NULL == selectedService)
        {
            // Let Cardholder select the service
            printCandidateList();
            uint8_t temp8ID[2];
            if (serviceTrxData.serviceID[0] != 0x00 && serviceTrxData.serviceID[1] != 0x00)
            {
                temp8ID[0] = serviceTrxData.serviceID[0];
                temp8ID[1] = serviceTrxData.serviceID[1];
            }
            else
            {
                printf("Please select one of the following Service IDs or 0 for NONE:\n");
                unsigned int tempID;
                scanf("%X", &tempID);
                if (tempID > 0xFFFF)
                {
                    continue;
                }
                if (tempID == 0x0000)
                {
                    selectedService = NULL;
                    break;
                }
                temp8ID[0] = (uint8_t) (tempID >> 8) & 0xFF;
                temp8ID[1] = (uint8_t) tempID & 0xFF;
            }
            for (size_t k = 0; k < serviceCandidatelist_len; k++) {
                if (NULL != serviceCandidatelist[k]) {
                    if(0 == memcmp(temp8ID, &serviceCandidatelist[k]->serviceQualifier[1], 2)) {
                        selectedService = serviceCandidatelist[k];
                        break;
                    }
                }
            }
            if (serviceTrxData.serviceID[0] != 0x00 && serviceTrxData.serviceID[1] != 0x00) {
                serviceTrxData.serviceID[0] = 0x00;
                serviceTrxData.serviceID[1] = 0x00;
            }
        }*/
    }

    if (NULL != service_qualifier_list)
        free(service_qualifier_list);

    if (NULL != selectedService)
    {
        logHexData("Selected Service", &selectedService->serviceQualifier[1], 2);
        logInfo("Selected Service : %02X %02X",
                selectedService->serviceQualifier[1], selectedService->serviceQualifier[2]);
        printServiceQualifier(selectedService->serviceQualifier, 5);
        printServiceControl(selectedService->serviceControl, 2);

        sprintf(currentTxnData.serviceControl, "%02X%02X",
                selectedService->serviceControl[0], selectedService->serviceControl[1]);

        sprintf(currentTxnData.serviceId, "%02X%02X",
                selectedService->serviceQualifier[1], selectedService->serviceQualifier[2]);

        logData("Service Selected Control : %s", currentTxnData.serviceControl);
        logData("Service Selected Qualifier : %s", currentTxnData.serviceId);
    }

    if (NULL != selectedService)
    {
        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, &selectedService->serviceQualifier[1]);
    }
    else
    {
        *tlv_reply = tlv_new(C13_ID_SERVICE_ID, 2, no_service_id);
    }

    long long endGPO = getCurrentSeconds();
    if (appConfig.isTimingEnabled)
    {
        logTimeWarnData("DE, GPO Handling Call back end : %lld", endGPO);
        logTimeWarnData("DE, GPO Handling Call back time took (AzCode) : %lld\n", (endGPO - deEvent));
    }

    deAzCompletedTime = endGPO;

    if (tlv_data)
    {
        tlv_free(tlv_data);
    }

    return rc;
}

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
int c13_service_signal_req_post_gpo_handling(struct tlv *tlv_req, struct tlv **tlv_reply)
{
    int rc = EMVCO_RC_OK;

    if (appConfig.isTimingEnabled)
        logTimeWarnData("DE, Post GPO Handling");

    logData("%s()\n", __func__);

    // Could be used for triggering time critical Service Data Update processing
    if (appConfig.isDebugEnabled == 1)
    {
        if (tlv_req)
        {
            uint8_t buf[256] = {0};
            size_t buf_len = sizeof(buf);
            tlv_encode_value(tlv_req, buf, &buf_len);
            logHexData("TLV Request", buf, buf_len);
        }
    }

    long long eventEnd = getCurrentSeconds();
    if (appConfig.isTimingEnabled)
    {
        logTimeWarnData("DE, Post GPO Handling Call back end : %lld", eventEnd);
        logTimeWarnData("DE, Post GPO Handling Call back time took (AzCode) : %lld\n", (eventEnd - deEvent));
    }
    deAzCompletedTime = eventEnd;

    // Otherwise do nothing
    return rc;
}

/*
FEIG_C13_ID_SERVICE_SIGNAL_REQ_RR
This data exchange callback is only executed,
if the PDOL related data contains a Service ID (DF16) != 0000 in the GPO request.
The request data contain
•	CA Public Key Index (8F)
•	DF Name (84)
The response data should be empty.
*/
int c13_service_signal_req_rr_handling(struct tlv *tlv_req, struct tlv **tlv_reply)
{
    int rc = EMVCO_RC_OK;

    if (appConfig.isTimingEnabled)
        logTimeWarnData("DE, Read Record Handling");

    logData("%s()\n", __func__);

    if (appConfig.isDebugEnabled == 1)
    {
        if (tlv_req)
        {
            uint8_t buf[256] = {0};
            size_t buf_len = sizeof(buf);
            tlv_encode_value(tlv_req, buf, &buf_len);
            logHexData("TLV Request", buf, buf_len);
        }
    }

    long long eventEnd = getCurrentSeconds();
    if (appConfig.isTimingEnabled)
    {
        logTimeWarnData("DE, Read Record Handling Call back end : %lld", eventEnd);
        logTimeWarnData("DE, Read Record Handling Call back time took (AzCode) : %lld\n", (eventEnd - deEvent));
    }
    deAzCompletedTime = eventEnd;

    return rc;
}

/*
FEIG_C13_ID_SERVICE_SIGNAL_REQ_GAC
This data exchange callback is only executed,
if the PDOL related data contains a Service ID (DF16) != 0000 in the GPO request.
The request data contain a DOL with the following elements
•	Transaction Certificate Type (FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE )
•	Application PAN (5A)
•	Application PAN Sequence Number (5F34)
•	ICC Dynamic Number (9F4C)
•	Amount, Authorised  (9F02)
•	Amount, Other (9F03)
Unpredictable Number (Terminal) (9F37)
•	Application Version Number (Card) (9F08)
Only returned if Card is a Non-Legacy Card (Application Version Number (Card) < ‘0002’):
•	Partial Session key (Kpsk) (FEIG_C13_ID_PARTIAL_SESSION_KEY_KPSK)
If Service ID is available on card:
•	Service Data Format (DF44)
•	Service Related Data (DF33)
else Service ID is not available on card:
•	Service Availability Info (DF03)
*/
int c13_service_signal_req_gac_handling(struct fetpf *client, struct tlv *tlv_req, struct tlv **tlv_reply)
{
    logData("%s:%s()[%d]\n", __FILE__, __func__, __LINE__);
    if (appConfig.isTimingEnabled)
        logTimeWarnData("DE, GAC Handling");

    int rc = EMVCO_RC_OK;
    int tlv_rc = TLV_RC_OK;
    struct tlv *tlv_attr = NULL;

    uint8_t applicationVersionNumberCard_9F08[2] = {0};
    uint8_t applicationPAN_5A[32] = {0};
    size_t applicationPAN_5A_len = 0;
    uint8_t applicationPSN_5F34[1] = {0};
    uint8_t prmicc_DF49_9F4C[8] = {0};
    uint8_t terminalUnpredictableNumber_9F37[4] = {0};
    uint8_t kpsk[16] = {0};
    uint8_t serviceAvailabilityInfo_DF03[1] = {0};
    uint8_t serviceDataFormat_DF44[128] = {0};
    size_t serviceDataFormat_DF44_len = 0;
    uint8_t transactionCertificateType[1] = {0};
    uint8_t amountAuthorised_9F02[6] = {0};
    uint8_t amountOther_9F03[6] = {0};
    uint8_t serviceRelatedData_DF33[128] = {0};
    size_t serviceRelatedData_DF33_len = 0;
    uint8_t serviceTerminalData[96] = {0};
    size_t serviceTerminalData_len = 0;
    uint8_t applicationEffectiveDate_5F25[3] = {0};
    uint8_t terminalVerificationResults_95[5] = {0};

    if (appConfig.isDebugEnabled == 1)
    {
        if (tlv_req)
        {
            uint8_t buf[256] = {0};
            size_t buf_len = sizeof(buf);
            tlv_encode_value(tlv_req, buf, &buf_len);
            logHexData("TLV Request", buf, buf_len);
        }
    }

    /* Collect data */
    for (tlv_attr = tlv_req; tlv_attr; tlv_attr = tlv_get_next(tlv_attr))
    {
        uint8_t tag[TLV_MAX_TAG_LENGTH];
        size_t tag_sz = sizeof(tag);
        tlv_rc = tlv_encode_identifier(tlv_attr, tag, &tag_sz);
        if (TLV_RC_OK != tlv_rc)
            printf("tlv_rc: %d\n", tlv_rc);

        uint8_t value[256];
        size_t value_sz = sizeof(value);
        tlv_rc = tlv_encode_value(tlv_attr, value, &value_sz);
        if (TLV_RC_OK != tlv_rc)
            printf("tlv_rc: %d\n", tlv_rc);

        if (tag_sz > 0)
        {
            if (tag[0] == 0x5A)
            {
                logHexData("PAN : ", value, value_sz); // TODO : Remove
                char pan[21];
                byteToHex(value, value_sz, pan);
                sprintf(currentTxnData.maskPan, "%s", maskPan(pan));
                char sha[65];
                generatePanToken(pan, currentTxnData.plainTrack2, sha);
                logData("SHA Generated for Pan : %s\n", sha);
                strcpy(currentTxnData.plainPan, pan);
                strcpy(currentTxnData.token, sha);
            }

            if (tag[0] == 0x5F && tag[1] == 0x25)
            {
                byteToHex(value, value_sz, currentTxnData.effectiveDate);
            }
        }

        if (appConfig.isDebugEnabled == 1)
        {
            if (tag_sz > 0)
            {
                size_t i = 0;
                for (i = 0; i < tag_sz; i++)
                    printf("%02X", tag[i]);
                printf(" ");
            }
            if (value_sz > 0)
            {
                size_t i = 0;
                printf("%zu ", value_sz);
                for (i = 0; i < value_sz; i++)
                    printf("%02X", value[i]);
            }
            printf("\n");
        }

        if (0 == libtlv_compare_ids(EMV_ID_APPLICATION_VERSION_NUMBER_CARD, tag))
        {
            memcpy(applicationVersionNumberCard_9F08, value, 2);
        }
        else if (0 == libtlv_compare_ids(EMV_ID_APPLICATION_PAN, tag))
        {
            memcpy(applicationPAN_5A, value, value_sz);
            applicationPAN_5A_len = value_sz;
        }
        else if (0 == libtlv_compare_ids(EMV_ID_APPLICATION_PAN_SEQUENCE_NUMBER, tag))
        {
            memcpy(applicationPSN_5F34, value, 1);
        }
        else if (0 == libtlv_compare_ids(EMV_ID_APPLICATION_EFFECTIVE_DATE, tag))
        {
            memcpy(applicationEffectiveDate_5F25, value, 3);
        }
        else if (0 == libtlv_compare_ids(EMV_ID_TERMINAL_VERIFICATION_RESULTS, tag))
        {
            memcpy(terminalVerificationResults_95, value, 5);
        }
        else if (0 == libtlv_compare_ids(EMV_ID_ICC_DYNAMIC_NUMBER, tag))
        {
            memcpy(prmicc_DF49_9F4C, value, 8);
        }
        else if (0 == libtlv_compare_ids(EMV_ID_UNPREDICTABLE_NUMBER, tag))
        {
            memcpy(terminalUnpredictableNumber_9F37, value, 4);
        }
        else if (0 == libtlv_compare_ids(FEIG_C13_ID_PARTIAL_SESSION_KEY_KPSK, tag))
        {
            memcpy(kpsk, value, 16);
        }
        else if (0 == libtlv_compare_ids(C13_ID_SERVICE_AVAILABILITY_INFO, tag))
        {
            memcpy(serviceAvailabilityInfo_DF03, value, 1);
        }
        else if (0 == libtlv_compare_ids(FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE, tag))
        {
            memcpy(transactionCertificateType, value, 1);
        }
        else if (0 == libtlv_compare_ids(C13_ID_SERVICE_DATA_FORMAT, tag))
        {
            memcpy(serviceDataFormat_DF44, value, value_sz);
            serviceDataFormat_DF44_len = value_sz;
        }
        else if (0 == libtlv_compare_ids(EMV_ID_AMOUNT_AUTHORIZED, tag))
        {
            memcpy(amountAuthorised_9F02, value, 6);
        }
        else if (0 == libtlv_compare_ids(EMV_ID_AMOUNT_OTHER, tag))
        {
            memcpy(amountOther_9F03, value, 6);
        }
        else if (0 == libtlv_compare_ids(C13_ID_SERVICE_RELATED_DATA, tag))
        {
            memcpy(serviceRelatedData_DF33, value, value_sz);
            serviceRelatedData_DF33_len = value_sz;
            logData("Selected Service Related Data:");
            printServiceRelatedData(value, value_sz);

            sprintf(currentTxnData.serviceIndex, "%02X", value[0]);
            sprintf(currentTxnData.serviceBalance, "%02X%02X%02X%02X%02X%02X",
                    value[23], value[24], value[25], value[26], value[27], value[28]);
            byteToHex(&value[32], value[31], currentTxnData.serviceData);
        }
        else
        {
            logData(" UNHANDLED tag");
        }
    }

    pthread_mutex_lock(&lockRupayService);
    // changeAppState(APP_STATUS_CARD_PRESENTED);
    appData.status = APP_STATUS_CARD_PRESENTED;
    pthread_mutex_unlock(&lockRupayService);

    handleServiceData();

    /* Read data of all service records */
    // logData("SERVICE LIMIT BEFORE RR : %02X", serviceTrxData.serviceLimit);
    // readServiceRecords(client, serviceTrxData.serviceLimit);
    // logData("Service data is read");

    uint8_t *serviceID = &selectedService->serviceQualifier[1];
    uint8_t *defaultSMI = &selectedService->serviceQualifier[3];
    /* Check if Service ID is FFFE */
    if (serviceID[0] == 0xFF && serviceID[1] == 0xFE)
    {
        printf("Global offline balance is to be updated\n");
        *tlv_reply = NULL;
        return EMVCO_RC_OK;
    }

    if (0 == serviceDataFormat_DF44_len)
    {
        printf("No service data format (DF44) available\n");
    }

    logData("Going to check the type of service change");

    /* Check if Service Space for a Key Plant is available */
    if (isServiceCreation(defaultSMI))
    {
        if (!isServiceSpaceAvailable(defaultSMI, serviceAvailabilityInfo_DF03))
        {
            strcpy(currentTxnData.trxIssueDetail, "No space for requested service available!");
            printf("No space for requested service available!\n");
            // Set Transaction Certificate Type to 0x00 to decline Transaction -> AAC
            transactionCertificateType[0] = 0x00;
            *tlv_reply = tlv_new(FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE, 1, transactionCertificateType);
            return EMVCO_RC_OK;
        }
    }
    else
    {
        /* Service update is not possible if service related data is not available */
        if (0 == serviceRelatedData_DF33_len)
        {
            printf("Mandatory service related data (DF33) is not available!!!\n");
            // Set Transaction Certificate Type to 0x00 to decline Transaction -> AAC
            transactionCertificateType[0] = 0x00;
            *tlv_reply = tlv_new(FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE, 1, transactionCertificateType);
            return EMVCO_RC_OK;
        }
        if (32 > serviceRelatedData_DF33_len)
        {
            printf("Mandatory service related data (DF33) is to short!!!\n");
            // Set Transaction Certificate Type to 0x00 to decline Transaction -> AAC
            transactionCertificateType[0] = 0x00;
            *tlv_reply = tlv_new(FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE, 1, transactionCertificateType);
            return EMVCO_RC_OK;
        }

        serviceTerminalData_len = serviceRelatedData_DF33[31];
        memcpy(serviceTerminalData, &serviceRelatedData_DF33[32], serviceTerminalData_len);
        logHexData("SERVICE TERMINAL BEFORE UPDATE : ", serviceTerminalData, serviceTerminalData_len);

        // Modify the amount and service related data here if its received from client

        // If the trxk type is money add then set the amount here
        if (appData.trxTypeBin == 0x028)
        {
            libtlv_u64_to_bcd(currentTxnData.amount, amountAuthorised_9F02, sizeof(amountAuthorised_9F02));
            logHexData("HEX Amount : ", amountAuthorised_9F02, 6);

            // Verify the contactless CVM limit here
            if (currentTxnData.amount > appConfig.moneyAddLimitAmount)
            {
                logError("User provided amount is more than the Money Add limit");
                logError("Rejecting the transaction");

                return buildDeclineReply(amountAuthorised_9F02, tlv_reply);
            }

            /*
            // Verify the contactless limit here
            if (currentTxnData.amount > appConfig.contactlessLimit)
            {
                logError("User provided amount is more than the contactless limit");
                logError("Rejecting the transaction");

                return buildDeclineReply(amountAuthorised_9F02, tlv_reply);
            }*/
        }

        // If the trx type is purchase then get the amount from the socket
        if (appData.trxTypeBin == 0x00 && appData.amountKnownAtStart == 0)
        {
            logInfo("Amount to be updated here, wait for the write card");
            logInfo("Going to wait for write command for : %d ms", appData.writeCardWaitTimeMs);

            int msec = 0, trigger = appData.writeCardWaitTimeMs; // ms
            clock_t before = clock();
            int writeCommand = 0;

            long long eventAmount = getCurrentSeconds();
            if (appConfig.isTimingEnabled)
                logTimeWarnData("GAC, Waiting for Amount : %lld", eventAmount);

            do
            {
                clock_t difference = clock() - before;
                msec = difference * 1000 / CLOCKS_PER_SEC;

                pthread_mutex_lock(&lockRupayService);
                writeCommand = isWriteCardCommand;
                pthread_mutex_unlock(&lockRupayService);

                if (writeCommand == 1)
                {
                    displayLight(LED_ST_WRITE_SUCCESS);
                    logInfo("Write command received, so breaking the waiting");
                    break;
                }

            } while (msec < trigger);

            long long eventAmountEnd = getCurrentSeconds();
            if (appConfig.isTimingEnabled)
            {
                logTimeWarnData("GAC, Waiting for Amount End: %lld", eventAmountEnd);
                logTimeWarnData("GAC, Waiting for Amount Time took: %lld", eventAmountEnd - eventAmount);
            }

            logData("Write Card command : %d", writeCommand);
            logData("Write Card amount : %s", writeCardAmount);
            logData("Write card service Data : %s", writeCardServiceData);

            if (writeCommand == 0)
            {
                displayLight(LED_ST_WRITE_FAILED);
                logWarn("No write card command received, abort transaction");
                // Set Transaction Certificate Type to 0x00 to decline Transaction -> AAC
                transactionCertificateType[0] = 0x00;
                *tlv_reply = tlv_new(FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE, 1, transactionCertificateType);
                return EMVCO_RC_OK;
            }

            // Reset
            pthread_mutex_lock(&lockRupayService);
            isWriteCardCommand = 0;
            pthread_mutex_unlock(&lockRupayService);

            // Copy the amount
            uint64_t userAmount = strtol(writeCardAmount, NULL, 10);
            logData("Amount by User = %llu.%02llu", userAmount / 100, userAmount % 100);
            logData("Contactless Limit : %llu.%02llu", appConfig.purchaseLimitAmount / 100, appConfig.purchaseLimitAmount % 100);

            currentTxnData.amount = userAmount;
            char sAmount[13];
            convertAmount(currentTxnData.amount, sAmount);
            strcpy(currentTxnData.sAmount, sAmount);

            // Convert to BCD
            libtlv_u64_to_bcd(userAmount, amountAuthorised_9F02, sizeof(amountAuthorised_9F02));
            logHexData("HEX Amount : ", amountAuthorised_9F02, 6);

            // Verify the contactless CVM limit here
            if (userAmount > appConfig.purchaseLimitAmount)
            {
                logError("User provided amount is more than the purchase limit");
                logError("Rejecting the transaction");

                return buildDeclineReply(amountAuthorised_9F02, tlv_reply);
            }

            /*
            // Verify the contactless limit here
            if (userAmount > appConfig.contactlessLimit)
            {
                logError("User provided amount is more than the contactless limit");
                logError("Rejecting the transaction");

                return buildDeclineReply(amountAuthorised_9F02, tlv_reply);
            }
            */

            logData("Rupay Service, Service Data Received : %s", writeCardServiceData);
            if (isServiceDataAvailable == 1)
            {
                logData("Service data available");
                hexToByte(writeCardServiceData, serviceTerminalData);
            }
            else
            {
                logData("No data provided to write");
            }
            /*
            if (serviceTerminalData_len > 84) {
                uint64_t lastFromStation = bin2uint64(&serviceTerminalData[75]);
                //logData("Station before : %02X", lastFromStation);
                lastFromStation++;
                //logData("Station after : %02X", lastFromStation);
                uint642bin(lastFromStation, &serviceTerminalData[75]);
            }*/
        }

        /* ######################################################################*/
    }

    logHexData("Final amount authorized filled that is input to SBT : ", amountAuthorised_9F02, 6);

    struct rupay_sbt_crypto crypto_data;
    memset(&crypto_data, 0x00, sizeof(struct rupay_sbt_crypto));
    struct rupay_sbt_input input;
    memset(&input, 0x00, sizeof(struct rupay_sbt_input));
    struct rupay_sbt_output output;
    memset(&output, 0x00, sizeof(struct rupay_sbt_output));

    /* Fill generic crypto_data */
    crypto_data.applicationVersionNumberCard_9F08 = applicationVersionNumberCard_9F08;
    crypto_data.PRMicc_9F4C = prmicc_DF49_9F4C;
    crypto_data.amountAuthorised_9F02 = amountAuthorised_9F02;
    crypto_data.amountOther_9F03 = amountOther_9F03;
    crypto_data.terminalUN_9F37 = terminalUnpredictableNumber_9F37;
    crypto_data.transactionCertificateType_DFFECD0F = transactionCertificateType;

    /* Fill generic input */
    input.serviceManagementInfo_DF15 = defaultSMI;

    /* Fill operation dependent data */
    if (isLegacy(applicationVersionNumberCard_9F08))
    {
        /* Legacy (Version 1) */
        crypto_data.version1.applicationPAN_5A = applicationPAN_5A;
        crypto_data.version1.applicationPAN_5A_length = applicationPAN_5A_len;
        crypto_data.version1.applicationPSN_5F34 = applicationPSN_5F34;
        if (isServiceCreation(defaultSMI))
        {
            /* Key Plant */
            input.serviceData = NULL;
            input.serviceDataLength = 0;
        }
    }
    else
    {
        /* Non-Legacy (Version 2) */
        crypto_data.version2.encryptedKpsk = kpsk;
        if (isServiceCreation(defaultSMI))
        {
            /* Key Plant */
            input.serviceData = serviceTrxData.balance;
            input.serviceDataLength = 6;
        }
    }
    if (isServiceCreation(defaultSMI))
    {
        /* Key Plant */
        input.keyPlant.serviceID = serviceID;
        input.keyPlant.PRMacqKeyIndex = serviceTrxData.PRMacqKeyIndex;
        input.keyPlant.serviceCurrencyCode = serviceTrxData.currencyCode;

        logData("Service creation input data");
        logData("Input service id : %02x %02x", input.keyPlant.serviceID[0], input.keyPlant.serviceID[1]);
        logData("PRM ACQ : %02X", input.keyPlant.PRMacqKeyIndex[0]);
        logData("Current code : %02X %02X", input.keyPlant.serviceCurrencyCode[0], input.keyPlant.serviceCurrencyCode[1]);
    }
    else if (isServiceBalanceLimitUpdate(defaultSMI))
    {

        logData("SERVICE BALANCE LIMIT UPDATE CALLED");

        /* Service Balance Limit Update */
        input.update.serviceRelatedData_DF33 = serviceRelatedData_DF33;
        input.update.serviceRelatedData_DF33_length = serviceRelatedData_DF33_len;
        // input.serviceData = serviceTrxData.balance;
        input.serviceData = appData.serviceBalanceLimit;

        input.serviceDataLength = 6;
    }
    else if (isServiceDataUpdate(defaultSMI))
    {
        /* Service Data Update */
        input.update.serviceRelatedData_DF33 = serviceRelatedData_DF33;
        input.update.serviceRelatedData_DF33_length = serviceRelatedData_DF33_len;

        /* Update Service Related Data with the new Terminal Specific Data */
        input.serviceDataLength = serviceTerminalData_len;
        input.serviceData = serviceTerminalData;

        logHexData("SERVICE TERMINAL TO BE UPDATED : ", serviceTerminalData, serviceTerminalData_len);
    }

    crypto_data.slotID = slot_id;
    if (NULL != crypto)
    {
        crypto_data.p11 = crypto->p11;
        crypto_data.hSession = crypto->hSession;
    }

    // #ifdef WRITE_RUPAY_SBT_DATA_TO_FILE
    //	input.save_data_for_replay = 1;
    // #else
    input.save_data_for_replay = 0;
    // #endif

    long long cryptTime1 = getCurrentSeconds();
    if (appConfig.isTimingEnabled)
        logTimeWarnData("Kernel crypto calculation : %lld", cryptTime1);

    rc = rupay_sbt_finalize(&crypto_data, &input, &output);
    if (RS_RC_OK != rc)
    {
        strcpy(currentTxnData.trxIssueDetail, "Key not injected or its wrong");
        logError("Key not injected or its wrong, rupay_sbt_finalize failed!!!");
        // Set Transaction Certificate Type to 0x00 to decline Transaction -> AAC
        transactionCertificateType[0] = 0x00;
        *tlv_reply = tlv_new(FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE, 1, transactionCertificateType);
        return EMVCO_RC_OK;
    }

    rc = buildReply(output.serviceManagementInfo_DF15,
                    output.serviceTerminalData_DF45, output.serviceTerminalData_DF45_length,
                    output.serviceSummary_DF22,
                    output.serviceSignature_DF23,
                    crypto_data.amountAuthorised_9F02,
                    crypto_data.amountOther_9F03,
                    tlv_reply);

    long long cryptTime2 = getCurrentSeconds();
    long long gacEnd = getCurrentSeconds();

    if (appConfig.isTimingEnabled)
    {
        logTimeWarnData("Kernel crypto calculation complete : %lld", cryptTime2);
        logTimeWarnData("Kernel crypto time taken : %lld", (cryptTime2 - cryptTime1));
        logTimeWarnData("DE, GAC Call back End : %lld", gacEnd);
        logTimeWarnData("DE, GAC Time took (AzCode): %lld\n", (gacEnd - deEvent));
    }

    deAzCompletedTime = gacEnd;

    if (tlv_attr)
    {
        tlv_free(tlv_attr);
    }

    return rc;
}

int buildDeclineReply(
    const uint8_t amount_authorized[6],
    struct tlv **tlv_reply)
{
    logData("%s()\n", __func__);
    uint8_t transactionCertificateType[1] = {0};

    struct tlv *tlv_attr = NULL;
    *tlv_reply = tlv_new(FEIG_C13_ID_TRANSACTION_CERTIFICATE_TYPE, 1, transactionCertificateType);
    tlv_attr = tlv_new(EMV_ID_AMOUNT_AUTHORIZED, 6, amount_authorized);
    tlv_insert_after(*tlv_reply, tlv_attr);

    if (tlv_attr)
    {
        tlv_free(tlv_attr);
    }

    return EMVCO_RC_OK;
}

int buildReply(const uint8_t managementInfo[2],
               const uint8_t *data, size_t data_len,
               const uint8_t summary[8],
               const uint8_t signature[8],
               const uint8_t amount_authorized[6],
               const uint8_t amount_other[6],
               struct tlv **tlv_reply)
{
    logData("%s()\n", __func__);

    struct tlv *tlv_tail = NULL;
    struct tlv *tlv_attr = NULL;

    *tlv_reply = tlv_new(C13_ID_SERVICE_MANAGEMENT_INFO, 2, managementInfo);
    tlv_attr = tlv_new(C13_ID_SERVICE_TERMINAL_DATA, data_len, data);
    tlv_tail = tlv_insert_after(*tlv_reply, tlv_attr);
    tlv_attr = tlv_new(C13_ID_SERVICE_SUMMARY, 8, summary);
    tlv_tail = tlv_insert_after(tlv_tail, tlv_attr);
    tlv_attr = tlv_new(C13_ID_SERVICE_SIGNATURE, 8, signature);
    tlv_tail = tlv_insert_after(tlv_tail, tlv_attr);
    tlv_attr = tlv_new(EMV_ID_AMOUNT_AUTHORIZED, 6, amount_authorized);
    tlv_tail = tlv_insert_after(tlv_tail, tlv_attr);
    tlv_attr = tlv_new(EMV_ID_AMOUNT_OTHER, 6, amount_other);
    tlv_tail = tlv_insert_after(tlv_tail, tlv_attr);

    if (tlv_attr)
    {
        tlv_free(tlv_attr);
    }

    return EMVCO_RC_OK;
}

/* Read all service records from card */
void readServiceRecords(struct fetpf *client, uint8_t service_limit)
{
    logData("%s:%s()[%d]\n", __FILE__, __func__, __LINE__);
    /*
    int rc = EMVCO_RC_OK;
    int fd_feclr = -1;
    uint64_t status;
    // READ SERVICE RECORD
    // CLA: 0x00
    // INS: 0xB3
    // P1:  0x00 Record Number
    // P2:  0x7C Reference Control Parameter with SFI 15 and P1 is record number
    // Lc:  Not Present
    // Data:  Not Present
    // Le:  0x00
    uint8_t tx_frame[] = { 0x00, 0xB3, 0x00, 0x7C, 0x00 };
    uint8_t rx_frame[256];
    uint8_t rx_last_bits = 0;
    size_t rx_frame_size = sizeof(rx_frame);
    fd_feclr = (int)fetpf_get_user_data(client);
    for (uint8_t recordNumber = 1; recordNumber <= service_limit; recordNumber++) {
        memset(rx_frame, 0x00, sizeof(rx_frame));
        rx_frame_size = sizeof(rx_frame);
        tx_frame[2] = recordNumber;
        rc = feclr_transceive(fd_feclr, 0, tx_frame, sizeof(tx_frame),
                        0, rx_frame, sizeof(rx_frame),
                        &rx_frame_size, &rx_last_bits,
                        0, &status);
        if (rc < 0) {
            printf("Transceive failed with rc: 0x%08x: %m\n", rc);
            continue;
        }
        if (status != FECLR_STS_OK) {
            printf("Transceive status: 0x%08llX\n", status);
            continue;
        }
        if (rx_frame_size < 2)  {
            printf("Command response is to short\n");
            continue;
        }
        if ((rx_frame[rx_frame_size - 2] != 0x90) ||
            (rx_frame[rx_frame_size - 1] != 0x00)) {
            printf("Command failed with SW1SW2: %02X%02X\n",
                   rx_frame[rx_frame_size - 2],
                   rx_frame[rx_frame_size - 1]);
            continue;
        }
        //print_data("Service Data", rx_frame, rx_frame_size);
        printServiceRelatedData(&rx_frame[7], rx_frame_size - 7 - 2);
        //if (0xFE == rx_frame[8] && 0x41 == rx_frame[9]) {
        //	printf("Update Service FE41\n");
        //	update_service_record(client, recordNumber);
        //}
    }*/
}

void resetServiceCandidateList()
{
    struct serviceMetadata **list = serviceCandidatelist;
    selectedService = NULL;

    size_t i = 0;
    for (i = 0; i < serviceCandidatelist_len; i++)
    {
        if (list[i] != NULL)
        {
            free(list[i]);
            list[i] = NULL;
        }
    }
    serviceCandidatelist_len = 0;
    return;
}

void handleServiceData()
{
    char *transactionId = malloc(UUID_STR_LEN);
    generateUUID(transactionId);
    logInfo("Unique Transaction Id generated : %s", transactionId);
    strcpy(currentTxnData.transactionId, transactionId);
    free(transactionId);

    if (appConfig.beepOnCardFound)
    {
        beepInThread();
    }

    // Card Presented message
    displayLight(LED_ST_CARD_PRESENTED);
    currentTxnData.cardPresentedSent = true;
    sendBalanceMessage(currentTxnData);

    if (appConfig.isDebugEnabled != 1)
    {
        return;
    }

    logInfo("Service Information");
    logInfo("Service Index : %s", currentTxnData.serviceIndex);
    logInfo("Service Id : %s", currentTxnData.serviceId);
    logInfo("Service Balance : %s", currentTxnData.serviceBalance);
    logInfo("Service Data : %s", currentTxnData.serviceData);
}

void addCandidate(uint8_t service_qualifier[5], uint8_t service_control[2])
{
    struct serviceMetadata *sM = (struct serviceMetadata *)malloc(sizeof(struct serviceMetadata));
    memset(sM, 0x00, sizeof(struct serviceMetadata));
    memcpy(sM->serviceQualifier, service_qualifier, 5);
    if (NULL != service_control)
        memcpy(sM->serviceControl, service_control, 2);
    /* Add to candidate list (unsorted) */
    serviceCandidatelist[serviceCandidatelist_len] = sM;
    serviceCandidatelist_len++;
}

void printCandidateList()
{
    printf("| DF16 |");
    logData(" DF15 | DF52 | Priority Number |");
    printf("\n");
    printf("--------");
    logData("--------------------------------");
    printf("\n");
    for (size_t j = 0; j < serviceCandidatelist_len; j++)
    {
        if (NULL != serviceCandidatelist[j])
        {
            printf("| %02X%02X |", serviceCandidatelist[j]->serviceQualifier[1],
                   serviceCandidatelist[j]->serviceQualifier[2]);
            logData(" %02X%02X |", serviceCandidatelist[j]->serviceQualifier[3],
                    serviceCandidatelist[j]->serviceQualifier[4]);
            logData(" %02X%02X |", serviceCandidatelist[j]->serviceControl[0],
                    serviceCandidatelist[j]->serviceControl[1]);
            logData("              %02X |", (serviceCandidatelist[j]->serviceQualifier[0] & 0xFE));
            printf("\n");
        }
    }
}

int sortFunction(const void *a, const void *b)
{
    const struct serviceMetadata **sMa = (const struct serviceMetadata **)a;
    const struct serviceMetadata **sMb = (const struct serviceMetadata **)b;
    int ia = (int)(*sMa)->serviceQualifier[0] & 0xFE;
    int ib = (int)(*sMb)->serviceQualifier[0] & 0xFE;
    return (ia - ib);
}

void sortCandidateList()
{
    qsort(serviceCandidatelist, serviceCandidatelist_len, sizeof(struct serviceMetadata *), sortFunction);
}

bool isServiceCreation(const uint8_t smi[2])
{
    logData("%s(): ", __func__);
    /* Check if Service Creation should be executed */
    if (DF15_SERVICE_MANAGEMENT_INFO_SERVICE_KEY_PLANT ==
        (smi[0] &
         (DF15_SERVICE_MANAGEMENT_INFO_SERVICE_KEY_PLANT |
          DF15_SERVICE_MANAGEMENT_INFO_SERVICE_BALANCE_LIMIT_UPDATE)))
    {
        logData("yes\n");
        return TRUE;
    }
    logData("no\n");
    return FALSE;
}

bool isServiceBalanceLimitUpdate(const uint8_t smi[2])
{
    logData("%s(): ", __func__);
    /* Check if Service Balance Limit should be updated */
    if (DF15_SERVICE_MANAGEMENT_INFO_SERVICE_BALANCE_LIMIT_UPDATE ==
        (smi[0] &
         (DF15_SERVICE_MANAGEMENT_INFO_SERVICE_KEY_PLANT |
          DF15_SERVICE_MANAGEMENT_INFO_SERVICE_BALANCE_LIMIT_UPDATE)))
    {
        logData("yes\n");
        return TRUE;
    }
    logData("no\n");
    return FALSE;
}

bool isLegacy(const uint8_t avn[2])
{
    logData("%s(): ", __func__);

    /* Check if it is a legacy card */
    assert(NULL != avn);
    if (avn[0] == 0x00 &&
        avn[1] < 0x02)
    {
        logData("yes\n");
        return TRUE;
    }
    logData("no\n");
    return FALSE;
}

bool isServiceDataUpdate(const uint8_t smi[2])
{
    logData("%s(): ", __func__);
    /* Check if Service Balance Limit should be updated */
    if (DF15_SERVICE_MANAGEMENT_INFO_SERVICE_SPECIFIC_DATA ==
        (smi[0] &
         (DF15_SERVICE_MANAGEMENT_INFO_SERVICE_KEY_PLANT |
          DF15_SERVICE_MANAGEMENT_INFO_SERVICE_BALANCE_LIMIT_UPDATE)))
    {
        logData("yes\n");
        return TRUE;
    }
    logData("no\n");
    return FALSE;
}

bool isPermanentService(const uint8_t smi[2])
{
    logData("%s(): ", __func__);
    /* Check if a permanent service should be created */
    assert(NULL != smi);
    if (smi[0] & DF15_SERVICE_MANAGEMENT_INFO_PERMANENT_SERVICE)
    {
        logData("yes\n");
        return TRUE;
    }
    logData("no\n");
    return FALSE;
}

bool isServiceSpaceAvailable(const uint8_t smi[2], const uint8_t serviceAvailabilityInfo_DF03[1])
{
    logData("%s(): ", __func__);
    /* Check if space is available for service creation */
    assert(NULL != smi);
    assert(NULL != serviceAvailabilityInfo_DF03);
    if (isPermanentService(smi))
    {
        if (DF03_SERVICE_AVAILABILITY_INFO_PERMANENT_SERVICE_SPACE_AVAILABLE & serviceAvailabilityInfo_DF03[0])
        {
            logData("yes\n");
            return TRUE;
        }
    }
    else
    {
        if (DF03_SERVICE_AVAILABILITY_INFO_TEMPORARY_SERVICE_SPACE_AVAILABLE & serviceAvailabilityInfo_DF03[0])
        {
            logData("yes\n");
            return TRUE;
        }
    }
    logData("no\n");
    return FALSE;
}