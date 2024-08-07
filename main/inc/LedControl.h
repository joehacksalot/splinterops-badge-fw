#ifndef LED_TASK_H_
#define LED_TASK_H_

#include "freertos/FreeRTOS.h"
#include "led_strip.h"
#include "cJSON.h"

#include "BatterySensor.h"
#include "GameState.h"
#include "NotificationDispatcher.h"
#include "InteractiveGame.h"
#include "SynthModeNotifications.h"
#include "TouchSensor.h"
#include "UserSettings.h"
#include "WifiClient.h"

// Hardware configuration
#define LED_STRIP_GPIO 25
#define LED_TYPE LED_MODEL_WS2812

typedef struct
{
    union {
        uint8_t r;
        uint8_t red;
    };
    union {
        uint8_t g;
        uint8_t green;
    };
    union {
        uint8_t b;
        uint8_t blue;
    };
} rgb_t;


// LED configurations
#if defined(TRON_BADGE)
  #define BRIGHTNESS_NORMAL     (10)
  #define LED_STRIP_LEN         (77)
  #define OUTER_RING_LED_OFFSET (27)
  #define OUTER_RING_LED_COUNT  (50)
  #define INNER_RING_LED_OFFSET (0)
  #define INNER_RING_LED_COUNT  (27)
#elif defined(REACTOR_BADGE)
  #define BRIGHTNESS_NORMAL     (40)
  #define LED_STRIP_LEN         (48)
  #define OUTER_RING_LED_OFFSET (24)
  #define OUTER_RING_LED_COUNT  (24) /* outer ring led are led index 24->47 */
  #define INNER_RING_LED_OFFSET (0)
  #define INNER_RING_LED_COUNT  (24) /* inner ring led are led index 0->23 */
#elif defined(CREST_BADGE)
  #define BRIGHTNESS_NORMAL     (25)
  #define LED_STRIP_LEN         (59)
  #define OUTER_RING_LED_OFFSET (6)
  #define OUTER_RING_LED_COUNT  (53) /* outer ring led are led index 6->53 */
  #define INNER_RING_LED_OFFSET (0)
  #define INNER_RING_LED_COUNT  (6)  /* inner ring led are led index 0->6 */
#endif

typedef enum InnerLedState_e
{
    INNER_LED_STATE_OFF = 0,
    INNER_LED_STATE_LED_SEQUENCE,
    INNER_LED_STATE_TOUCH_LIGHTING,
    INNER_LED_STATE_GAME_STATUS,
    INNER_LED_STATE_GAME_EVENT,
    INNER_LED_STATE_BATTERY_STATUS,
    INNER_LED_STATE_STATUS_INDICATOR,
    INNER_LED_MODE_BLE_FILE_XFER_PCNT,
    INNER_LED_STATE_NETWORK_TEST,
    NUM_INNER_LED_STATES
} InnerLedState;

typedef enum OuterLedState_t
{
    OUTER_LED_STATE_OFF = 0,
    OUTER_LED_STATE_LED_SEQUENCE,
    OUTER_LED_STATE_TOUCH_LIGHTING,
    OUTER_LED_STATE_GAME_EVENT,
    OUTER_LED_STATE_BATTERY_STATUS,
    OUTER_LED_STATE_BLE_FILE_TRANSFER_STATUS,
    OUTER_LED_STATE_BLE_SERVICE_ENABLE,
    OUTER_LED_STATE_BLE_SERVICE_CONNECTED,
    OUTER_LED_STATE_OTA_DOWNLOAD_IP,
    OUTER_LED_STATE_STATUS_INDICATOR,
    OUTER_LED_STATE_GAME_STATUS,
    OUTER_LED_STATE_GAME_INTERACTIVE,
    OUTER_LED_STATE_BLE_RECONNECTING,
    OUTER_LED_STATE_BLE_FILE_XFER_PCNT,
    OUTER_LED_STATE_NETWORK_TEST,
    OUTER_LED_STATE_SONG_MODE,
    NUM_OUTER_LED_STATES
} OuterLedState;

typedef enum LedMode_e
{
    LED_MODE_SEQUENCE,                   // 0
    LED_MODE_TOUCH,                      // 1
    LED_MODE_BATTERY,                    // 2
    LED_MODE_EVENT,                      // 3
    LED_MODE_GAME_STATUS,                // 4
    LED_MODE_BLE_FILE_TRANSFER_ENABLED,  // 5
    LED_MODE_BLE_FILE_TRANSFER_CONNECTED,// 6
    LED_MODE_BLE_FILE_TRANSFER_PERCENT,  // 7
    LED_MODE_BLE_RECONNECTING,           // 8
    LED_MODE_NETWORK_TEST,               // 9
    LED_MODE_SONG,                       // 10
    LED_MODE_INTERACTIVE_GAME,           // 11
    LED_MODE_OTA_DOWNLOAD_IP             // 12
} LedMode;

typedef struct 
{
    int r;
    int g;
    int b;
    int i;
} color_t;

// Note: This structure's contents are memset to 0 on selected led sequence index change
//       Dont put anything in here you want to persist across led sequence changes
typedef struct JsonLedSequenceRuntimeSettings_t
{
    bool valid;
    cJSON *root;
    cJSON *frames;
    int numFrames;
    int curFrameIndex;
    TickType_t nextFrameDrawTime;
} JsonLedSequenceRuntimeSettings;

typedef struct BatteryIndicatorRuntimeSettings_t
{
    rgb_t initColor;
    rgb_t greatColor;
    rgb_t goodColor;
    rgb_t warnColor;
    rgb_t badColor;
    int curFrame;
    int numFrames;
    int numOuterLeds;
    int numInnerLeds;
    rgb_t color;
    uint32_t holdTime;
    int innerLedsDrawIterator;
    int outerLedsDrawIterator;
    TickType_t startDrawTime;
    TickType_t nextInnerFrameDrawTime;
    TickType_t nextOuterFrameDrawTime;
} BatteryIndicatorRuntimeSettings;

typedef struct BleFileTransferPercentRuntimeSettings_t
{
    rgb_t color;
    uint32_t percentComplete;
    uint32_t prevPercentComplete;
} BleFileTransferPercentRuntimeSettings;

typedef struct NetworkTestRuntimeSettings_t
{
    rgb_t color;
    bool success;
} NetworkTestRuntimeSettings;

typedef struct LedStatusIndicatorRuntimeSettings_t
{
    rgb_t initColor;
    rgb_t errorColor;
    rgb_t bleServiceEnabledColor;
    rgb_t bleConnectedColor;
    rgb_t bleReconnectingColor;
    rgb_t otaUpdateSuccessColor;
    rgb_t otaUpdateInProgressColor;
    rgb_t networkTestSuccessColor;
    TickType_t nextInnerDrawTime;
    TickType_t nextOuterDrawTime;
    uint32_t innerLedWidth;
    uint32_t outerLedWidth;
    uint32_t curInnerPosition;
    uint32_t curOuterPosition;
    uint32_t revolutionsPerSecond;
} LedStatusIndicatorRuntimeSettings;

typedef struct TouchModeRuntimeSettings_t
{
    TouchSensorEvent touchSensorValue[TOUCH_SENSOR_NUM_BUTTONS];
    rgb_t initColor;
    rgb_t touchColor;
    rgb_t shortPressColor;
    rgb_t longPressColor;
    rgb_t veryLongPressColor;
    TickType_t nextInnerDrawTime;
    TickType_t nextOuterDrawTime;
    uint32_t updatePeriod;
} TouchModeRuntimeSettings;

typedef struct SongModeRuntimeSettings_t
{
    SongNoteChangeEventNotificationData lastSongNoteChangeEventNotificationData;
    bool updateNeeded;
} SongModeRuntimeSettings;


typedef struct InteractiveGameModeRuntimeSettings_t
{
    InteractiveGameData touchSensorsToLightBits;
    bool updateNeeded;
} InteractiveGameModeRuntimeSettings;

typedef struct LedControlModeSettings_t
{
    LedMode mode;
    InnerLedState innerLedState;
    OuterLedState outerLedState;
    uint32_t ledNormalModeInnerLedCycleHoldtime;
    TickType_t nextNormalModeInnerStateCycleTime;
} LedControlModeSettings;

typedef struct GameStatusRuntimeSettings_t
{
    bool updateNeeded;
} GameStatusRuntimeSettings;

typedef struct GameEventRuntimeSettings_t
{
    rgb_t initColor;
    TickType_t nextInnerDrawTime;
    TickType_t nextOuterDrawTime;
    uint32_t updatePeriod;
    double maxEventPulsesPerSecond;
    double minEventPulsesPerSecond;
    uint8_t outerLedWidth;
    uint8_t curOuterPosition;
    uint8_t revolutionsPerSecond;
    int8_t curPulseDirection;
    double curIntensity;
} GameEventRuntimeSettings;

typedef struct LedControl_t
{
    uint32_t ledLoadedIndex;
    color_t pixelColorState[LED_STRIP_LEN];
    bool flushNeeded;
    led_strip_config_t ledStrip;
    led_strip_rmt_config_t ledStripRmt;
    led_strip_spi_config_t ledStripSpi;
    led_strip_handle_t ledStripHandle;
    SemaphoreHandle_t jsonMutex;
    uint32_t selectedIndex;
    uint32_t loadRequired;
    bool drawLedNoneUpdateRequired;
    LedControlModeSettings ledControlModeSettings;
    JsonLedSequenceRuntimeSettings jsonSequenceRuntimeInfo;
    BatteryIndicatorRuntimeSettings batteryIndicatorRuntimeInfo;
    BleFileTransferPercentRuntimeSettings bleFileTransferPercentRuntimeInfo;
    TouchModeRuntimeSettings touchModeRuntimeInfo;
    SongModeRuntimeSettings songModeRuntimeInfo;
    InteractiveGameModeRuntimeSettings interactiveGameModeRuntimeSettings;
    LedStatusIndicatorRuntimeSettings ledStatusIndicatorRuntimeInfo;
    GameStatusRuntimeSettings gameStatusRuntimeInfo;
    GameEventRuntimeSettings gameEventRuntimeInfo;
    NetworkTestRuntimeSettings networkTestRuntimeInfo;
    NotificationDispatcher *pNotificationDispatcher;
    UserSettings *pUserSettings;
    BatterySensor *pBatterySensor;
    GameState *pGameState;
} LedControl;

esp_err_t LedControl_Init(LedControl *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings, BatterySensor *pBatterySensor, GameState *pGameState, uint32_t batteryIndicatorHoldTime);
esp_err_t LedControl_SetInnerLedState(LedControl *this, InnerLedState state);
esp_err_t LedControl_SetOuterLedState(LedControl *this, OuterLedState state);
esp_err_t LedControl_SetLedCustomSequence(LedControl *this, int customIndex);
// esp_err_t LedControl_InitiateStatusIndicatorSequence(LedControl *this, LedStatusIndicator ledIndicatorStatus, uint32_t holdTime);
esp_err_t LedControl_SetLedMode(LedControl *this, LedMode mode);
esp_err_t LedControl_SetCurrentLedSequenceIndex(LedControl *this, int sequenceIndex);
esp_err_t LedControl_CycleSelectedLedSequence(LedControl *this, bool direction);
int LedControl_GetCurrentLedSequenceIndex(LedControl *this);
void LedControl_SetTouchSensorUpdate(LedControl *this, TouchSensorEvent touchSensorEvent, int touchSensorIdx);

#endif // LED_TASK_H_