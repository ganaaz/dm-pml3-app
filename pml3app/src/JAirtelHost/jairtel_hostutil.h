#ifndef JAIRTELHOSTUTIL_H
#define JAIRTELHOSTUTIL_H

int sendAirtelHostRequest(const char *body, const char *resourceName, char responseMessage[1024 * 32],
                          char orderId[40], char signature[200]);

void generateAirtelHostMessage(const char *body, const char *resourceName, char message[1024 * 32],
                               char orderId[40], char signature[200]);

#endif