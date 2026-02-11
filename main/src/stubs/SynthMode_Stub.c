
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "SynthMode.h"

static const char *TAG = "SYNTH_STUB";

esp_err_t SynthMode_Init(SynthMode *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings)
{
    assert(this);
    memset(this, 0, sizeof(SynthMode));
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pUserSettings = pUserSettings;
    this->queueMutex = xSemaphoreCreateMutex();
    this->toneMutex = xSemaphoreCreateMutex();
    this->initialized = true;
    ESP_LOGI(TAG, "QEMU stub: SynthMode_Init (no DAC/PWM)");
    return ESP_OK;
}

esp_err_t SynthMode_SetTouchSoundEnabled(SynthMode *this, bool enabled, int octaveShift)
{
    assert(this);
    this->touchSoundEnabled = enabled;
    this->octaveShift = octaveShift;
    ESP_LOGI(TAG, "QEMU stub: SynthMode_SetTouchSoundEnabled(%d, %d)", enabled, octaveShift);
    return ESP_OK;
}

bool SynthMode_GetTouchSoundEnabled(SynthMode *this)
{
    assert(this);
    return this->touchSoundEnabled;
}
