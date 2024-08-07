#include <string.h>

#include "esp_log.h"

#include "LedModing.h"
#include "LedControl.h"
#include "LedSequences.h"

static const char *TAG = "MOD";

esp_err_t LedModing_Init(LedModing *this, LedControl *pLedControl)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    this->pLedControl = pLedControl;
    return ESP_OK;
}

static esp_err_t LedMode_SetLedMode(LedModing *this)
{
    esp_err_t ret = ESP_OK;
    assert(this);

    // if (this->curStatusIndicator != LED_STATUS_INDICATOR_NONE)
    // {
    //     ESP_LOGI(TAG, "Setting Led Mode to Status for status %d", this->curStatusIndicator);
    //     ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_STATUS_INDICATOR);
    // }
    // else 
    if (this->bleReconnecting)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Ble Reconnecting Mode");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_BLE_RECONNECTING);
    }
    else if (this->ledGameInteractiveActive)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Interactive Game Mode");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_INTERACTIVE_GAME);
    }
    else if (this->songActiveStatus)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Song Mode");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_SONG);
    }
    else if (this->ledSequencePreviewActive)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Sequence Preview");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_SEQUENCE);
    }
    else if (this->otaDownloadInitiatedActive)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Ota Download");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_OTA_DOWNLOAD_IP);
    }
    else if (this->bleFileTransferInProgress)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Ble File Transfer In Progress");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_BLE_FILE_TRANSFER_PERCENT);
    }
    else if (this->bleConnected)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Ble Service Connected");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_BLE_FILE_TRANSFER_CONNECTED);
    }
    else if (this->bleServiceEnabled)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Ble Service Enabled");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_BLE_FILE_TRANSFER_ENABLED);
    }
    else if (this->networkTestActive)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Network Test");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_NETWORK_TEST);
    }
    else if (this->batteryIndicatorActive)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Battery");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_BATTERY);
    }
    else if (this->touchActive)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Touch");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_TOUCH);
    }
    else if (this->gameEventActive)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Event");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_EVENT);
    }
    else if (this->ledGameStatusActive)
    {
        ESP_LOGI(TAG, "Setting Led Game Status");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_GAME_STATUS);
    }
    else
    {
        ESP_LOGI(TAG, "Setting Led Mode to Normal");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_SEQUENCE);
    }
    return ret;
}

esp_err_t LedModing_SetTouchActive(LedModing *this, bool active)
{
    this->touchActive = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetGameEventActive(LedModing *this, bool active)
{
    this->gameEventActive = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetBleServiceEnableActive(LedModing *this, bool active)
{
    this->bleServiceEnabled = active;
    return LedMode_SetLedMode(this);
}
esp_err_t LedModing_SetBleConnectedActive(LedModing *this, bool active)
{
    this->bleConnected = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetBatteryIndicatorActive(LedModing *this, bool active)
{
    this->batteryIndicatorActive = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetBleReconnectingActive(LedModing *this, bool active)
{
    this->bleReconnecting = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetOtaDownloadInitiatedActive(LedModing *this, bool active)
{
    esp_err_t ret = ESP_OK;
    if (this->otaDownloadInitiatedActive != active )
    {
        this->otaDownloadInitiatedActive = active;
        ret = LedMode_SetLedMode(this);
    }
    return ret;
}

esp_err_t LedModing_SetBleFileTransferIPActive(LedModing *this, bool active)
{
    esp_err_t ret = ESP_OK;
    if (this->bleFileTransferInProgress != active )
    {
        this->bleFileTransferInProgress = active;
        ret = LedMode_SetLedMode(this);
    }
    return ret;
}

esp_err_t LedModing_SetNetworkTestActive(LedModing *this, bool active)
{
    this->networkTestActive = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetLedCustomSequence(LedModing *this, int newCustomIndex)
{
    return LedControl_SetLedCustomSequence(this->pLedControl, newCustomIndex);
}

esp_err_t LedModing_CycleSelectedLedSequence(LedModing *this, bool direction)
{
    return LedControl_CycleSelectedLedSequence(this->pLedControl, direction);
}

esp_err_t LedModing_SetLedSequencePreviewActive(LedModing *this, bool active)
{
    this->ledSequencePreviewActive = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetGameStatusActive(LedModing *this, bool active)
{
    this->ledGameStatusActive = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetInteractiveGameActive(LedModing *this, bool active)
{
    this->ledGameInteractiveActive = active;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetSongActiveStatusActive(LedModing *this, bool active)
{
    this->songActiveStatus = active;
    return LedMode_SetLedMode(this);
}


