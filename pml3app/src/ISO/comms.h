#include "ISO8583.h"

/*  func : process_with_host
    description : Processed the iso8583 message with host. (Connect, send, receive)
    in: address : {Type : const char *} : Host IP address.
    in: port : {Type : int} : Host service Port.
    in: timeout_sec : {Type : int} : Timeout in Seconds.
    in: request : {Type : const unsigned char *} : ISO8583 request message to be sent to host.
    in: request_len : {Type : int} : ISO8583 request message length.
    out: response {Type : unsigned char** } : ISO8583 response message received from host.
    out: response_len {Type : int* } : ISO8583 response message length.
    out: {Type : ISO8583_ERROR_CODES } : Status code : return TXN_SUCCESS if Success, Other Errorcodes(ISO8583_ERROR_CODES) if failure.
*/
ISO8583_ERROR_CODES process_with_host(const char *address, int port, int timeout_sec, const unsigned char *request, int request_len, unsigned char **response, int *response_len);
