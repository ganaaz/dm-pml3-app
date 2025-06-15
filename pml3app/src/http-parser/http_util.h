#ifndef HTTP_UTIL_H
#define HTTP_UTIL_H

typedef struct http_response_data
{
    int code;
    char status[20];
    char* message;
    int messageLen;
    char contentLength[10];
    char contentType[100];
    char headerField[50];

} HttpResponseData;

HttpResponseData parseHttpResponse(const char* responseMessage);

#endif