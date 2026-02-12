/*
 * SPDX-FileCopyrightText: 2025 SplinterOps
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Shared UART2 transport for QEMU visualization (LED frames + touch input).
 * Used by led_strip_qemu.c (TX) and TouchSensor_QemuViz.c (RX).
 */

#ifndef QEMU_VIZ_TRANSPORT_H_
#define QEMU_VIZ_TRANSPORT_H_

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol markers */
#define QEMU_VIZ_FRAME_START    0xAA
#define QEMU_VIZ_FRAME_END      0x55

/* Message types */
#define QEMU_VIZ_MSG_LED_FRAME      0x01
#define QEMU_VIZ_MSG_TOUCH_EVENT    0x02
#define QEMU_VIZ_MSG_MODE_CHANGE    0x03
#define QEMU_VIZ_MSG_TONE_EVENT     0x04

/* Touch event types (matches TouchSensorEvent enum) */
#define QEMU_VIZ_TOUCH_RELEASED         0
#define QEMU_VIZ_TOUCH_TOUCHED          1
#define QEMU_VIZ_TOUCH_SHORT_PRESSED    2
#define QEMU_VIZ_TOUCH_LONG_PRESSED     3
#define QEMU_VIZ_TOUCH_VERY_LONG_PRESSED 4

/* RX callback type for demuxing incoming messages */
typedef void (*qemu_viz_rx_callback_t)(uint8_t msg_type, const uint8_t *data, size_t len);

/**
 * Initialize the QEMU visualization transport on UART2.
 * Must be called once before any send/receive operations.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t qemu_viz_transport_init(void);

/**
 * Send data over the visualization transport (UART2 TX).
 *
 * @param data  Pointer to data buffer to send.
 * @param len   Number of bytes to send.
 * @return Number of bytes written, or -1 on error.
 */
int qemu_viz_send(const uint8_t *data, size_t len);

/**
 * Acquire/release the TX mutex for atomic multi-part sends.
 * Callers that need to send header + payload + footer without
 * interleaving from other tasks should wrap their qemu_viz_send()
 * calls with these.
 */
void qemu_viz_tx_lock(void);
void qemu_viz_tx_unlock(void);

/**
 * Register a callback for a specific incoming message type.
 * The RX task will parse frames and dispatch to the registered handler.
 *
 * @param msg_type  The message type byte to handle (e.g. QEMU_VIZ_MSG_TOUCH_EVENT).
 * @param cb        Callback function to invoke when a message of this type arrives.
 * @return ESP_OK on success.
 */
esp_err_t qemu_viz_register_rx_handler(uint8_t msg_type, qemu_viz_rx_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* QEMU_VIZ_TRANSPORT_H_ */
