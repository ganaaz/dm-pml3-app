#include <json-c/json.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <feig/fetrm.h>

#include "jairtel_host_interface.h"
#include "jairtel_hostutil.h"
#include "../include/dboperations.h"
#include "../include/config.h"
#include "../include/commonutil.h"
#include "../include/logutil.h"

extern struct applicationConfig appConfig;

/**
 * Health check request for Airtel
 */
char *generateAirtelHealthCheckRequest()
{
    // Top level object
    json_object *jobj = json_object_new_object();

    // health check body
    json_object_object_add(jobj, "terminalId", json_object_new_string(appConfig.terminalId));
    json_object_object_add(jobj, "merchantId", json_object_new_string(appConfig.merchantId));

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Parse health check response for airtel
 **/
AirtelHealthCheckResponse parseAirtelHostHealthCheckResponse(const char *data)
{
    AirtelHealthCheckResponse hostResponse;
    hostResponse.status = -1;
    memset(hostResponse.code, 0, sizeof(hostResponse.code));
    memset(hostResponse.description, 0, sizeof(hostResponse.description));
    memset(hostResponse.serverStatus, 0, sizeof(hostResponse.serverStatus));
    memset(hostResponse.serverTime, 0, sizeof(hostResponse.serverTime));

    json_object *jObject = json_tokener_parse(data);
    if (jObject == NULL)
    {
        goto done;
    }

    json_object *jHead = json_object_object_get(jObject, "meta");
    if (jHead == NULL)
    {
        goto done;
    }

    if (json_object_object_get(jHead, "status") != NULL)
    {
        hostResponse.status = json_object_get_int(json_object_object_get(jHead, "status"));
    }

    if (json_object_object_get(jHead, "code") != NULL)
    {
        strcpy(hostResponse.code,
               (char *)json_object_get_string(json_object_object_get(jHead, "code")));
    }

    if (json_object_object_get(jHead, "description") != NULL)
    {
        strcpy(hostResponse.description,
               (char *)json_object_get_string(json_object_object_get(jHead, "description")));
    }

    json_object *jData = json_object_object_get(jObject, "data");
    if (jData == NULL)
    {
        goto done;
    }

    if (json_object_object_get(jData, "serverTime") != NULL)
    {
        strcpy(hostResponse.serverTime,
               (char *)json_object_get_string(json_object_object_get(jData, "serverTime")));
    }

    if (json_object_object_get(jData, "serverStatus") != NULL)
    {
        strcpy(hostResponse.serverStatus,
               (char *)json_object_get_string(json_object_object_get(jData, "serverStatus")));
    }

done:
    json_object_put(jObject); // Clear the json memory
    return hostResponse;
}
