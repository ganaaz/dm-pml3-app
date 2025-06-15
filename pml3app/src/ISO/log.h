#ifndef ISO8583_LOGGER
#define ISO8583_LOGGER
#include <stdio.h>

#define ISO8583_LOG_SIMPLE(msg)                                      \
    {                                                                \
        fprintf(stderr, "%s:%d:%s\t", __FILE__, __LINE__, __func__); \
        fprintf(stderr, msg);                                        \
        fprintf(stderr, "\n");                                       \
    }
#define ISO8583_LOG_ERROR(format, ...)                               \
    {                                                                \
        fprintf(stderr, "%s:%d:%s\t", __FILE__, __LINE__, __func__); \
        fprintf(stderr, format, __VA_ARGS__);                        \
        fprintf(stderr, "\n");                                       \
    }
#define ISO8583_LOG_INFO(format, ...)                                \
    {                                                                \
        fprintf(stdout, "%s:%d:%s\t", __FILE__, __LINE__, __func__); \
        fprintf(stdout, format, __VA_ARGS__);                        \
        fprintf(stdout, "\n");                                       \
    }
#define ISO8583_LOG_HEXDUMP(msg, buf, len)                                  \
    {                                                                       \
        fprintf(stdout, "%s:%d:%s\t%s", __FILE__, __LINE__, __func__, msg); \
        fprintf(stdout, "Buffer size : %d\t Buffer: ", len);                \
        for (int i = 0; i < len; i++)                                       \
        {                                                                   \
            fprintf(stdout, "%02x", buf[i]);                                \
        }                                                                   \
        fprintf(stdout, "\n");                                              \
    }
#endif // ISO8583_LOGGER