
#include <string.h>

#include "esp_log.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "WifiClient.h"
#include "NotificationDispatcher.h"
#include "UserSettings.h"
#include "TaskPriorities.h"
#include "TimeUtils.h"
#include "Utilities.h"

// Define events for wifi (same as real WifiClient.c)
#define WIFI_CONNECTED          BIT0
#define WIFI_DISCONNECTED       BIT1
#define WIFI_MUTEX_TIMEOUT_MS   5000

// Internal Constants
static const char *TAG = "WIFI_QEMU_ETH";

// File-scope Ethernet handle (WifiClient struct fields are WiFi-typed; store ETH handle here)
static esp_eth_handle_t s_eth_handle = NULL;
static esp_netif_t *s_eth_netif = NULL;

// Internal Function Declarations
static void _EthEventHandler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);
static void _GotIpHandler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data);
static void _WifiTask(void *pvParameters);
void _WifiClient_Enable(WifiClient *this);

// Should be called by app_main on boot to prevent race condition on initial initialization
esp_err_t WifiClient_Init(WifiClient *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings)
{
    assert(this);
    assert(pNotificationDispatcher);
    assert(pUserSettings);

    memset(this, 0, sizeof(WifiClient));
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pUserSettings = pUserSettings;
    this->state = WIFI_CLIENT_STATE_DISCONNECTED;
    this->clientMutex = xSemaphoreCreateMutex();
    this->wifiEventGroup = xEventGroupCreate();

    // Initialize TCP/IP stack and event loop (same as real WifiClient)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default Ethernet netif (replaces esp_netif_create_default_wifi_sta)
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);

    // Initialize OpenCores Ethernet MAC (emulated by QEMU)
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &s_eth_handle));
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle)));

    // Register event handlers (mirror the WiFi event handler logic)
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                &_EthEventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                &_GotIpHandler, this));

    // Start Ethernet (always on — no scan/connect cycle needed)
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    ESP_LOGI(TAG, "QEMU Ethernet: initialized OpenCores Ethernet MAC");

    assert(xTaskCreatePinnedToCore(_WifiTask, "WifiClientTask",
           configMINIMAL_STACK_SIZE * 2, this,
           WIFI_CONTROL_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);

    return ESP_OK;
}

// Assumes the mutex is already taken
void _WifiClient_Enable(WifiClient *this)
{
    if(this->state == WIFI_CLIENT_STATE_DISCONNECTED ||
       this->state == WIFI_CLIENT_STATE_WAITING ||
       this->state == WIFI_CLIENT_STATE_FAILED)
    {
        // For Ethernet, "enabling" just means we ensure the link is started
        // and transition to ATTEMPTING. The Ethernet link-up + DHCP will
        // move us to CONNECTED via the event handlers.
        if (s_eth_handle != NULL)
        {
            // Ethernet is already started in Init, but if we previously stopped it,
            // restart it now.
            esp_eth_start(s_eth_handle);
        }
        this->state = WIFI_CLIENT_STATE_ATTEMPTING;
        ESP_LOGI(TAG, "Ethernet enable: state -> ATTEMPTING");
    }
}

static void _WifiTask(void *pvParameters)
{
    WifiClient *this = (WifiClient *)pvParameters;
    assert(this);
    assert(this->clientMutex);

    while (true)
    {
        if(xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
        {
            // Do we need to start?
            if(this->state == WIFI_CLIENT_STATE_WAITING &&
               TimeUtils_IsTimeExpired(this->desiredStartTime))
            {
                _WifiClient_Enable(this);
            }

            xSemaphoreGive(this->clientMutex);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to take wifi client mutex");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// TODO: SH Test the immediate code with game logic later on
WifiClient_State WifiClient_Enable(WifiClient *this)
{
    assert(this);
    assert(this->clientMutex);

    if(xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        _WifiClient_Enable(this);

        // TODO: Change this to DEBUG
        ESP_LOGI(TAG, "WifiClient_Enable");
        xSemaphoreGive(this->clientMutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain mutex");
    }

    return this->state;
}

WifiClient_State WifiClient_RequestConnect(WifiClient *this, uint32_t waitTimeMS)
{
    assert(this);
    assert(this->clientMutex);

    if(xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        ++this->numClients;

        // Only process if we aren't connected or in the process of connecting
        if(this->state != WIFI_CLIENT_STATE_ATTEMPTING && 
           this->state != WIFI_CLIENT_STATE_CONNECTING &&
           this->state != WIFI_CLIENT_STATE_CONNECTED)
        {
            // First attempt to enable after either a failure or a connected state
            if(this->state == WIFI_CLIENT_STATE_DISCONNECTED ||
               this->state == WIFI_CLIENT_STATE_FAILED)
            {
                // Clear away all the event bits
                xEventGroupClearBits(this->wifiEventGroup, WIFI_CONNECTED | WIFI_DISCONNECTED);

                // Do we start right away
                if(waitTimeMS == 0)
                {
                    _WifiClient_Enable(this);
                    ESP_LOGI(TAG, "WifiClient_RequestConnect: started immediately");
                }
                else
                {
                    // Log the start time to start the pending. The task will handle the rest
                    this->state = WIFI_CLIENT_STATE_WAITING;
                    this->pendingStartTime = xTaskGetTickCount();
                    this->desiredStartTime = this->pendingStartTime + pdMS_TO_TICKS(waitTimeMS);

                    // TODO: Change this to DEBUG
                    ESP_LOGI(TAG, "WifiClient_RequestConnect: pending request started: %lu", waitTimeMS);
                }
            }
            // Handle tick rollovers by using pendingStartTime as the reference
            else if(this->state == WIFI_CLIENT_STATE_WAITING &&
                    (int)((this->pendingStartTime + pdMS_TO_TICKS(waitTimeMS)) - this->desiredStartTime) < 0)
            {
                // Wifi hasn't started but someone wants to start sooner
                this->desiredStartTime = this->pendingStartTime + pdMS_TO_TICKS(waitTimeMS);

                // TODO: Change this to DEBUG
                ESP_LOGI(TAG, "WifiClient_RequestConnect: pending request shortened: %lu", waitTimeMS);
            }
        }

        // TODO: Change this to DEBUG
        ESP_LOGI(TAG, "WifiClient_RequestConnect: numClients(%lu)", this->numClients);
        xSemaphoreGive(this->clientMutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain mutex");
    }

    return this->state;
}

esp_err_t WifiClient_WaitForConnected(WifiClient *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    EventBits_t bits = xEventGroupWaitBits(this->wifiEventGroup,
                                           WIFI_CONNECTED | WIFI_DISCONNECTED,
                                           pdTRUE,          // ClearOnExit
                                           pdFALSE,         // WaitForAllBits
                                           portMAX_DELAY);
    if(bits & WIFI_CONNECTED)
    {
        ret = ESP_OK;
    }
    else if (bits & WIFI_DISCONNECTED)
    {
        ret = ESP_FAIL;
    }
    else
    {
        ESP_LOGE(TAG, "Unknown event bits: %lx", bits);
    }

    return ret;
}

WifiClient_State WifiClient_GetState(WifiClient *this)
{
    WifiClient_State retVal = WIFI_CLIENT_STATE_UNKNOWN;
    assert(this);
    assert(this->clientMutex);

    if(xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        retVal = this->state;
        xSemaphoreGive(this->clientMutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain mutex");
    }

    return retVal;
}

void WifiClient_TestConnect(WifiClient *this)
{
    WifiClient_RequestConnect(this, 0);
    bool success = (ESP_OK == WifiClient_WaitForConnected(this));
    esp_err_t err;
    if ((err = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE, (void*)&success, sizeof(success), DEFAULT_NOTIFY_WAIT_DURATION)) != ESP_OK) {
        ESP_LOGE(TAG, "NotificationDispatcher_NotifyEvent NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE failed: %s", esp_err_to_name(err));
    }
    WifiClient_Disconnect(this);
}

esp_err_t WifiClient_Disconnect(WifiClient *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    assert(this->clientMutex);

    if(xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        // In QEMU mode, keep Ethernet always up to avoid DHCP
        // re-negotiation delays.  Just decrement the client count.
        if(--this->numClients <= 0)
        {
            this->numClients = 0; // Paranoia set just in case
            ESP_LOGI(TAG, "Disconnect requested — Ethernet kept alive (QEMU)");
        }
        ret = ESP_OK;

        xSemaphoreGive(this->clientMutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain mutex");
    }
    return ret;
}

// Ethernet event handler — mirrors the WiFi event handler structure
static void _EthEventHandler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    WifiClient *this = (WifiClient *)arg;
    assert(this);

    if(xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        switch (event_id)
        {
            case ETHERNET_EVENT_START:
                this->retryCount = 0;
                this->state = WIFI_CLIENT_STATE_CONNECTING;
                ESP_LOGI(TAG, "Ethernet started");
                break;
            case ETHERNET_EVENT_STOP:
                this->retryCount = 0;
                this->state = WIFI_CLIENT_STATE_DISCONNECTED;
                xEventGroupSetBits(this->wifiEventGroup, WIFI_DISCONNECTED);
                ESP_LOGI(TAG, "Ethernet stopped");
                break;
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG, "Ethernet link up");
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                this->state = WIFI_CLIENT_STATE_FAILED;
                xEventGroupSetBits(this->wifiEventGroup, WIFI_DISCONNECTED);
                ESP_LOGI(TAG, "Ethernet link down");
                break;
        }
        xSemaphoreGive(this->clientMutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain mutex");
    }
}

// IP event handler — mirrors the WiFi got-IP handler
static void _GotIpHandler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    WifiClient *this = (WifiClient *)arg;
    assert(this);
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

    if(xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        this->retryCount = 0;
        this->state = WIFI_CLIENT_STATE_CONNECTED;
        xEventGroupSetBits(this->wifiEventGroup, WIFI_CONNECTED);
        xSemaphoreGive(this->clientMutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain mutex");
    }
}
