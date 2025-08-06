/**
 * @file UserSettings.h
 * @brief User preferences and configuration management system
 * 
 * This module provides persistent storage and management of user preferences including:
 * - WiFi network credentials and connection settings
 * - Audio and vibration preferences
 * - Badge pairing and identification data
 * - User interface customization options
 * - Secure key management for authentication
 * - JSON-based configuration updates
 * - Thread-safe settings access and modification
 * 
 * The system ensures user preferences persist across power cycles and provides
 * secure storage for sensitive data like WiFi passwords and authentication keys.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef USER_SETTINGS_H_
#define USER_SETTINGS_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "BatterySensor.h"
#include "GameTypes.h"

#define MAX_SSID_LENGTH (32)
#define MAX_PASSWORD_LENGTH (64)

typedef struct WifiSettings_t
{
    uint8_t ssid[MAX_SSID_LENGTH];
    uint8_t password[MAX_PASSWORD_LENGTH];
} WifiSettings;

typedef struct UserSettingsFile_t
{
    uint32_t selectedIndex;
    uint8_t soundEnabled;
    uint8_t vibrationEnabled;
    uint8_t pairId[PAIR_ID_SIZE];
    WifiSettings wifiSettings;
    uint8_t reserved;
} UserSettingsFile;

typedef struct UserSettings_t
{
    UserSettingsFile settings;
    bool updateNeeded;
    uint8_t badgeId[BADGE_ID_SIZE];
    uint8_t badgeIdB64[BADGE_ID_B64_SIZE];
    uint8_t key[KEY_SIZE];
    uint8_t keyB64[KEY_B64_SIZE];
    SemaphoreHandle_t mutex;
    BatterySensor *pBatterySensor;
} UserSettings;

/**
 * @brief Initialize the user settings system
 * 
 * Initializes the user settings module, loads configuration from flash storage,
 * sets up default values, and integrates with the battery sensor for
 * power-aware settings management.
 * 
 * @param this Pointer to UserSettings instance to initialize
 * @param pBatterySensor Battery sensor for power management integration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t UserSettings_Init(UserSettings *this, BatterySensor * pBatterySensor);

/**
 * @brief Update user settings from JSON configuration
 * 
 * Parses and applies user settings from a JSON string. This allows
 * dynamic configuration updates from web interfaces, BLE transfers,
 * or configuration files.
 * 
 * @param this Pointer to UserSettings instance
 * @param settingsJson JSON string containing new settings
 * @return ESP_OK on success, error code on parsing or validation failure
 */
esp_err_t UserSettings_UpdateFromJson(UserSettings *this, uint8_t * settingsJson);

/**
 * @brief Set the badge pair identifier
 * 
 * Updates the pair ID used for badge-to-badge pairing and identification.
 * This enables specific badge interactions and personalized experiences.
 * 
 * @param this Pointer to UserSettings instance
 * @param pairId New pair identifier to set
 * @return ESP_OK on success, error code on failure
 */
esp_err_t UserSettings_SetPairId(UserSettings *this, uint8_t * pairId);

/**
 * @brief Set the selected index for user preferences
 * 
 * Updates the currently selected index for various user preference
 * categories such as LED sequences, themes, or game modes.
 * 
 * @param this Pointer to UserSettings instance
 * @param selectedIndex New selected index value
 * @return ESP_OK on success, error code on failure
 */
esp_err_t UserSettings_SetSelectedIndex(UserSettings *this, uint32_t selectedIndex);

#endif // USER_SETTINGS_H_