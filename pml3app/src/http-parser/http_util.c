#include <stdlib.h>
#include <string.h>

#include "http_util.h"
#include "http_parser.h"
#include "../include/logutil.h"

static int onBody(http_parser* parser, const char *data, size_t length)
{
    logData("Body Received");
    logData("Body length received : %d", length);
    
    HttpResponseData *httpResponseData = (struct http_response_data*)parser->data;
    httpResponseData->messageLen = length;
    httpResponseData->message = malloc(length + 1);
    strncpy(httpResponseData->message, data, length);
    httpResponseData->message[length] = '\0';

    return 0;
}

static int onStatus(http_parser* parser, const char *data, size_t length)
{
    logData("Status Received");
    logData("Status length received : %d", length);
    
    HttpResponseData *httpResponseData = (struct http_response_data*)parser->data;
    int maxLen = 19;
    if (length < maxLen)
    {
        maxLen = length;
    }

    strncpy(httpResponseData->status, data, maxLen);
    httpResponseData->status[length] = '\0';

    return 0;
}

static int onHeaderField(http_parser* parser, const char *data, size_t length)
{
    HttpResponseData *httpResponseData = (struct http_response_data*)parser->data;
    int maxLen = 49;
    if (length < maxLen)
    {
        maxLen = length;
    }

    strncpy(httpResponseData->headerField, data, maxLen);
    httpResponseData->headerField[length] = '\0';

    return 0;
}

static int onHeaderValue(http_parser* parser, const char *data, size_t length)
{
    HttpResponseData *httpResponseData = (struct http_response_data*)parser->data;

    if (strcmp(httpResponseData->headerField, "Content-Length") == 0)
    {
        int maxLen = 9;
        if (length < maxLen)
        {
            maxLen = length;
        }
        strncpy(httpResponseData->contentLength, data, maxLen);
        httpResponseData->contentLength[maxLen] = '\0';
    }

    if (strcmp(httpResponseData->headerField, "Content-Type") == 0)
    {
        int maxLen = 99;
        if (length < maxLen)
        {
            maxLen = length;
        }
        strncpy(httpResponseData->contentType, data, maxLen);
        httpResponseData->contentType[maxLen] = '\0';
    }

    return 0;
}

static http_parser_settings settings =
{
    .on_message_begin = 0,
    .on_headers_complete = 0,
    .on_message_complete = 0,
    .on_header_field = onHeaderField,
    .on_header_value = onHeaderValue,
    .on_url = 0,
    .on_status = onStatus,
    .on_body = onBody
};

HttpResponseData parseHttpResponse(const char* responseMessage)
{
    HttpResponseData httpResponseData;
    httpResponseData.messageLen = 0;
    httpResponseData.message = malloc(1);
    httpResponseData.message[0] = '\0';
    strcpy(httpResponseData.contentLength, "");
    strcpy(httpResponseData.contentType, "");
    http_parser parser;
    http_parser_init(&parser, HTTP_RESPONSE);
    parser.data = &httpResponseData;
    size_t parsed = http_parser_execute(&parser, &settings, responseMessage, strlen(responseMessage));

    logData("Parsed : %d", parsed);
    logData("Major : %d", parser.http_major);
    logData("Minor : %d", parser.http_minor);
    logData("Status Code : %d", parser.status_code);
    httpResponseData.code = parser.status_code;

    logData("-------------------------------------------------");
    logData("Http Response Parsed Object");
    logData("Code : %d", httpResponseData.code);
    logData("Status : %s", httpResponseData.status);
    logData("Message Len : %d", httpResponseData.messageLen);
    logData("Message : %s", httpResponseData.message);
    logData("Content Length : %s", httpResponseData.contentLength);
    logData("ContentType : %s", httpResponseData.contentType);
    logData("-------------------------------------------------");

    return httpResponseData;
}