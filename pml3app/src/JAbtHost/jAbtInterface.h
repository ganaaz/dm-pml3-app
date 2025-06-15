#ifndef JABTINTERFACE_H
#define JABTINTERFACE_H

#include "../include/config.h"
#include "../include/abtdbmanager.h"

/**
 * To genreate ABT tab request that can be sent to the server
 */
char *generateAbtTapRequest(AbtTransactionTable trxDataList[], int start, int trxCount);

/**
 * To parse the abt response
 **/
void parseAbtTapResponse(const char *data, int *count, AbtTapResponse abtTapResponses[]);

#endif