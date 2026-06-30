
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

int DATA_SOCKET_ID = -1;

extern struct applicationConfig appConfig;
extern volatile __sig_atomic_t shutdown_requested;

/**
 * Create a socket and listen for commands from client for fetch data
 * Handle the messages and send the response back if any
 **/
void *createAndListenForFetchData()
{
    struct sockaddr_in address;
    int serverFd;
    ssize_t reqstatus;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[1024] = {0};

    logInfoEx("Socket", "", "Creating data socket and going to wait for client");

    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        logErrorEx("L3-App", "Socket", "Socket creation failed.");
        return NULL; // Don't exit() in threads — it kills the whole process
    }

    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        logErrorEx("L3-App", "Socket", "Error setsockopt SO_REUSEADDR failed.");
        close(serverFd);
        return NULL;
    }

    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)))
    {
        logErrorEx("L3-App", "Socket", "Error setsockopt SO_REUSEPORT failed.");
        close(serverFd);
        return NULL;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (appConfig.socketTimeout != -1)
    {
        struct timeval tv;
        tv.tv_sec = appConfig.socketTimeout;
        tv.tv_usec = 0;
        setsockopt(serverFd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
        logDataEx("L3-App", "Socket", "Socket timeout is set as %d", appConfig.socketTimeout);
    }
    else
    {
        logDataEx("L3-App", "Socket", "Socket timeout is set as -1, so not setting the timeout val");
    }

    if (bind(serverFd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        logErrorEx("L3-App", "Socket", "Error bind failed");
        close(serverFd);
        return NULL;
    }

    if (listen(serverFd, 3) < 0)
    {
        logErrorEx("L3-App", "Socket", "Error listen failed");
        close(serverFd);
        return NULL;
    }

    while (1)
    {
        // --- Shutdown check before accept ---
        if (shutdown_requested)
        {
            logErrorEx("L3-App", "Socket", "Shutdown requested, exiting fetch data socket listener");
            break;
        }

        // Use select() with 1s timeout so we don't block forever on accept()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverFd, &readfds);
        struct timeval timeout = {1, 0}; // 1 second

        int activity = select(serverFd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0)
        {
            logErrorEx("L3-App", "Socket", "select() error on server socket");
            break;
        }
        if (activity == 0)
            continue; // Timeout — loop back and check shutdown_requested

        logInfoEx("Socket", "", "Waiting for the client socket for data ...");
        if ((DATA_SOCKET_ID = accept(serverFd, (struct sockaddr *)&address, &addrlen)) < 0)
        {
            logErrorEx("L3-App", "Socket", "Error in accepting");
            continue;
        }

        logInfoEx("Socket", "", "Client connected on fetch data socket: %d", DATA_SOCKET_ID);

        if (appConfig.socketTimeout != -1)
        {
            struct timeval tv;
            tv.tv_sec = appConfig.socketTimeout;
            tv.tv_usec = 0;
            setsockopt(DATA_SOCKET_ID, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
            logDataEx("L3-App", "Socket", "Socket timeout is set as %d", appConfig.socketTimeout);
        }
        else
        {
            logDataEx("L3-App", "Socket", "Socket timeout is set as -1, so not setting the timeout val");
        }

        // --- Inner read loop ---
        while (1)
        {
            if (shutdown_requested)
            {
                logInfoEx("Socket", "", "Shutdown requested, closing client fetch data socket");
                break;
            }

            // Use select() with 1s timeout so we don't block forever on read()
            fd_set clientfds;
            FD_ZERO(&clientfds);
            FD_SET(DATA_SOCKET_ID, &clientfds);
            struct timeval readTimeout = {1, 0}; // 1 second

            int clientActivity = select(DATA_SOCKET_ID + 1, &clientfds, NULL, NULL, &readTimeout);
            if (clientActivity < 0)
            {
                logErrorEx("L3-App", "Socket", "select() error on client socket");
                break;
            }
            if (clientActivity == 0)
                continue; // Timeout — loop back and check shutdown_requested

            logDataEx("L3-App", "Socket", "Listening for data from client on Fetch Data socket id: %d ...", DATA_SOCKET_ID);
            reqstatus = read(DATA_SOCKET_ID, buffer, sizeof(buffer));
            if (reqstatus <= 0)
            {
                logErrorEx("L3-App", "Socket", "No data received, closing the data socket");
                break;
            }

            char *data = (char *)malloc(reqstatus + 1);
            if (!data)
            {
                logErrorEx("L3-App", "Socket", "malloc failed for incoming data buffer");
                break;
            }
            memcpy(data, buffer, reqstatus);
            data[reqstatus] = '\0';
            logDataEx("L3-App", "Socket", "Raw Data received from client (Len: %zd): %s", reqstatus, data);

            char *response = handleClientFetchMessage(data);
            free(data);

            if (response == NULL)
            {
                logDataEx("L3-App", "Socket", "Null response");
            }
            else
            {
                logDataEx("L3-App", "Socket", "Length of response: %zu", strlen(response));
                if (strlen(response) == 0)
                {
                    logDataEx("L3-App", "Socket", "There is no response, nothing to send to client");
                }
                else
                {
                    if (send(DATA_SOCKET_ID, response, strlen(response), 0) < 0)
                    {
                        logErrorEx("L3-App", "Socket", "Error sending response to client");
                    }
                }
                free(response);
            }
        }

        close(DATA_SOCKET_ID);
        DATA_SOCKET_ID = -1;
    }

    // Clean shutdown — close server socket
    logErrorEx("L3-App", "Socket", "Closing fetch data server socket");
    close(serverFd);
    return NULL;
}