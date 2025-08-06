/**
 * @file WifiClient.h
 * @brief WiFi connectivity and network management system
 * 
 * This module provides comprehensive WiFi functionality for the badge including:
 * - WiFi station mode connection management
 * - Network credential storage and retrieval
 * - Connection state tracking and retry logic
 * - Event-driven connectivity notifications
 * - Integration with user settings for network configuration
 * - Thread-safe network operations
 * - Automatic reconnection handling
 * 
 * The WiFi client enables badge connectivity to internet services,
 * game servers, and OTA update systems.
 * 
 * @author Badge Development Team
 * @date 2024
 */

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
#include "UserSettings.h"

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
    WifiSettings defconWifiSettings;

    TickType_t pendingStartTime;
    TickType_t desiredStartTime;
    int32_t numClients;

    NotificationDispatcher *pNotificationDispatcher;
    UserSettings *pUserSettings;
} WifiClient;

/**
 * @brief Initialize the WiFi client system
 * 
 * Initializes the WiFi subsystem including ESP-IDF WiFi stack configuration,
 * event handlers, connection management, and integration with user settings
 * for network credentials and preferences.
 * 
 * @param this Pointer to WifiClient instance to initialize
 * @param pNotificationDispatcher Notification system for WiFi events
 * @param pUserSettings User settings for WiFi credentials and preferences
 * @return ESP_OK on success, error code on failure
 */
esp_err_t WifiClient_Init(WifiClient *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings);

/**
 * @brief Immediately enable WiFi without client tracking
 * 
 * Enables WiFi immediately without reference counting. This function
 * does NOT automatically turn WiFi off - it must be managed manually.
 * Use WifiClient_RequestConnect() for managed WiFi connections.
 * 
 * @param this Pointer to WifiClient instance
 * @return Current WiFi client state after enable attempt
 */
WifiClient_State WifiClient_Enable(WifiClient *this);

/**
 * @brief Request WiFi connection with client reference counting
 * 
 * Requests WiFi to be enabled with optional wait time. Uses reference
 * counting to track multiple clients and keeps WiFi enabled until all
 * clients disconnect. If WiFi fails to connect, clients should retry.
 * 
 * @param this Pointer to WifiClient instance
 * @param waitTimeMS Maximum time to wait for connection (0 for immediate)
 * @return Current WiFi client state after connection attempt
 */
WifiClient_State WifiClient_RequestConnect(WifiClient *this, uint32_t waitTimeMS);

/**
 * @brief Disconnect WiFi client and decrement reference count
 * 
 * Each client must call this when done to properly manage WiFi state.
 * WiFi will automatically disable when all clients have disconnected.
 * 
 * @param this Pointer to WifiClient instance
 * @return ESP_OK on success, error code on failure
 */
esp_err_t WifiClient_Disconnect(WifiClient *this);

/**
 * @brief Wait for WiFi connection to be established (blocking)
 * 
 * Blocks until WiFi connection is successfully established or fails.
 * Used when synchronous WiFi connection is required.
 * 
 * @param this Pointer to WifiClient instance
 * @return ESP_OK on successful connection, error code on failure
 */
esp_err_t WifiClient_WaitForConnected(WifiClient *this);

/**
 * @brief Get current WiFi client state (non-blocking)
 * 
 * Returns the current state of the WiFi client without blocking.
 * Used for polling WiFi connection status.
 * 
 * @param this Pointer to WifiClient instance
 * @return Current WiFi client state
 */
WifiClient_State WifiClient_GetState(WifiClient *this);

/**
 * @brief Test WiFi connectivity
 * 
 * Performs a connectivity test to verify WiFi connection is working
 * properly. Used for diagnostics and network validation.
 * 
 * @param this Pointer to WifiClient instance
 */
void WifiClient_TestConnect(WifiClient *this);

#endif // WIFI_CLIENT_H