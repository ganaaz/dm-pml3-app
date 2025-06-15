#include <stdlib.h>
#include <string.h>

#include "ISO8583_def.h"
#include "log.h"
#include "utils.h"
#include "../include/logutil.h"

ISO8583_ERROR_CODES construct_transaction_request_object_for_balance_update(ISO8583_TXN_REQ_OBJECT *req_txn_obj, BALANCE_UPDATE_REQUEST *txn_obj)
{
    int txn = MONEY_LOAD_BALANCE_UPDATE;
    ISO8583_ERROR_CODES ret = TXN_FAILED;

    for (int i = 0; i < 2; i++)
    {
        long bitmap = (i == 0) ? bitmap_def[txn].req_field_1_32 : bitmap_def[txn].req_field_33_64;
        int length = (i == 0) ? 1 : 33;
        for (int j = 0; j < 32; j++)
        {
            if (bitmap & 0x80000000 >> j)
            {
                switch (length + j)
                {

                case DE03_PROCESSING_CODE:
                    ret = copy_value(req_txn_obj, DE03_PROCESSING_CODE, pro_codes_def[txn].req_proc_code);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE04_AMOUNT_TRANSACTION:
                    ret = copy_value(req_txn_obj, DE04_AMOUNT_TRANSACTION, txn_obj->DE04_TXN_AMOUNT);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE11_STAN:
                    ret = copy_value(req_txn_obj, DE11_STAN, txn_obj->DE11_STAN);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE22_POS_ENTRY_MODE:
                    ret = copy_value(req_txn_obj, DE22_POS_ENTRY_MODE, static_data.DE22_POS_ENTRY_MODE);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE24_NII:
                    ret = copy_value(req_txn_obj, DE24_NII, static_data.DE24_NII);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE25_POS_CONDITION_CODE:
                    ret = copy_value(req_txn_obj, DE25_POS_CONDITION_CODE, static_data.DE25_POS_CONDITION_CODE);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE35_TRACK_2_DATA:
                    ret = copy_value(req_txn_obj, DE35_TRACK_2_DATA, txn_obj->DE35_TRACK_2_DATA);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE41_CARD_ACCEPTOR_TERMINAL_ID:
                    ret = copy_value(req_txn_obj, DE41_CARD_ACCEPTOR_TERMINAL_ID, static_data.DE41_TERMINAL_ID);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE42_CARD_ACCEPTOR_IDENTIFICATION_CODE:
                    ret = copy_value(req_txn_obj, DE42_CARD_ACCEPTOR_IDENTIFICATION_CODE, static_data.DE42_CARD_ACCEPTOR_ID);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE55_RESERVED_ISO_1:
                    ret = copy_value(req_txn_obj, DE55_RESERVED_ISO_1, txn_obj->DE55_ICC_DATA.value);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE53_SECURITY_RELATED_CONTROL_INFO:
                    ret = copy_value(req_txn_obj, DE53_SECURITY_RELATED_CONTROL_INFO, txn_obj->DE53_SECURITY_DATA);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE56_RESERVED_ISO_2:
                    ret = copy_value(req_txn_obj, DE56_RESERVED_ISO_2, txn_obj->DE56_BATCH_NUMBER);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE62_RESERVED_PRIVATE_2:
                    ret = copy_value(req_txn_obj, DE62_RESERVED_PRIVATE_2, txn_obj->DE62_INVOICE_NUMBER);
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                case DE63_RESERVED_PRIVATE_3:
                    ret = copy_value(req_txn_obj, DE63_RESERVED_PRIVATE_3, "03");
                    if (ret != TXN_SUCCESS)
                        return ret;
                    break;

                default:
                    logData("No Case for Field : %d", length + j);
                    break;
                }
            }
        }
    }
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES construct_transaction_response_object_for_balance_update(BALANCE_UPDATE_RESPONSE *resp_obj, ISO8583_TXN_RESP_OBJECT *resp_txn_obj)
{
    if (resp_txn_obj->data[1].field_value.simple_field.value == NULL)
    {
        logData("BITMAP is not present in the Response");
        return TXN_PARSE_VALIDATION_FAILED;
    }

    int txn = MONEY_LOAD_BALANCE_UPDATE;
    char *resp_bm = (char *)resp_txn_obj->data[DE01_BITMAP].field_value.simple_field.value;
    unsigned char *value = NULL;
    int len = 0;

    for (int i = 0; i < 2; i++)
    {
        long bitmap = (i == 0) ? bitmap_def[txn].res_field_1_32 : bitmap_def[txn].res_field_33_64;
        long resp_bitmap = (i == 0) ? hexstr_to_long(resp_bm, 8) : hexstr_to_long(resp_bm + 8, 8);
        int length = (i == 0) ? 1 : 33;
        for (int j = 0; j < 32; j++)
        {
            if (bitmap & 0x80000000 >> j)
            {
                if (resp_bitmap & 0x80000000 >> j)
                {
                    switch (length + j)
                    {
                    case DE04_AMOUNT_TRANSACTION:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            memcpy(resp_obj->DE04_TXN_AMOUNT, value, len + NULL_BYTE_LEN);
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    case DE11_STAN:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            memcpy(resp_obj->DE11_STAN, value, len + NULL_BYTE_LEN);
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    case DE12_TRANSACTION_TIME_LOCAL:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            memcpy(resp_obj->DE12_TXN_TIME, value, len + NULL_BYTE_LEN);
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    case DE13_TRANSACTION_DATE_LOCAL:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            memcpy(resp_obj->DE13_TXN_DATE, value, len + NULL_BYTE_LEN);
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    case DE37_RRN:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            char *rrn = NULL;
                            int rrn_len = 0;
                            int ret = hexstr_to_bytearray((char *)value, (unsigned char **)&rrn, &rrn_len);
                            if (ret == TXN_SUCCESS && rrn != NULL)
                            {
                                memcpy(resp_obj->DE37_RRN, rrn, rrn_len);
                                resp_obj->DE37_RRN[rrn_len] = '\0';
                            }
                            if (rrn)
                                free(rrn);
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    case DE38_AUTHORIZATION_IDENTIFICATION_RESPONSE:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            char *auth_code = NULL;
                            int auth_code_len = 0;
                            int ret = hexstr_to_bytearray((char *)value, (unsigned char **)&auth_code, &auth_code_len);
                            if (ret == TXN_SUCCESS && auth_code != NULL)
                            {
                                memcpy(resp_obj->DE38_AUTH_CODE, auth_code, auth_code_len);
                                resp_obj->DE38_AUTH_CODE[auth_code_len] = '\0';
                            }
                            if (auth_code)
                                free(auth_code);
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    case DE39_RESPONSE_CODE:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            char *resp_code = NULL;
                            int resp_code_len = 0;
                            int ret = hexstr_to_bytearray((char *)value, (unsigned char **)&resp_code, &resp_code_len);
                            if (ret == TXN_SUCCESS && resp_code != NULL)
                            {
                                memcpy(resp_obj->DE39_RESPONSE_CODE, resp_code, resp_code_len);
                                resp_obj->DE39_RESPONSE_CODE[resp_code_len] = '\0';
                            }
                            if (resp_code)
                                free(resp_code);
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    case DE55_RESERVED_ISO_1:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            resp_obj->DE55_ICC_DATA.value = (char *)malloc(len + NULL_BYTE_LEN);
                            memcpy(resp_obj->DE55_ICC_DATA.value, value, len + NULL_BYTE_LEN);
                            resp_obj->DE55_ICC_DATA.len = len;
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    case DE63_RESERVED_PRIVATE_3:
                        value = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.value : resp_txn_obj->data[length + j].field_value.complex_field.complex_value;
                        len = (resp_txn_obj->data[length + j].type == SIMPLE) ? resp_txn_obj->data[length + j].field_value.simple_field.len : resp_txn_obj->data[length + j].field_value.complex_field.comp_len;
                        if ((value != NULL) && (len <= get_hex_string_len(field_def[length + j].syntax, field_def[length + j].max_length)))
                        {
                            char bal_update_fund_src_type[5] = "3033";
                            if (strncmp((const char *)value, bal_update_fund_src_type, strlen(bal_update_fund_src_type)))
                            {
                                logData("Invalid Fund Source Type");
                                return TXN_PARSE_VALIDATION_FAILED;
                            }
                        }
                        else
                            logData("Failed to copy the field %d into balance update response buffer", length + j);
                        break;

                    default:
                        // logData("No Case for Field : %d", length + j);
                        break;
                    }
                }
                else
                {
                    logData("Field %d is not in the response", length + j);
                }
            }
        }
    }
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES process_balance_update_transaction(BALANCE_UPDATE_REQUEST *txn_obj, BALANCE_UPDATE_RESPONSE *resp)
{
    if (txn_obj == NULL)
    {
        logData("NULL BALANCE_UPDATE_REQUEST Object Passed");
        return TXN_FAILED;
    }

    ISO8583_ERROR_CODES ret = TXN_SUCCESS;
    ISO8583_TXN_REQ_OBJECT *req_txn_obj = get_transaction_object();

    memset(resp, 0x00, sizeof(BALANCE_UPDATE_RESPONSE));

    ret = construct_transaction_request_object_for_balance_update(req_txn_obj, txn_obj);
    if (ret != TXN_SUCCESS)
    {
        if (req_txn_obj)
        {
            clear_transaction_object(req_txn_obj);
        }
        logData("Failed to construct the Transaction Object for balance update");
        return ret;
    }

    ISO8583_TXN_RESP_OBJECT *resp_txn_obj = NULL;
    ret = process_transaction(req_txn_obj, MONEY_LOAD_BALANCE_UPDATE, &resp_txn_obj);
    if (ret != TXN_SUCCESS || resp_txn_obj == NULL)
    {
        if (req_txn_obj)
        {
            clear_transaction_object(req_txn_obj);
        }
        logData("Failed to Process Transaction for balance update");
        return ret != TXN_SUCCESS ? ret : TXN_FAILED;
    }

    ret = construct_transaction_response_object_for_balance_update(resp, resp_txn_obj);
    if (ret != TXN_SUCCESS)
    {
        if (req_txn_obj)
            clear_transaction_object(req_txn_obj);
        if (resp_txn_obj)
            clear_transaction_object(resp_txn_obj);

        logData("Failed to construct the BALANCE_UPDATE_RESPONSE Object for balance update");
        return ret;
    }

    if (req_txn_obj)
        clear_transaction_object(req_txn_obj);
    if (resp_txn_obj)
        clear_transaction_object(resp_txn_obj);

    return ret;
}