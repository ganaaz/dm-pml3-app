#include <stdio.h>
#include <string.h>
#include <fcntl.h>   // Contains file controls like O_RDWR
#include <errno.h>   // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h>  // write(), read(), close()
#include <stdlib.h>
#include <stdbool.h>

#include "include/serialmanager.h"
#include "include/config.h"
#include "include/logutil.h"
#include "include/commandmanager.h"
#include "include/appcodes.h"

#define COMMAND_DOWNLOAD_FILE_WITH_QUOTE "\"download_file\""

int SERIAL_PORT;
int IS_SERIAL_CONNECTED;

void *createAndListenForUSB()
{
    logInfo("Going to create serial listener");
    // Open the serial port. Change device path as needed (currently set to an standard FTDI USB-UART cable type device)
    SERIAL_PORT = open("/dev/ttyGS0", O_RDWR);

    // Create new termios struct, we call it 'tty' for convention
    struct termios tty;

    // Read in existing settings, and handle any error
    if (tcgetattr(SERIAL_PORT, &tty) != 0)
    {
        logError("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        return NULL;
    }

    logInfo("Serial port attr get done\n");

    tty.c_cflag &= ~PARENB;        // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB;        // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag &= ~CSIZE;         // Clear all bits that set the data size
    tty.c_cflag |= CS8;            // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;                                                        // Disable echo
    tty.c_lflag &= ~ECHOE;                                                       // Disable erasure
    tty.c_lflag &= ~ECHONL;                                                      // Disable new-line echo
    tty.c_lflag &= ~ISIG;                                                        // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);                                      // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
    // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

    tty.c_cc[VTIME] = 10; // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;

    // Set in/out baud rate to be 9600
    cfsetispeed(&tty, B57600);
    cfsetospeed(&tty, B57600);

    // Save tty settings, also checking for error
    if (tcsetattr(SERIAL_PORT, TCSANOW, &tty) != 0)
    {
        logError("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        return NULL;
    }

    char readBuffer[1024 * 5];
    while (1)
    {
        memset(&readBuffer, '\0', sizeof(readBuffer));
        int bytesRead = 0;
        int readCount = 0;
        strcpy(readBuffer, "");
        do
        {
            char buffer[256] = {0};
            bytesRead = read(SERIAL_PORT, &buffer, 256);
            if (bytesRead != 0)
            {
                logData("Serial Read first part of data with length : %d", bytesRead);
                logData("Data is : %s", buffer);
                strncat(readBuffer, buffer, bytesRead);
                readCount += bytesRead;
            }
        } while (bytesRead > 0);

        readBuffer[readCount] = '\0';

        if (readCount != 0)
        {
            logData("Total data read count : %d", readCount);
            logData("Complete data : %s", readBuffer);

            IS_SERIAL_CONNECTED = 1;
            char *data = (char *)malloc(readCount);
            memcpy(data, readBuffer, readCount);
            data[readCount] = '\0';

            logData("Serial Received Message Full to be used :  %s", data);
            bool isDataCommand = isDataSocketCommand(data);
            if (isDataCommand == true)
            {
                logData("Data socket command message received");
                char *response = handleClientFetchMessage(data);
                logData("Length of response : %d", strlen(response));
                if (strlen(response) == 0)
                {
                    logData("There is no response nothing to send to client");
                }
                else
                {
                    write(SERIAL_PORT, response, strlen(response));
                    free(response);
                }
            }
            else
            {
                logData("Normal transaction data message received");
                char *response = handleClientMessage(data);
                logData("Length of response : %d", strlen(response));
                if (strlen(response) == 0)
                {
                    logData("There is no response nothing to send to client");
                }
                else
                {
                    write(SERIAL_PORT, response, strlen(response));
                    free(response);
                }
            }

            free(data);
        }
    }

    /*

    // Write to serial port
    unsigned char msg[] = { 'H', 'e', 'l', 'l', 'o', '\r' };
    write(serial_port, msg, sizeof(msg));

    // Allocate memory for read buffer, set size according to your needs
    char read_buf [256];

    // Normally you wouldn't do this memset() call, but since we will just receive
    // ASCII data for this example, we'll set everything to 0 so we can
    // call printf() easily.
    memset(&read_buf, '\0', sizeof(read_buf));

    // Read bytes. The behaviour of read() (e.g. does it block?,
    // how long does it block for?) depends on the configuration
    // settings above, specifically VMIN and VTIME
    int num_bytes = read(serial_port, &read_buf, sizeof(read_buf));

    // n is the number of bytes read. n may be 0 if no bytes were received, and can also be -1 to signal an error.
    if (num_bytes < 0) {
        printf("Error reading: %s", strerror(errno));
        return 1;
    }

    // Here we assume we received ASCII data, but you might be sending raw bytes (in that case, don't try and
    // print it to the screen like this!)
    printf("Read %i bytes. Received message: %s", num_bytes, read_buf);
    */

    close(SERIAL_PORT);

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
