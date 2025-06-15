
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <arpa/inet.h>

#include "include/socketmanager.h"
#include "include/commonutil.h"
#include "include/commandmanager.h"
#include "include/logutil.h"

#define PORT 9090

extern struct applicationConfig appConfig;

int CLIENT_SOCKET;

/**
 * Create a socket and listen for commands from client
 * Handle the messages and send the response back if any
 **/
void createAndListenServer()
{
    struct sockaddr_in address;
    int serverFd, reqstatus;
    int opt = 1;
    int addrlen = sizeof(address);
    socklen_t optlen = sizeof(opt);
    char buffer[1024 * 5] = {0};

    logInfo("Creating server socket and going to wait for client");

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
        logInfo("Waiting for the client socket ... ");
        if ((CLIENT_SOCKET = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            logError("Error in accepting");
            continue;
        }

        struct sockaddr_in *pV4Addr = (struct sockaddr_in *)&address;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char ipString[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, ipString, INET_ADDRSTRLEN);

        while (1)
        {
            logData("Listening for data from client (%s) on socket id : %d ...", ipString, CLIENT_SOCKET);
            reqstatus = read(CLIENT_SOCKET, buffer, 1024 * 5);
            if (reqstatus < 0)
            {
                logError("No data received, quiting the main socket");
                break;
            }

            if (reqstatus == 0)
            {
                logError("Empty data of length 0");
                break;
            }

            char *data = (char *)malloc(reqstatus + 1);
            memcpy(data, buffer, reqstatus);
            data[reqstatus] = '\0';

            logData("Raw Data received from client %s, socket : %d (Len : %d) : %s",
                    ipString, CLIENT_SOCKET, reqstatus, data);

            char *response = handleClientMessage(data);
            logData("Length of response : %d", strlen(response));
            if (strlen(response) == 0)
            {
                logData("There is no response nothing to send to client");
            }
            else
            {
                send(CLIENT_SOCKET, response, strlen(response), 0);
                free(response);
            }

            free(data);
        }

        close(CLIENT_SOCKET);
        logData("Client socket with id %d is closed", CLIENT_SOCKET);

        // Stop the thread on socket closed
        stopCardSearch();
    }
}
