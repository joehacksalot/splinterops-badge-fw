/*
 * SPDX-FileCopyrightText: 2025 SplinterOps
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Shared UART2 transport for QEMU visualization.
 * Multiplexes LED frame output (TX) and touch event input (RX)
 * over a single UART channel mapped to a QEMU TCP chardev.
 */

#include "sdkconfig.h"

#ifdef CONFIG_BADGE_QEMU_MODE

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"

#include "freertos/semphr.h"
#include "qemu_viz_transport.h"

#define TAG "VIZ_TRANSPORT"

/* UART configuration */
#define VIZ_UART_NUM        UART_NUM_2
#define VIZ_UART_BAUD       921600
#define VIZ_UART_TX_PIN     17   /* arbitrary — QEMU ignores pin mux */
#define VIZ_UART_RX_PIN     16
#define VIZ_UART_BUF_SIZE   2048
#define VIZ_RX_TASK_STACK   4096
#define VIZ_RX_TASK_PRIO    10

/* Maximum payload we expect to receive (touch events are small) */
#define VIZ_RX_MAX_PAYLOAD  16

/* Maximum number of registered RX handlers */
#define VIZ_MAX_RX_HANDLERS 8

/* RX handler registry */
typedef struct {
    uint8_t msg_type;
    qemu_viz_rx_callback_t cb;
} viz_rx_handler_t;

static viz_rx_handler_t s_rx_handlers[VIZ_MAX_RX_HANDLERS];
static int s_rx_handler_count = 0;
static bool s_initialized = false;
static TaskHandle_t s_rx_task_handle = NULL;
static SemaphoreHandle_t s_tx_mutex = NULL;

/* ------------------------------------------------------------------ */
/* RX task — reads framed messages from UART2 and dispatches           */
/* ------------------------------------------------------------------ */

static void viz_rx_task(void *param)
{
    ESP_LOGI(TAG, "Viz RX task started");
    uint8_t byte;

    while (1) {
        /* Wait for frame start marker */
        int n = uart_read_bytes(VIZ_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (n <= 0) {
            continue;
        }
        if (byte != QEMU_VIZ_FRAME_START) {
            continue;
        }

        /* Read message type */
        n = uart_read_bytes(VIZ_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (n <= 0) {
            continue;
        }
        uint8_t msg_type = byte;

        /* Determine payload length based on message type */
        size_t payload_len = 0;
        switch (msg_type) {
        case QEMU_VIZ_MSG_TOUCH_EVENT:
            payload_len = 2; /* sensor_idx + event_type */
            break;
        default:
            ESP_LOGW(TAG, "Unknown RX msg type: 0x%02x", msg_type);
            continue;
        }

        /* Read payload */
        uint8_t payload[VIZ_RX_MAX_PAYLOAD];
        if (payload_len > 0) {
            int remaining = payload_len;
            int offset = 0;
            while (remaining > 0) {
                n = uart_read_bytes(VIZ_UART_NUM, payload + offset, remaining, pdMS_TO_TICKS(200));
                if (n > 0) {
                    offset += n;
                    remaining -= n;
                } else if (n == 0) {
                    continue;
                } else {
                    ESP_LOGE(TAG, "UART read error in payload");
                    break;
                }
            }
            if (remaining > 0) {
                continue; /* incomplete read */
            }
        }

        /* Read frame end marker */
        n = uart_read_bytes(VIZ_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (n <= 0 || byte != QEMU_VIZ_FRAME_END) {
            ESP_LOGW(TAG, "Missing frame end marker");
            continue;
        }

        /* Dispatch to registered handler */
        for (int i = 0; i < s_rx_handler_count; i++) {
            if (s_rx_handlers[i].msg_type == msg_type && s_rx_handlers[i].cb) {
                s_rx_handlers[i].cb(msg_type, payload, payload_len);
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t qemu_viz_transport_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing QEMU viz transport on UART%d", VIZ_UART_NUM);

    uart_config_t uart_config = {
        .baud_rate  = VIZ_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(VIZ_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(VIZ_UART_NUM, VIZ_UART_TX_PIN, VIZ_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(VIZ_UART_NUM, VIZ_UART_BUF_SIZE * 2,
                                        VIZ_UART_BUF_SIZE * 2, 0, NULL, 0));

    /* Create TX mutex for atomic multi-part sends */
    s_tx_mutex = xSemaphoreCreateMutex();
    assert(s_tx_mutex);

    /* Start receive task */
    BaseType_t ret = xTaskCreate(viz_rx_task, "viz_rx", VIZ_RX_TASK_STACK,
                                 NULL, VIZ_RX_TASK_PRIO, &s_rx_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create viz RX task");
        uart_driver_delete(VIZ_UART_NUM);
        return ESP_FAIL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "QEMU viz transport initialized");
    return ESP_OK;
}

int qemu_viz_send(const uint8_t *data, size_t len)
{
    if (!s_initialized) {
        return -1;
    }
    int written = uart_write_bytes(VIZ_UART_NUM, data, len);
    if (written < 0) {
        ESP_LOGE(TAG, "uart_write_bytes failed");
    }
    return written;
}

void qemu_viz_tx_lock(void)
{
    if (s_tx_mutex) {
        xSemaphoreTake(s_tx_mutex, portMAX_DELAY);
    }
}

void qemu_viz_tx_unlock(void)
{
    if (s_tx_mutex) {
        xSemaphoreGive(s_tx_mutex);
    }
}

esp_err_t qemu_viz_register_rx_handler(uint8_t msg_type, qemu_viz_rx_callback_t cb)
{
    if (s_rx_handler_count >= VIZ_MAX_RX_HANDLERS) {
        ESP_LOGE(TAG, "Too many RX handlers");
        return ESP_ERR_NO_MEM;
    }
    s_rx_handlers[s_rx_handler_count].msg_type = msg_type;
    s_rx_handlers[s_rx_handler_count].cb = cb;
    s_rx_handler_count++;
    return ESP_OK;
}

#endif /* CONFIG_BADGE_QEMU_MODE */
