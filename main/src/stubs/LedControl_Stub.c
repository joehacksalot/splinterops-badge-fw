
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "LedControl.h"

static const char *TAG = "LED_STUB";

esp_err_t LedControl_Init(LedControl *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings, BatterySensor *pBatterySensor, GameState *pGameState, uint32_t batteryIndicatorHoldTime)
{
    assert(this);
    memset(this, 0, sizeof(LedControl));
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pUserSettings = pUserSettings;
    this->pBatterySensor = pBatterySensor;
    this->pGameState = pGameState;
    this->jsonMutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "QEMU stub: LedControl_Init (no hardware)");
    return ESP_OK;
}

esp_err_t LedControl_SetInnerLedState(LedControl *this, InnerLedState state)
{
    assert(this);
    ESP_LOGD(TAG, "QEMU stub: SetInnerLedState(%d)", state);
    return ESP_OK;
}

esp_err_t LedControl_SetOuterLedState(LedControl *this, OuterLedState state)
{
    assert(this);
    ESP_LOGD(TAG, "QEMU stub: SetOuterLedState(%d)", state);
    return ESP_OK;
}

esp_err_t LedControl_SetLedCustomSequence(LedControl *this, int customIndex)
{
    assert(this);
    ESP_LOGD(TAG, "QEMU stub: SetLedCustomSequence(%d)", customIndex);
    return ESP_OK;
}

esp_err_t LedControl_SetLedMode(LedControl *this, LedMode mode)
{
    assert(this);
    ESP_LOGD(TAG, "QEMU stub: SetLedMode(%d)", mode);
    return ESP_OK;
}

esp_err_t LedControl_SetCurrentLedSequenceIndex(LedControl *this, int sequenceIndex)
{
    assert(this);
    this->selectedIndex = sequenceIndex;
    ESP_LOGD(TAG, "QEMU stub: SetCurrentLedSequenceIndex(%d)", sequenceIndex);
    return ESP_OK;
}

esp_err_t LedControl_CycleSelectedLedSequence(LedControl *this, bool direction)
{
    assert(this);
    ESP_LOGD(TAG, "QEMU stub: CycleSelectedLedSequence(%d)", direction);
    return ESP_OK;
}

int LedControl_GetCurrentLedSequenceIndex(LedControl *this)
{
    assert(this);
    return this->selectedIndex;
}

void LedControl_SetTouchSensorUpdate(LedControl *this, TouchSensorEvent touchSensorEvent, int touchSensorIdx)
{
    assert(this);
    ESP_LOGD(TAG, "QEMU stub: SetTouchSensorUpdate(%d, %d)", touchSensorEvent, touchSensorIdx);
}
