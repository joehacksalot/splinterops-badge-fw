
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "BatterySensor.h"

static const char *TAG = "BAT_STUB";

#define STUB_BATTERY_PERCENT (75)
#define STUB_BATTERY_VOLTAGE (3.8f)

esp_err_t BatterySensor_Init(BatterySensor *this, NotificationDispatcher *pNotificationDispatcher)
{
    assert(this);
    assert(pNotificationDispatcher);
    memset(this, 0, sizeof(BatterySensor));
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->batteryPercent = STUB_BATTERY_PERCENT;
    this->batteryVoltage = STUB_BATTERY_VOLTAGE;
    this->batteryPercentMutex = xSemaphoreCreateMutex();
    this->initialized = true;
    ESP_LOGI(TAG, "QEMU stub: BatterySensor_Init (returning %d%%)", STUB_BATTERY_PERCENT);
    return ESP_OK;
}

int BatterySensor_GetBatteryPercent(BatterySensor *this)
{
    assert(this);
    return STUB_BATTERY_PERCENT;
}
