#include <stdio.h>
#include <string.h>
#include <fcntl.h>   // Contains file controls like O_RDWR
#include <errno.h>   // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h>  // write(), read(), close()
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <sys/socket.h>

#include "include/commonutil.h"
#include "include/serialmanager.h"
#include "include/config.h"
#include "include/logutil.h"
#include "include/commandmanager.h"
#include "include/appcodes.h"
#include <sys/select.h>

#define COMMAND_DOWNLOAD_FILE_WITH_QUOTE "\"download_file\""

int SERIAL_PORT;
int IS_SERIAL_CONNECTED;
extern volatile __sig_atomic_t shutdown_requested;

void *createAndListenForUSB()
{
    logInfo("Going to create serial listener");

    SERIAL_PORT = open("/dev/ttyGS0", O_RDWR);
    if (SERIAL_PORT < 0)
    {
        logError("Failed to open serial port: %s", strerror(errno));
        return NULL;
    }

    struct termios tty;
    if (tcgetattr(SERIAL_PORT, &tty) != 0)
    {
        logError("Error %i from tcgetattr: %s", errno, strerror(errno));
        close(SERIAL_PORT);
        return NULL;
    }

    logInfo("Serial port attr get done");

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    tty.c_cc[VTIME] = 0; // No blocking wait — we use select() instead
    tty.c_cc[VMIN] = 0;

    cfsetispeed(&tty, B57600);
    cfsetospeed(&tty, B57600);

    if (tcsetattr(SERIAL_PORT, TCSANOW, &tty) != 0)
    {
        logError("Error %i from tcsetattr: %s", errno, strerror(errno));
        close(SERIAL_PORT);
        return NULL;
    }

    char readBuffer[1024 * 5];

    while (1)
    {
        // --- Shutdown check ---
        if (shutdown_requested)
        {
            logError("Shutdown requested, exiting USB serial listener");
            break;
        }

        // Wait up to 1 second for data to be available on the serial port
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(SERIAL_PORT, &readfds);
        struct timeval timeout = {1, 0}; // 1 second

        int activity = select(SERIAL_PORT + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0)
        {
            logError("select() error on serial port: %s", strerror(errno));
            break;
        }
        if (activity == 0)
            continue; // Timeout — loop back and check shutdown_requested

        // Data is available — read it
        memset(readBuffer, '\0', sizeof(readBuffer));
        int readCount = 0;

        do
        {
            char buffer[256] = {0};
            int bytesRead = read(SERIAL_PORT, buffer, sizeof(buffer));
            if (bytesRead > 0)
            {
                logData("Serial read chunk length: %d", bytesRead);
                logData("Data is: %s", buffer);

                // Guard against overflow
                if (readCount + bytesRead >= (int)sizeof(readBuffer) - 1)
                {
                    logError("Serial read buffer overflow, discarding message");
                    readCount = 0;
                    break;
                }

                memcpy(readBuffer + readCount, buffer, bytesRead);
                readCount += bytesRead;
            }
            else
            {
                break;
            }
        } while (1);

        if (readCount == 0)
            continue;

        readBuffer[readCount] = '\0';
        logData("Total data read count: %d", readCount);
        logData("Complete data: %s", readBuffer);

        IS_SERIAL_CONNECTED = 1;

        // +1 for null terminator
        char *data = (char *)malloc(readCount + 1);
        if (data == NULL)
        {
            logError("malloc failed for serial data");
            continue;
        }
        memcpy(data, readBuffer, readCount);
        data[readCount] = '\0';

        logData("Serial received message: %s", data);

        char *response = NULL;
        if (isDataSocketCommand(data))
        {
            logData("Data socket command message received");
            response = handleClientFetchMessage(data);
        }
        else
        {
            logData("Normal transaction data message received");
            response = handleClientMessage(data);
        }
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
            write(SERIAL_PORT, response, strlen(response));
            free(response);
        }
    }

    logError("Closing serial port");
    close(SERIAL_PORT);
    SERIAL_PORT = -1;
    return NULL;
}

/**
 * To check whether the command is of data socket
 */
bool isDataSocketCommand(const char data[1024 * 2])
{
    if (strstr(data, COMMAND_FETCH_AUTH) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_FETCH_ACK) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_GET_DEVICE_ID) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_GET_PENDING_OFFLINE) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_DO_BEEP) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_SET_TIME) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_IS_KEY_PRESENT) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_DESTROY_KEY) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_DOWNLOAD_FILE_WITH_QUOTE) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_GET_FIRMWARE_VERSION) != NULL)
    {
        return true;
    }

    if (strstr(data, COMMAND_GET_PRODUCT_ORDER_NUMBER) != NULL)
    {
        return true;
    }

    return false;
}
