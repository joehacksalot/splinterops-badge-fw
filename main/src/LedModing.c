
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

    if (this->curStatusIndicator != LED_STATUS_INDICATOR_NONE)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Status for status %d", this->curStatusIndicator);
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_STATUS_INDICATOR);
    }
    else if (this->ledSequencePreviewActive)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Sequence Preview");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_NORMAL);
    }
    else if (this->bleXferInProgress)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Ble Xfer");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_BLE_XFER_PERCENT);
    }
    else if (this->bleConnected)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Ble Xfer Connected");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_BLE_XFER_CONNECTED);
    }
    else if (this->bleEnabled)
    {
        ESP_LOGI(TAG, "Setting Led Mode to Ble Xfer Enabled");
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_BLE_XFER_ENABLED);
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
        ret = LedControl_SetLedMode(this->pLedControl, LED_MODE_NORMAL);
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

esp_err_t LedModing_SetStatusIndicator(LedModing *this, LedStatusIndicator ledStatusIndicator)
{
    this->curStatusIndicator = ledStatusIndicator;
    return LedMode_SetLedMode(this);
}

esp_err_t LedModing_SetBleXferEnableActive(LedModing *this, bool active)
{
    this->bleEnabled = active;
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

esp_err_t LedModing_SetBleXferActive(LedModing *this, bool active)
{
    esp_err_t ret = ESP_OK;
    if (this->bleXferInProgress != active )
    {
        this->bleXferInProgress = active;
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


