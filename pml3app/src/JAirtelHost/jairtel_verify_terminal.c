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
 * Verify terminal request for Airtel
 */
char *generateAirtelVerifyTerminalRequest()
{
    // Top level object
    json_object *jobj = json_object_new_object();

    char deviceId[20];
    getDeviceId(deviceId);

    // verify terminal body
    json_object_object_add(jobj, "terminalId", json_object_new_string(appConfig.terminalId));
    json_object_object_add(jobj, "merchantId", json_object_new_string(appConfig.merchantId));
    json_object_object_add(jobj, "serialNo", json_object_new_string(deviceId));

    const char *jsonData = (char *)json_object_to_json_string(jobj);
    int len = strlen(jsonData);
    char *data = malloc(len + 1);
    strncpy(data, jsonData, len);
    data[len] = '\0';
    json_object_put(jobj); // Clear JSON memory

    return data;
}

/**
 * Parse verify terminal response for airtel
 **/
AirtelVerifyTerminalResponse parseAirtelHostVerifyTerminalResponse(const char *data)
{
    AirtelVerifyTerminalResponse hostResponse;
    hostResponse.status = -1;
    memset(hostResponse.code, 0, sizeof(hostResponse.code));
    memset(hostResponse.description, 0, sizeof(hostResponse.description));

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

done:
    json_object_put(jObject); // Clear the json memory
    return hostResponse;
}
