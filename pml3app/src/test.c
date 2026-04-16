#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <log4c.h>
#include <time.h>
#include <signal.h>

#include <feig/sdk.h>
#include <feig/fetpf.h>
#include <feig/leds.h>
#include <feig/buzzer.h>
#include <feig/fetrm.h>

#include "include/commonutil.h"
#include "include/config.h"
#include "include/pkcshelper.h"
#include "include/rupayservice.h"
#include "include/feigtransaction.h"
#include "include/feiginit.h"
#include "include/socketmanager.h"
#include "include/datasocketmanager.h"
#include "include/logutil.h"
#include "include/aztimer.h"
#include "include/hostmanager.h"
#include "include/commandmanager.h"
#include "include/appcodes.h"
#include "include/keymanager.h"
#include "include/emvconfig.h"
#include "include/dukpt-util.h"
#include "http-parser/http_parser.h"
#include "http-parser/http_util.h"

extern struct transactionData currentTxnData;

void testIccTagRemoval()
{
    // char iccData[] = "57136083263242000025D26126201060000000000F5F3401015F24032612319F0607A00000052410109F0702AB809F34031F00029F090200029F3501989F4104000000004F07A0000005241010820219009F360218CB9F10200FB5019400000020140010000000781236000000000000000010109100010000950500000000009F2608FDC31C2B07196B669F2701409F3704D3DE907D9F02060000000000019F03060000000000009F1A0203565F2A0203569F21030651209A032312129C01009F33030008089B02E8008A025931DF15029100DF16021010";
    int counter = 0;
    while (true)
    {
        char iccData[] = "5F3401015F24032612319F0607A00000052410109F0702AB809F34031F00029F090200029F3501989F4104000000004F07A0000005241010820219009F360218CB9F10200FB5019400000020140010000000781236000000000000000010109100010000950500000000009F2608FDC31C2B07196B669F2701409F3704D3DE907D9F02060000000000019F030600000000000057136083263242000025D26126201060000000000F9F1A0203565F2A0203569F21030651209A032312129C01009F33030008089B02E8008A025931DF15029100DF16021010";
        unsigned char buffer[216];
        hexToByte(iccData, buffer);
        removeIccTags(buffer, 216);
        printf("ICC Data : %s\n", currentTxnData.iccData);
        printf("ICC Data Len : %d\n", currentTxnData.iccDataLen);
        counter++;
        printf("Counter : %d\n", counter);
    }
    exit(0);
}

// Get the device id
/*
uint32_t device_id;
int rd = fetrm_get_devid(&device_id);
printf("Device id result : %d\n", rd);
printf("Device id : %lu\n", device_id);
printf("%08" PRIx32 "\n", device_id);
*/

// Read emv config file and generate config data
// parseEmvConfigFile();
// if (true)
//    exit(0);

/*
static http_parser_settings settings_null =
  {.on_message_begin = 0,
  .on_headers_complete = 0,
  .on_message_complete = 0,
  .on_header_field = on_data,
  .on_header_value = on_data,
  .on_url = 0,
  .on_status = on_data,
  .on_body = on_data
  };
*/
void hostcheck()
{
    // Temp for host check
    if (false)
    {
        /*
        char* message = generateOfflineSaleRequest();
        printf("Message : \n");
        printf(message);
        printf("\n");

        logData("Testing the paytm connectivity");
        char responseMessage[1024 * 256];
        sendHostRequest(message, "/eos/offlineSale", responseMessage);
        logData("Response length from server : %d", strlen(responseMessage));
        logData("Response Message : %s", responseMessage);
        free(message);
        return 0;
        */
    }
}

void testHttpParse()
{
    // http_parser parser;
    // http_parser_init(&parser, HTTP_RESPONSE);

    const char message1[] = "HTTP/1.0 200 OK\r\n"
                            "X-Application-Context: application:8080\r\n"
                            "Content-Type: application/json;charset=UTF-8\r\n"
                            "Access-Control-Allow-Origin: *\r\n"
                            "Access-Control-Allow-Methods: GET, POST, OPTIONS, PUT\r\n"
                            "Access-Control-Allow-Headers: DNT, X-CustomHeader, Keep-Alive, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control, Content-Type\r\n"
                            "Strict-Transport-Security: max-age=31536000; includeSubDomains;\r\n"
                            "Content-Length: 667\r\n"
                            "Expires: Thu, 10 Nov 2022 08:39:17 GMT\r\n"
                            "Cache-Control: max-age=0, no-cache, no-store\r\n"
                            "Pragma: no-cache\r\n"
                            "Date: Thu, 10 Nov 2022 08:39:17 GMT\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "{\"head\":{\"responseTimestamp\":\"2022-11-1014:09:16\",\"buildVersion\":\"PAYTM POS Version 1.1.0.0\"},\"body\":{\"resultStatus\":\"FAIL\",\"resultCode\":\"F\",\"resultMsg\":\"Unable to connect with issuer bank, Please try again\",\"resultCodeId\":\"EOS_0046\",\"bankResultMsg\":\"Unable to connect with issuer bank, Please try again\",\"bankResultCode\":null,\"invoiceNumber\":\"000105\",\"amount\":\"0\",\"retrievalReferenceNumber\":null,\"authorizationCode\":null,\"saleQrCodeMetadata\":null,\"iccData\":null,\"bankTid\":null,\"bankMid\":null,\"acquirementId\":\"20221110111212800110168642803537671\",\"orderId\":\"2022111014074800010514034986\",\"gateway\":null,\"merchantName\":\"TOUCH WOOD LIMITED\",\"issuingBank\":\"ICICI Bank\"}}";

    const char message[] = "HTTP/1.0 200 OK\r\n"
                           "X-Application-Context application080\r\n"
                           "Content-Type: application/json;charset=UTF-8and some other data here and there\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Methods: GET, POST, OPTIONS, PUT\r\n"
                           "Access-Control-Allow-Headers: DNT, X-CustomHeader, Keep-Alive, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control, Content-Type\r\n"
                           "Strict-Transport-Security: max-age=31536000; includeSubDomains;\r\n"

                           "Expires: Thu, 10 Nov 2022 08:39:17 GMT\r\n"
                           "Cache-Control: max-age=0, no-cache, no-store\r\n"
                           "Pragma: no-cache\r\n"
                           "Date: Thu, 10 Nov 2022 08:39:17 GMT\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "{\"head\":{\"body\":{\"resultStatus\":\"FAIL\",\"resultCode\":\"F\",\"resultMsg\":\"Unable to connect with issuer bank, Please try again\",\"resultCodeId\":\"EOS_0046\",\"bankResultMsg\":\"Unable to connect with issuer bank, Please try again\",\"bankResultCode\":null,\"invoiceNumber\":\"000105\",\"amount\":\"0\",\"retrievalReferenceNumber\":null,\"authorizationCode\":null,\"saleQrCodeMetadata\":null,\"iccData\":null,\"bankTid\":null,\"bankMid\":null,\"acquirementId\":\"20221110111212800110168642803537671\",\"orderId\":\"2022111014074800010514034986\",\"gateway\":null,\"merchantName\":\"TOUCH WOOD LIMITED\",\"issuingBank\":\"ICICI Bank\"}}";

    int idx = 0;
    while (true)
    {
        printf("COUNT :::::: %d\n", idx++);
        HttpResponseData httpResponseData;
        if (idx % 2 == 0)
            httpResponseData = parseHttpResponse(message1);
        else
            httpResponseData = parseHttpResponse(message);

        free(httpResponseData.message);
    }

    /*
    size_t parsed = http_parser_execute(&parser, &settings_null, message, strlen(message));

    printf("Parsed : %d\n", parsed);
    printf("Major : %d\n", parser.http_major);
    printf("Minor : %d\n", parser.http_minor);
    printf("Status Code : %d\n", parser.status_code);
    printf("Content Length : %d\n", parser.content_length);
    */
    exit(0);
}
