#include <stdlib.h>
#include <string.h>

#include "http_util.h"
#include "http_parser.h"
#include "../include/logutil.h"
#include "../include/commonutil.h"

extern char currentTrxType[100];
extern char lastPanDigit[10];

static int onBody(http_parser *parser, const char *data, size_t length)
{
    logDataEx(currentTrxType, lastPanDigit, "Body Received");
    logDataEx(currentTrxType, lastPanDigit, "Body length received : %d", length);

    HttpResponseData *httpResponseData = (struct http_response_data *)parser->data;
    httpResponseData->messageLen = length;
    free(httpResponseData->message);
    httpResponseData->message = malloc(length + 1);
    if (httpResponseData->message == NULL)
    {
        logDataEx(currentTrxType, lastPanDigit, "ERROR: malloc failed in onBody");
        httpResponseData->messageLen = 0;
        return -1;
    }
    safe_strncpy(httpResponseData->message, length + 1, data, length);
    httpResponseData->message[length] = '\0';

    return 0;
}

static int onStatus(http_parser *parser, const char *data, size_t length)
{
    logDataEx(currentTrxType, lastPanDigit, "Status Received");
    logDataEx(currentTrxType, lastPanDigit, "Status length received : %d", length);

    HttpResponseData *httpResponseData = (struct http_response_data *)parser->data;
    int maxLen = 19;
    if (length < maxLen)
    {
        maxLen = length;
    }

    safe_strncpy(httpResponseData->status, sizeof(httpResponseData->status), data, maxLen);
    httpResponseData->status[maxLen] = '\0';

    return 0;
}

static int onHeaderField(http_parser *parser, const char *data, size_t length)
{
    HttpResponseData *httpResponseData = (struct http_response_data *)parser->data;
    int maxLen = 49;
    if (length < maxLen)
    {
        maxLen = length;
    }

    safe_strncpy(httpResponseData->headerField, sizeof(httpResponseData->headerField), data, maxLen);
    httpResponseData->headerField[maxLen] = '\0';

    return 0;
}

static int onHeaderValue(http_parser *parser, const char *data, size_t length)
{
    HttpResponseData *httpResponseData = (struct http_response_data *)parser->data;

    if (strcmp(httpResponseData->headerField, "Content-Length") == 0)
    {
        int maxLen = 9;
        if (length < maxLen)
        {
            maxLen = length;
        }
        safe_strncpy(httpResponseData->contentLength, sizeof(httpResponseData->contentLength), data, maxLen);
        httpResponseData->contentLength[maxLen] = '\0';
    }

    if (strcmp(httpResponseData->headerField, "Content-Type") == 0)
    {
        int maxLen = 99;
        if (length < maxLen)
        {
            maxLen = length;
        }
        safe_strncpy(httpResponseData->contentType, sizeof(httpResponseData->contentType), data, maxLen);
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
        .on_body = onBody};

HttpResponseData parseHttpResponse(const char *responseMessage)
{
    HttpResponseData httpResponseData;
    httpResponseData.messageLen = 0;
    httpResponseData.message = NULL;
    if (responseMessage == NULL)
    {
        logDataEx(currentTrxType, lastPanDigit, "ERROR: NULL response message");
        httpResponseData.code = 0;
        safe_strcpy(httpResponseData.status, sizeof(httpResponseData.status), "");
        safe_strcpy(httpResponseData.contentLength, sizeof(httpResponseData.contentLength), "");
        safe_strcpy(httpResponseData.contentType, sizeof(httpResponseData.contentType), "");
        safe_strcpy(httpResponseData.headerField, sizeof(httpResponseData.headerField), "");
        return httpResponseData;
    }
    safe_strcpy(httpResponseData.contentLength, sizeof(httpResponseData.contentLength), "");
    safe_strcpy(httpResponseData.contentType, sizeof(httpResponseData.contentType), "");
    http_parser parser;
    http_parser_init(&parser, HTTP_RESPONSE);
    parser.data = &httpResponseData;
    size_t parsed = http_parser_execute(&parser, &settings, responseMessage, strlen(responseMessage));

    logDataEx(currentTrxType, lastPanDigit, "Parsed : %d", parsed);
    logDataEx(currentTrxType, lastPanDigit, "Major : %d", parser.http_major);
    logDataEx(currentTrxType, lastPanDigit, "Minor : %d", parser.http_minor);
    logDataEx(currentTrxType, lastPanDigit, "Status Code : %d", parser.status_code);
    httpResponseData.code = parser.status_code;

    logDataEx(currentTrxType, lastPanDigit, "-------------------------------------------------");
    logDataEx(currentTrxType, lastPanDigit, "Http Response Parsed Object");
    logDataEx(currentTrxType, lastPanDigit, "Code : %d", httpResponseData.code);
    logDataEx(currentTrxType, lastPanDigit, "Status : %s", httpResponseData.status);
    logDataEx(currentTrxType, lastPanDigit, "Message Len : %d", httpResponseData.messageLen);
    logDataEx(currentTrxType, lastPanDigit, "Message : %s", httpResponseData.message);
    logDataEx(currentTrxType, lastPanDigit, "Content Length : %s", httpResponseData.contentLength);
    logDataEx(currentTrxType, lastPanDigit, "ContentType : %s", httpResponseData.contentType);
    logDataEx(currentTrxType, lastPanDigit, "-------------------------------------------------");

    return httpResponseData;
}

void httpResponseData_free(HttpResponseData *d)
{
    if (d == NULL)
        return;
    free(d->message);
    d->message = NULL;
    d->messageLen = 0;
}