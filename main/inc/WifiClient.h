#ifndef WIFI_CLIENT_H
#define WIFI_CLIENT_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_err.h"

#include "NotificationDispatcher.h"

typedef enum WifiClient_State_e
{
    WIFI_CLIENT_STATE_UNKNOWN = 0,
    WIFI_CLIENT_STATE_DISCONNECTED,
    WIFI_CLIENT_STATE_WAITING,
    WIFI_CLIENT_STATE_ATTEMPTING,
    WIFI_CLIENT_STATE_CONNECTING,
    WIFI_CLIENT_STATE_CONNECTED,
    WIFI_CLIENT_STATE_FAILED,
} WifiClient_State;

typedef struct WifiClient_t
{
    WifiClient_State state;
    int retryCount;
    SemaphoreHandle_t clientMutex;
    esp_netif_t * wifiStationConfig;
    EventGroupHandle_t wifiEventGroup;
    esp_event_handler_instance_t instanceAnyId;
    esp_event_handler_instance_t instanceGotIp;
    wifi_config_t wifiConfig;

    TickType_t pendingStartTime;
    TickType_t desiredStartTime;
    int32_t numClients;

    NotificationDispatcher *pNotificationDispatcher;
} WifiClient;

esp_err_t WifiClient_Init(WifiClient *this, NotificationDispatcher *pNotificationDispatcher);

// Immediately enable wifi
// DOES NOT turn wifi off on its own
WifiClient_State WifiClient_Enable(WifiClient *this);

// Request for wifi to be enabled with an amount of time you are willing to wait
// Pass in 0 for waitTimeMS to immediately turn on wifi
// This will increate numClients and keep wifi enabled until all clients are done
// If wifi fails to connect, clients will be expected to try again
WifiClient_State WifiClient_RequestConnect(WifiClient *this, uint32_t waitTimeMS);

// Each client will need to call disable when they are done for wifi to disable itself
esp_err_t WifiClient_Disconnect(WifiClient *this);

// Blocking call for wifi connect
// Returns ESP_OK on success
// Returns others on failure
esp_err_t WifiClient_WaitForConnected(WifiClient *this);

// Non-blocking call for wifi connect status
// Returns state of wifi client
WifiClient_State WifiClient_GetState(WifiClient *this);

void WifiClient_TestConnect(WifiClient *this);

#endif // WIFI_CLIENT_H