/**
 * @file BleControl_ServiceChar_FileTransfer.h
 * @brief BLE file transfer service characteristic implementation
 * 
 * This module provides BLE file transfer functionality including:
 * - Frame-based file transfer protocol over BLE
 * - File transfer read response generation
 * - Touch sensor state management for BLE communication
 * - Frame context management and reset operations
 * - Integration with BLE service layer
 * - Reliable data transmission with frame validation
 * 
 * The file transfer characteristic enables wireless transfer of LED sequences,
 * configuration files, and other badge data over BLE connections.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef BLECONTROL_SERVICECHAR_FILETRANSFER_H_
#define BLECONTROL_SERVICECHAR_FILETRANSFER_H_

#include "BleControl.h"

/**
 * @brief Generate file transfer read response data
 * 
 * Creates a properly formatted response for BLE file transfer read requests.
 * This function handles the frame-based protocol for transferring files
 * over BLE connections, including LED sequences and configuration data.
 * 
 * @param this Pointer to BleControl instance
 * @param buffer Output buffer to store response data
 * @param size Maximum size of the output buffer
 * @param pLength Pointer to store actual response data length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t _BleControl_GetFileTransferReadResponse(BleControl *this, uint8_t * buffer, uint32_t size, uint16_t * pLength);

/**
 * @brief Set touch sensor active state for BLE communication
 * 
 * Updates the touch sensor state that is communicated over BLE.
 * This allows other badges to know which touch sensors are currently
 * active on this badge for interactive gaming features.
 * 
 * @param this Pointer to BleControl instance
 * @param touchSensorIndex Index of the touch sensor (0-based)
 * @param active True to mark sensor as active, false for inactive
 */
void BleControl_SetTouchSensorActive(BleControl *this, uint32_t touchSensorIndex, bool active);

/**
 * @brief Reset the file transfer frame context
 * 
 * Resets the internal frame context used for BLE file transfers.
 * This should be called when starting a new file transfer or
 * when recovering from transfer errors.
 * 
 * @param this Pointer to BleControl instance
 */
void _BleControl_ResetFrameContext(BleControl *this);

#endif // BLECONTROL_SERVICECHAR_FILETRANSFER_H_
