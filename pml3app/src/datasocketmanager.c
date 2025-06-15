
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#include "include/datasocketmanager.h"
#include "include/commonutil.h"
#include "include/commandmanager.h"
#include "include/logutil.h"

#define PORT 9091

int DATA_SOCKET_ID;

extern struct applicationConfig appConfig;

/**
 * Create a socket and listen for commands from client for fetch data
 * Handle the messages and send the response back if any
 **/
void *createAndListenForFetchData()
{
    struct sockaddr_in address;
    int serverFd, reqstatus;
    int opt = 1;
    int addrlen = sizeof(address);
    socklen_t optlen = sizeof(opt);
    char buffer[1024] = {0};

    logInfo("Creating data socket and going to wait for client");

    // Creating socket file descriptor
    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        logError("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        logError("Error in setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Check the status for the keepalive option
    if (getsockopt(serverFd, SOL_SOCKET, SO_KEEPALIVE, &opt, &optlen) < 0)
    {
        logError("Error in getsockopt");
        close(serverFd);
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        logError("Error setsockopt failed.");
        exit(EXIT_FAILURE);
    }

    if (appConfig.socketTimeout != -1)
    {
        struct timeval tv;
        tv.tv_sec = appConfig.socketTimeout;
        tv.tv_usec = 0;
        setsockopt(serverFd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
        logData("Socket timeout is set as %d", appConfig.socketTimeout);
    }
    else
    {
        logData("Socket time out is set as -1, so not setting the timeout val");
    }

    // Forcefully attaching socket to the port
    if (bind(serverFd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        logError("Error bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(serverFd, 3) < 0)
    {
        logError("Error listen failed");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        logInfo("Waiting for the client socket for data ... ");
        if ((DATA_SOCKET_ID = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            logError("Error in accepting");
            continue;
        }

        while (1)
        {
            logData("Listening for data from client on Fetch Data socket id : %d ...", DATA_SOCKET_ID);
            reqstatus = read(DATA_SOCKET_ID, buffer, 1024);
            if (reqstatus <= 0)
            {
                logError("No data received, quiting the data socket");
                break;
            }

            char *data = (char *)malloc(reqstatus + 1);
            memcpy(data, buffer, reqstatus);
            data[reqstatus] = '\0';

            logData("Raw Data received from client (Len : %d) : %s", reqstatus, data);

            char *response = handleClientFetchMessage(data);
            if (response == NULL)
            {
                logData("Null response");
            }
            else
            {
                logData("Length of response : %d", strlen(response));
                if (strlen(response) == 0)
                {
                    logData("There is no response nothing to send to client");
                }
                else
                {
                    send(DATA_SOCKET_ID, response, strlen(response), 0);
                    free(response);
                }
            }

            free(data);
        }

        close(DATA_SOCKET_ID);
    }

    return NULL;
}
