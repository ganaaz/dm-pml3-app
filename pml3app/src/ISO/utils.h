#ifndef ISO8583_UTILS_H
#define ISO8583_UTILS_H

#include "ISO8583.h"

/*  func : hexstr_to_decimal
    description : Converts hexstr to long.
    in: hexstr : {Type : char*} : Null terminated Hex string.
    in: size : {Type : int} : size of the Hex string. only size upto 4 is allowed.
    out: {Type : int} : converted int value.
*/
int hexstr_to_decimal(char *hexstr, int size);

/*  func : hexstr_to_long
    description : Converts hexstr to long.
    in: hexstr : {Type : char*} : Null terminated Hex string.
    in: size : {Type : int} : size of the Hex string. only size upto 8 is allowed.
    out: {Type : long} : converted long value.
*/
long hexstr_to_long(char *hexstr, int size);

/*  func : get_hex_string_len
    description : get hex strin length of the field.
    in: syntax : {Type : ISO8583_FIELD_SYNTAX} : field syntax.
    in: max_length : {Type : int} : field maxlength.
    out : {Type : int} : hexstring length.
*/
int get_hex_string_len(ISO8583_FIELD_SYNTAX syntax, int max_length);

/*  func : get_parser_hex_string_len
    description : get hex strin length of the field.
    in: syntax : {Type : ISO8583_FIELD_SYNTAX} : field syntax.
    in: len : {Type : int} : field len.
    out : {Type : int} : hexstring length.
*/
int get_parser_hex_string_len(ISO8583_FIELD_SYNTAX syntax, int len);

/*  func : convert_value
    description : get hex strin length of the field.
    in: syntax : {Type : ISO8583_FIELD_SYNTAX} : field syntax.
    in: value : {Type : char*} : field value.
    out : outstr {Type : char**} : output hex string.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES convert_value(ISO8583_FIELD_SYNTAX syntax, char *value, char **outstr);

/*  func : padd_left_s
    description : Left padd the hex string with pad_char for the padlength passed.
    in: hexstr {Type : char*} : input null terminated string in case of RAW data pass a hex string and double the length.
    in: syntax { Type : ISO8583_FIELD_SYNTAX} : syntax of the field.
    in: padlength { Type : int} : Length required after padding.
    in: pad_char { Type : char} : Char to be used for padding.
    out: outstr {Type : char**} : Padded output string, memory will be allocated for this string with padlength size. caller should free the memory post using the outstr.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES padd_left_s(char *hexstr, ISO8583_FIELD_SYNTAX syntax, int padlength, char pad_char, char **outstr);

/*  func : padd_right_s
    description : Right padd the hex string with pad_char for the padlength passed.
    in: hexstr {Type : char*} : input null terminated string in case of RAW data pass a hex string and double the length.
    in: syntax { Type : ISO8583_FIELD_SYNTAX} : syntax of the field.
    in: padlength { Type : int} : Length required after padding.
    in: pad_char { Type : char} : Char to be used for padding.
    out: outstr {Type : char**} : Padded output string, memory will be allocated for this string with padlength size. caller should free the memory post using the outstr.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES padd_right_s(char *hexstr, ISO8583_FIELD_SYNTAX syntax, int padlength, char pad_char, char **outstr);

/*  func : padd_left_with_zero_i
    description : Left padd the integer val with zero for the padlength passed.
    in: val {Type : int} : input integer value for padding.
    in: padlength { Type : int} : Length required after padding.
    out: outstr {Type : char**} : Padded output string, memory will be allocated for this string with padlength size. caller should free the memory post using the outstr.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES padd_left_with_zero_i(int val, int padlength, char **outstr);

/*  func : padd_left_with_zero_h
    description : Left padd the integer hex value with zero for the padlength passed.
    in: val {Type : int} : input integer value for padding.
    in: padlength { Type : int} : Length required after padding.
    out: outstr {Type : char**} : Padded output string, memory will be allocated for this string with padlength size. caller should free the memory post using the outstr.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES padd_left_with_zero_h(int val, int padlength, char **outstr);

/*  func : is_hex_str
    description : Validates if the string is Hex String.
    in: str {Type : char*} : input hex string for validation.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES is_hex_str(char *str);

/*  func : hexstr_to_bytearray
    description : Coverts Hex string to byte array.
    in: hexstr {Type : char*} : input hex string which needs to be converted to byte array.
    out: bytearray {Type : char**} : output byte array. memory will be allocated for this string with half the size of hexstr. caller should free the memory post using the bytearray.
    out: len {Type: int*} : length of the bytearray.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES hexstr_to_bytearray(char *hexstr, unsigned char **bytearray, int *len);

/*  func : bytearray_to_hexstr
    description : Coverts byte array hex string.
    in: bytearray {Type : const unsigned char *} : input byte array to be converted to hexstr.
    in: length {Type : int} : input byte array length.
    out: hexstr {Type : char**} : output hex string. memory will be allocated for this string with double + 1 the size of bytearray. caller should free the memory post using the hexstr.
    out: {Type : ISO8583_ERROR_CODES} : Status code : return TXN_SUCCESS if Success, Other ErrorCodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES bytearray_to_hexstr(const unsigned char *bytearray, int length, char **hexstr);

#endif /* ISO8583_UTILS_H */