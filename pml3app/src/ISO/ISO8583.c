#include "ISO8583_def.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "comms.h"
#include "../include/logutil.h"

/********************************************************************** DEFENISIONS **********************************************************************/
const int NULL_BYTE_LEN = 1;
const int TPDU_LEN = 10;
const int LENGTH_OF_MSG_LEN = 4;
char RESPONSE_CODE_SUCCESS[5] = "3030";
ISO8583_STATIC_DATA static_data;
// Value indicator signifies, that the length of a field is the value of some other field.
#define VALUE_INDICATOR 10000 
// Make Sure this is always alligned with ISO8583_SUB_FIELD. This ISO8583_SUB_FIELD enum becomes the index of this sub_field_def array.
ISO8583_SUB_COMPONENT_DEF sub_component_def[] = {
    {DE48_PIN_KEY_LENGTH,           NUMBER,                 FIXED,  2,                                      PAD_LEFT, 0x00, PAD_FIXED_LENGTH},
    {DE48_TABLE_ID,                 ALPHA_NUMERIC_SYMBOL,   FIXED,  2,                                      PAD_LEFT, 0x30, PAD_FIXED_LENGTH},
    {DE48_PIN_KEY,                  HEX,                    FIXED,  VALUE_INDICATOR + DE48_PIN_KEY_LENGTH,  PAD_NONE, 0x00, PAD_NO_METHOD},      //DE48_PIN_KEY : Indicates the length of this field is the value of DE48_PIN_KEY_LENGTH field.
    {DE60_BATCH_NUMBER,             ALPHA_NUMERIC_SYMBOL,   FIXED,  6,                                      PAD_LEFT, 0x30, PAD_FIXED_LENGTH},
    {DE60_ORIGINAL_AMOUNT,          ALPHA_NUMERIC,          FIXED,  12,                                     PAD_LEFT, 0x30, PAD_FIXED_LENGTH},
    {DE62_INVOICE_NUMBER,           ALPHA_NUMERIC,          FIXED,  6,                                      PAD_LEFT, 0x30, PAD_FIXED_LENGTH},
    {DE63_FUND_SOURCE_TYPE,         ALPHA_NUMERIC,          FIXED,  2,                                      PAD_LEFT, 0x30, PAD_FIXED_LENGTH},
    {DE63_UNIQUE_TRANSACTION_ID,    NUMBER,                 FIXED,  10,                                     PAD_LEFT, 0x00, PAD_FIXED_LENGTH},
    {DE63_SOURCE_ID,                NUMBER,                 FIXED,  10,                                     PAD_NONE, 0x00, PAD_NO_METHOD},      //DE63_SOURCE_ID : SOURCE ID in the document is ALPHA_NUMERIC, But the length Suggests its NUMBER
    {DE63_PC_POS_TID,               ALPHA_NUMERIC,          FIXED,  8,                                      PAD_NONE, 0x00, PAD_NO_METHOD},
    {DE63_RRN,                      NUMBER,                 FIXED,  6,                                      PAD_NONE, 0x00, PAD_NO_METHOD},
    {DE63_SERIAL_NUMBER,            ALPHA_NUMERIC,          FIXED,  12,                                     PAD_NONE, 0x00, PAD_NO_METHOD},
    {DE63_NARRATION_DATA,           ALPHA_NUMERIC,          FIXED,  30,                                     PAD_NONE, 0x00, PAD_NO_METHOD},
    {DE63_TRANSACTION_ID,           ALPHA_NUMERIC,          FIXED,  12,                                     PAD_NONE, 0x00, PAD_NO_METHOD},
    {DE63_BLACK_LIST_ID,            BLACK_LIST_RECORD,      LLLVAR, 12,                                     PAD_NONE, 0x00, PAD_NO_METHOD},
    {DE63_BLACK_LIST_CARD_RECORDS,  BLACK_LIST_RECORD,      LLLVAR, 0,                                      PAD_NONE, 0x00, PAD_NO_METHOD}
};


ISO8583_SUB_FIELD_DEF sub_field_def[] = {
    {DE48_SF_PIN_ENCRYPTION_KEY,    LLLVAR, 60, {DE48_PIN_KEY_LENGTH, DE48_TABLE_ID, DE48_PIN_KEY, NO_SUB_COMPONENT}},
    {DE60_SF_BATCH_NUMBER,          LLLVAR, 16, {DE60_BATCH_NUMBER, NO_SUB_COMPONENT}},
    {DE60_SF_ORIGINAL_AMOUNT,       LLLVAR, 28, {DE60_ORIGINAL_AMOUNT, NO_SUB_COMPONENT}},
    {DE62_SF_INVOICE_NUMBER,        LLLVAR, 16, {DE62_INVOICE_NUMBER, NO_SUB_COMPONENT}},
    {DE63_SF_MONEY_RELOAD_TYPE,     LLLVAR, 76, {DE63_FUND_SOURCE_TYPE, DE63_UNIQUE_TRANSACTION_ID, DE63_SOURCE_ID, DE63_PC_POS_TID, DE63_RRN, NO_SUB_COMPONENT}},
    {DE63_SF_KEY_MAPPING,           LLLVAR, 28, {DE63_SERIAL_NUMBER, NO_SUB_COMPONENT}},
    {DE63_SF_NARRATION_OF_JOURNEY,  LLLVAR, 108,{DE63_NARRATION_DATA, DE63_TRANSACTION_ID, DE63_UNIQUE_TRANSACTION_ID, NO_SUB_COMPONENT}},
    {DE63_SF_BLACK_LIST_REQUEST,    LLLVAR, 28, {DE63_BLACK_LIST_ID, NO_SUB_COMPONENT}},
    {DE63_SF_BLACK_LIST_RESPONSE,   LLLVAR, 4,  {DE63_BLACK_LIST_CARD_RECORDS, NO_SUB_COMPONENT}},
};

ISO8583_FIELD_DEF field_def[] = {
    {DE00_MTI,                                          SIMPLE, 	FIXED,      NUMBER, 				4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE01_BITMAP,                                       SIMPLE, 	BITMAP,     HEX, 				    16, 	PAD_NONE,   '0',   PAD_NO_METHOD,       {NO_SUB_FIELD}},
    {DE02_PAN,                                          SIMPLE, 	LLVARENC,   NUMBER, 				32, 	PAD_RIGHT,  'F',   PAD_NIBBLE,          {NO_SUB_FIELD}},
    {DE03_PROCESSING_CODE,                              SIMPLE, 	FIXED,      NUMBER,  				6,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE04_AMOUNT_TRANSACTION,                           SIMPLE, 	FIXED,      NUMBER, 				12, 	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE05_AMOUNT_SETTLEMET,                             SIMPLE, 	FIXED,      NUMBER, 				12, 	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE06_AMOUNT_CARD_HOLDER_BILLING,                   SIMPLE, 	FIXED,      NUMBER, 				12, 	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE07_TRANSACTION_DATE_TIME,                        SIMPLE, 	FIXED,      NUMBER, 				10, 	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE08_AMOUNT_CARD_HOLDER_BILLING_FEE,               SIMPLE, 	FIXED,      NUMBER, 				8, 		PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE09_CONVERSION_RATE_SETTLEMENT,                   SIMPLE, 	FIXED,      NUMBER, 				8, 		PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE10_CONVERSION_RATE_CARD_HOLDER_BILLING,          SIMPLE, 	FIXED,      NUMBER, 				8, 		PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE11_STAN,                                         SIMPLE, 	FIXED,      NUMBER, 				6,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE12_TRANSACTION_TIME_LOCAL,                       SIMPLE, 	FIXED,      NUMBER, 				6,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH, 	{NO_SUB_FIELD}},
    {DE13_TRANSACTION_DATE_LOCAL,                       SIMPLE, 	FIXED,      NUMBER, 				4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE14_EXPIRATION_DATE,                              SIMPLE, 	FIXED,      NUMBER, 				4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE15_SETTLEMENT_DATE,                              SIMPLE, 	FIXED,      NUMBER, 				4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE16_CONVERSION_DATE,                              SIMPLE, 	FIXED,      NUMBER, 				4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE17_CAPTURE_DATE,                                 SIMPLE, 	FIXED,      NUMBER, 				4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE18_MERCHANT_TYPE,                                SIMPLE, 	FIXED,      NUMBER, 				4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE19_ACQUIRING_INSTITUTION_COUNTRY_CODE,           SIMPLE, 	FIXED,      NUMBER,                 4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE20_PAN_EXTENDED_COUNTRY_CODE,                    SIMPLE, 	FIXED,      NUMBER,                 4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE21_FORWARDING_INSTITUTION_COUNTRY_CODE,          SIMPLE, 	FIXED,      NUMBER,                 4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE22_POS_ENTRY_MODE,                               SIMPLE, 	FIXED,      NUMBER,                 4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE23_APPLICATION_PAN_NUMBER,                       SIMPLE, 	FIXED,      NUMBER,                 4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE24_NII,                                          SIMPLE, 	FIXED,      NUMBER,                 4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE25_POS_CONDITION_CODE,                           SIMPLE, 	FIXED,      NUMBER,                 2,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE26_POS_CAPTURE_CODE,                             SIMPLE, 	FIXED,      NUMBER,                 2,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE27_AUTHORIZING_IDENTIFICATION_RESPONSE_LENGTH,   SIMPLE, 	FIXED,      NUMBER,                 2,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE28_AMOUNT_TRANSACTION_FEE,                       SIMPLE, 	FIXED,      X_NUMERIC,              8, 		PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE29_AMOUNT_SETTLEMENT_FEE,                        SIMPLE, 	FIXED,      X_NUMERIC,              8, 		PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE30_AMOUNT_TRANSACTION_PROCESSING_FEE,            SIMPLE, 	FIXED,      X_NUMERIC,              8, 		PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE31_AMOUNT_SETTLEMENT_PROCESSING_FEE,             SIMPLE, 	FIXED,      X_NUMERIC,              8, 		PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE32_ACQUIRING_INSTITUTION_IDENTIFICATION_CODE,    SIMPLE, 	LLVAR,      NUMBER,                 12, 	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE33_FORWARDING_INSTITUTION_IDENTIFICATION_CODE,   SIMPLE, 	LLVAR,      NUMBER,                 12, 	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE34_PAN_EXTENDED,                                 SIMPLE, 	LLVAR,      NUMBER_SYMBOL,          28, 	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE35_TRACK_2_DATA,                                 SIMPLE, 	TRACK_LEN,  TRACK,                  48,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE36_TRACK_3_DATA,                                 SIMPLE, 	LLLVAR,     NUMBER,                 104, 	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE37_RRN,                                          SIMPLE, 	FIXED,      ALPHA_NUMERIC,          12,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE38_AUTHORIZATION_IDENTIFICATION_RESPONSE,        SIMPLE, 	FIXED,      ALPHA_NUMERIC,          6,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE39_RESPONSE_CODE,                                SIMPLE, 	FIXED,      ALPHA_NUMERIC,          2,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE40_SERVICE_RESTRICTION_CODE,                     SIMPLE, 	FIXED,      ALPHA_NUMERIC,          4,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE41_CARD_ACCEPTOR_TERMINAL_ID,                    SIMPLE, 	FIXED,      ALPHA_NUMERIC_SYMBOL,   8,  	PAD_RIGHT,  0x20,  PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE42_CARD_ACCEPTOR_IDENTIFICATION_CODE,            SIMPLE, 	FIXED,      ALPHA_NUMERIC_SYMBOL,   15,  	PAD_RIGHT,  0x20,  PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE43_CARD_ACCEPTOR_ADDRESS,                        SIMPLE, 	FIXED,      ALPHA_NUMERIC_SYMBOL,   40,  	PAD_RIGHT,  0x20,  PAD_FIXED_LENGTH,  	{NO_SUB_FIELD}},
    {DE44_ADDITIONAL_RESPONSE_DATA,                     SIMPLE, 	LLVAR,      ALPHA_NUMERIC,          25,  	PAD_NONE,   '0',   PAD_NO_METHOD,  	    {NO_SUB_FIELD}},
    {DE45_TRACK_1_DATA,                                 SIMPLE, 	LLVAR,      ALPHA_NUMERIC,          76,  	PAD_NONE,   '0',   PAD_NO_METHOD,  	    {NO_SUB_FIELD}},
    {DE46_ADDITIONAL_ISO_DATA,                          SIMPLE, 	LLLVAR,     ALPHA_NUMERIC,          999,  	PAD_NONE,   '0',   PAD_NO_METHOD,  	    {NO_SUB_FIELD}},
    {DE47_ADDITIONAL_NATIONAL_DATA,                     SIMPLE, 	LLLVAR,     ALPHA_NUMERIC,          999,  	PAD_NONE,   '0',   PAD_NO_METHOD,  	    {NO_SUB_FIELD}},
    {DE48_ADDITIONAL_PRIVATE_DATA,                      COMPLEX, 	LLLVAR,     ALPHA_NUMERIC,          999,  	PAD_NONE,   '0',   PAD_NO_METHOD,  	    {DE48_SF_PIN_ENCRYPTION_KEY, NO_SUB_FIELD}},
    {DE49_CURRENCY_CODE_TRANSACTION,                    SIMPLE, 	FIXED,      ALPHA_OR_NUMERIC,       3,    	MIXED_PAD,  0x20,  PAD_FIXED_LENGTH, 	{NO_SUB_FIELD}},
    {DE50_CURRENCY_CODE_SETTLEMENT,                     SIMPLE, 	FIXED,      ALPHA_OR_NUMERIC,       3,    	MIXED_PAD,  0x20,  PAD_FIXED_LENGTH, 	{NO_SUB_FIELD}},
    {DE51_CURRENCY_CODE_CARDHOLDER_BILLING,             SIMPLE, 	FIXED,      ALPHA_OR_NUMERIC,       3,    	MIXED_PAD,  0x20,  PAD_FIXED_LENGTH, 	{NO_SUB_FIELD}},
    {DE52_PIN_DATA,                                     SIMPLE, 	FIXED,      HEX,                    16,  	PAD_NONE,   '0',   PAD_NO_METHOD,  	    {NO_SUB_FIELD}},
    {DE53_SECURITY_RELATED_CONTROL_INFO,                SIMPLE, 	FIXED,      NUMBER,                 44,  	PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE54_AMOUNT_ADDITIONAL,                            SIMPLE, 	LLLVAR,     ALPHA_NUMERIC,          120,  	PAD_NONE,   '0',   PAD_NO_METHOD,       {NO_SUB_FIELD}},
    {DE55_RESERVED_ISO_1,                               SIMPLE, 	LLLVAR,     HEX,                    512,    PAD_NONE,   '0',   PAD_NO_METHOD,     	{NO_SUB_FIELD}},
    {DE56_RESERVED_ISO_2,                               SIMPLE, 	FIXED,      ALPHA_NUMERIC_SYMBOL,   6,      PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE57_RESERVED_NATIONAL_1,                          SIMPLE, 	LLLVAR,     ALPHA_NUMERIC_SYMBOL,   999,    PAD_NONE,   '0',   PAD_NO_METHOD,     	{NO_SUB_FIELD}},
    {DE58_RESERVED_NATIONAL_2,                          SIMPLE, 	LLLVAR,     ALPHA_NUMERIC_SYMBOL,   999,    PAD_NONE,   '0',   PAD_NO_METHOD,     	{NO_SUB_FIELD}},
    {DE59_RESERVED_NATIONAL_3,                          SIMPLE, 	LLLVAR,     ALPHA_NUMERIC_SYMBOL,   999,    PAD_NONE,   '0',   PAD_NO_METHOD,     	{NO_SUB_FIELD}},
    {DE60_RESERVED_PRIVATE_ADVICE_REASON_CODE,          SIMPLE, 	LLLVAR,     ALPHA_NUMERIC_SYMBOL,   6,      PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {DE60_SF_BATCH_NUMBER, DE60_SF_ORIGINAL_AMOUNT, NO_SUB_FIELD}},
    {DE61_RESERVED_PRIVATE_1,                           SIMPLE, 	LLLVAR,     ALPHA_NUMERIC_SYMBOL,   12,     PAD_LEFT,   '0',   PAD_FIXED_LENGTH,     {NO_SUB_FIELD}},
    {DE62_RESERVED_PRIVATE_2,                           SIMPLE, 	LLLVAR,     ALPHA_NUMERIC_SYMBOL,   6,      PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {NO_SUB_FIELD}},
    {DE63_RESERVED_PRIVATE_3,                           SIMPLE, 	LLLVAR,     ALPHA_NUMERIC_SYMBOL,   2,      PAD_LEFT,   '0',   PAD_FIXED_LENGTH,    {DE63_SF_MONEY_RELOAD_TYPE, DE63_SF_KEY_MAPPING, DE63_SF_NARRATION_OF_JOURNEY, DE63_SF_BLACK_LIST_REQUEST, DE63_SF_BLACK_LIST_RESPONSE, NO_SUB_FIELD}},
    {DE64_MAC,                                          SIMPLE, 	FIXED,      HEX,                    16,     PAD_NONE,   '0',   PAD_NO_METHOD,     	{NO_SUB_FIELD}}
};

ISO8583_MTI mti_def[] = {
    { MONEY_LOAD_BALANCE_UPDATE,    "0200", "0210"},
    { SALE_OFFLINE,                 "0220", "0230"},
    { BALANCE_ENQUIRY,              "0220", "0230"},
    { SETTLEMENT,                   "0500", "0510"},
    { REVERSAL,                     "0400", "0410"},
    { REVERSAL_TIMEOUT,              "0400", "0410"}
};

ISO8583_PROCESSING_CODES pro_codes_def[] = {
    { MONEY_LOAD_BALANCE_UPDATE,    "840000", "840000"},
    { SALE_OFFLINE,                 "000000", "000000"},
    { BALANCE_ENQUIRY,              "000000", "000000"},
    { SETTLEMENT,                   "920000", "920000"},
    { REVERSAL,                     "000000", "840000"},
    { REVERSAL_TIMEOUT,              "000000", "840000"}
};

ISO8583_BITMAP_MAPPING bitmap_def[] = {
    //{ MONEY_LOAD_BALANCE_UPDATE,    0x30200580, 0x20c00306, 0x30380000, 0x0e800202},
    // { MONEY_LOAD_BALANCE_UPDATE,    0x30200580, 0x20C00B06, 0x30380000, 0x0e800202}, // With security
    { MONEY_LOAD_BALANCE_UPDATE,    0x30200580, 0x20C00B06, 0x30380000, 0x0e800e02}, // With security and for paycraft
    //{ SALE_OFFLINE,                 0x70380480, 0x08C00204, 0x30380000, 0x0e800000}, // Plain
    // { SALE_OFFLINE,                 0x30380580, 0x28C00B04, 0x30380000, 0x0e800000}, // With Security Track 2
    { SALE_OFFLINE,                 0x70380580, 0x08C00B04, 0x70380480, 0x0e800000}, // With Security Pan
    { BALANCE_ENQUIRY,              0x00000000, 0x00000000, 0x00000000, 0x00000000},
    { SETTLEMENT,                   0x20200000, 0x00C00012, 0x20380000, 0x02800018},
    //{ REVERSAL,                     0x703C0480, 0x00C00206, 0x70380000, 0x0EC00006}
    { REVERSAL,                     0x30380480, 0x2EC00206, 0x30380000, 0x0E800002},
    { REVERSAL_TIMEOUT,             0x30380480, 0x22C00206, 0x30380000, 0x0E800002}
};

/********************************************************************** END DEFENISIONS **********************************************************************/

ISO8583_TXN_REQ_OBJECT *get_transaction_object()
{
    ISO8583_TXN_REQ_OBJECT *txn_obj = (ISO8583_TXN_REQ_OBJECT *)malloc(sizeof(ISO8583_TXN_REQ_OBJECT));
    txn_obj->error_code = TXN_SUCCESS;
    txn_obj->len = 0;
    txn_obj->message = NULL;
    for (int i = 0; i < 65; i++)
    {
        if (field_def[i].type == SIMPLE)
        {
            txn_obj->data[i].type = SIMPLE;
            txn_obj->data[i].field_value.simple_field.field = i;
            txn_obj->data[i].field_value.simple_field.value = NULL;
            txn_obj->data[i].field_value.simple_field.len = 0;
        }
        else
        {
            txn_obj->data[i].type = COMPLEX;
            txn_obj->data[i].field_value.complex_field.field = i;
            txn_obj->data[i].field_value.complex_field.value = NULL;
            txn_obj->data[i].field_value.complex_field.count = 0;
            txn_obj->data[i].field_value.complex_field.complex_value = NULL;
            txn_obj->data[i].field_value.complex_field.comp_len = 0;
        }
    }
    return txn_obj;
}

void clear_transaction_object(ISO8583_TXN_REQ_OBJECT *txn_obj)
{
    if (txn_obj != NULL)
    {
        if (txn_obj->message)
            free(txn_obj->message);

        txn_obj->len = 0;

        for (int i = 0; i < 65; i++)
        {
            if (txn_obj->data[i].type == SIMPLE)
            {
                if (txn_obj->data[i].field_value.simple_field.value)
                    free(txn_obj->data[i].field_value.simple_field.value);

                txn_obj->data[i].field_value.simple_field.len = 0;
            }
            else
            {
                if (txn_obj->data[i].field_value.complex_field.complex_value)
                    free(txn_obj->data[i].field_value.complex_field.complex_value);
                txn_obj->data[i].field_value.complex_field.comp_len = 0;

                if (txn_obj->data[i].field_value.complex_field.value)
                {
                    int count = txn_obj->data[i].field_value.complex_field.count;
                    for (int i = 0; i < count; i++)
                    {
                        if ((txn_obj->data[i].field_value.complex_field.value + i)->value)
                        {
                            int comp_count = (txn_obj->data[i].field_value.complex_field.value + i)->count;
                            for (int j = 0; j < comp_count; j++)
                            {
                                if(((txn_obj->data[i].field_value.complex_field.value + i)->value + j)->value)
                                {
                                    free(((txn_obj->data[i].field_value.complex_field.value + i)->value + j)->value);
                                    ((txn_obj->data[i].field_value.complex_field.value + i)->value + j)->len = 0;
                                }
                            }
                            if ((txn_obj->data[i].field_value.complex_field.value + i)->value)
                                free((txn_obj->data[i].field_value.complex_field.value + i)->value);
                            (txn_obj->data[i].field_value.complex_field.value + i)->count = 0;

                            if ((txn_obj->data[i].field_value.complex_field.value + i)->sf_value)
                                free((txn_obj->data[i].field_value.complex_field.value + i)->sf_value);
                            (txn_obj->data[i].field_value.complex_field.value + i)->sf_len = 0;

                        }

                    }
                    free(txn_obj->data[i].field_value.complex_field.value);
                    txn_obj->data[i].field_value.complex_field.count = 0;
                }
            }
        }
    }

    if (txn_obj)
        free(txn_obj);

    txn_obj = NULL;
}

ISO8583_ERROR_CODES initialize_static_data(ISO8583_STATIC_DATA *data)
{
    if (strlen(data->TPDU) >= sizeof(static_data.TPDU))
        return TXN_FAILED;
    memcpy(static_data.TPDU, data->TPDU, strlen(data->TPDU) + 1);

    if (strlen(data->DE22_POS_ENTRY_MODE) >= sizeof(static_data.DE22_POS_ENTRY_MODE))
        return TXN_FAILED;
    memcpy(static_data.DE22_POS_ENTRY_MODE, data->DE22_POS_ENTRY_MODE, strlen(data->DE22_POS_ENTRY_MODE) + 1);

    if (strlen(data->DE24_NII) >= sizeof(static_data.DE24_NII))
        return TXN_FAILED;
    memcpy(static_data.DE24_NII, data->DE24_NII, strlen(data->DE24_NII) + 1);

    if (strlen(data->DE25_POS_CONDITION_CODE) >= sizeof(static_data.DE25_POS_CONDITION_CODE))
        return TXN_FAILED;
    memcpy(static_data.DE25_POS_CONDITION_CODE, data->DE25_POS_CONDITION_CODE, strlen(data->DE25_POS_CONDITION_CODE) + 1);

    if (strlen(data->DE41_TERMINAL_ID) >= sizeof(static_data.DE41_TERMINAL_ID))
        return TXN_FAILED;
    memcpy(static_data.DE41_TERMINAL_ID, data->DE41_TERMINAL_ID, strlen(data->DE41_TERMINAL_ID) + 1);

    if (strlen(data->DE42_CARD_ACCEPTOR_ID) >= sizeof(static_data.DE42_CARD_ACCEPTOR_ID))
        return TXN_FAILED;
    memcpy(static_data.DE42_CARD_ACCEPTOR_ID, data->DE42_CARD_ACCEPTOR_ID, strlen(data->DE42_CARD_ACCEPTOR_ID) + 1);

    if (strlen(data->HOST_IP_ADDRESS) >= sizeof(static_data.HOST_IP_ADDRESS))
        return TXN_FAILED;
    memcpy(static_data.HOST_IP_ADDRESS, data->HOST_IP_ADDRESS, strlen(data->HOST_IP_ADDRESS) + 1);

    static_data.HOST_PORT = data->HOST_PORT;
    static_data.TRANSACTION_TIMOUT = data->TRANSACTION_TIMOUT;

    logData("Static Data initialisation Success");
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES get_field_length(ISO8583_FIELD_LENGTH length_type, char *field_data_hex_str, char **length_hexstr)
{
    int len = 0;
    switch (length_type)
    {
    case FIXED:
        return TXN_SUCCESS;
    case LLVAR:
        len = strlen(field_data_hex_str) / 2;
        if (len <= 99)
        {
            padd_left_with_zero_i(len, 2, length_hexstr);
            return TXN_SUCCESS;
        }
        logData("Invalid Length");
        break;
    case LLVARENC:
        len = strlen(field_data_hex_str);
        if (len <= 99)
        {
            padd_left_with_zero_i(len, 2, length_hexstr);
            return TXN_SUCCESS;
        }
        logData("Invalid Length");
        break;
    case TRACK_LEN:
        len = strlen(field_data_hex_str);
        if (len <= 99)
        {
            if (len > 48)
            {
                len = 48;
            }
            padd_left_with_zero_i(len, 2, length_hexstr);
            return TXN_SUCCESS;
        }
        logData("Invalid Length");
        break;
    case LLLVAR:
        len = strlen(field_data_hex_str) / 2;
        if (len <= 999)
        {
            padd_left_with_zero_i(len, 4, length_hexstr);
            return TXN_SUCCESS;
        }
        logData("Invalid Length");
        break;
    case BITMAP:
        logData("Field Length for BITMAP not implemented");
        break;
    case LLLLVAR:
        len = strlen(field_data_hex_str) / 2;
        if (len <= 9999)
        {
            padd_left_with_zero_i(len, 4, length_hexstr);
            return TXN_SUCCESS;
        }
        break;
    default:
        logData("Invalid Length type");
        break;
    }
    return TXN_PACK_FAILED;
}

ISO8583_ERROR_CODES get_total_msg_length(char *msg_hex_str, char **length_hexstr)
{
    int len = strlen(msg_hex_str) / 2;
    if (len <= 0x7FFF)
    {
        padd_left_with_zero_h(len, 4, length_hexstr);
        return TXN_SUCCESS;
    }
    return TXN_PACK_FAILED;
}

ISO8583_ERROR_CODES pad_field_data(ISO8583_PAD_TYPE pad_type, ISO8583_FIELD_SYNTAX syntax, ISO8583_PAD_METHOD pad_method, char pad_char, int max_length, char *field_data_hex_str, char **outstr)
{
    ISO8583_ERROR_CODES ret = TXN_PACK_FAILED;
    int len = 0;
    switch (pad_type)
    {
    case PAD_LEFT:
        switch (pad_method)
        {
        case PAD_FIXED_LENGTH:
            return padd_left_s(field_data_hex_str, syntax, max_length, pad_char, outstr) == TXN_SUCCESS ? TXN_SUCCESS : ret;
        case PAD_NO_METHOD:
        case PAD_NIBBLE:
            len = (strlen(field_data_hex_str) % 2) ? strlen(field_data_hex_str) + 1 : strlen(field_data_hex_str);
            return padd_left_s(field_data_hex_str, syntax, len, pad_char, outstr) == TXN_SUCCESS ? TXN_SUCCESS : ret;
        default:
            logData("Invalid PAD Method");
            break;
        }
        break;
    case PAD_NONE:
        if (strlen(field_data_hex_str) % 2)
        {
            logData("Invalid hex string field Data");
            break;
        }
        else
        {
            len = strlen(field_data_hex_str) + NULL_BYTE_LEN;
            *outstr = (char*)malloc(len);
            memcpy(*outstr, field_data_hex_str, len);
            return TXN_SUCCESS;
        }
    case PAD_RIGHT:
        switch (pad_method)
        {
        case PAD_FIXED_LENGTH:
            return padd_right_s(field_data_hex_str, syntax, max_length, pad_char, outstr) == TXN_SUCCESS ? TXN_SUCCESS : ret;
        case PAD_NO_METHOD:
        case PAD_NIBBLE:
            len = (strlen(field_data_hex_str) % 2) ? strlen(field_data_hex_str) + 1 : strlen(field_data_hex_str);
            return padd_right_s(field_data_hex_str, syntax, len, pad_char, outstr) == TXN_SUCCESS ? TXN_SUCCESS : ret;
        default:
            logData("Invalid PAD Method");
            break;
        }
        break;
    case MIXED_PAD:
        logData("Mixed PAD Padding Type not implemented");
        break;
    default:
        logData("Invalid PAD Type");
        break;
    }

    return TXN_PACK_FAILED;
}

ISO8583_ERROR_CODES copy_value(ISO8583_TXN_REQ_OBJECT *txn_obj, ISO8583_FIELD field, char *value)
{
    if (value == NULL)
    {
        logData("Value for the Field %d is NULL", field);
        return TXN_PACK_VALIDATION_FAILED;
    }

    if (strlen(value) > field_def[field].max_length)
    {
        logData("value size is exceeding maxlength of the field for the Field %d MaxLen: %d and Length : %d", field, field_def[field].max_length, (int)strlen(value));
        return TXN_PACK_VALIDATION_FAILED;
    }

    int len = get_hex_string_len(field_def[field].syntax, field_def[field].max_length) + NULL_BYTE_LEN;
    txn_obj->data[field].type = field_def[field].type;
    txn_obj->data[field].field_value.simple_field.field = field_def[field].field;
    txn_obj->data[field].field_value.simple_field.value = (unsigned char *)malloc(len);
    memcpy(txn_obj->data[field].field_value.simple_field.value, value, strlen(value));
    txn_obj->data[field].field_value.simple_field.value[strlen(value)] = '\0';
    txn_obj->data[field].field_value.simple_field.len = strlen(value);

    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES copy_tpdu(ISO8583_TXN_REQ_OBJECT *txn_obj)
{
    if (txn_obj == NULL)
    {
        logData("NULL Transaction Object Received");
        return TXN_PACK_FAILED;
    }

    if (txn_obj->message == NULL)
    {
        logData("NULL message buffer Received");
        return TXN_PACK_FAILED;
    }

    memcpy(txn_obj->message + txn_obj->len, static_data.TPDU, TPDU_LEN);
    txn_obj->len += TPDU_LEN;
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES copy_mti(ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn)
{
    if (txn_obj == NULL)
    {
        logData("NULL Transaction Object Received");
        return TXN_PACK_FAILED;
    }

    if (txn_obj->message == NULL)
    {
        logData("NULL message buffer Received");
        return TXN_PACK_FAILED;
    }

    if (txn >= END_TRANSACTION)
    {
        logData("Invalid Transaction Received");
        return TXN_PACK_FAILED;
    }

    memcpy(txn_obj->message + txn_obj->len, mti_def[txn].req_mti, strlen(mti_def[txn].req_mti));
    txn_obj->len += strlen(mti_def[txn].req_mti);
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES copy_bitmap(ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn)
{
    if (txn_obj == NULL)
    {
        logData("NULL Transaction Object Received");
        return TXN_PACK_FAILED;
    }

    if (txn_obj->message == NULL)
    {
        logData("NULL message buffer Received");
        return TXN_PACK_FAILED;
    }

    if (txn >= END_TRANSACTION)
    {
        logData("Invalid Transaction Received");
        return TXN_PACK_FAILED;
    }

    long lower_bm = bitmap_def[txn].req_field_1_32;
    long upper_bm = bitmap_def[txn].req_field_33_64;
    unsigned char bm[] = {(lower_bm & 0xFF000000) >> 24, (lower_bm & 0x00FF0000) >> 16, (lower_bm & 0x0000FF00) >> 8, lower_bm & 0x000000FF,
                          (upper_bm & 0xFF000000) >> 24, (upper_bm & 0x00FF0000) >> 16, (upper_bm & 0x0000FF00) >> 8, upper_bm & 0x000000FF};
    char *bitmap_c = NULL;
    bytearray_to_hexstr(bm, sizeof(bm), &bitmap_c);
    memcpy(txn_obj->message + txn_obj->len, bitmap_c, strlen(bitmap_c));
    txn_obj->len += strlen(bitmap_c);
    free(bitmap_c);
    return TXN_SUCCESS;
}

ISO8583_SUB_FIELD_VALUE *get_sub_field_value(ISO8583_COMPLEX_FIELD_VALUE *complex_val, ISO8583_SUB_FIELD sub_field)
{
    if (complex_val->count <= 0 || complex_val->value == NULL)
    {
        logData("Invalid complex value");
        return (ISO8583_SUB_FIELD_VALUE *)NULL;
    }

    for (int idx = 0; idx < complex_val->count; idx++)
    {
        if ((complex_val->value + idx)->field == sub_field)
            return complex_val->value + idx;
    }
    return (ISO8583_SUB_FIELD_VALUE *)NULL;
}

ISO8583_SUB_COMPONENT_VALUE *get_sub_component_value(ISO8583_SUB_FIELD_VALUE *sub_field_val, ISO8583_SUB_COMPONENT component)
{
    if (sub_field_val->count <= 0 || sub_field_val->value == NULL)
    {
        logData("Invalid Subfield value");
        return (ISO8583_SUB_COMPONENT_VALUE *)NULL;
    }

    for (int idx = 0; idx < sub_field_val->count; idx++)
    {
        if ((sub_field_val->value + idx)->field == component)
            return sub_field_val->value + idx;
    }
    return (ISO8583_SUB_COMPONENT_VALUE *)NULL;
}

ISO8583_ERROR_CODES pack_simple_field(ISO8583_TXN_REQ_OBJECT *txn_obj, ISO8583_FIELD field)
{
    ISO8583_ERROR_CODES ret = TXN_PACK_FAILED;
    if (txn_obj == NULL)
    {
        logData("Invalid Transaction Request object Received for packing Simple field");
        return ret;
    }
    if (txn_obj->data[field].type != SIMPLE)
    {
        logData("Invalid field type for Field : %d and Type : %d", field, txn_obj->data[field].type);
        return ret;
    }

    if (txn_obj->data[field].field_value.simple_field.value == NULL)
    {
        logData("NULL Value Received for packing field %d", field);
        return ret;
    }

    char *padded_value = NULL;
    char *padded_len = NULL;
    ISO8583_FIELD_DEF f_def = field_def[field];

    ret = pad_field_data(f_def.pad_type, f_def.syntax, f_def.pad_method, f_def.pad_char, f_def.max_length, (char*)txn_obj->data[field].field_value.simple_field.value, &padded_value);
    if (ret == TXN_SUCCESS && padded_value != NULL)
    {
        ret = get_field_length(f_def.length_type, padded_value, &padded_len);
        if (ret == TXN_SUCCESS)
        {
            if (padded_len != NULL)
            {
                txn_obj->len += sprintf((char*)txn_obj->message + txn_obj->len, "%s%s", padded_len, padded_value);
            }
            else
            {
                txn_obj->len += sprintf((char*)txn_obj->message + txn_obj->len, "%s", padded_value);
            }
            txn_obj->data[field].field_value.simple_field.len = sprintf((char*)txn_obj->data[field].field_value.simple_field.value, "%s", padded_value);
        }
        else
        {
            logData("Packing Field Length Failed");
            ret = TXN_PACK_FAILED;
        }
    }
    else
    {
        logData("Packing Field Data Failed");
        ret = TXN_PACK_FAILED;
    }

    if (padded_value)
        free(padded_value);
    if (padded_len)
        free(padded_len);

    return ret;
}

ISO8583_ERROR_CODES pack_complex_field(ISO8583_TXN_REQ_OBJECT *txn_obj, ISO8583_FIELD field)
{
    ISO8583_ERROR_CODES ret = TXN_PACK_FAILED;
    if (txn_obj == NULL)
    {
        logData("Invalid Transaction Request object Received for packing Complex field");
        return ret;
    }

    if (txn_obj->data[field].field_value.complex_field.value == NULL)
    {
        logData("NULL Sub Field Value Received");
        return ret;
    }

    if (txn_obj->data[field].type == COMPLEX)
    {
        logData("Invalid transaction object or field type");
        return ret;
    }

    char *padded_len = NULL;
    char *sc_value = NULL;
    ISO8583_FIELD_DEF f_def = field_def[field];
    
    txn_obj->data[field].field_value.complex_field.complex_value = (unsigned char *)malloc(get_hex_string_len(f_def.syntax,f_def.max_length) + NULL_BYTE_LEN);
    for (int i = 0; i < 10; i++) //Iterate Through Sub Fields
    {
        ISO8583_SUB_FIELD sub_field = f_def.sub_field[i];
        if (sub_field == NO_SUB_FIELD)
        {
            ret = TXN_SUCCESS;
            break;
        }
        ISO8583_SUB_FIELD_VALUE *current_sub_field_value = get_sub_field_value(&txn_obj->data[field].field_value.complex_field, sub_field);
        if (current_sub_field_value == NULL)
            continue;
        ISO8583_SUB_FIELD_DEF sub_f_def = sub_field_def[sub_field];
        current_sub_field_value->sf_value = (unsigned char *)malloc(sub_f_def.max_length + NULL_BYTE_LEN);
        for (int j = 0; j < 10; j++) //Iterate Through Sub Components
        {
            ISO8583_SUB_COMPONENT sub_comp = sub_f_def.sub_components[j];
            if (sub_comp == NO_SUB_COMPONENT)
            {
                ret = TXN_SUCCESS;
                break;
            }
            ISO8583_SUB_COMPONENT_DEF sub_c_def = sub_component_def[sub_comp];
            ISO8583_SUB_COMPONENT_VALUE *current_sub_comp_value = get_sub_component_value(current_sub_field_value, sub_comp);
            if (current_sub_field_value == NULL)
                continue;
            ret = pad_field_data(sub_c_def.pad_type, sub_c_def.syntax, sub_c_def.pad_method, sub_c_def.pad_char, sub_c_def.max_length, (char*)current_sub_comp_value->value, &sc_value);
            if (ret == TXN_SUCCESS && sc_value != NULL)
            {
                current_sub_field_value->sf_len += sprintf((char*)current_sub_field_value->sf_value + current_sub_field_value->sf_len, "%s", sc_value);
                free(sc_value);
                sc_value = NULL;
            }
        }
        current_sub_field_value->sf_value[current_sub_field_value->sf_len] = '\0';
        ret = get_field_length(sub_f_def.length_type, (char*)current_sub_field_value->sf_value, &padded_len);
        if (ret == TXN_SUCCESS && padded_len != NULL)
        {
            txn_obj->data[field].field_value.complex_field.comp_len += sprintf((char*)txn_obj->data[field].field_value.complex_field.complex_value + txn_obj->data[field].field_value.complex_field.comp_len, "%s%s", padded_len, current_sub_field_value->sf_value);
            if (padded_len)
            {
                free(padded_len);
                padded_len = NULL;
            }
        }
        else
        {
            logData("Packing Field Length Failed");
            ret = TXN_PACK_FAILED;
            break;
        }
    }
    txn_obj->len += sprintf((char*)txn_obj->message + txn_obj->len, "%s", txn_obj->data[field].field_value.complex_field.complex_value);
    ret = TXN_SUCCESS;

    if (padded_len)
        free(padded_len);
    if (sc_value)
        free(sc_value);

    return ret;
}

ISO8583_ERROR_CODES pack(ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn, unsigned char **final_msg, int *msg_len)
{
    ISO8583_ERROR_CODES ret = TXN_PACK_FAILED;
    if (bitmap_def[txn].transaction == txn)
    {
        txn_obj->message = (unsigned char *)malloc(1024);
        ret = copy_tpdu(txn_obj);
        if (ret != TXN_SUCCESS)
            return ret;
        ret = copy_mti(txn_obj, txn);
        if (ret != TXN_SUCCESS)
            return ret;
        ret = copy_bitmap(txn_obj, txn);
        if (ret != TXN_SUCCESS)
            return ret;

        logData("TXN : %d", txn);
        logData("Bitmap 1 : %02X", bitmap_def[txn].req_field_1_32);
        logData("Bitmap 2 : %02X", bitmap_def[txn].req_field_33_64);

        for (int i = 0; i < 2; i++)
        {
            long bitmap = (i == 0) ? bitmap_def[txn].req_field_1_32 : bitmap_def[txn].req_field_33_64;
            int length = (i == 0) ? 1 : 33;
            for (int j = 0; j < 32; j++)
            {
                if (bitmap & 0x80000000 >> j)
                {
                    if (txn_obj->data[j + length].type == SIMPLE)
                    {
                        ret = pack_simple_field(txn_obj, (ISO8583_FIELD)j + length);
                    }
                    else
                    {
                        ret = pack_complex_field(txn_obj, (ISO8583_FIELD)j + length);
                    }

                    if (ret != TXN_SUCCESS)
                    {
                        logData("Packing failed for field: %d", j + length);
                        return ret;
                    }
                }
            }
        }

        char *total_msg_length = NULL;
        ret = get_total_msg_length((char*)txn_obj->message, &total_msg_length);
        if (ret != TXN_SUCCESS)
        {
            logData("Calculating Total Message length failed");
            ret = TXN_PACK_FAILED;
        }
        char *msg = (char *)malloc(txn_obj->len + LENGTH_OF_MSG_LEN + 1); // TODO Check 
        sprintf(msg, "%s%s", total_msg_length, txn_obj->message);
        logData("Total message : %s", msg);

        if (total_msg_length)
            free(total_msg_length);

        ret = hexstr_to_bytearray(msg, final_msg, msg_len);
        if (ret != TXN_SUCCESS)
        {
            if (*final_msg)
            {
                free(*final_msg);
                final_msg = NULL;
            }
            if (msg)
                free(msg);
            *msg_len = 0;
            logData("Failed to Pack the final Message");
            ret = TXN_PACK_FAILED;
        }

        if (msg)
            free(msg);
    }
    else
        logData("Invalid Transaction for Packing");

    return ret;
}

ISO8583_ERROR_CODES parse_simple_field(ISO8583_TXN_RESP_OBJECT *resp_txn_obj, ISO8583_FIELD field, ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn, char **buffer, int *len)
{
    ISO8583_ERROR_CODES ret = TXN_PARSE_FAILED;
    if (*buffer == NULL)
    {
        logData("NULL Buffer received for parsing simple field : %d", field);
        return ret;
    }

    if (*len <= 0)
    {
        logData("Invalid Buffer length received for parsing simple field : %d", field);
        return ret;
    }

    int length = 0;

    switch (field_def[field].length_type)
    {

    case FIXED:
        length = get_hex_string_len(field_def[field].syntax, field_def[field].max_length);
        resp_txn_obj->data[field].field_value.simple_field.value = (unsigned char *)malloc(length + NULL_BYTE_LEN);
        memcpy(resp_txn_obj->data[field].field_value.simple_field.value, *buffer, length);
        resp_txn_obj->data[field].field_value.simple_field.value[length] = '\0';
        resp_txn_obj->data[field].field_value.simple_field.len = length;
        *buffer = *buffer + length;
        *len = *len - length;
        break;
    case LLVAR:
    case LLVARENC:
        length = hexstr_to_decimal(*buffer, 2);
        length = get_parser_hex_string_len(field_def[field].syntax, length);
        *buffer = *buffer + 2;
        *len = *len - 2;
        if (length > get_parser_hex_string_len(field_def[field].syntax, field_def[field].max_length))
        {
            logData("Invalid length for variable length in Response Message for field %d.", field);
            return ret;
        }
        resp_txn_obj->data[field].field_value.simple_field.value = (unsigned char *)malloc(length + NULL_BYTE_LEN);
        memcpy(resp_txn_obj->data[field].field_value.simple_field.value, *buffer, length);
        resp_txn_obj->data[field].field_value.simple_field.value[length] = '\0';
        resp_txn_obj->data[field].field_value.simple_field.len = length;
        *buffer = *buffer + length;
        *len = *len - length;
        break;
    case LLLVAR:
    case LLLLVAR:
        length = hexstr_to_decimal(*buffer, 4);
        length = get_parser_hex_string_len(field_def[field].syntax, length);
        logData("Length : %d", length);
        *buffer = *buffer + 4;
        *len = *len - 4;
        if (length > get_parser_hex_string_len(field_def[field].syntax, field_def[field].max_length))
        {
            logData("Invalid length for variable length in Response Message for field %d.", field);
            return ret;
        }
        resp_txn_obj->data[field].field_value.simple_field.value = (unsigned char *)malloc(length + NULL_BYTE_LEN);
        memcpy(resp_txn_obj->data[field].field_value.simple_field.value, *buffer, length);
        resp_txn_obj->data[field].field_value.simple_field.value[length] = '\0';
        resp_txn_obj->data[field].field_value.simple_field.len = length;
        *buffer = *buffer + length;
        *len = *len - length;
        break;
    default:
        logData("invalid length type for field %d", field);
        break;
    }

    //Validate Field:
    switch (field)
    {

    case DE03_PROCESSING_CODE:
        if (strncmp(pro_codes_def[txn].resp_proc_code, (const char*)resp_txn_obj->data[field].field_value.simple_field.value, strlen((const char*)resp_txn_obj->data[field].field_value.simple_field.value)))
        {
            logData("Processing Code Validation Failed");
            return TXN_PACK_VALIDATION_FAILED;
        }
        break;
    case DE11_STAN:
        if (strncmp((const char*)txn_obj->data[field].field_value.simple_field.value, (const char*)resp_txn_obj->data[field].field_value.simple_field.value, strlen((const char*)resp_txn_obj->data[field].field_value.simple_field.value)))
        {
            logData("STAN Validation Failed");
            return TXN_PACK_VALIDATION_FAILED;
        }
        break;

    case DE39_RESPONSE_CODE:
        if (strncmp((const char*)resp_txn_obj->data[field].field_value.simple_field.value, RESPONSE_CODE_SUCCESS, strlen(RESPONSE_CODE_SUCCESS)))
        {
            logData("RESPONSE_CODE Validation Failed");
            // return TXN_PACK_VALIDATION_FAILED;
        }
        break;

    case DE41_CARD_ACCEPTOR_TERMINAL_ID:
        if (strncmp((const char*)txn_obj->data[field].field_value.simple_field.value, (const char*)resp_txn_obj->data[field].field_value.simple_field.value, resp_txn_obj->data[field].field_value.simple_field.len))
        {
            logData("TERMINAL_ID Validation Failed");
            return TXN_PACK_VALIDATION_FAILED;
        }
        break;

    default:
        break;
    }

    return TXN_SUCCESS;
}

// int parse_sub_field(ISO8583_SUB_FIELD_VALUE *req_sub_field_value, ISO8583_SUB_FIELD field, ISO8583_SUB_FIELD_VALUE *resp_sub_field, unsigned char **buffer, int *len)
// {
//     if (*buffer == NULL)
//     {
//         logData("NULL Buffer received for parsing Sub field : %d", field);
//         return TXN_FAILED;
//     }

//     if (*len <= 0)
//     {
//         logData("Invalid Buffer length received for parsing Sub field : %d", field);
//         return TXN_FAILED;
//     }
//     switch (sub_field_def[field].length_type)
//     {

//     case FIXED:
//         resp_sub_field->value = (unsigned char *)malloc(sub_field_def[field].max_length);
//         memcpy(resp_sub_field->value, *buffer, sub_field_def[field].max_length);
//         resp_sub_field->len = sub_field_def[field].max_length;
//         *buffer = *buffer + sub_field_def[field].max_length;
//         *len = *len - sub_field_def[field].max_length;
//         break;
//     case LLVAR:
//         int length = *buffer[0];
//         *buffer = *buffer + 1;
//         *len = *len - 1;
//         length = ((length & 0xF0) >> 4) * 10 + (length & 0x0F);
//         if (length > sub_field_def[field].max_length)
//         {
//             logData("Invalid length for variable length in Response Message for sub field %d.", field);
//             return TXN_FAILED;
//         }
//         resp_sub_field->value = (unsigned char *)malloc(length);
//         memcpy(resp_sub_field->value, *buffer, length);
//         resp_sub_field->len = length;
//         *buffer = *buffer + length;
//         *len = *len - length;
//         break;
//     case LLLVAR:
//     case LLLLVAR:
//         int length = *buffer[0] << 8 | *buffer[1];
//         *buffer = *buffer + 2;
//         *len = *len - 2;
//         length = ((length & 0xF000) >> 12) * 1000 + ((length & 0x0F00) >> 8) * 100 + ((length & 0x00F0) >> 4) * 10 + (length & 0x000F);
//         if (length > sub_field_def[field].max_length)
//         {
//             logData("Invalid length for variable length in Response Message for sub field %d.", field);
//             return TXN_FAILED;
//         }
//         resp_sub_field->value = (unsigned char *)malloc(length);
//         memcpy(resp_sub_field->value, *buffer, length);
//         resp_sub_field->len = length;
//         *buffer = *buffer + length;
//         *len = *len - length;
//         break;
//     default:
//         logData("invalid length type for sub field %d", field);
//         break;
//     }

//     if (req_sub_field_value == NULL)
//     {
//         logData("Request Sub Field Value is NULL, Skipping Sub Field validation.");
//         return TXN_SUCCESS;
//     }
//     //Validate Sub Field:
//     // unsigned char* value = NULL;
//     // switch (field)
//     // {
//     // case DE63_FUND_SOURCE_TYPE:
//     //     bytearray_to_hexstr(resp_txn_obj->data[field].field_value.simple_field.value, resp_txn_obj->data[field].field_value.simple_field.len, &value);
//     //     if (strncmp(txn_obj->data[DE11_STAN].field_value.simple_field.value, value, strlen(value)))
//     //     {
//     //         logData("STAN Validation Failed");
//     //         return TXN_FAILED;
//     //     }
//     //     if (value)
//     //     {
//     //         free(value);
//     //         value = NULL;
//     //     }
//     //     break;

//     // case DE39_RESPONSE_CODE:
//     //     if (strncmp(resp_txn_obj->data[field].field_value.simple_field.value, "00", strlen("00")))
//     //     {
//     //         logData("RESPONSE_CODE Validation Failed");
//     //         return TXN_FAILED;
//     //     }
//     //     break;

//     // case DE41_CARD_ACCEPTOR_TERMINAL_ID:
//     //     if (strncmp(txn_obj->data[DE11_STAN].field_value.simple_field.value, resp_txn_obj->data[field].field_value.simple_field.value, resp_txn_obj->data[field].field_value.simple_field.len))
//     //     {
//     //         logData("TERMINAL_ID Validation Failed");
//     //         return TXN_FAILED;
//     //     }
//     //     break;

//     // default:
//     //     break;
//     // }
//     return TXN_SUCCESS;
// }

// int parse_complex_field(ISO8583_TXN_RESP_OBJECT *resp_txn_obj, ISO8583_FIELD field, ISO8583_TXN_REQ_OBJECT *txn_obj, unsigned char **buffer, int *len)
// {
//     if (*buffer == NULL)
//     {
//         logData("NULL Buffer received for parsing Complex field : %d", field);
//         return TXN_FAILED;
//     }

//     if (*len <= 0)
//     {
//         logData("Invalid Buffer length received for parsing Complex field : %d", field);
//         return TXN_FAILED;
//     }
//     switch (field_def[field].length_type)
//     {

//     case FIXED:
//         resp_txn_obj->data[field].field_value.complex_field.complex_value = (unsigned char *)malloc(field_def[field].max_length);
//         memcpy(resp_txn_obj->data[field].field_value.complex_field.complex_value, *buffer, field_def[field].max_length);
//         resp_txn_obj->data[field].field_value.complex_field.comp_len = field_def[field].max_length;
//         *buffer = *buffer + field_def[field].max_length;
//         *len = *len - field_def[field].max_length;
//         break;
//     case LLVAR:
//         int length = *buffer[0];
//         *buffer = *buffer + 1;
//         *len = *len - 1;
//         length = ((length & 0xF0) >> 4) * 10 + (length & 0x0F);
//         if (length > field_def[field].max_length)
//         {
//             logData("Invalid length for variable length in Response Message for field %d.", field);
//             return TXN_FAILED;
//         }
//         resp_txn_obj->data[field].field_value.complex_field.complex_value = (unsigned char *)malloc(length);
//         memcpy(resp_txn_obj->data[field].field_value.complex_field.complex_value, *buffer, length);
//         resp_txn_obj->data[field].field_value.complex_field.comp_len = length;
//         *buffer = *buffer + length;
//         *len = *len - length;
//         break;
//     case LLLVAR:
//     case LLLLVAR:
//         int length = *buffer[0] << 8 | *buffer[1];
//         *buffer = *buffer + 2;
//         *len = *len - 2;
//         length = ((length & 0xF000) >> 12) * 1000 + ((length & 0x0F00) >> 8) * 100 + ((length & 0x00F0) >> 4) * 10 + (length & 0x000F);
//         if (length > field_def[field].max_length)
//         {
//             logData("Invalid length for variable length in Response Message for field %d.", field);
//             return TXN_FAILED;
//         }
//         resp_txn_obj->data[field].field_value.complex_field.complex_value = (unsigned char *)malloc(length);
//         memcpy(resp_txn_obj->data[field].field_value.complex_field.complex_value, *buffer, length);
//         resp_txn_obj->data[field].field_value.complex_field.comp_len = length;
//         *buffer = *buffer + length;
//         *len = *len - length;
//         break;
//     default:
//         logData("invalid length type for field %d", field);
//         break;
//     }

//     //Parse Subfields:
//     unsigned char *temp_buffer = resp_txn_obj->data[field].field_value.complex_field.complex_value;
//     int temp_len = resp_txn_obj->data[field].field_value.complex_field.comp_len;
//     for (int i = 0; i < 10; i++)
//     {
//         if (field_def[field].sub_field[i] == NULL)
//             break;
//         int count = 0;
//         for (; field_def[field].sub_field[i][count] != NO_SUB_FIELD; count++)
//             ;
//         resp_txn_obj->data[field].field_value.complex_field.value = (ISO8583_SUB_FIELD_VALUE *)malloc(count * sizeof(ISO8583_SUB_FIELD_VALUE));
//         for (int j = 0; j < count; j++)
//         {
//             if (field_def[field].sub_field[i][j] == NO_SUB_FIELD)
//                 break;
//             (resp_txn_obj->data[field].field_value.complex_field.value + j)->field = field_def[field].sub_field[i][j];
//             int ret = parse_sub_field(NULL, field_def[field].sub_field[i][j], resp_txn_obj->data[field].field_value.complex_field.value + j, &temp_buffer, &temp_len);
//             if (ret != TXN_SUCCESS)
//             {
//             }
//         }
//     }
//     return TXN_SUCCESS;
// }

ISO8583_ERROR_CODES parse(unsigned char *response, int response_len, ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn, ISO8583_TXN_RESP_OBJECT **resp)
{
    ISO8583_ERROR_CODES ret = TXN_PARSE_FAILED;
    if (response == NULL)
    {
        logData("NULL Response Buffer Received for parsing");
        return ret;
    }

    char* hex_response = NULL;
    int hex_response_len = 2 * response_len;
    bytearray_to_hexstr(response, response_len, &hex_response);

    if (hex_response_len < LENGTH_OF_MSG_LEN)
    {
        logData("Invalid Response length Received for parsing");
        if (hex_response)
            free(hex_response);
        return ret;
    }

    int len = hexstr_to_long(hex_response, LENGTH_OF_MSG_LEN);
    if (len * 2 != hex_response_len - LENGTH_OF_MSG_LEN)
    {
        logData("Corrupted Response Data Received for parsing");
        if (hex_response)
            free(hex_response);
        return ret;
    }

    len = strlen(mti_def[txn].resp_mti);
    if (strncmp(hex_response + 14, mti_def[txn].resp_mti, len))
    {
        logData("Invalid MTI received in the response");
        if (hex_response)
            free(hex_response);
        return TXN_PARSE_VALIDATION_FAILED;
    }

    ISO8583_TXN_RESP_OBJECT *resp_obj = (ISO8583_TXN_RESP_OBJECT *)get_transaction_object();
    //Copy MTI:
    resp_obj->data[DE00_MTI].field_value.simple_field.value = (unsigned char *)malloc(len + NULL_BYTE_LEN);
    resp_obj->data[DE00_MTI].field_value.simple_field.len = len;
    memcpy(resp_obj->data[DE00_MTI].field_value.simple_field.value, hex_response + 14, len);
    resp_obj->data[DE00_MTI].field_value.simple_field.value[len] = '\0';

    //Copy BITMAP:
    len = get_hex_string_len(field_def[DE01_BITMAP].syntax, field_def[DE01_BITMAP].max_length);
    resp_obj->data[DE01_BITMAP].field_value.simple_field.value = (unsigned char *)malloc(len + NULL_BYTE_LEN);
    resp_obj->data[DE01_BITMAP].field_value.simple_field.len = len;
    memcpy(resp_obj->data[DE01_BITMAP].field_value.simple_field.value, hex_response + 18, len);
    resp_obj->data[DE01_BITMAP].field_value.simple_field.value[len] = '\0';
    
    char *resp_bm = (char*)resp_obj->data[DE01_BITMAP].field_value.simple_field.value;

    char *temp_response = hex_response + 34;
    int temp_response_len = hex_response_len - 34;

    for (int i = 0; i < 2; i++)
    {
        long bitmap = (i == 0) ? bitmap_def[txn].res_field_1_32 : bitmap_def[txn].res_field_33_64;
        long resp_bitmap = (i == 0) ?  hexstr_to_long(resp_bm, 8): hexstr_to_long(resp_bm + 8, 8);
        int length = (i == 0) ? 1 : 33;
        for (int j = 0; j < 32; j++)
        {
            if (bitmap & 0x80000000 >> j)
            {
                if (resp_bitmap & 0x80000000 >> j)
                {
                    if (field_def[length + j].type == SIMPLE)
                    {
                        int ret = parse_simple_field(resp_obj, field_def[length + j].field, txn_obj, txn, &temp_response, &temp_response_len);
                        if (ret != TXN_SUCCESS)
                        {
                            logData("Failed to parse field %d", length + j);
                            if (resp_obj)
                            {
                                clear_transaction_object(resp_obj);
                            }
                            if (hex_response)
                                free(hex_response);
                            return ret;
                        }
                    }
                    else
                    {
                        // ret = parse_complex_field(resp_obj, field_def[length + j].field, txn_obj, txn, &temp_response, &temp_response_len);
                        // if (ret != TXN_SUCCESS)
                        // {
                        //     logData("Failed to parse field %d", length + j);
                        //     if (resp_obj)
                        //     {
                        //         clear_transaction_object(resp_obj);
                        //     }
                        //     return (ISO8583_TXN_RESP_OBJECT *)NULL;
                        // }
                        logData("Complex Field parsing Not supported");
                        if (resp_obj)
                        {
                            clear_transaction_object(resp_obj);
                        }
                        if (hex_response)
                            free(hex_response);
                        return TXN_PARSE_FAILED;
                    }
                }
                else
                {
                    logData("Field %d is not found in the response", length + j);
                }
            }
        }
    }
    *resp = resp_obj;
    if (hex_response)
        free(hex_response);
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES process_transaction(ISO8583_TXN_REQ_OBJECT *txn_obj, TRANSACTION txn, ISO8583_TXN_RESP_OBJECT** resp)
{
    if (txn >= END_TRANSACTION){
        logData("Invalid Transaction Type: %d", txn);
        return TXN_FAILED;
    }

    unsigned char *request = NULL;
    int request_len = 0;
    unsigned char *response = NULL;
    int response_len = 0;

    ISO8583_ERROR_CODES ret = pack(txn_obj, txn, &request, &request_len);
    if (ret != TXN_SUCCESS)
    {
        logData("Failed To pack the message for Transaction : %d", txn);
        if (request)
            free(request);
        return ret;
    }

    ret = process_with_host(static_data.HOST_IP_ADDRESS, static_data.HOST_PORT, static_data.TRANSACTION_TIMOUT, request, request_len, &response, &response_len);
    if (ret != TXN_SUCCESS)
    {
        logData("Failed To process transaction to host for Transaction : %d", txn);
        if (request)
            free(request);
        if (response)
            free(response);
        return ret;
    }

    ret = parse(response, response_len, txn_obj, txn, resp);
    if (ret != TXN_SUCCESS)
    {
        logData("Failed To parse response for Transaction : %d", txn);
        if (request)
            free(request);
        if (response)
            free(response);
        return ret;
    }

    if (request)
        free(request);
    if (response)
        free(response);
    return ret;
}
