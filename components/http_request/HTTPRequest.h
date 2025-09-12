#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/list.h"

#include "WifiClient.h"
#include "Vector.h"

#define HTTP_REQUEST_MAX_REQUEST_DATA_SIZE    (1024)
#define HTTP_REQUEST_MAX_RESPONSE_DATA_SIZE   (8192)
#define HTTP_REQUEST_MAX_URL_LENGTH           (256)
#define HTTP_REQUEST_MAX_CONTENT_TYPE_LENGTH  (64)

typedef enum HTTPRequest_HTTPMethods_e
{
    HTTP_REQUEST_HTTP_METHOD_GET = 0,
    HTTP_REQUEST_HTTP_METHOD_POST,
} HTTPRequest_HTTPMethodTypes;

typedef struct HTTPRequest_Response_t
{
    int32_t statusCode;
    uint8_t pData[HTTP_REQUEST_MAX_RESPONSE_DATA_SIZE+1]; // +1 for null terminator
    uint32_t dataLength;
} HTTPRequest_Response;

typedef struct HTTPRequest_t
{
    WifiClient *pWifiClient;
    SemaphoreHandle_t requestMutex;
    Vector pendingRequests;
} HTTPRequest;

// Function pointer type for request handlers
typedef esp_err_t (*HTTPRequest_ResponseHandler)(HTTPRequest *this, HTTPRequest_Response *pResponse);

typedef struct HTTPRequest_Request_t
{
    TickType_t sendTime;
    TickType_t expireTime;
    HTTPRequest_HTTPMethodTypes methodType;
    uint32_t waitTimeMs;      // Time willing to wait before request is sent
    uint8_t url[HTTP_REQUEST_MAX_URL_LENGTH];
    uint8_t pData[HTTP_REQUEST_MAX_REQUEST_DATA_SIZE];
    uint32_t dataLength;
    uint8_t contentType[HTTP_REQUEST_MAX_CONTENT_TYPE_LENGTH];
    uint32_t contentTypeLength;
    bool disableAutoRedirect;
    bool skipCertCommonNameCheck;
    uint32_t timeoutMs;
    HTTPRequest_ResponseHandler responseHandler;
    HTTPRequest_Response response;
} HTTPRequest_Request;


esp_err_t HTTPRequest_Init(HTTPRequest *this, WifiClient *pWifiClient, int taskPriority, int cpu);

esp_err_t HTTPRequest_Create(HTTPRequest *this, HTTPRequest_HTTPMethodTypes methodType, 
                             uint32_t waitTimeMs, const uint8_t *url, HTTPRequest_ResponseHandler responseHandler,
                             uint8_t *pData, uint32_t dataLength,
                             const uint8_t *pContentType, uint32_t contentTypeLength,
                             bool disableAutoRedirect, bool skipCertCommonNameCheck,
                             uint32_t timeoutMs);
                             

#endif // HTTP_CLIENT_H