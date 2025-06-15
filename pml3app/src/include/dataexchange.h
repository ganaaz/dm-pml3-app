#ifndef DATAEXCHANGE_H_
#define DATAEXCHANGE_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <feig/emvco_ep.h>
#include <feig/feig_tags.h>

/**
 * Handle the data exchange from the FEIG reader
 * Process the request and then genereate the reply as required
 **/
int handleDataExchange(struct fetpf *client, const void *request, size_t request_len,
                       void *reply, size_t *reply_len);

#endif