#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "BatterySensor.h"
#include "GameState.h"
#include "NotificationDispatcher.h"
#include "WifiClient.h"

#define HTTPGAMECLIENT_MAX_REQUEST_DATA_SIZE    (8192)
#define HTTPGAMECLIENT_MAX_RESPONSE_DATA_SIZE   (8192)
#define PEER_REPORT_MAX_SIZE                    (1024*7)

typedef enum HTTPGameClient_HTTPRequestTypes_e
{
    HTTPGAMECLIENT_HTTPREQUEST_NONE = 0,
    // HTTPGAMECLIENT_HTTPREQUEST_LOGIN,
    HTTPGAMECLIENT_HTTPREQUEST_HEARTBEAT,
    // HTTPGAMECLIENT_HTTPREQUEST_VERIFY,
} HTTPGameClient_HTTPRequestTypes;

typedef enum HTTPGameClient_HTTPMethods_e
{
    HTTPGAMECLIENT_HTTPMETHOD_GET = 0,
    HTTPGAMECLIENT_HTTPMETHOD_POST,
} HTTPGameClient_HTTPMethodTypes;

typedef struct HTTPGameClient_Request_t
{
    HTTPGameClient_HTTPMethodTypes methodType;
    HTTPGameClient_HTTPRequestTypes requestType;
    uint32_t waitTimeMs;      // Time willing to wait before request is sent
    uint8_t pData[HTTPGAMECLIENT_MAX_REQUEST_DATA_SIZE];
    uint32_t dataLength;
} HTTPGameClient_Request;

typedef struct HTTPGameClient_Response_t
{
    int32_t statusCode;
    uint8_t pData[HTTPGAMECLIENT_MAX_RESPONSE_DATA_SIZE+1]; // +1 for null terminator
    uint32_t dataLength;
} HTTPGameClient_Response;


// Private Structures
typedef struct HTTPGameClient_RequestItem_t
{
    TickType_t sendTime;
    TickType_t expireTime;
    HTTPGameClient_Request request;
    struct HTTPGameClient_RequestItem_t *pNext;
} HTTPGameClient_RequestItem;

typedef struct HTTPGameClient_RequestList_t
{
    HTTPGameClient_RequestItem * head;
    HTTPGameClient_RequestItem * tail;
    uint32_t size, capacity;
} HTTPGameClientRequestList;
// End Private Structures

typedef struct HTTPGameClient_t
{
    WifiClient *pWifiClient;
    SemaphoreHandle_t requestMutex;
    HTTPGameClientRequestList requestQueue;
    HTTPGameClient_Response response;
    HeartBeatResponse responseStruct;
    char peerReport[PEER_REPORT_MAX_SIZE];
    NotificationDispatcher *pNotificationDispatcher;
    BatterySensor *pBatterySensor;
} HTTPGameClient;


esp_err_t HTTPGameClient_Init(HTTPGameClient *this, WifiClient *pWifiClient, NotificationDispatcher *pNotificationDispatcher, BatterySensor *pBatterySensor);


#endif // HTTP_CLIENT_H