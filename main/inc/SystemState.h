/**
 * @file SystemState.h
 * @brief Central system state management for the 2024 Badge Application
 * 
 * This module provides the core state machine and coordination logic for the badge.
 * It manages all major subsystems including BLE, touch sensors, LED control, audio,
 * and interactive games. The SystemState acts as the central hub for event processing
 * and inter-component communication.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef SYSTEM_STATE_H_
#define SYSTEM_STATE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "BadgeStats.h"
#include "BatterySensor.h"
#include "BleControl.h"
#include "GameState.h"
#include "GpioControl.h"
#include "InteractiveGame.h"
#include "LedModing.h"
#include "NotificationDispatcher.h"
#include "OtaUpdate.h"
#include "TouchSensor.h"
#include "TouchActions.h"
#include "UserSettings.h"
#include "WifiClient.h"
#include "HTTPGameClient.h"
#include "SynthMode.h"
#include "Ocarina.h"

/**
 * @brief Application configuration structure
 * 
 * Contains compile-time and runtime configuration flags that control
 * badge behavior and hardware feature availability.
 */
typedef struct AppConfig_t
{
    bool touchActionCommandEnabled;  /**< Enable touch-based command processing */
    bool buzzerPresent;             /**< Hardware has audio buzzer/speaker */
    bool eyeGpioLedsPresent;        /**< Hardware has eye GPIO LEDs */
} AppConfig;

/**
 * @brief Main system state structure
 * 
 * Central state container that holds all badge subsystem states, timers,
 * and component instances. This structure serves as the primary coordination
 * point for all badge functionality.
 */
typedef struct SystemState_t
{
    // State flags
    bool touchActive;                    /**< Touch sensors are currently active */
    bool batteryIndicatorActive;         /**< Battery level indicator is being displayed */
    bool gameEventActive;                /**< Interactive game event is in progress */
    bool touchActionCmdClearRequired;    /**< Touch action command buffer needs clearing */
    bool ledGameStatusActive;            /**< LED game status indicator is active */
    bool networkTestActive;              /**< Network connectivity test is running */
    bool peerSongPlaying;                /**< Peer badge is playing a song */
    bool peerSongWaitingCooldown;        /**< Waiting for peer song cooldown period */
    bool bleReconnecting;                /**< BLE is in reconnection state */
    
    // Game data
    InteractiveGameData interactiveGameTouchSensorsToLightBits; /**< Touch-to-LED mapping for games */
    
    // Timer handles for various system timeouts
    TimerHandle_t touchActiveTimer;              /**< Timer for touch activity timeout */
    TimerHandle_t drawBatteryIndicatorActiveTimer; /**< Timer for battery indicator display */
    TimerHandle_t drawNetworkTestTimer;          /**< Timer for network test display */
    TimerHandle_t drawNetworkTestSuccessTimer;   /**< Timer for network success indication */
    TimerHandle_t ledSequencePreviewTimer;       /**< Timer for LED sequence preview */
    TimerHandle_t ledGameStatusToggleTimer;      /**< Timer for game status LED toggle */
    TimerHandle_t peerSongCooldownTimer;         /**< Timer for peer song cooldown period */
    
    // Component instances - all major badge subsystems
    AppConfig appConfig;                         /**< Application configuration settings */
    BadgeStats badgeStats;                       /**< Badge statistics and usage tracking */
    BatterySensor batterySensor;                 /**< Battery level monitoring */
    BleControl bleControl;                       /**< Bluetooth Low Energy communication */
    GameState gameState;                         /**< Interactive game state management */
    GpioControl gpioControl;                     /**< GPIO pin control and management */
    LedControl ledControl;                       /**< LED array control and patterns */
    LedModing ledModing;                         /**< LED mode and sequence management */
    NotificationDispatcher notificationDispatcher; /**< Event system for inter-component communication */
    OtaUpdate otaUpdate;                         /**< Over-the-air firmware update handler */
    TouchSensor touchSensor;                     /**< Touch sensor input processing */
    TouchActions touchActions;                   /**< Touch gesture recognition and actions */
    UserSettings userSettings;                   /**< User preferences and configuration */
    WifiClient wifiClient;                       /**< WiFi connectivity management */
    HTTPGameClient httpGameClient;               /**< HTTP client for game server communication */
    SynthMode synthMode;                         /**< Audio synthesis and music generation */
    Ocarina ocarina;                             /**< Musical instrument simulation */
} SystemState;

/**
 * @brief Get the singleton instance of SystemState
 * 
 * Returns a pointer to the global SystemState instance. This follows the
 * singleton pattern to ensure only one system state exists throughout the
 * application lifecycle.
 * 
 * @return Pointer to the SystemState singleton instance
 * @retval SystemState* Valid pointer to initialized SystemState
 * @retval NULL If system state has not been properly initialized
 * 
 * @note This function is thread-safe
 * @see SystemState_Init()
 */
SystemState *SystemState_GetInstance();

/**
 * @brief Initialize the SystemState and all subsystems
 * 
 * Performs complete initialization of the badge system including:
 * - Hardware component initialization
 * - Timer creation and configuration
 * - Event system setup
 * - BLE stack initialization
 * - Touch sensor calibration
 * - LED system setup
 * - Audio system initialization
 * 
 * This function must be called once during system startup before any
 * other SystemState functions are used.
 * 
 * @param[in,out] this Pointer to SystemState structure to initialize
 * 
 * @return ESP error code indicating success or failure
 * @retval ESP_OK Initialization completed successfully
 * @retval ESP_ERR_INVALID_ARG Invalid SystemState pointer provided
 * @retval ESP_ERR_NO_MEM Insufficient memory for initialization
 * @retval ESP_FAIL General initialization failure
 * 
 * @note This function may take several seconds to complete
 * @note Should only be called once during system startup
 * @see SystemState_GetInstance()
 */
esp_err_t SystemState_Init(SystemState *this);

#endif // SYSTEM_STATE_H_
