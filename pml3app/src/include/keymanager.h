
#ifndef KEYMANAGER_H_
#define KEYMANAGER_H_

/**
 * Perform a single key injection
 **/
int doKeyInjection();

/**
 * Do the key injection in loop
 **/
void *processKeyInjection();

/**
 * To check whether the key is present
 **/
int checkKeyPresent(char *label, bool isBdk);

/**
 * To destroy the key
 **/
int destroyKey(char *label, bool isBdk);

int destroyObject(CK_FUNCTION_LIST_PTR p11, CK_SESSION_HANDLE hSession, CK_OBJECT_CLASS keyClass, CK_KEY_TYPE keyType, char *label);

#endif