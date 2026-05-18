
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

#include "include/socketmanager.h"
#include "include/commonutil.h"
#include "include/commandmanager.h"
#include "include/logutil.h"

#define PORT 9090

extern struct applicationConfig appConfig;
extern volatile __sig_atomic_t shutdown_requested;

int CLIENT_SOCKET;

/**
 * Create a socket and listen for commands from client
 * Handle the messages and send the response back if any
 **/
void *createAndListenServer()
{
    struct sockaddr_in address;
    int serverFd, reqstatus;
    int opt = 1;
    int addrlen = sizeof(address);
    socklen_t optlen = sizeof(opt);
    char buffer[1024 * 5] = {0};

    logInfo("Creating server socket and going to wait for client");

    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        logError("Socket creation failed.");
        return NULL; // Never exit() in a thread
    }

    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        logError("Error in setsockopt");
        close(serverFd);
        return NULL;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (getsockopt(serverFd, SOL_SOCKET, SO_KEEPALIVE, &opt, &optlen) < 0)
    {
        logError("Error in getsockopt");
        close(serverFd);
        return NULL;
    }

    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        logError("Error setsockopt failed.");
        close(serverFd);
        return NULL;
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
        logData("Socket timeout is set as -1, so not setting the timeout val");
    }

    if (bind(serverFd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        logError("Error bind failed");
        close(serverFd);
        return NULL;
    }

    if (listen(serverFd, 3) < 0)
    {
        logError("Error listen failed");
        close(serverFd);
        return NULL;
    }

    while (1)
    {
        // --- Shutdown check before accept ---
        if (shutdown_requested)
        {
            logError("Shutdown requested, exiting main server listener");
            break;
        }

        // Wait up to 1 second for an incoming connection
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverFd, &readfds);
        struct timeval timeout = {1, 0};

        int activity = select(serverFd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0)
        {
            logError("select() error on server socket: %s", strerror(errno));
            break;
        }
        if (activity == 0)
            continue; // Timeout — loop back and check shutdown_requested

        logInfo("Waiting for the client socket ...");
        if ((CLIENT_SOCKET = accept(serverFd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            logError("Error in accepting");
            continue;
        }

        struct sockaddr_in *pV4Addr = (struct sockaddr_in *)&address;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char ipString[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, ipString, INET_ADDRSTRLEN);
        logInfo("Client connected from %s on socket id: %d", ipString, CLIENT_SOCKET);

        // --- Inner read loop ---
        while (1)
        {
            if (shutdown_requested)
            {
                logInfo("Shutdown requested, closing client socket %d", CLIENT_SOCKET);
                break;
            }

            // Wait up to 1 second for data from the connected client
            fd_set clientfds;
            FD_ZERO(&clientfds);
            FD_SET(CLIENT_SOCKET, &clientfds);
            struct timeval readTimeout = {1, 0};

            int clientActivity = select(CLIENT_SOCKET + 1, &clientfds, NULL, NULL, &readTimeout);
            if (clientActivity < 0)
            {
                logError("select() error on client socket: %s", strerror(errno));
                break;
            }
            if (clientActivity == 0)
                continue; // Timeout — loop back and check shutdown_requested

            logData("Listening for data from client (%s) on socket id: %d ...", ipString, CLIENT_SOCKET);
            reqstatus = read(CLIENT_SOCKET, buffer, sizeof(buffer));
            if (reqstatus < 0)
            {
                logError("Read error on main socket: %s", strerror(errno));
                break;
            }
            if (reqstatus == 0)
            {
                logInfo("Client %s disconnected", ipString);
                break;
            }

            char *data = (char *)malloc(reqstatus + 1);
            if (data == NULL)
            {
                logError("malloc failed for incoming data");
                break;
            }
            memcpy(data, buffer, reqstatus);
            data[reqstatus] = '\0';

            logData("Raw data received from client %s, socket: %d (Len: %d): %s",
                    ipString, CLIENT_SOCKET, reqstatus, data);

            char *response = handleClientMessage(data);
            free(data);

            if (response == NULL)
            {
                logData("Null response, nothing to send");
            }
            else if (strlen(response) == 0)
            {
                logData("Empty response, nothing to send");
            }
            else
            {
                logData("Length of response: %d", strlen(response));
                send(CLIENT_SOCKET, response, strlen(response), 0);
                free(response);
            }
        }

        close(CLIENT_SOCKET);
        CLIENT_SOCKET = -1;
        logError("Client socket closed");

        stopCardSearch();
    }

    logError("Closing main server socket");
    close(serverFd);
    return NULL;
}