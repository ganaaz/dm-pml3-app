#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "log.h"
#include "ISO8583_def.h"
#include "../include/logutil.h"

int hexstr_to_decimal(char *hexstr, int size)
{
    if (size > 4)
        return TXN_FAILED;

    char *len = (char *)malloc(size + NULL_BYTE_LEN);
    strncpy(len, hexstr, size);
    len[size] = '\0';
    int length = strtod(len, NULL);
    if (len)
        free(len);
    return length;
}

long hexstr_to_long(char *hexstr, int size)
{
    if (size > 8)
        return TXN_FAILED;

    char *len = (char *)malloc(size + NULL_BYTE_LEN);
    strncpy(len, hexstr, size);
    len[size] = '\0';
    long length = strtol(len, NULL, 16);
    if (len)
        free(len);
    return length;
}

int get_hex_string_len(ISO8583_FIELD_SYNTAX syntax, int max_length)
{
    switch (syntax)
    {
    case NUMBER:
    case HEX:
    case TRACK:
        return (max_length % 2) ? max_length + 1 : max_length;
    case ALPHA:
    case SYMBOL:
    case ALPHA_NUMERIC:
    case ALPHA_SYMBOL:
    case NUMBER_SYMBOL:
    case ALPHA_NUMERIC_SYMBOL:
        return max_length * 2;
    case BLACK_LIST_RECORD:
    case X_NUMERIC:
    case ALPHA_OR_NUMERIC:
        return 0;
    default:
        logData("Invalid Syntax");
        return 0;
    }
}

int get_parser_hex_string_len(ISO8583_FIELD_SYNTAX syntax, int len)
{
    switch (syntax)
    {
    case NUMBER:
        return len;
    case ALPHA:
    case HEX:
    case SYMBOL:
    case ALPHA_NUMERIC:
    case ALPHA_SYMBOL:
    case NUMBER_SYMBOL:
    case ALPHA_NUMERIC_SYMBOL:
        return len * 2;
    case BLACK_LIST_RECORD:
    case X_NUMERIC:
    case ALPHA_OR_NUMERIC:
        return 0;
    default:
        logData("Invalid Syntax");
        return 0;
    }
}

ISO8583_ERROR_CODES convert_value(ISO8583_FIELD_SYNTAX syntax, char *value, char **outstr)
{
    switch (syntax)
    {
    case NUMBER:
    case HEX:
    case TRACK:
    case BLACK_LIST_RECORD:
    case X_NUMERIC:
    case ALPHA_OR_NUMERIC:
        *outstr = (char *)malloc(strlen(value) + NULL_BYTE_LEN);
        memcpy(*outstr, value, strlen(value) + NULL_BYTE_LEN);
        break;
    case ALPHA:
    case SYMBOL:
    case ALPHA_NUMERIC:
    case ALPHA_SYMBOL:
    case NUMBER_SYMBOL:
    case ALPHA_NUMERIC_SYMBOL:
        bytearray_to_hexstr((unsigned char *)value, strlen(value), outstr);
        break;
    // case TRACK:
    //     if (strlen(value) % 2 )
    //         {
    //             int length = strlen(value) + 2;
    //             char pad_str[length];
    //             memset(pad_str, '0', length);
    //             pad_str[length] = '\0';

    //             *outstr = (char *)malloc(strlen(value) + NULL_BYTE_LEN);
    //             sprintf(*outstr, "%s%s", pad_str, value);
    //         }
    //     break;
    default:
        logData("Invalid Syntax");
        break;
        ;
    }
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES padd_left_s(char *hexstr, ISO8583_FIELD_SYNTAX syntax, int padlength, char pad_char, char **outstr)
{
    int length = padlength - strlen(hexstr);
    if (length < 0)
    {
        logData("Invalid Pad Length");
        return TXN_FAILED;
    }

    char pad_str[length + 1];
    memset(pad_str, pad_char, length);
    pad_str[length] = '\0';

    char *tempstr = (char *)malloc(padlength + 1);
    sprintf(tempstr, "%s%s", pad_str, hexstr);

    convert_value(syntax, tempstr, outstr);
    length = get_hex_string_len(syntax, padlength);

    (*outstr)[length] = '\0';

    if (tempstr)
        free(tempstr);

    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES padd_right_s(char *hexstr, ISO8583_FIELD_SYNTAX syntax, int padlength, char pad_char, char **outstr)
{
    int length = padlength - strlen(hexstr);
    if (length < 0)
    {
        logData("Invalid Pad Length : %d hexstring Length %d", padlength, (int)strlen(hexstr));
        return TXN_FAILED;
    }

    char pad_str[length + 1];
    memset(pad_str, pad_char, length);
    pad_str[length] = '\0';

    char *tempstr = (char *)malloc(padlength + 1);
    sprintf(tempstr, "%s%s", hexstr, pad_str);

    convert_value(syntax, tempstr, outstr);
    length = get_hex_string_len(syntax, padlength);
    (*outstr)[length] = '\0';

    if (tempstr)
        free(tempstr);

    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES padd_left_with_zero_i(int val, int padlength, char **outstr)
{
    *outstr = (char *)malloc(padlength + 1);
    sprintf(*outstr, "%0*d", padlength, val);
    (*outstr)[padlength] = '\0';
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES padd_left_with_zero_h(int val, int padlength, char **outstr)
{
    *outstr = (char *)malloc(padlength + 1);
    sprintf(*outstr, "%0*X", padlength, val);
    (*outstr)[padlength] = '\0';
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES is_hex_str(char *str)
{
    int length = strlen(str);
    for (int i = 0; i < length; i++)
    {
        if ((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'f') || (str[i] >= 'A' && str[i] <= 'F'))
            continue;
        else
            return TXN_FAILED;
    }
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES hexstr_to_bytearray(char *hexstr, unsigned char **bytearray, int *len)
{
    if (strlen(hexstr) % 2 != 0)
    {
        logData("invalid Hex str length for hexstr_to_bytearray conversion");
        return TXN_FAILED;
    }

    if (is_hex_str(hexstr) == TXN_FAILED)
    {
        logData("invalid Hex str for hexstr_to_bytearray conversion");
        return TXN_FAILED;
    }

    int length = strlen(hexstr) / 2;
    const char *pos = hexstr;
    unsigned char temparray[length];

    for (size_t idx = 0; idx < length; idx++)
    {
        sscanf(pos, "%2hhx", &temparray[idx]);
        pos += 2;
    }

    *bytearray = (unsigned char *)malloc(length);
    memcpy(*bytearray, temparray, length);
    *len = length;

    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES bytearray_to_hexstr(const unsigned char *bytearray, int length, char **hexstr)
{
    int outlength = (length * 2) + 1;
    char temparray[outlength];
    char *pos = temparray;

    for (size_t idx = 0; idx < length; idx++)
    {
        pos += sprintf(pos, "%02hhx", bytearray[idx]);
    }

    temparray[outlength - 1] = '\0';
    *hexstr = (char *)malloc(outlength);
    memcpy(*hexstr, temparray, outlength);

    return TXN_SUCCESS;
}