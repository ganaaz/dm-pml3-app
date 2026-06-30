#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include <errno.h>
#include <openssl/err.h>

#include "jAbtHostUtil.h"
#include "../include/logutil.h"
#include "../include/config.h"
#include "../include/commonutil.h"

#define FAIL -1

extern struct applicationConfig appConfig;

int sendAbtHostRequest(const char *body, const char *resourceName, char responseMessage[1024 * 32])
{
    SSL_CTX *ctx;
    SSL *ssl = NULL;
    int retStatus = -1;
    int sd = 0;
    struct timeval timeout;

    ctx = SSL_CTX_new(SSLv23_client_method());
    SSL_CTX_set_timeout(ctx, appConfig.hostTxnTimeout);
    ssl = SSL_new(ctx);
    logDataEx("ABT", "", "ABT SSL Is initialized");

    timeout.tv_usec = 0;
    timeout.tv_sec = appConfig.hostTxnTimeout;

    struct hostent *host;
    struct sockaddr_in addr;
    if ((host = gethostbyname(appConfig.abtIP)) == NULL)
    {
        retStatus = -1;
        goto done;
    }

    sd = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(appConfig.abtPort);
    addr.sin_addr.s_addr = *(long *)(host->h_addr_list[0]);

    logDataEx("ABT", "", "ABT Host names are set and going to connect the socket");

    if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        logErrorEx("ABT", "Host", "ABT Connect failed");
        retStatus = -1;
        goto done;
    }

    logDataEx("ABT", "", "ABT Socket Connect succeeded");

    SSL_set_fd(ssl, sd);

    logDataEx("ABT", "", "Going to perform SSL Connect");
    if (SSL_connect(ssl) == FAIL)
    {
        retStatus = -1;
        goto done;
    }

    logDataEx("ABT", "", "ABT SSL Connect succeeded");
    char message[1024 * 16] = {0}; // Max message request is 16 kb
    generateAbtMessage(body, resourceName, message);

    logDataEx("ABT", "", "Raw message to be sent to ABT server : %s", message);
    logDataEx("ABT", "", "Raw message length to be sent to ABT server : %d", strlen(message));
    logHostData("ABT Request");
    logHostData(message);

    SSL_write(ssl, message, strlen(message));
    retStatus = -2; // we have send the message to server

    int len = 0, count = 0;
    int total = 0;
    logDataEx("ABT", "", "Waiting to read the data");
    do
    {
        char buffer[1024];
        len = SSL_read(ssl, buffer, 1024);
        logDataEx("ABT", "", "Data read of length : %d", len);

        if (len == 0)
            logDataEx("ABT", "", "No data received");

        for (int i = 0; i < len; ++i)
        {
            responseMessage[count] = buffer[i];
            count++;
        }
        total += len;
        logDataEx("ABT", "", "Now the total length : %d", total);
    } while (len > 0);

    // if we receive any data its success or else its -2 (reversal)
    if (count != 0)
        retStatus = 0;

    logDataEx("ABT", "", "Total message received length : %d", count);
    logDataEx("ABT", "", "Return status : %d", retStatus);
    responseMessage[count] = '\0';

    logHostData("Response");
    logHostData(responseMessage);

done:

    if (ssl != NULL)
        SSL_free(ssl);

    if (sd != 0)
        close(sd);

    SSL_CTX_free(ctx);
    logDataEx("ABT", "", "ABT All resources freed");
    return retStatus;
}

void generateAbtMessage(const char *body, const char *resourceName, char message[1024 * 32])
{
    sprintf(message, "%s%s%s", "POST ", resourceName, " HTTP/1.1\r\n");
    sprintf(message + strlen(message), "%s%s%s", "Host: ", appConfig.abtHostName, "\r\n");
    sprintf(message + strlen(message), "%s", "Content-Type: application/json\r\n");
    sprintf(message + strlen(message), "%s", "Connection: keep-alive\r\n");
    sprintf(message + strlen(message), "%s%d%s", "Content-Length: ", strlen(body), "\r\n");
    sprintf(message + strlen(message), "%s", "\r\n");
    sprintf(message + strlen(message), "%s", body);
    sprintf(message + strlen(message), "%s", "\r\n");
}
