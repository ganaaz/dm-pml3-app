#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

/**
 * Create and listen the serial usb
 **/
void *createAndListenForUSB();

/**
 * To check whether the command is of data socket
 */
bool isDataSocketCommand(const char data[1024 * 2]);

#endif