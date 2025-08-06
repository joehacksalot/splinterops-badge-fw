/**
 * @file BleControl_ServiceChar_InteractiveGame.h
 * @brief BLE interactive game service characteristic implementation
 * 
 * This module provides BLE interactive gaming functionality including:
 * - Multi-badge game communication over BLE
 * - Interactive game state synchronization
 * - Game data read response generation
 * - Peer badge interaction protocols
 * - Integration with game state management
 * - Real-time gaming event transmission
 * 
 * The interactive game characteristic enables coordinated gaming experiences
 * between multiple badges through BLE communication channels.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef BLECONTROL_SERVICECHAR_INTERACTIVEGAME_H_
#define BLECONTROL_SERVICECHAR_INTERACTIVEGAME_H_

/**
 * @brief Generate interactive game read response data
 * 
 * Creates a properly formatted response for BLE interactive game read requests.
 * This function packages the current game state, touch sensor status, and
 * other interactive data for transmission to peer badges during gaming sessions.
 * 
 * @param this Pointer to BleControl instance
 * @param buffer Output buffer to store response data
 * @param size Maximum size of the output buffer
 * @param pLength Pointer to store actual response data length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t _BleControl_GetInteractiveGameReadResponse(BleControl *this, uint8_t * buffer, uint32_t size, uint16_t * pLength);

#endif // BLECONTROL_SERVICECHAR_INTERACTIVEGAME_H_
