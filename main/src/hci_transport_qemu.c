/*
 * SPDX-FileCopyrightText: 2025 SplinterOps
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * HCI H4 transport over UART for QEMU BLE emulation.
 *
 * This module replaces esp_nimble_hci when running under QEMU.  It talks
 * HCI H4 (UART transport) over UART1, which QEMU maps to a TCP chardev
 * connected to an external virtual BLE controller (e.g. Google Bumble).
 *
 * The NimBLE host stack calls the ble_transport_to_ll_*_impl() functions
 * defined here to send HCI commands and ACL data.  A dedicated FreeRTOS task reads
 * incoming H4 frames from UART1 and dispatches them to the host via the
 * registered callbacks.
 */

#include <string.h>
#include <assert.h>

#include "sdkconfig.h"

#ifdef CONFIG_BADGE_QEMU_MODE

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "driver/uart.h"

/* NimBLE host transport API */
#include "nimble/nimble_port.h"
#include "nimble/hci_common.h"
#include "nimble/transport.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"

#include "hci_transport_qemu.h"

#define TAG "HCI_QEMU"

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */

/* UART peripheral used for HCI transport in QEMU.
 * UART0 is the console; we use UART1 for HCI.                        */
#define HCI_UART_NUM        UART_NUM_1
#define HCI_UART_BAUD       115200
#define HCI_UART_TX_PIN     10   /* arbitrary — QEMU ignores pin mux */
#define HCI_UART_RX_PIN     9
#define HCI_UART_BUF_SIZE   1024
#define HCI_RX_TASK_STACK   4096
#define HCI_RX_TASK_PRIO    (configMAX_PRIORITIES - 2)

/* ------------------------------------------------------------------ */
/* HCI H4 packet type indicators                                       */
/* ------------------------------------------------------------------ */
#define H4_CMD   0x01
#define H4_ACL   0x02
#define H4_SCO   0x03
#define H4_EVT   0x04

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static void hci_rx_task(void *param);
static int  hci_uart_read(uint8_t *buf, int len, TickType_t timeout);
static int  hci_uart_write(const uint8_t *buf, int len);

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */
static TaskHandle_t s_rx_task_handle;
static bool         s_initialized;

/* ------------------------------------------------------------------ */
/* NimBLE transport callbacks (host → controller)                      */
/* ------------------------------------------------------------------ */

/*
 * Send an HCI command from the host to the controller.
 * The buffer was allocated by ble_hci_trans_buf_alloc(); we must free it.
 */
int ble_transport_to_ll_cmd_impl(void *buf)
{
    uint8_t *cmd = (uint8_t *)buf;
    uint8_t h4_type = H4_CMD;
    /* HCI command: opcode (2) + param_len (1) + params */
    uint8_t param_len = cmd[2];
    int total = 3 + param_len;

    ESP_LOGD(TAG, "TX CMD opcode=0x%02x%02x len=%d", cmd[1], cmd[0], param_len);

    hci_uart_write(&h4_type, 1);
    hci_uart_write(cmd, total);

    ble_transport_free(buf);
    return 0;
}

/*
 * Send ACL data from the host to the controller.
 * The mbuf chain is freed after transmission.
 */
int ble_transport_to_ll_acl_impl(struct os_mbuf *om)
{
    uint8_t h4_type = H4_ACL;
    uint16_t total_len = OS_MBUF_PKTLEN(om);
    uint8_t flat[HCI_UART_BUF_SIZE];

    if (total_len > sizeof(flat)) {
        ESP_LOGE(TAG, "ACL packet too large: %u", total_len);
        os_mbuf_free_chain(om);
        return BLE_ERR_MEM_CAPACITY;
    }

    /* Flatten the mbuf chain into a contiguous buffer */
    int rc = os_mbuf_copydata(om, 0, total_len, flat);
    if (rc != 0) {
        ESP_LOGE(TAG, "os_mbuf_copydata failed");
        os_mbuf_free_chain(om);
        return BLE_ERR_MEM_CAPACITY;
    }

    ESP_LOGD(TAG, "TX ACL len=%u", total_len);

    hci_uart_write(&h4_type, 1);
    hci_uart_write(flat, total_len);

    os_mbuf_free_chain(om);
    return 0;
}

/* ------------------------------------------------------------------ */
/* UART helpers                                                        */
/* ------------------------------------------------------------------ */

static int hci_uart_write(const uint8_t *buf, int len)
{
    int written = uart_write_bytes(HCI_UART_NUM, buf, len);
    if (written < 0) {
        ESP_LOGE(TAG, "uart_write_bytes failed");
    }
    return written;
}

static int hci_uart_read(uint8_t *buf, int len, TickType_t timeout)
{
    int n = uart_read_bytes(HCI_UART_NUM, buf, len, timeout);
    return n;
}

/* Read exactly `len` bytes, blocking until available. Returns 0 on
 * success, -1 on failure.                                             */
static int hci_uart_read_exact(uint8_t *buf, int len)
{
    int remaining = len;
    while (remaining > 0) {
        int n = hci_uart_read(buf + (len - remaining), remaining,
                              pdMS_TO_TICKS(1000));
        if (n > 0) {
            remaining -= n;
        } else if (n == 0) {
            /* Timeout — keep trying */
            continue;
        } else {
            ESP_LOGE(TAG, "UART read error");
            return -1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Receive task — reads H4 frames and delivers to NimBLE host          */
/* ------------------------------------------------------------------ */

static void hci_rx_task(void *param)
{
    ESP_LOGI(TAG, "HCI RX task started");

    while (1) {
        uint8_t h4_type;
        int n = hci_uart_read(&h4_type, 1, pdMS_TO_TICKS(100));
        if (n <= 0) {
            continue;  /* timeout or no data yet */
        }

        switch (h4_type) {
        case H4_EVT: {
            /* HCI Event: header = event_code(1) + param_len(1) */
            uint8_t hdr[2];
            if (hci_uart_read_exact(hdr, 2) != 0) {
                ESP_LOGE(TAG, "Failed to read event header");
                break;
            }
            uint8_t evt_code  = hdr[0];
            uint8_t param_len = hdr[1];

            /* Allocate a flat buffer for the event via NimBLE transport */
            uint8_t *evbuf = ble_transport_alloc_evt(0);
            if (!evbuf) {
                ESP_LOGE(TAG, "Failed to allocate event buffer");
                /* Drain remaining bytes */
                uint8_t drain[256];
                hci_uart_read(drain, param_len, pdMS_TO_TICKS(100));
                break;
            }

            evbuf[0] = evt_code;
            evbuf[1] = param_len;

            if (param_len > 0) {
                if (hci_uart_read_exact(evbuf + 2, param_len) != 0) {
                    ESP_LOGE(TAG, "Failed to read event params");
                    ble_transport_free(evbuf);
                    break;
                }
            }

            ESP_LOGD(TAG, "RX EVT code=0x%02x len=%d", evt_code, param_len);
            ble_transport_to_hs_evt_impl(evbuf);
            break;
        }

        case H4_ACL: {
            /* ACL Data: header = handle(2) + data_len(2) */
            uint8_t hdr[4];
            if (hci_uart_read_exact(hdr, 4) != 0) {
                ESP_LOGE(TAG, "Failed to read ACL header");
                break;
            }
            uint16_t data_len = (uint16_t)hdr[2] | ((uint16_t)hdr[3] << 8);

            struct os_mbuf *om = ble_transport_alloc_acl_from_ll();
            if (!om) {
                ESP_LOGE(TAG, "Failed to allocate ACL mbuf");
                uint8_t drain[256];
                int left = data_len;
                while (left > 0) {
                    int chunk = left > (int)sizeof(drain) ? (int)sizeof(drain) : left;
                    hci_uart_read(drain, chunk, pdMS_TO_TICKS(100));
                    left -= chunk;
                }
                break;
            }

            /* Append the 4-byte ACL header */
            if (os_mbuf_append(om, hdr, 4) != 0) {
                ESP_LOGE(TAG, "Failed to append ACL header to mbuf");
                os_mbuf_free_chain(om);
                break;
            }

            /* Read and append payload */
            if (data_len > 0) {
                uint8_t tmp[256];
                int left = data_len;
                while (left > 0) {
                    int chunk = left > (int)sizeof(tmp) ? (int)sizeof(tmp) : left;
                    if (hci_uart_read_exact(tmp, chunk) != 0) {
                        ESP_LOGE(TAG, "Failed to read ACL payload");
                        os_mbuf_free_chain(om);
                        om = NULL;
                        break;
                    }
                    if (os_mbuf_append(om, tmp, chunk) != 0) {
                        ESP_LOGE(TAG, "Failed to append ACL data to mbuf");
                        os_mbuf_free_chain(om);
                        om = NULL;
                        break;
                    }
                    left -= chunk;
                }
                if (!om) break;
            }

            ESP_LOGD(TAG, "RX ACL len=%u", data_len);
            ble_transport_to_hs_acl_impl(om);
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown H4 type: 0x%02x", h4_type);
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* NimBLE transport init (called by nimble_port during esp_nimble_init) */
/* ------------------------------------------------------------------ */

void ble_transport_ll_init(void)
{
    /* Actual UART setup is deferred to hci_transport_qemu_init() which the
     * application calls explicitly.  This stub satisfies the linker.       */
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t hci_transport_qemu_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing QEMU HCI transport on UART%d", HCI_UART_NUM);

    /* Configure UART */
    uart_config_t uart_config = {
        .baud_rate  = HCI_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(HCI_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(HCI_UART_NUM, HCI_UART_TX_PIN, HCI_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(HCI_UART_NUM, HCI_UART_BUF_SIZE * 2,
                                        HCI_UART_BUF_SIZE * 2, 0, NULL, 0));

    /* Start receive task */
    BaseType_t ret = xTaskCreate(hci_rx_task, "hci_rx", HCI_RX_TASK_STACK,
                                 NULL, HCI_RX_TASK_PRIO, &s_rx_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HCI RX task");
        uart_driver_delete(HCI_UART_NUM);
        return ESP_FAIL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "QEMU HCI transport initialized");
    return ESP_OK;
}

esp_err_t hci_transport_qemu_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_rx_task_handle) {
        vTaskDelete(s_rx_task_handle);
        s_rx_task_handle = NULL;
    }

    uart_driver_delete(HCI_UART_NUM);
    s_initialized = false;

    ESP_LOGI(TAG, "QEMU HCI transport deinitialized");
    return ESP_OK;
}

#endif /* CONFIG_BADGE_QEMU_MODE */
