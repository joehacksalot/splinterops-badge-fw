/*
 * SPDX-FileCopyrightText: 2025 SplinterOps
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Test injection handler for QEMU visualization mode.
 * Receives inject commands from the viz portal control panel via UART2
 * and fires the corresponding notification events in the firmware.
 *
 * Currently supported sub-commands:
 *   0x01 — SEND_HEARTBEAT: fires NOTIFICATION_EVENTS_SEND_HEARTBEAT
 */

#include "sdkconfig.h"

#ifdef CONFIG_BADGE_QEMU_MODE

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "NotificationDispatcher.h"
#include "SystemState.h"
#include "qemu_viz_transport.h"

#define TAG "QEMU_TEST_INJECT"

/* ------------------------------------------------------------------ */
/* RX callback — invoked by viz transport when a test inject arrives    */
/* ------------------------------------------------------------------ */

static void test_inject_rx_callback(uint8_t msg_type, const uint8_t *data, size_t len)
{
    if (msg_type != QEMU_VIZ_MSG_TEST_INJECT || len < 1) {
        return;
    }

    SystemState *sys = SystemState_GetInstance();
    if (!sys) {
        ESP_LOGW(TAG, "SystemState not available, ignoring inject");
        return;
    }

    uint8_t sub_cmd = data[0];

    switch (sub_cmd) {
    case QEMU_VIZ_INJECT_CMD_SEND_HEARTBEAT:
        ESP_LOGI(TAG, "Inject: SEND_HEARTBEAT");
        NotificationDispatcher_NotifyEvent(
            &sys->notificationDispatcher,
            NOTIFICATION_EVENTS_SEND_HEARTBEAT,
            NULL, 0,
            DEFAULT_NOTIFY_WAIT_DURATION
        );
        break;

    default:
        ESP_LOGW(TAG, "Unknown inject sub-command: 0x%02x", sub_cmd);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Initialization — called from SystemState_Init or AppMain            */
/* ------------------------------------------------------------------ */

void QemuTestInject_Init(void)
{
    /* Ensure the shared viz transport is initialized (no-op if already done) */
    qemu_viz_transport_init();

    esp_err_t ret = qemu_viz_register_rx_handler(QEMU_VIZ_MSG_TEST_INJECT, test_inject_rx_callback);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register test inject RX handler: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "QEMU test injection handler initialized");
}

#endif /* CONFIG_BADGE_QEMU_MODE */
