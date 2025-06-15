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

#include "jairtel_hostutil.h"
#include "../include/logutil.h"
#include "../include/config.h"
#include "../include/commonutil.h"

#define FAIL -1

extern struct applicationConfig appConfig;

int sendAirtelHostRequest(const char *body, const char *resourceName, char responseMessage[1024 * 32],
                          char orderId[40], char signature[200])
{
    SSL_CTX *ctx;
    SSL *ssl = NULL;
    int retStatus = -1;
    int sd = 0;
    struct timeval timeout;

    ctx = SSL_CTX_new(SSLv23_client_method());
    SSL_CTX_set_timeout(ctx, appConfig.hostTxnTimeout);
    ssl = SSL_new(ctx);
    logData("SSL Is initialized for Airtel Host Util");

    timeout.tv_usec = 0;
    timeout.tv_sec = appConfig.hostTxnTimeout;

    struct hostent *host;
    struct sockaddr_in addr;
    if ((host = gethostbyname(appConfig.airtelHostIP)) == NULL)
    {
        retStatus = -1;
        goto done;
    }

    sd = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(appConfig.airtelHostPort);
    addr.sin_addr.s_addr = *(long *)(host->h_addr_list[0]);

    logData("Airtel Host names are set and going to connect the socket");

    if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        logError("Connect failed");
        retStatus = -1;
        goto done;
    }

    logData("Airtel Socket Connect succeeded");

    SSL_set_fd(ssl, sd);

    logData("Going to perform SSL Connect for Airtel");
    if (SSL_connect(ssl) == FAIL)
    {
        retStatus = -1;
        goto done;
    }

    logData("Airtel SSL Connect succeeded");
    char message[1024 * 16] = {0}; // Max message request is 16 kb
    generateAirtelHostMessage(body, resourceName, message, orderId, signature);

    logData("Raw message to be sent to server : %s", message);
    logData("Raw message length to be sent to server : %d", strlen(message));
    logHostData("Request");
    logHostData(message);

    SSL_write(ssl, message, strlen(message));
    retStatus = -2; // we have send the message to server

    int len = 0, count = 0;
    int total = 0;
    logData("Waiting to read the data");
    do
    {
        char buffer[1024];
        len = SSL_read(ssl, buffer, 1024);
        logData("Data read of length : %d", len);

        if (len == 0)
            logData("No data received");

        for (int i = 0; i < len; ++i)
        {
            responseMessage[count] = buffer[i];
            count++;
        }
        total += len;
        logData("Now the total length : %d", total);
    } while (len > 0);

    // if we receive any data its success or else its -2 (reversal)
    if (count != 0)
        retStatus = 0;

    logData("Total message received length : %d", count);
    logData("Return status : %d", retStatus);
    responseMessage[count] = '\0';

    logHostData("Response");
    logHostData(responseMessage);

done:

    if (ssl != NULL)
        SSL_free(ssl);

    if (sd != 0)
        close(sd);

    SSL_CTX_free(ctx);
    logData("All resources freed");
    return retStatus;
}

void generateAirtelHostMessage(const char *body, const char *resourceName, char message[1024 * 32],
                               char orderId[40], char signature[200])
{
    char timeData[20];
    populateCurrentTime(timeData);

    sprintf(message, "%s%s%s", "POST ", resourceName, " HTTP/1.0\r\n");
    sprintf(message + strlen(message), "%s%s%s", "Host: ", appConfig.airtelHttpsHostName, ":9443 \r\n");
    sprintf(message + strlen(message), "%s%d%s", "Content-Length: ", strlen(body), "\r\n");
    sprintf(message + strlen(message), "%s%s%s", "X-Request-Timestamp: ", timeData, "\r\n");
    sprintf(message + strlen(message), "%s%s%s", "X-Request-Id: ", orderId, "\r\n");
    sprintf(message + strlen(message), "%s%s%s", "X-Client-Id: ", appConfig.merchantId, "\r\n");
    sprintf(message + strlen(message), "%s%s%s", "Signature: ", signature, "\r\n");
    sprintf(message + strlen(message), "%s", "Content-Type: application/json\r\n");
    sprintf(message + strlen(message), "%s", "\r\n");
    sprintf(message + strlen(message), "%s", body);
    sprintf(message + strlen(message), "%s", "\r\n");
    sprintf(message + strlen(message), "%s", "Connection: close\r\n\r\n");
    sprintf(message + strlen(message), "%s", "<EOF>\r\n");
}
