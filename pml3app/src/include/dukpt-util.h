#ifndef DUKPTUTIL_H_
#define DUKPTUTIL_H_

#include "config.h"

/**
 * To encrypt the pan and exp date available in current txn data and pouplate the ksn
 * in the same current txn data object, used for offline sale
 */
void encryptPanExpDate();

/**
 * To generate the Mac for the input data as per the PayTM requirement
 * this will update the current txn data mac and ksn
 **/
void generateMac(const char *macInput);

/**
 * To generate the Mac for the input data as per the PayTM requirement
 * and return the KSN and data used for reversal and echo
 **/
void generateMacReversalEcho(const char *macInput, char ksnData[21], char macData[17]);

/**
 * To encrypt the pan, track2 and exp date available in current txn data and pouplate the ksn
 * in the same current txn data object, used for immediate online scenarios
 */
void encryptPanTrack2ExpDate();

#endif