#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include "log.h"
#include "comms.h"
#include "utils.h"
#include "../include/logutil.h"

ISO8583_ERROR_CODES clean_socket(int soc_fd, int event_fd)
{
    if (shutdown(soc_fd, SHUT_RDWR) < 0)
    {
        logData("Socket shutdown Failed, soc_fd: %d, errno: %d (%s)", soc_fd, errno, strerror(errno));
    }
    close(event_fd);
    close(soc_fd);
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES connect_to_host(const char *address, int port, int *sock_fd, int *event_fd, int timeout_sec)
{
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;
    struct epoll_event event;
    struct epoll_event events[1];

    int soc_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (soc_fd < 0)
    {
        logData("Socket Creation Failed, soc_fd: %d, errno: %d (%s)", soc_fd, errno, strerror(errno));
        return TXN_HOST_CONNECTION_FAILED;
    }

    int epfd = epoll_create(1);
    event.events = EPOLLOUT;
    event.data.fd = soc_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, soc_fd, &event) < 0)
    {
        logData("Add timeout event to socket failed, event_fd %d, soc_fd: %d, errno: %d (%s)", epfd, soc_fd, errno, strerror(errno));
        clean_socket(soc_fd, epfd);
        return TXN_HOST_CONNECTION_FAILED;
    }

    bzero(&addr, addrlen);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &addr.sin_addr.s_addr) == 0)
    {
        logData("Invalid host address: %s", address);
        clean_socket(soc_fd, epfd);
        return TXN_HOST_CONNECTION_FAILED;
    }

    if (connect(soc_fd, (struct sockaddr *)&addr, addrlen) < 0)
    {
        if ((errno != EWOULDBLOCK) && (errno != EINPROGRESS))
        {
            logData("Connect to host failed: soc_fd: %d, errno: %d (%s)", soc_fd, errno, strerror(errno));
            clean_socket(soc_fd, epfd);
            return TXN_HOST_CONNECTION_FAILED;
        }
        int ready = epoll_wait(epfd, events, 1, timeout_sec * 1000);
        if (ready == 0)
        {
            errno = ETIMEDOUT;
            logData("Unable to connect to host : Timeout, errno: %d (%s)", errno, strerror(errno));
            clean_socket(soc_fd, epfd);
            return TXN_HOST_CONNECTION_TIMEOUT;
        }
        for (int i = 0; i < ready; i++)
        {
            if (events[i].events & EPOLLOUT)
            {
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(events[i].data.fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0)
                {
                    if (error != 0)
                    {
                        logData("Connection Error: error : %d(%s), errno: %d (%s)", error, strerror(error), errno, strerror(errno));
                        clean_socket(soc_fd, epfd);
                        return TXN_HOST_CONNECTION_FAILED;
                    }
                }
                else
                {
                    logData("Got Socket options : error : %d(%s), errno: %d (%s)", error, strerror(error), errno, strerror(errno));
                    clean_socket(soc_fd, epfd);
                    return TXN_HOST_CONNECTION_FAILED;
                }
            }
        }
    }
    *sock_fd = soc_fd;
    *event_fd = epfd;
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES send_to_host(int soc_fd, const unsigned char *request, int request_len)
{
    if (soc_fd < 0 || request == NULL || request_len <= 0)
    {
        logData("Unable to send. invalid arguments");
        return TXN_SEND_TO_HOST_FAILED;
    }
    int bytes_sent = send(soc_fd, request, request_len, 0);
    if (bytes_sent < 0 || bytes_sent != request_len)
    {
        logData("Send Failed, Bytes Send %d, meg_len %d, errno: %d (%s)", bytes_sent, request_len, errno, strerror(errno));
        return TXN_SEND_TO_HOST_FAILED;
    }
    {
        char *hex_response = NULL;
        bytearray_to_hexstr(request, request_len, &hex_response);
        logData("Request Sent : %s", hex_response);
        free(hex_response);
    }
    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES receive_from_host(unsigned char **outbuf, int *outbuf_len, int soc_fd, int epfd, int timeout_sec, int close_connection_on_success)
{
    struct epoll_event events[1];
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = soc_fd;
    unsigned char *receive_buf = NULL;
    int receive_len = 4096;

    if (epoll_ctl(epfd, EPOLL_CTL_MOD, soc_fd, &event) < 0)
    {
        logData("Add timeout event to socket failed, event_fd %d, soc_fd: %d, errno: %d (%s)", epfd, soc_fd, errno, strerror(errno));
        clean_socket(soc_fd, epfd);
        return TXN_RECEIVE_FROM_HOST_FAILED;
    }
    int ready = epoll_wait(epfd, events, 1, timeout_sec * 1000);
    if (ready == 0)
    {
        errno = ETIMEDOUT;
        logData("Unable receive from host :Timeout, errno: %d (%s)", errno, strerror(errno));
        clean_socket(soc_fd, epfd);
        return TXN_RECEIVE_FROM_HOST_TIMEOUT;
    }
    for (int i = 0; i < ready; i++)
    {
        if (events[i].events & EPOLLIN)
        {
            receive_buf = (unsigned char *)malloc(receive_len);
            if (NULL != receive_buf)
            {
                int bytes_received = recv(events[i].data.fd, receive_buf, receive_len, 0);
                {
                    char *hex_response = NULL;
                    bytearray_to_hexstr(receive_buf, bytes_received, &hex_response);
                    logData("Response Received : %s", hex_response);
                    free(hex_response);
                }
                receive_len = bytes_received;
                *outbuf = (unsigned char *)malloc(receive_len);
                memcpy(*outbuf, receive_buf, receive_len);
                *outbuf_len = receive_len;
            }
            else
            {
                logData("NULL Receive Buffer");
                if (receive_buf)
                    free(receive_buf);
                clean_socket(soc_fd, epfd);
                return TXN_RECEIVE_FROM_HOST_FAILED;
            }
        }
        else
        {
            logData("Undesired Event Received. errno: %d (%s)", errno, strerror(errno));
            clean_socket(soc_fd, epfd);
            return TXN_RECEIVE_FROM_HOST_FAILED;
        }
    }

    if (close_connection_on_success)
    {
        logData("Closing The Connection");
        clean_socket(soc_fd, epfd);
    }

    if (receive_buf)
        free(receive_buf);

    return TXN_SUCCESS;
}

ISO8583_ERROR_CODES process_with_host(const char *address, int port, int timeout_sec, const unsigned char *request, int request_len, unsigned char **response, int *response_len)
{
    int soc_fd = -1;
    int epfd = -1;
    struct timeval t1, t2;

    gettimeofday(&t1, NULL);

    ISO8583_ERROR_CODES ret = connect_to_host(address, port, &soc_fd, &epfd, timeout_sec);
    if (ret != TXN_SUCCESS)
    {
        return ret;
    }

    ret = send_to_host(soc_fd, request, request_len);
    if (ret != TXN_SUCCESS)
    {
        return ret;
    }

    gettimeofday(&t2, NULL);

    ret = receive_from_host(response, response_len, soc_fd, epfd, timeout_sec - (t2.tv_sec - t1.tv_sec), 1);
    if (ret != TXN_SUCCESS)
    {
        return ret;
    }

    return TXN_SUCCESS;
}