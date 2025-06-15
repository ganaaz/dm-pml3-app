#ifndef JHOSTUTIL_H
#define JHOSTUTIL_H

int sendHostRequest(const char *body, const char *resourceName, char responseMessage[1024 * 32]);

void generateMessage(const char *body, const char *resourceName, char message[1024 * 32]);

#endif