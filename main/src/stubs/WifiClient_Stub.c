
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "WifiClient.h"

static const char *TAG = "WIFI_STUB";

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
    ESP_LOGI(TAG, "QEMU stub: WifiClient_Init (no WiFi radio)");
    return ESP_OK;
}

WifiClient_State WifiClient_Enable(WifiClient *this)
{
    assert(this);
    ESP_LOGI(TAG, "QEMU stub: WifiClient_Enable");
    return WIFI_CLIENT_STATE_DISCONNECTED;
}

WifiClient_State WifiClient_RequestConnect(WifiClient *this, uint32_t waitTimeMS)
{
    assert(this);
    ESP_LOGI(TAG, "QEMU stub: WifiClient_RequestConnect (wait=%lu ms)", waitTimeMS);
    return WIFI_CLIENT_STATE_DISCONNECTED;
}

esp_err_t WifiClient_Disconnect(WifiClient *this)
{
    assert(this);
    ESP_LOGI(TAG, "QEMU stub: WifiClient_Disconnect");
    return ESP_OK;
}

esp_err_t WifiClient_WaitForConnected(WifiClient *this)
{
    assert(this);
    ESP_LOGI(TAG, "QEMU stub: WifiClient_WaitForConnected (returning FAIL - no WiFi)");
    return ESP_FAIL;
}

WifiClient_State WifiClient_GetState(WifiClient *this)
{
    assert(this);
    return this->state;
}

void WifiClient_TestConnect(WifiClient *this)
{
    assert(this);
    ESP_LOGI(TAG, "QEMU stub: WifiClient_TestConnect (no-op)");
}
