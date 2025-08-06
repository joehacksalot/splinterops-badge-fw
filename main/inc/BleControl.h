/**
 * @file BleControl.h
 * @brief Bluetooth Low Energy control and communication system
 * 
 * This module provides the core BLE functionality for the badge including:
 * - BLE stack initialization and management
 * - Service and characteristic definitions
 * - Advertising and scanning capabilities
 * - Peer discovery and connection management
 * - File transfer over BLE
 * - Interactive game communication
 * - Event-based communication with other badge components
 * 
 * The BLE system supports multiple service profiles including file transfer
 * and interactive gaming, with robust frame-based data transmission protocols.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef BLE_CONFIG_H_
#define BLE_CONFIG_H_

// #include "esp_gatts_api.h"
// #include "esp_gap_ble_api.h"

#include "host/ble_uuid.h"
#include "GameState.h"
#include "InteractiveGame.h"
#include "NotificationDispatcher.h"
#include "UserSettings.h"

#define DATA_FRAME_HEADER_SIZE   (2)
#define DATA_FRAME_MAX_SIZE      (500)
#define CONFIG_FRAME_HEADER_SIZE (15)
#define MAX_BLE_FRAMES           (1024)
#define EVENT_ADV_MAGIC_NUMBER   (0x1337)
#define BLE_NAME_MAX_SIZE        (24)
#define BLE_MUTEX_WAIT_TIME_MS   (100)

#define MAX_BLE_FILE_TRANSFER_FILE_SIZE (128*1024) // Must match MAX_CUSTOM_LED_SEQUENCE_SIZE

typedef enum BleServiceProfile_t
{
    BLE_PROFILE_FILE_TRANSFER_APP_ID = 0,
    BLE_PROFILE_INTERACTIVE_GAME_APP_ID,
    BLE_PROFILE_NUM_PROFILES
} BleServiceProfile;

typedef struct IwcAdvertisingPayload_t
{
    uint16_t magicNum;
    uint8_t badgeType;
    uint8_t badgeId[BADGE_ID_SIZE];
    uint8_t eventId[EVENT_ID_SIZE];
} __attribute__((packed)) IwcAdvertisingPayload;

typedef struct FrameContext_t
{
    bool configFrameProcessed;
    bool fileProcessed;
    bool frameInProgress;
    uint8_t fileType;
    int curNumFrames;
    int frameLen;
    int curCustomSeqSlot;
    int frameBytesReceived;
    uint8_t frameReceived[MAX_BLE_FRAMES];
    const uint8_t * rcvBuffer;
} FrameContext;

typedef struct BleControl_t
{
    uint8_t ownAddrType;
    InteractiveGameData touchSensorsActiveBits;
    InteractiveGameData feathersToLightBits;
    char bleName[BLE_NAME_MAX_SIZE];
    bool bleServiceEnabled;
    ble_uuid128_t serviceUuid;
    esp_timer_handle_t bleServiceDisableTimerHandle;  // Timeout timer to disable BLE if not used
    esp_timer_create_args_t bleServiceDisableTimerHandleArgs;
    FrameContext fileTransferFrameContext;
    IwcAdvertisingPayload iwcAdvPayload;
    NotificationDispatcher *pNotificationDispatcher;
    UserSettings *pUserSettings;
    GameState *pGameState;
    SemaphoreHandle_t bleMutex;
} BleControl;

/**
 * @brief Get the singleton instance of the BLE control system
 * 
 * Returns the global BLE control instance for system-wide access.
 * This follows the singleton pattern to ensure only one BLE controller
 * exists throughout the badge system lifecycle.
 * 
 * @return Pointer to the BLE control singleton instance
 */
BleControl * BleControl_GetInstance();

/**
 * @brief Initialize the BLE control system with required dependencies
 * 
 * Initializes the complete BLE stack including:
 * - BLE stack configuration and startup
 * - Service and characteristic registration
 * - Advertising configuration with badge identification
 * - Connection management setup
 * - Frame-based file transfer protocol initialization
 * - Interactive game communication setup
 * - Integration with notification dispatcher for events
 * 
 * @param this Pointer to BleControl instance to initialize
 * @param pNotificationDispatcher Notification system for BLE events
 * @param pUserSettings User settings for BLE configuration
 * @param pGameSettings Game state for interactive BLE features
 * @return ESP_OK on success, error code on failure
 */
esp_err_t BleControl_Init(BleControl *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings, GameState *pGameSettings);

/**
 * @brief Update the event identifier for BLE advertising
 * 
 * Updates the event ID used in BLE advertising payload to identify
 * the current badge event or conference. This allows badges to
 * discover and connect to other badges from the same event.
 * 
 * @param this Pointer to BleControl instance
 * @param newEventId New event identifier string to broadcast
 * @return ESP_OK on success, error code on failure
 */
esp_err_t BleControl_UpdateEventId(BleControl *this, char *newEventId);

#endif // BLE_CONFIG_H_
