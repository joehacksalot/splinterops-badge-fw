/*
 * SPDX-FileCopyrightText: 2025 SplinterOps
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * HCI H4 transport over TCP/UART for QEMU BLE emulation.
 * Replaces esp_nimble_hci when CONFIG_BADGE_QEMU_MODE is enabled.
 * Bridges HCI packets between the NimBLE host stack and an external
 * virtual BLE controller (e.g. Google Bumble) via a QEMU serial port.
 */

#ifndef HCI_TRANSPORT_QEMU_H_
#define HCI_TRANSPORT_QEMU_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the QEMU HCI transport.
 *
 * Opens UART1 (mapped to a QEMU TCP chardev) and registers the NimBLE
 * HCI transport function pointers so that the host stack sends/receives
 * HCI H4 packets through this channel.
 *
 * Must be called BEFORE nimble_port_init().
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t hci_transport_qemu_init(void);

/**
 * Deinitialize the QEMU HCI transport.
 *
 * Stops the receive task and releases UART resources.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t hci_transport_qemu_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* HCI_TRANSPORT_QEMU_H_ */
