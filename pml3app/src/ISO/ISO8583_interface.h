#ifndef ISO8583_INTERFACE_H
#define ISO8583_INTERFACE_H

/*  Typedefed Enum : ISO8583_ERROR_CODES
    description : ISO8583 Error Codes
*/
typedef enum ERRORS
{
    TXN_FAILED = -1,
    TXN_SUCCESS,
    TXN_HOST_CONNECTION_FAILED,
    TXN_SEND_TO_HOST_FAILED,
    TXN_RECEIVE_FROM_HOST_FAILED,
    TXN_HOST_CONNECTION_TIMEOUT,
    TXN_RECEIVE_FROM_HOST_TIMEOUT,
    TXN_PACK_VALIDATION_FAILED,
    TXN_PARSE_VALIDATION_FAILED,
    TXN_PACK_FAILED,
    TXN_PARSE_FAILED
} ISO8583_ERROR_CODES;

typedef struct STATIC_DATA
{
    char TPDU[11];
    char DE22_POS_ENTRY_MODE[5];
    char DE24_NII[5];
    char DE25_POS_CONDITION_CODE[3];
    char DE41_TERMINAL_ID[9];
    char DE42_CARD_ACCEPTOR_ID[16];
    char HOST_IP_ADDRESS[16];
    int HOST_PORT;
    int TRANSACTION_TIMOUT;
} ISO8583_STATIC_DATA;

typedef struct VAR_LEN_FIELD
{
    char *value;
    int len;
} VARIABLE_LENGTH_FIELD;

typedef struct OFFLINE_SALE_REQ
{
    char DE02_PAN_NUMBER[33];
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE12_TXN_TIME[7];
    char DE13_TXN_DATE[5];
    char DE37_RRN[13];
    // char DE35_TRACK_2_DATA[49];
    VARIABLE_LENGTH_FIELD DE55_ICC_DATA;
    char DE53_SECURITY_DATA[45];
    char DE56_BATCH_NUMBER[7];
    char DE62_INVOICE_NUMBER[7];
    VARIABLE_LENGTH_FIELD DE63_NARRATION_DATA;
} OFFLINE_SALE_REQUEST;

typedef struct OFFLINE_SALE_RESP
{
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE12_TXN_TIME[7];
    char DE13_TXN_DATE[5];
    char DE37_RRN[13];
    char DE38_AUTH_CODE[7];
    char DE39_RESPONSE_CODE[3];
} OFFLINE_SALE_RESPONSE;

typedef struct BALANCE_UPDATE_REQ
{
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE35_TRACK_2_DATA[49];
    VARIABLE_LENGTH_FIELD DE55_ICC_DATA;
    char DE53_SECURITY_DATA[45];
    char DE56_BATCH_NUMBER[7];
    char DE62_INVOICE_NUMBER[7];
} BALANCE_UPDATE_REQUEST;

typedef struct BALANCE_UPDATE_RESP
{
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE12_TXN_TIME[7];
    char DE13_TXN_DATE[5];
    char DE37_RRN[13];
    char DE38_AUTH_CODE[7];
    char DE39_RESPONSE_CODE[3];
    VARIABLE_LENGTH_FIELD DE55_ICC_DATA;
} BALANCE_UPDATE_RESPONSE;

typedef struct BALANCE_ENQUIRY_REQ
{
    char DE02_PAN_NUMBER[20];
    char DE03_PROC_CODE[7];
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE12_TXN_TIME[7];
    char DE13_TXN_DATE[5];
    char DE14_EXPIRY_DATE[5];
    VARIABLE_LENGTH_FIELD DE55_ICC_DATA;
    char DE62_INVOICE_NUMBER[7];
    VARIABLE_LENGTH_FIELD DE63_PRIVATE_DATA;
} BALANCE_ENQUIRY_REQUEST;

typedef struct BALANCE_ENQUIRY_RESP
{
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE12_TXN_TIME[7];
    char DE13_TXN_DATE[5];
    char DE37_RRN[13];
    char DE38_AUTH_CODE[7];
    char DE39_RESPONSE_CODE[3];
} BALANCE_ENQUIRY_RESPONSE;

typedef struct SETTLEMENT_REQ
{
    char DE03_PROC_CODE[7];
    char DE11_STAN[7];
    VARIABLE_LENGTH_FIELD DE60_BATCH_NUMBER;
    VARIABLE_LENGTH_FIELD DE63_PRIVATE_DATA;
} SETTLEMENT_REQUEST;

typedef struct SETTLEMENT_RESP
{
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE12_TXN_TIME[7];
    char DE13_TXN_DATE[5];
    char DE39_RESPONSE_CODE[3];
    VARIABLE_LENGTH_FIELD DE60_BATCH_NUMBER;
    VARIABLE_LENGTH_FIELD DE61_HOST_DATE_TIME;
} SETTLEMENT_RESPONSE;

typedef struct REVERSAL_REQ
{
    // char DE02_PAN_NUMBER[20];
    char DE03_PROC_CODE[7];
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE12_TXN_TIME[7];
    char DE13_TXN_DATE[5];
    // char DE14_EXPIRY_DATE[5];
    char DE35_TRACK_2_DATA[38];
    char DE37_RRN[13];
    char DE38_AUTH_CODE[7];
    char DE39_RESPONSE_CODE[3];
    VARIABLE_LENGTH_FIELD DE55_ICC_DATA;
    char DE62_INVOICE_NUMBER[7];
    VARIABLE_LENGTH_FIELD DE63_PRIVATE_DATA;
} REVERSAL_REQUEST;

typedef struct REVERSAL_RESP
{
    char DE04_TXN_AMOUNT[13];
    char DE11_STAN[7];
    char DE12_TXN_TIME[7];
    char DE13_TXN_DATE[5];
    char DE37_RRN[13];
    char DE38_AUTH_CODE[7];
    char DE39_RESPONSE_CODE[3];
} REVERSAL_RESPONSE;

/*  func : initialize_static_data
    description : Initializes static data elements. Should be called once during bootup.
    in: data : {Type : ISO8583_STATIC_DATA*} : Static Data object which has the static data info.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES initialize_static_data(ISO8583_STATIC_DATA *data);

/*  func : process_balance_update_transaction
    description : Processes the balance update transaction
                  1) Validate and Pack ISO8583 request message.
                  2) Process Transaction with Host.
                  3) Parse and Validate ISO8583 response message from Host.
                  4) Construct BALANCE_UPDATE_RESPONSE* and return.
    in: txn_obj : {Type : BALANCE_UPDATE_REQUEST*} : Transaction Request Object to be passed after setting all the required fields.
    out: resp : {Type : BALANCE_UPDATE_RESPONSE*} : Transaction Response Object post processing the transaction with Host.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES process_balance_update_transaction(BALANCE_UPDATE_REQUEST *txn_obj, BALANCE_UPDATE_RESPONSE *resp);

/*  func : process_offline_sale_transaction
    description : Processes the offline sale transaction
                  1) Validate and Pack ISO8583 request message.
                  2) Process Transaction with Host.
                  3) Parse and Validate ISO8583 response message from Host.
                  4) Construct OFFLINE_SALE_RESPONSE* and return.
    in: txn_obj : {Type : OFFLINE_SALE_REQUEST*} : Transaction Request Object to be passed after setting all the required fields.
    out: resp : {Type : OFFLINE_SALE_RESPONSE*} : Transaction Response Object post processing the transaction with Host.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES process_offline_sale_transaction(OFFLINE_SALE_REQUEST *txn_obj, OFFLINE_SALE_RESPONSE *resp);

/*  func : process_balance_enquiry_transaction
    description : Processes the balance enquiry transaction
                  1) Validate and Pack ISO8583 request message.
                  2) Process Transaction with Host.
                  3) Parse and Validate ISO8583 response message from Host.
                  4) Construct BALANCE_ENQUIRY_RESPONSE* and return.
    in: txn_obj : {Type : BALANCE_ENQUIRY_REQUEST*} : Transaction Request Object to be passed after setting all the required fields.
    out: resp : {Type : BALANCE_ENQUIRY_RESPONSE*} : Transaction Response Object post processing the transaction with Host.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES process_balance_enquiry_transaction(BALANCE_ENQUIRY_REQUEST *txn_obj, BALANCE_ENQUIRY_RESPONSE *resp);

/*  func : process_settlement_transaction
    description : Processes the reversal transaction
                  1) Validate and Pack ISO8583 request message.
                  2) Process Transaction with Host.
                  3) Parse and Validate ISO8583 response message from Host.
                  4) Construct SETTLEMENT_RESPONSE* and return.
    in: txn_obj : {Type : SETTLEMENT_REQUEST*} : Transaction Request Object to be passed after setting all the required fields.
    out: resp : {Type : SETTLEMENT_RESPONSE*} : Transaction Response Object post processing the transaction with Host.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES process_settlement_transaction(SETTLEMENT_REQUEST *txn_obj, SETTLEMENT_RESPONSE *resp);

/*  func : process_reversal_transaction
    description : Processes the reversal transaction
                  1) Validate and Pack ISO8583 request message.
                  2) Process Transaction with Host.
                  3) Parse and Validate ISO8583 response message from Host.
                  4) Construct REVERSAL_RESPONSE* and return.
    in: txn_obj : {Type : REVERSAL_REQUEST*} : Transaction Request Object to be passed after setting all the required fields.
    out: resp :{Type : REVERSAL_RESPONSE*} : Transaction Response Object post processing the transaction with Host.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES process_reversal_transaction(REVERSAL_REQUEST *txn_obj, REVERSAL_RESPONSE *resp, int txn);
#endif /* ISO8583_INTERFACE_H */