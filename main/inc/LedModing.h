/**
 * @file LedModing.h
 * @brief LED mode coordination and state management system
 * 
 * This module provides LED mode coordination functionality including:
 * - Touch interaction LED state tracking
 * - Game event LED status coordination
 * - BLE service and connection LED indicators
 * - OTA download progress LED feedback
 * - Battery level LED indication
 * - LED sequence preview coordination
 * - Network test LED status display
 * - Multi-modal LED state conflict resolution
 * 
 * The LED moding system ensures proper LED behavior coordination
 * across multiple badge subsystems and user interactions.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef LED_MODING_H_
#define LED_MODING_H_

#include <stdio.h>

#include "LedControl.h"

typedef struct LedModing_t
{
    bool touchActive;
    bool gameEventActive;
    bool bleServiceEnabled;
    bool bleConnected;
    bool otaDownloadInitiatedActive;
    bool batteryIndicatorActive;
    bool ledSequencePreviewActive;
    bool ledGameStatusActive;
    bool ledGameInteractiveActive;
    bool songActiveStatus;
    bool bleFileTransferInProgress;
    bool networkTestActive;
    bool bleReconnecting;
    // LedStatusIndicator curStatusIndicator;
    LedControl *pLedControl;
} LedModing;

/**
 * @brief Initialize the LED mode coordination system
 * 
 * Initializes the LED mode manager that coordinates different LED states
 * and priorities across the badge system. Integrates with LED control
 * for unified LED behavior management.
 * 
 * @param this Pointer to LedModing instance to initialize
 * @param pLedControl LED control system for hardware management
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_Init(LedModing *this, LedControl *pLedControl);

/**
 * @brief Set touch interaction LED indicator state
 * 
 * Controls LED indicators for touch sensor interactions and feedback.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate touch LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetTouchActive(LedModing *this, bool active);

/**
 * @brief Set game event LED indicator state
 * 
 * Controls LED indicators for game-related events and activities.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate game event LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetGameEventActive(LedModing *this, bool active);

/**
 * @brief Set battery level LED indicator state
 * 
 * Controls LED indicators for battery level and charging status.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate battery LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetBatteryIndicatorActive(LedModing *this, bool active);

/**
 * @brief Set OTA download LED indicator state
 * 
 * Controls LED indicators for Over-The-Air firmware update progress.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate OTA LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetOtaDownloadInitiatedActive(LedModing *this, bool active);

/**
 * @brief Set BLE reconnection LED indicator state
 * 
 * Controls LED indicators for Bluetooth reconnection attempts.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate reconnection LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetBleReconnectingActive(LedModing *this, bool active);

/**
 * @brief Set BLE file transfer LED indicator state
 * 
 * Controls LED indicators for Bluetooth file transfer operations.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate file transfer LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetBleFileTransferIPActive(LedModing *this, bool active);

/**
 * @brief Set BLE service enable LED indicator state
 * 
 * Controls LED indicators for Bluetooth service activation.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate service LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetBleServiceEnableActive(LedModing *this, bool active);

/**
 * @brief Set BLE connection LED indicator state
 * 
 * Controls LED indicators for Bluetooth connection status.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate connection LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetBleConnectedActive(LedModing *this, bool active);

/**
 * @brief Set LED sequence preview indicator state
 * 
 * Controls LED indicators for sequence preview and editing mode.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate preview LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetLedSequencePreviewActive(LedModing *this, bool active);

/**
 * @brief Set game status LED indicator state
 * 
 * Controls LED indicators for overall game status and state.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate game status LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetGameStatusActive(LedModing *this, bool active);

/**
 * @brief Set interactive game LED indicator state
 * 
 * Controls LED indicators for interactive game sessions.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate interactive game LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetInteractiveGameActive(LedModing *this, bool active);

/**
 * @brief Set network test LED indicator state
 * 
 * Controls LED indicators for network connectivity testing.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate network test LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetNetworkTestActive(LedModing *this, bool active);

/**
 * @brief Set song playback LED indicator state
 * 
 * Controls LED indicators for audio/song playback status.
 * 
 * @param this Pointer to LedModing instance
 * @param active True to activate song LEDs, false to deactivate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetSongActiveStatusActive(LedModing *this, bool active);

/**
 * @brief Set custom LED sequence by index
 * 
 * Selects and activates a specific custom LED sequence for playback.
 * 
 * @param this Pointer to LedModing instance
 * @param newCustomIndex Index of the custom sequence to activate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_SetLedCustomSequence(LedModing *this, int newCustomIndex);

/**
 * @brief Cycle through available LED sequences
 * 
 * Cycles to the next or previous LED sequence based on direction.
 * Allows users to browse through available sequences.
 * 
 * @param this Pointer to LedModing instance
 * @param direction True for forward, false for backward cycling
 * @return ESP_OK on success, error code on failure
 */
esp_err_t LedModing_CycleSelectedLedSequence(LedModing *this, bool direction);


#endif // LED_MODING_H_