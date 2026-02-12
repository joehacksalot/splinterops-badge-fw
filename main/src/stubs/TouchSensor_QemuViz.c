/*
 * SPDX-FileCopyrightText: 2025 SplinterOps
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Enhanced touch sensor stub for QEMU visualization mode.
 * Receives touch commands from the visualization UI over UART2
 * and fires the same notification events as the real TouchSensor.c.
 *
 * Replaces TouchSensor_Stub.c in QEMU mode.
 */

#include "sdkconfig.h"

#ifdef CONFIG_BADGE_QEMU_MODE

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "TouchSensor.h"
#include "NotificationDispatcher.h"
#include "qemu_viz_transport.h"

#define TAG "TOUCH_QEMU_VIZ"

/* Module state — we need access to the TouchSensor instance from the RX callback */
static TouchSensor *s_touch_sensor = NULL;

/* ------------------------------------------------------------------ */
/* RX callback — invoked by viz transport when a touch event arrives    */
/* ------------------------------------------------------------------ */

static void touch_rx_callback(uint8_t msg_type, const uint8_t *data, size_t len)
{
    if (msg_type != QEMU_VIZ_MSG_TOUCH_EVENT || len < 2) {
        return;
    }
    if (!s_touch_sensor || !s_touch_sensor->pNotificationDispatcher) {
        ESP_LOGW(TAG, "Touch sensor not initialized, ignoring event");
        return;
    }

    uint8_t sensor_idx = data[0];
    uint8_t event_type = data[1];

    if (sensor_idx >= TOUCH_SENSOR_NUM_BUTTONS) {
        ESP_LOGW(TAG, "Invalid sensor index: %d", sensor_idx);
        return;
    }
    if (event_type > TOUCH_SENSOR_EVENT_VERY_LONG_PRESSED) {
        ESP_LOGW(TAG, "Invalid event type: %d", event_type);
        return;
    }

    ESP_LOGI(TAG, "Touch event: sensor=%d event=%d", sensor_idx, event_type);

    /* Update internal state */
    s_touch_sensor->touchSensorActive[sensor_idx] = event_type;
    s_touch_sensor->touchSensorActiveTimeStamp[sensor_idx] = xTaskGetTickCount();

    /* Fire notification — same as real TouchSensor.c */
    TouchSensorEventNotificationData notificationData = {
        .touchSensorIdx = sensor_idx,
        .touchSensorEvent = (TouchSensorEvent)event_type
    };

    esp_err_t ret = NotificationDispatcher_NotifyEvent(
        s_touch_sensor->pNotificationDispatcher,
        NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION,
        &notificationData,
        sizeof(notificationData),
        DEFAULT_NOTIFY_WAIT_DURATION
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NotificationDispatcher_NotifyEvent failed: %s", esp_err_to_name(ret));
    }
}

/* ------------------------------------------------------------------ */
/* Public API (same signatures as TouchSensor.c / TouchSensor_Stub.c)  */
/* ------------------------------------------------------------------ */

esp_err_t TouchSensor_Init(TouchSensor *this, NotificationDispatcher *pNotificationDispatcher)
{
    assert(this);
    assert(pNotificationDispatcher);

    memset(this, 0, sizeof(TouchSensor));
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->touchEnabled = false;

    /* Store reference for the RX callback */
    s_touch_sensor = this;

    /* Ensure the shared viz transport is initialized.
     * Normally led_strip_qemu.c does this during LedControl_Init (called first),
     * but we call it here too for safety — it's a no-op if already initialized. */
    qemu_viz_transport_init();

    /* Register our handler with the viz transport. */
    esp_err_t ret = qemu_viz_register_rx_handler(QEMU_VIZ_MSG_TOUCH_EVENT, touch_rx_callback);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register touch RX handler: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "QEMU viz: TouchSensor_Init (receiving touch events via UART2)");
    return ESP_OK;
}

int TouchSensor_GetTouchSensorActive(TouchSensor *this, int pad_num)
{
    assert(this);
    if (pad_num >= TOUCH_SENSOR_NUM_BUTTONS) {
        ESP_LOGE(TAG, "Invalid pad_num: %d", pad_num);
        return 0;
    }
    return this->touchSensorActiveTimeStamp[pad_num];
}

esp_err_t TouchSensor_SetTouchEnabled(TouchSensor *this, bool enabled)
{
    assert(this);
    this->touchEnabled = enabled;
    ESP_LOGI(TAG, "QEMU viz: TouchSensor_SetTouchEnabled(%d)", enabled);
    return ESP_OK;
}

#endif /* CONFIG_BADGE_QEMU_MODE */
