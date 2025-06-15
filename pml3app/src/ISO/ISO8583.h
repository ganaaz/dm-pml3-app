#ifndef ISO8583_H
#define ISO8583_H
#include "ISO8583_interface.h"

/*  Typedefed Enum : TRANSACTION
    description : Transactions Supported for the HITACHI HOST SPEC
*/
typedef enum TXNS
{
    MONEY_LOAD_BALANCE_UPDATE,
    SALE_OFFLINE,
    BALANCE_ENQUIRY,
    SETTLEMENT,
    REVERSAL,
    REVERSAL_TIMEOUT,
    /* ALL THE TRANSACTIONS SHOULD BE ADDED BEFROE END_TRANSACTION */
    END_TRANSACTION
} TRANSACTION;

/*  Typedefed Enum : ISO8583_FIELD
    description : ISO8583 Data Elements
*/
typedef enum FIELD
{
    NO_FIELD = -1,
    DE00_MTI,
    DE01_BITMAP,
    DE02_PAN,
    DE03_PROCESSING_CODE,
    DE04_AMOUNT_TRANSACTION,
    DE05_AMOUNT_SETTLEMET,
    DE06_AMOUNT_CARD_HOLDER_BILLING,
    DE07_TRANSACTION_DATE_TIME,
    DE08_AMOUNT_CARD_HOLDER_BILLING_FEE,
    DE09_CONVERSION_RATE_SETTLEMENT,
    DE10_CONVERSION_RATE_CARD_HOLDER_BILLING,
    DE11_STAN,
    DE12_TRANSACTION_TIME_LOCAL,
    DE13_TRANSACTION_DATE_LOCAL,
    DE14_EXPIRATION_DATE,
    DE15_SETTLEMENT_DATE,
    DE16_CONVERSION_DATE,
    DE17_CAPTURE_DATE,
    DE18_MERCHANT_TYPE,
    DE19_ACQUIRING_INSTITUTION_COUNTRY_CODE,
    DE20_PAN_EXTENDED_COUNTRY_CODE,
    DE21_FORWARDING_INSTITUTION_COUNTRY_CODE,
    DE22_POS_ENTRY_MODE,
    DE23_APPLICATION_PAN_NUMBER,
    DE24_NII,
    DE25_POS_CONDITION_CODE,
    DE26_POS_CAPTURE_CODE,
    DE27_AUTHORIZING_IDENTIFICATION_RESPONSE_LENGTH,
    DE28_AMOUNT_TRANSACTION_FEE,
    DE29_AMOUNT_SETTLEMENT_FEE,
    DE30_AMOUNT_TRANSACTION_PROCESSING_FEE,
    DE31_AMOUNT_SETTLEMENT_PROCESSING_FEE,
    DE32_ACQUIRING_INSTITUTION_IDENTIFICATION_CODE,
    DE33_FORWARDING_INSTITUTION_IDENTIFICATION_CODE,
    DE34_PAN_EXTENDED,
    DE35_TRACK_2_DATA,
    DE36_TRACK_3_DATA,
    DE37_RRN,
    DE38_AUTHORIZATION_IDENTIFICATION_RESPONSE,
    DE39_RESPONSE_CODE,
    DE40_SERVICE_RESTRICTION_CODE,
    DE41_CARD_ACCEPTOR_TERMINAL_ID,
    DE42_CARD_ACCEPTOR_IDENTIFICATION_CODE,
    DE43_CARD_ACCEPTOR_ADDRESS, //(1-23 address 24-36 city 37-38 state 39-40 country)
    DE44_ADDITIONAL_RESPONSE_DATA,
    DE45_TRACK_1_DATA,
    DE46_ADDITIONAL_ISO_DATA,
    DE47_ADDITIONAL_NATIONAL_DATA,
    DE48_ADDITIONAL_PRIVATE_DATA,
    DE49_CURRENCY_CODE_TRANSACTION,
    DE50_CURRENCY_CODE_SETTLEMENT,
    DE51_CURRENCY_CODE_CARDHOLDER_BILLING,
    DE52_PIN_DATA,
    DE53_SECURITY_RELATED_CONTROL_INFO,
    DE54_AMOUNT_ADDITIONAL,
    DE55_RESERVED_ISO_1,
    DE56_RESERVED_ISO_2,
    DE57_RESERVED_NATIONAL_1,
    DE58_RESERVED_NATIONAL_2,
    DE59_RESERVED_NATIONAL_3,
    DE60_RESERVED_PRIVATE_ADVICE_REASON_CODE,
    DE61_RESERVED_PRIVATE_1,
    DE62_RESERVED_PRIVATE_2,
    DE63_RESERVED_PRIVATE_3,
    DE64_MAC
} ISO8583_FIELD;

/*  Typedefed Enum : SUB_COMPONENET
    description : ISO8583 sub field
    Caution: Make Sure this is always alligned with sub_component_def. This SUB_COMPONENET enum becomes the index of this sub_component_def array.
*/
typedef enum SUB_COMPONENT
{
    NO_SUB_COMPONENT = -1,
    DE48_PIN_KEY_LENGTH,
    DE48_TABLE_ID,
    DE48_PIN_KEY,
    DE60_BATCH_NUMBER,
    DE60_ORIGINAL_AMOUNT,
    DE62_INVOICE_NUMBER,
    DE63_FUND_SOURCE_TYPE,
    DE63_UNIQUE_TRANSACTION_ID,
    DE63_SOURCE_ID,
    DE63_PC_POS_TID,
    DE63_RRN,
    DE63_SERIAL_NUMBER,
    DE63_NARRATION_DATA,
    DE63_TRANSACTION_ID,
    DE63_BLACK_LIST_ID,
    DE63_BLACK_LIST_CARD_RECORDS
} ISO8583_SUB_COMPONENT;

/*  Typedefed Enum : ISO8583_SUB_FIELD
    description : ISO8583 sub field
    Caution: Make Sure this is always alligned with sub_field_def. This ISO8583_SUB_FIELD enum becomes the index of this sub_field_def array.
*/
typedef enum SUB_FIELD
{
    NO_SUB_FIELD = -1,
    DE48_SF_PIN_ENCRYPTION_KEY,
    DE60_SF_BATCH_NUMBER,
    DE60_SF_ORIGINAL_AMOUNT,
    DE62_SF_INVOICE_NUMBER,
    DE63_SF_MONEY_RELOAD_TYPE,
    DE63_SF_KEY_MAPPING,
    DE63_SF_NARRATION_OF_JOURNEY,
    DE63_SF_BLACK_LIST_REQUEST,
    DE63_SF_BLACK_LIST_RESPONSE
} ISO8583_SUB_FIELD;

/*  Typedefed Enum : ISO8583_FIELD_TYPE
    description : ISO8583 Field Types
*/
typedef enum FIELD_TYPE
{
    SIMPLE,
    COMPLEX
} ISO8583_FIELD_TYPE;

/*  Typedefed struct : ISO8583_SIMPLE_FIELD
    description : ISO8583 Simple Field value
*/
typedef struct SIMPLE_FIELD_VALUE
{
    ISO8583_FIELD field;
    unsigned char *value;
    int len;
} ISO8583_SIMPLE_FIELD_VALUE;

/*  Typedefed struct : ISO8583_SUB_FIELD
    description : ISO8583 Sub Field value
*/
typedef struct SUB_COMPONENT_VALUE
{
    ISO8583_SUB_COMPONENT field;
    unsigned char *value;
    int len;
} ISO8583_SUB_COMPONENT_VALUE;

/*  Typedefed struct : ISO8583_SUB_FIELD
    description : ISO8583 Sub Field value
*/
typedef struct SUB_FIELD_VALUE
{
    ISO8583_SUB_FIELD field;
    ISO8583_SUB_COMPONENT_VALUE *value;
    int count;
    unsigned char *sf_value;
    int sf_len;
} ISO8583_SUB_FIELD_VALUE;

/*  Typedefed struct : ISO8583_COMPLEX_FIELD
    description : ISO8583 Complex Field value
*/
typedef struct COMPLEX_FIELD_VALUE
{
    ISO8583_FIELD field;
    ISO8583_SUB_FIELD_VALUE *value;
    int count;
    unsigned char *complex_value;
    int comp_len;
} ISO8583_COMPLEX_FIELD_VALUE;

/*  Typedefed Union : ISO8583_FIELD_VALUE
    description : ISO8583 Field value
*/
typedef union FIELD_VALUE
{
    ISO8583_SIMPLE_FIELD_VALUE simple_field;
    ISO8583_COMPLEX_FIELD_VALUE complex_field;
} ISO8583_FIELD_VALUE;

/*  Typedefed struct : ISO8583_FIELD_DATA
    description : ISO8583 Field Data
*/
typedef struct FIELD_DATA
{
    ISO8583_FIELD_TYPE type;
    ISO8583_FIELD_VALUE field_value;
} ISO8583_FIELD_DATA;

/*  Typedefed struct : ISO8583_TXN_REQ_OBJECT, ISO8583_TXN_RESP_OBJECT
    description : ISO8583 Transaction Object
*/
typedef struct TXN_OBJECT
{
    ISO8583_FIELD_DATA data[65];
    ISO8583_ERROR_CODES error_code;
    unsigned char *message;
    int len;
} ISO8583_TXN_REQ_OBJECT, ISO8583_TXN_RESP_OBJECT;

/*  Typedefed Struct : ISO8583_MTI
    description : ISO8583 MTI
*/
typedef struct MTI
{
    TRANSACTION transaction;
    char req_mti[5];
    char resp_mti[5];
} ISO8583_MTI;

/*  Typedefed Struct : ISO8583_PROCESSING_CODES
    description : ISO8583 PROCESSING_CODES
*/
typedef struct PROC_CODES
{
    TRANSACTION transaction;
    char req_proc_code[7];
    char resp_proc_code[7];
} ISO8583_PROCESSING_CODES;

/*  Typedefed Struct : ISO8583_BITMAP_MAPPING
    description : ISO8583 BITMAPS
*/
typedef struct BITMAPS
{
    TRANSACTION transaction;
    long req_field_1_32;
    long req_field_33_64;
    long res_field_1_32;
    long res_field_33_64;
} ISO8583_BITMAP_MAPPING;

/*  Typedefed Enum : ISO8583_FIELD_SYNTAX
    description : ISO8583 Field Syntax
*/
typedef enum FIELD_SYNTAX
{
    NUMBER,
    ALPHA,
    SYMBOL,
    ALPHA_NUMERIC,
    ALPHA_SYMBOL,
    NUMBER_SYMBOL,
    ALPHA_NUMERIC_SYMBOL,
    HEX,
    TRACK,
    BLACK_LIST_RECORD,
    X_NUMERIC,
    ALPHA_OR_NUMERIC
} ISO8583_FIELD_SYNTAX;

/*  Typedefed Enum : ISO8583_FIELD_LENGTH
    description : ISO8583 Field Length
*/
typedef enum FIELD_LENGTH
{
    FIXED,
    LLVAR,
    LLVARENC,
    LLLVAR,
    LLLLVAR,
    BITMAP,
    TRACK_LEN
} ISO8583_FIELD_LENGTH;

/*  Typedefed Enum : ISO8583_PAD_TYPE
    description : ISO8583 Pad Type
*/
typedef enum PADDING
{
    PAD_LEFT,
    PAD_RIGHT,
    MIXED_PAD,
    PAD_NONE
} ISO8583_PAD_TYPE;

/*  Typedefed Enum : ISO8583_PAD_METHOD
    description : ISO8583 Pad Method
*/
typedef enum PAD_METHOD
{
    PAD_NIBBLE,
    PAD_FIXED_LENGTH,
    PAD_NO_METHOD
} ISO8583_PAD_METHOD;

/*  Typedefed Enum : ISO8583_SUB_COMPONENT_DEF
    description : ISO8583 sub component definision
*/
typedef struct SUB_COMPONENT_DEF
{
    ISO8583_SUB_COMPONENT sub_component;
    ISO8583_FIELD_SYNTAX syntax;
    ISO8583_FIELD_LENGTH length_type;
    int max_length;
    ISO8583_PAD_TYPE pad_type;
    char pad_char;
    ISO8583_PAD_METHOD pad_method;
} ISO8583_SUB_COMPONENT_DEF;

/*  Typedefed Enum : ISO8583_SUB_FIELD_DEF
    description : ISO8583 sub field definision
*/
typedef struct SUB_FIELD_DEF
{
    ISO8583_SUB_FIELD sub_field;
    ISO8583_FIELD_LENGTH length_type;
    int max_length;
    ISO8583_SUB_COMPONENT sub_components[10];
} ISO8583_SUB_FIELD_DEF;

/*  Typedefed Enum : ISO8583_FIELD_DEF
    description : ISO8583 field definision
*/
typedef struct FIELD_DEF
{
    ISO8583_FIELD field;
    ISO8583_FIELD_TYPE type;
    ISO8583_FIELD_LENGTH length_type;
    ISO8583_FIELD_SYNTAX syntax;
    int max_length;
    ISO8583_PAD_TYPE pad_type;
    char pad_char;
    ISO8583_PAD_METHOD pad_method;
    ISO8583_SUB_FIELD sub_field[10];
} ISO8583_FIELD_DEF;

/*  func : get_transaction_object
    description : Allocates memory and returns ISO8583_TXN_REQ_OBJECT*. Should be the first call to initiate a Transaction.
    out: {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request object.
*/
ISO8583_TXN_REQ_OBJECT *get_transaction_object();

/*  func : clear_transaction_object
    description : Deallocates the memory allocated during processing the transaction.
    in: txn_obj : {Type : ISO8583_TXN_REQ_OBJECT* | ISO8583_TXN_RESP_OBJECT*} : Transaction Object.
*/
void clear_transaction_object(ISO8583_TXN_REQ_OBJECT *txn_obj);

/*  func : pad_field_data
    description : Pad the field data.
    in: pad_type : {Type : ISO8583_PAD_TYPE} : Type of the required padding.
    in: syntax : {Type : ISO8583_FIELD_SYNTAX} : syntax of the field.
    in: pad_method : {Type : ISO8583_PAD_METHOD} : Type of the required padding method.
    in: pad_char : {Type : char} : Padchar to be used for padding.
    in: max_length : {Type : int} : maxlength of the field in chars.
    in: field_data_hex_str : {Type : char*} : Value of the field to be padded.
    out: outstr : {Type : char**} : Padded output string.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES pad_field_data(ISO8583_PAD_TYPE pad_type, ISO8583_FIELD_SYNTAX syntax, ISO8583_PAD_METHOD pad_method, char pad_char, int max_length, char *field_data_hex_str, char **outstr);

/*  func : copy_value
    description : Copies the field value into transaction object
    in: txn_obj : {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request Object.
    in: field : {Type : ISO8583_FIELD} : field to be copied.
    in: value : {Type : char*} : value to be copied. Should pass a null terminated string. in case Binary Data hexstr should be passed.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/

ISO8583_ERROR_CODES copy_value(ISO8583_TXN_REQ_OBJECT *txn_obj, ISO8583_FIELD field, char *value);

/*  func : copy_tpdu
    description : Copies TPDU into transaction object
    in: txn_obj : {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request Object.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES copy_tpdu(ISO8583_TXN_REQ_OBJECT *txn_obj);

/*  func : copy_mti
    description : Copies MTI into transaction object
    in: txn_obj : {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request Object.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES copy_mti(ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn);

/*  func : copy_bitmap
    description : Copies BITMAP into transaction object
    in: txn_obj : {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request Object.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES copy_bitmap(ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn);

/*  func : get_sub_field_value
    description : get the subfield value from the complex value.
    in: complex_val : {Type : ISO8583_COMPLEX_FIELD_VALUE*} : Complex Field value object.
    in: sub_field : {Type : ISO8583_SUB_FIELD} : Sub field to be retrieved.
    out : {Type : ISO8583_SUB_FIELD_VALUE*} : Subfield object if Success, NULL if failure.
*/
ISO8583_SUB_FIELD_VALUE *get_sub_field_value(ISO8583_COMPLEX_FIELD_VALUE *complex_val, ISO8583_SUB_FIELD sub_field);

/*  func : get_sub_component_value
    description : get the subcomponent value from the subfield value.
    in: sub_field_val : {Type : ISO8583_SUB_FIELD_VALUE*} : Sub Field value object.
    in: component : {Type : ISO8583_SUB_FIELD} : Sub component to be retrieved.
    out : {Type : ISO8583_SUB_COMPONENT_VALUE*} : Sub Component object if Success, NULL if failure.
*/
ISO8583_SUB_COMPONENT_VALUE *get_sub_component_value(ISO8583_SUB_FIELD_VALUE *sub_field_val, ISO8583_SUB_COMPONENT component);

/********************************************************************** Packer ***********************************************************************/

/*  func : pack
    description : Pack ISO8583 Protocol Message.
    in: txn_obj {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request Object passed for processing transaction.
    in: txn {Type : TRANSACTION} : Transaction to be packed.
    out: final_msg {Type: unsigned char**} : Final packed ISO8583 message.
    out: msg_len {Type: int*} : Final packed ISO8583 message length.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES pack(ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn, unsigned char **final_msg, int *msg_len);

/*  func : pack_simple_field
    description : Pack simple fields.
    in: txn_obj {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request Object passed for processing transaction.
    in: field {Type : ISO8583_FIELD} : simple Field to be packed.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES pack_simple_field(ISO8583_TXN_REQ_OBJECT *txn_obj, ISO8583_FIELD field);

/*  func : pack_complex_field
    description : Pack complex fields.
    in: txn_obj {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request Object passed for processing transaction.
    in: field {Type : ISO8583_FIELD} : complex Field to be packed.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES pack_complex_field(ISO8583_TXN_REQ_OBJECT *txn_obj, ISO8583_FIELD field);

/******************************************************************** END Packer *********************************************************************/
/********************************************************************** Parser ***********************************************************************/

/*  func : parse
    description : Parse ISO8583 Protocol Message.
    in: response {Type : const unsigned char*} : Response Received from Host fot the transaction {txn}.
    in: response_len {Type : int} : Response buffer length.
    in: txn_obj {Type: ISO8583_TXN_REQ_OBJECT*} : Request Tranansaction object user for packing the ISO8583 Protocol message.
    in: txn {Type : TRANSACTION} : Transaction to be Parsed.
    out: {Type : ISO8583_TXN_RESP_OBJECT*} : response transaction object : return valid ISO8583_TXN_RESP_OBJECT* if Success, NULL if failure.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES parse(unsigned char *response, int response_len, ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn, ISO8583_TXN_RESP_OBJECT **resp);

/*  func : parse_simple_field
    description : Parse simple fields.
    in: txn_obj {Type : ISO8583_TXN_RESP_OBJECT*} : Transaction Response Object passed for processing transaction.
    in: field {Type : ISO8583_FIELD} : simple Field to be parsed.
    in: txn_obj {Type: ISO8583_TXN_REQ_OBJECT*} : Request Tranansaction object user for packing the ISO8583 Protocol message.
    in: txn {Type: TRANSACTION} : Transaction Type.
    in/out: buffer {Type : unsigned char**} : buffer with the value for parsing the field. Reads the data for parsing and moves the pointer to the next field once parsing is complete.
    in/out: len {Type : int*} : buffer length. once the parsing is completed, the length will be updated excluding the current field parsing.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES parse_simple_field(ISO8583_TXN_RESP_OBJECT *resp_txn_obj, ISO8583_FIELD field, ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn, char **buffer, int *len);

/*  func : parse_complex_field
    description : Parse complex fields.
    in: txn_obj {Type : ISO8583_TXN_RESP_OBJECT*} : Transaction Response Object passed for processing transaction.
    in: field {Type : ISO8583_FIELD} : complex Field to be parsed.
    in: txn_obj {Type: ISO8583_TXN_REQ_OBJECT*} : Request Tranansaction object user for packing the ISO8583 Protocol message.
    in: txn {Type: TRANSACTION} : Transaction Type.
    in/out: buffer {Type : unsigned char**} : buffer with the value for parsing the field. Reads the data for parsing and moves the pointer to the next field once parsing is complete.
    in/out: len {Type : int*} : buffer length. once the parsing is completed, the length will be updated excluding the current field parsing.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
// int parse_complex_field(ISO8583_TXN_RESP_OBJECT* resp_txn_obj, ISO8583_FIELD field, ISO8583_TXN_REQ_OBJECT* txn_obj, TRANSACTION txn,  char** buffer, int* len);

/*  func : parse_sub_field
    description : Pack sub fields.
    in: req_sub_field_value {Type : ISO8583_SUB_FIELD_VALUE*} : Subfield value object for validation.
    in: field {Type : ISO8583_SUB_FIELD} : sub Field to be packed.
    out: resp_sub_field {Type : ISO8583_SUB_FIELD_VALUE*} : Subfield value object for parse and store the data.
    in/out: buffer {Type : unsigned char**} : buffer with the value for parsing the field. Reads the data for parsing and moves the pointer to the next field once parsing is complete.
    in/out: len {Type : int*} : buffer length. once the parsing is completed, the length will be updated excluding the current field parsing.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
// int parse_sub_field(ISO8583_SUB_FIELD_VALUE* req_sub_field_value, ISO8583_SUB_FIELD field, ISO8583_SUB_FIELD_VALUE* resp_sub_field, char** buffer, int* len);

/******************************************************************** END Parser *********************************************************************/

/*  func : process_transaction
    description : 1) Validate and Pack ISO8583 request message.
                  2) Process Transaction with Host.
                  3) Parse and Validate ISO8583 response message from Host.
                  4) Construct ISO8583_TXN_RESP_OBJECT* and return.
    in: txn_obj : {Type : ISO8583_TXN_REQ_OBJECT*} : Transaction Request Object got from get_transaction_object() and set all the required fields then pass it.
    in: txn : {Type : TRANSACTION} : Transaction type to be processed.
    out: resp : {Type : ISO8583_TXN_RESP_OBJECT*} : Transaction Response Object post processing the transaction with Host.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES process_transaction(ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn, ISO8583_TXN_RESP_OBJECT **resp);

#endif /* ISO8583_H */