
#include <string.h>

#include "esp_log.h"

#include "WifiClient.h"
#include "NotificationDispatcher.h"
#include "UserSettings.h"
#include "TaskPriorities.h"
#include "TimeUtils.h"
#include "Utilities.h"

// Define events for wifi
#define WIFI_CONNECTED          BIT0
#define WIFI_DISCONNECTED       BIT1
#define WIFI_MUTEX_TIMEOUT_MS   5000

// Define wifi scan settings
#define WIFI_SCAN_LIST_SIZE     16      // Number of APs to scan for
#define WIFI_SCAN_RSSI_MINIMUM  -127

// Internal Function Declarations
static void WifiIpEventHandler(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data);

// Internal Constants
static const char * TAG = "wifi_client";

// Internal Function Declarations
static void _WifiTask(void *pvParameters);
void _WifiClient_Enable(WifiClient *this);

static void _WifiClient_Print_Authmode(int authmode)
{
    switch (authmode)
    {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    case WIFI_AUTH_WAPI_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WAPI_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

// Should be called by app_main on boot to prevent race condition on initial initialization
esp_err_t WifiClient_Init(WifiClient *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings)
{
    esp_err_t retVal = ESP_FAIL;
    assert(this);
    assert(pNotificationDispatcher);
    assert(pUserSettings);

    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pUserSettings = pUserSettings;
    this->state = WIFI_CLIENT_STATE_DISCONNECTED;

    this->clientMutex = xSemaphoreCreateMutex();

    // Create event group
    this->wifiEventGroup = xEventGroupCreate();

    // Initialize TCP/IP Stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create event loop for ???
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    this->wifiStationConfig = esp_netif_create_default_wifi_sta();
    if(this->wifiStationConfig == NULL)
    {
        ESP_LOGE(TAG, "Failed to create wifi station");
        retVal = ESP_ERR_NO_MEM;
    }
    else
    {
        // Initialize and allocate resources for driver. Starts WiFi task
        wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wifiInitConfig));

        // Subscribe to wifi events
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &WifiIpEventHandler,
                                                            this,
                                                            &this->instanceAnyId));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &WifiIpEventHandler,
                                                            this,
                                                            &this->instanceGotIp));

        // Create configuration
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

        // Change the power saving mode to WiFi power saving
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

        ESP_LOGI(TAG, "Initialize finished!");
        retVal = ESP_OK;
    }

    xTaskCreate(_WifiTask, "WifiClientTask", configMINIMAL_STACK_SIZE * 4, this, WIFI_CONTROL_TASK_PRIORITY, NULL);

    return retVal;
}

// Assumes the mutex is already taken
void _WifiClient_Enable(WifiClient *this)
{
    if(this->state == WIFI_CLIENT_STATE_DISCONNECTED || 
       this->state == WIFI_CLIENT_STATE_WAITING ||
       this->state == WIFI_CLIENT_STATE_FAILED)
    {
        // Needed stop or else subsequent starts may not work
        ESP_ERROR_CHECK(esp_wifi_stop());

        memset((char*)this->wifiConfig.sta.ssid, 0, sizeof(this->wifiConfig.sta.ssid));
        memset((char*)this->wifiConfig.sta.password, 0, sizeof(this->wifiConfig.sta.password));
        this->wifiConfig.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        this->wifiConfig.sta.threshold.rssi = WIFI_SCAN_RSSI_MINIMUM;
        this->wifiConfig.sta.threshold.authmode = WIFI_AUTH_OPEN;        // we accept all APs
        
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &this->wifiConfig));

        // Scan for strongest AP, compare AP names to list stored
        wifi_ap_record_t ap_info[WIFI_SCAN_LIST_SIZE];
        uint16_t ap_scan_count = WIFI_SCAN_LIST_SIZE;   // this will get updated by api later
        memset(ap_info, 0, sizeof(ap_info));

        esp_err_t ret = esp_wifi_start();
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Attempting to start wifi(%d)", this->state);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to start WiFi. error code = %s", esp_err_to_name(ret));
        }

        // TODO: Change this to use actual AP list from user
        // CONFIG_WIFI_SSID
        // CONFIG_WIFI_PASSWORD
        char TEST_SSID[sizeof(this->pUserSettings->settings.wifiSettings.ssid)];
        char TEST_PASS[sizeof(this->pUserSettings->settings.wifiSettings.password)];
        strncpy(TEST_SSID, this->pUserSettings->settings.wifiSettings.ssid, sizeof(TEST_SSID));
        strncpy(TEST_PASS, this->pUserSettings->settings.wifiSettings.password, sizeof(TEST_PASS));

        ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_scan_count, ap_info));  // esp_wifi_scan_get_ap_records clears memory allocated from scan_start
        ESP_LOGI(TAG, "Total APIs scanned: %d", ap_scan_count);

        for(uint32_t i = 0; i < ap_scan_count; ++i)
        {
            if(strncmp((char*)ap_info[i].ssid, CONFIG_WIFI_SSID, sizeof(ap_info[i].ssid)) == 0)
            {
                ESP_LOGI(TAG, "Found AP(%s)", CONFIG_WIFI_SSID);
                strncpy((char*)this->wifiConfig.sta.ssid, CONFIG_WIFI_SSID, sizeof(this->wifiConfig.sta.ssid));
                strncpy((char*)this->wifiConfig.sta.password, CONFIG_WIFI_PASSWORD, sizeof(this->wifiConfig.sta.password));
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &this->wifiConfig));
                ESP_ERROR_CHECK(esp_wifi_start());
                break;
            }
            else if(strncmp((char*)ap_info[i].ssid, TEST_SSID, sizeof(ap_info[i].ssid)) == 0)
            {
                ESP_LOGI(TAG, "Found AP(%s)", TEST_SSID);
                strncpy((char*)this->wifiConfig.sta.ssid, TEST_SSID, sizeof(this->wifiConfig.sta.ssid));
                strncpy((char*)this->wifiConfig.sta.password, TEST_PASS, sizeof(this->wifiConfig.sta.password));
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &this->wifiConfig));
                ESP_ERROR_CHECK(esp_wifi_start());
                break;
            }
        }

        this->state = WIFI_CLIENT_STATE_ATTEMPTING;
    }
}

static void _WifiTask(void *pvParameters)
{
    WifiClient *this = (WifiClient *)pvParameters;
    assert(this);
    assert(this->clientMutex);
    registerCurrentTaskInfo();

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
                    ESP_LOGI(TAG, "WifiClient_RequestConnect: pending request started: %d", waitTimeMS);
                }
            }
            // Handle tick rollovers by using pendingStartTime as the reference
            else if(this->state == WIFI_CLIENT_STATE_WAITING &&
                    (int)((this->pendingStartTime + pdMS_TO_TICKS(waitTimeMS)) - this->desiredStartTime) < 0)
            {
                // Wifi hasn't started but someone wants to start sooner
                this->desiredStartTime = this->pendingStartTime + pdMS_TO_TICKS(waitTimeMS);

                // TODO: Change this to DEBUG
                ESP_LOGI(TAG, "WifiClient_RequestConnect: pending request shortened: %d", waitTimeMS);
            }
        }

        // TODO: Change this to DEBUG
        ESP_LOGI(TAG, "WifiClient_RequestConnect: numClients(%d)", this->numClients);
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
        ESP_LOGE(TAG, "Unknown event bits: %d", bits);
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
    WifiClient_State state = WifiClient_RequestConnect(this, 0);
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
        // Only tear down wifi when there are no more active clients
        if(--this->numClients <= 0)
        {
            if(this->state == WIFI_CLIENT_STATE_ATTEMPTING ||
               this->state == WIFI_CLIENT_STATE_CONNECTING ||
               this->state == WIFI_CLIENT_STATE_CONNECTED)
            {
                // Tear down wifi
                ret = esp_wifi_stop();
                if (ret == ESP_OK)
                {
                    ESP_LOGI(TAG, "Disconnecting from AP(%s)", this->wifiConfig.sta.ssid);
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to disconnect from AP(%s). error code = %s", this->wifiConfig.sta.ssid, esp_err_to_name(ret));
                }

                // The handler will handle changing wifi state to the final state
            }

            this->numClients = 0; // Paranoia set just in case
        }

        xSemaphoreGive(this->clientMutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain mutex");
    }
    return ret;
}

// Assuming this handler is being called serially
static void WifiIpEventHandler(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
    WifiClient *this = (WifiClient *)arg;
    assert(this);

    if(xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        // Attempt to connect to AP
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
        {
            this->retryCount = 0;
            this->state = WIFI_CLIENT_STATE_CONNECTING;
            esp_wifi_connect();
            ESP_LOGI(TAG, "start commanded");
        }
        else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP)
        {
            // Set the retry to max to prevent further retry attempts
            this->retryCount = CONFIG_WIFI_MAX_RETRY;
            this->state = WIFI_CLIENT_STATE_DISCONNECTED;
            xEventGroupSetBits(this->wifiEventGroup, WIFI_DISCONNECTED);
            ESP_LOGI(TAG, "stop commanded");
        }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            // Attempt to reconnect if we disconnect or it fails
            if (this->retryCount < CONFIG_WIFI_MAX_RETRY) 
            {
                ++this->retryCount;
                this->state = WIFI_CLIENT_STATE_CONNECTING;
                esp_wifi_connect();
                ESP_LOGI(TAG, "retry(%d) connect to AP", this->retryCount);
            }
            else
            {
                this->state = WIFI_CLIENT_STATE_FAILED;
                xEventGroupSetBits(this->wifiEventGroup, WIFI_DISCONNECTED);
                ESP_LOGI(TAG, "failed to connect to AP");
            }
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&event->ip_info.ip));
            this->retryCount = 0;

            xEventGroupSetBits(this->wifiEventGroup, WIFI_CONNECTED);
            this->state = WIFI_CLIENT_STATE_CONNECTED;
        }

        xSemaphoreGive(this->clientMutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain mutex");
    }
}
