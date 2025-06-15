#ifndef JABTHOSTUTIL_H
#define JABTHOSTUTIL_H

int sendAbtHostRequest(const char *body, const char *resourceName, char responseMessage[1024 * 32]);

void generateAbtMessage(const char *body, const char *resourceName, char message[1024 * 32]);

#endif