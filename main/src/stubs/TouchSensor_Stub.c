
#include <string.h>

#include "esp_log.h"
#include "TouchSensor.h"

static const char *TAG = "TOUCH_STUB";

esp_err_t TouchSensor_Init(TouchSensor *this, NotificationDispatcher *pNotificationDispatcher)
{
    assert(this);
    assert(pNotificationDispatcher);
    memset(this, 0, sizeof(TouchSensor));
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->touchEnabled = false;
    ESP_LOGI(TAG, "QEMU stub: TouchSensor_Init (no-op)");
    return ESP_OK;
}

int TouchSensor_GetTouchSensorActive(TouchSensor *this, int pad_num)
{
    assert(this);
    (void)pad_num;
    return 0;
}

esp_err_t TouchSensor_SetTouchEnabled(TouchSensor *this, bool enabled)
{
    assert(this);
    this->touchEnabled = enabled;
    ESP_LOGI(TAG, "QEMU stub: TouchSensor_SetTouchEnabled(%d)", enabled);
    return ESP_OK;
}
