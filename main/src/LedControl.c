/**
 * @file LedControl.c
 * @brief LED strip control and pattern management implementation
 * 
 * This module implements comprehensive LED control functionality including:
 * - WS2812B LED strip control with RMT peripheral
 * - JSON-based LED sequence parsing and execution
 * - Dynamic pattern loading from disk storage
 * - Multi-badge LED synchronization support
 * - Battery-aware brightness control
 * - Thread-safe LED operations with mutex protection
 * - Status indicator LED patterns (BLE, error, success)
 * - Custom LED sequence management and sharing
 * - Real-time LED pattern updates and transitions
 * 
 * The LED control system manages up to 256 addressable LEDs with
 * full RGB color control and configurable timing parameters.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"

// #include "JsonUtils.h"
#include "BatterySensor.h"
#include "DiskUtilities.h"
#include "GameState.h"
#include "JsonUtils.h"
#include "LedControl.h"
#include "LedSequences.h"
#include "NotificationDispatcher.h"
#include "SynthModeNotifications.h"
#include "TaskPriorities.h"
#include "TimeUtils.h"
#include "Utilities.h"
#include "WifiClient.h"

#define MUTEX_MAX_WAIT_MS   (500)
#define MAX_EVENT_TIME_MSEC (15*60*1000)

#define LED_CONTROL_TASK_PERIOD (50)
#define NUM_LED_NOTES (15)
#define TOUCH_NOTE_OFFSET (7)

// Internal Function Declarations
static void LedControlTask(void *pvParameters);
// static void LedControl_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void LedControl_GameStatusNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void LedControl_BleControlPercentChangedNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void LedControl_SongNoteActionNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void LedControl_InteractiveGameActionNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static esp_err_t LedControl_InitServiceDrawBatteryIndicatorSequence(LedControl *this);
static esp_err_t LedControl_ServiceDrawJsonLedSequence(LedControl * this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawBatteryIndicatorSequence(LedControl * this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawPercentCompleteSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawGameStatusSequence(LedControl * this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_InitDrawGameEventSequence(LedControl * this);
static esp_err_t LedControl_ServiceDrawGameEventSequence(LedControl * this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawGameInteractiveSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawNoneSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawTouchLightingSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawBleEnabledSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawBleConnectedSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawBleReconnectingSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawOtaDownloadInProgSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
// static esp_err_t LedControl_ServiceDrawStatusIndicatorSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawNetworkTestSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_ServiceDrawSongModeSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing);
static esp_err_t LedControl_LoadJsonLedSequence(LedControl * this);
static esp_err_t LedControll_FillPixels(LedControl *this, rgb_t color, int ledStartIndex, int numLedsToFill);
static esp_err_t LedControll_FillPixelsWithIntensity(LedControl *this, rgb_t color, int intensity, int ledStartIndex, int numLedsToFill);
static esp_err_t LedControl_SetPixel(LedControl * this, color_t in_color, int pix_num);
static esp_err_t LedControl_SetPixelFromJson(LedControl * this, int n, cJSON *r, cJSON *g, cJSON *b, cJSON *i, bool *pChangeDetected);
static esp_err_t LedControl_FlushLedStrip(LedControl * this);
static bool LedControl_IndexIsInnerRing(int pixelIndex);
static bool LedControl_IndexIsOuterRing(int pixelIndex);

// Internal Constants
static const char *TAG = "LED";

static const int correctedPixelOffset[] = 
{
#if defined(TRON_BADGE)
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 
    72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 76, 75, 74, 73
#elif defined(REACTOR_BADGE)
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25
#elif defined(CREST_BADGE)
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58
#elif defined(FMAN25_BADGE)
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44
#endif
};

#define MAX_TOUCH_MAP_INDEX_COUNT 7
typedef struct TouchMap_t
{
    int numIndexes;
    int indexes[MAX_TOUCH_MAP_INDEX_COUNT];
} LedMap;

static const LedMap songMap[NUM_LED_NOTES] = 
{
#if defined(TRON_BADGE) || defined(REACTOR_BADGE)
// TODO: Needs tested on reactor and tron badges, almost positive this doesnt work for tron
    {.numIndexes=3, .indexes={24,25,47}}, // 12   D
    {.numIndexes=3, .indexes={25,26,27}}, // 12.5 D#
    {.numIndexes=3, .indexes={26,27,24}}, // 1    E
    {.numIndexes=3, .indexes={28,29,30}}, // 2    F
    {.numIndexes=3, .indexes={29,30,31}}, // 2.5  F#
    {.numIndexes=3, .indexes={30,31,32}}, // 4    G
    {.numIndexes=3, .indexes={31,32,33}}, // 4.5  G#
    {.numIndexes=3, .indexes={33,34,24}}, // 5    A
    {.numIndexes=3, .indexes={35,36,37}}, // 6    A#
    {.numIndexes=3, .indexes={38,39,24}}, // 7    B
    {.numIndexes=3, .indexes={40,41,42}}, // 8    C
    {.numIndexes=3, .indexes={41,42,43}}, // 8.5  C#
    {.numIndexes=3, .indexes={42,43,44}}, // 10   D
    {.numIndexes=3, .indexes={44,45,46}}, // 10.5 D#
    {.numIndexes=3, .indexes={45,46,47}}  // 11   E
#elif defined(CREST_BADGE)
    {.numIndexes=7, .indexes={7,8,9,10,11,12,13}},   // 0    NOTE_BASE_D
    {.numIndexes=5, .indexes={11,12,13,15,16}},      // 0.5  NOTE_BASE_DS
    {.numIndexes=5, .indexes={15,16,17,18,19}},      // 1    NOTE_BASE_E
    {.numIndexes=5, .indexes={22,23,24,25,26}},      // 2    NOTE_BASE_F
    {.numIndexes=5, .indexes={24,25,26,28,29}},      // 2.5  NOTE_BASE_FS
    {.numIndexes=3, .indexes={28,29,30}},            // 3    NOTE_BASE_G
    {.numIndexes=3, .indexes={30,31,32}},            // 3.5  NOTE_BASE_GS
    {.numIndexes=3, .indexes={31,32,33}},            // 4    NOTE_BASE_A
    {.numIndexes=3, .indexes={33,34,35}},            // 4.5  NOTE_BASE_AS
    {.numIndexes=3, .indexes={34,35,36}},            // 5    NOTE_BASE_B
    {.numIndexes=5, .indexes={38,39,40,41,42}},      // 6    NOTE_BASE_C
    {.numIndexes=5, .indexes={41,42,44,45,46}},      // 6.5  NOTE_BASE_CS
    {.numIndexes=5, .indexes={44,45,46,47,48}},      // 7    NOTE_BASE_D
    {.numIndexes=5, .indexes={47,48,51,52,53}},      // 7.5  NOTE_BASE_DS
    {.numIndexes=7, .indexes={51,52,53,54,55,56,57}} // 8    NOTE_BASE_E
#elif defined(FMAN25_BADGE)
    {.numIndexes=3, .indexes={21,22,23}},        // 0    NOTE_BASE_D
    {.numIndexes=2, .indexes={20,21}},           // 0.5  NOTE_BASE_DS
    {.numIndexes=3, .indexes={18,19,20}},        // 1    NOTE_BASE_E
    {.numIndexes=3, .indexes={15,16,17}},        // 2    NOTE_BASE_F
    {.numIndexes=1, .indexes={15}},              // 2.5  NOTE_BASE_FS
    {.numIndexes=3, .indexes={13,14,15}},        // 3    NOTE_BASE_G
    {.numIndexes=2, .indexes={12,13}},           // 3.5  NOTE_BASE_GS
    {.numIndexes=3, .indexes={10,11,12}},        // 4    NOTE_BASE_A    Center
    {.numIndexes=2, .indexes={9,10}},            // 4.5  NOTE_BASE_AS
    {.numIndexes=3, .indexes={7,8,9}},           // 5    NOTE_BASE_B
    {.numIndexes=3, .indexes={5,6,7}},           // 6    NOTE_BASE_C
    {.numIndexes=2, .indexes={4,5}} ,            // 6.5  NOTE_BASE_CS
    {.numIndexes=3, .indexes={2,3,4}},           // 7    NOTE_BASE_D
    {.numIndexes=2, .indexes={1,2}},             // 7.5  NOTE_BASE_DS
    {.numIndexes=3, .indexes={0,1,31}}           // 8    NOTE_BASE_E
#endif
};

static const color_t songColorMap[NUM_OCTAVES] = 
{
    { .r = 128, .g = 128, .b = 128, .i = 100 }, // 0
    { .r = 128, .g = 0,   .b = 128, .i = 100 }, // 1
    { .r = 0,   .g = 128, .b = 128, .i = 100 }, // 2
    { .r = 255, .g = 0,   .b = 0,   .i = 100 }, // 3
    { .r = 0,   .g = 255, .b = 0,   .i = 100 }, // 4
    { .r = 0,   .g = 0,   .b = 255, .i = 100 }, // 5
    { .r = 255, .g = 255, .b = 0,   .i = 100 }, // 6
    { .r = 255, .g = 0,   .b = 255, .i = 100 }, // 7
    { .r = 0,   .g = 255, .b = 255, .i = 100 }  // 8
};

static const LedMap touchMap[TOUCH_SENSOR_NUM_BUTTONS] = 
{
#if defined(TRON_BADGE)
// TODO: This is all crap. We need actual touch values to put here
    {.numIndexes=6, .indexes={24,25,47,35,36,37}},  // 0  12
    {.numIndexes=3, .indexes={26,27,24}},  // 1  1
    {.numIndexes=3, .indexes={28,29,30}},  // 2  2
    {.numIndexes=3, .indexes={30,31,32}},  // 3  4
    {.numIndexes=3, .indexes={33,34,24}},  // 4  5
    // {.numIndexes=0, .indexes={0,  0,  0   }},  // 5  6
    {.numIndexes=3, .indexes={38,39,24}},  // 6  7
    {.numIndexes=3, .indexes={40,41,42}},  // 7  8
    {.numIndexes=3, .indexes={42,43,44}},  // 8  10
    {.numIndexes=3, .indexes={45,46,47}}   // 9  11
#elif defined(REACTOR_BADGE)
    {.numIndexes=6, .indexes={24,25,47,35,36,37}},  // 0  12
    {.numIndexes=3, .indexes={26,27,24}},  // 1  1
    {.numIndexes=3, .indexes={28,29,30}},  // 2  2
    {.numIndexes=3, .indexes={30,31,32}},  // 3  4
    {.numIndexes=3, .indexes={33,34,24}},  // 4  5
    // {.numIndexes=0, .indexes={0,  0,  0   }},  // 5  6
    {.numIndexes=3, .indexes={38,39,24}},  // 6  7
    {.numIndexes=3, .indexes={40,41,42}},  // 7  8
    {.numIndexes=3, .indexes={42,43,44}},  // 8  10
    {.numIndexes=3, .indexes={45,46,47}}   // 9  11
#elif defined(CREST_BADGE)
    {.numIndexes=5, .indexes={8,9,10,11,12}},        // 0  0
    {.numIndexes=3, .indexes={16,17,18}},            // 1  1
    {.numIndexes=2, .indexes={23,24}},               // 2  2
    {.numIndexes=1, .indexes={28}},                  // 3  4
    {.numIndexes=1, .indexes={31}},                  // 4  4
    {.numIndexes=1, .indexes={35}},                  // 5  5
    {.numIndexes=2, .indexes={40,41}},               // 6  6
    {.numIndexes=3, .indexes={46,47,48}},            // 7  7
    {.numIndexes=5, .indexes={52,53,54,55,56}}       // 8  8
#elif defined(FMAN25_BADGE)
    {.numIndexes=3, .indexes={21,22,23}},      // 0
    {.numIndexes=3, .indexes={18,19,20}},      // 1
    {.numIndexes=3, .indexes={15,16,17}},      // 2
    {.numIndexes=3, .indexes={13,14,15}},      // 3
    {.numIndexes=3, .indexes={10,11,12}},      // 4
    {.numIndexes=3, .indexes={7,8,9}},         // 5
    {.numIndexes=3, .indexes={5,6,7}},         // 6
    {.numIndexes=3, .indexes={2,3,4}},         // 7
    {.numIndexes=3, .indexes={0,1,31}}         // 8
#endif
};

static const LedMap gameStatusMap[NUM_GAMESTATE_EVENTCOLORS] = 
{
#if defined(TRON_BADGE) || defined(REACTOR_BADGE)
// TODO: Needs tested on reactor and tron badges, almost positive this doesnt work for tron
    {.numIndexes=2, .indexes={1,  2 }},
    {.numIndexes=2, .indexes={5,  6 }},
    {.numIndexes=2, .indexes={9,  10}},
    {.numIndexes=2, .indexes={13, 14}},
    {.numIndexes=2, .indexes={17, 18}},
    {.numIndexes=2, .indexes={21, 22}} 
#elif defined(CREST_BADGE)
    {.numIndexes=1, .indexes={1}},
    {.numIndexes=1, .indexes={2}},
    {.numIndexes=1, .indexes={3}},
    {.numIndexes=1, .indexes={4}},
    {.numIndexes=1, .indexes={5}},
    {.numIndexes=1, .indexes={6}} 
#elif defined(FMAN25_BADGE)
    {.numIndexes=1, .indexes={32}},
    {.numIndexes=1, .indexes={34}},
    {.numIndexes=1, .indexes={36}},
    {.numIndexes=1, .indexes={38}},
    {.numIndexes=1, .indexes={40}},
    {.numIndexes=1, .indexes={42}} 
#endif
};

static const rgb_t stoneColorMap[NUM_GAMESTATE_EVENTCOLORS] = 
{
    {.r = 255, .g =   0, .b = 0  }, // red
    {.r = 255, .g = 255, .b = 0  }, // yellow 
    {.r =   0, .g = 255, .b = 0  }, // green
    {.r =   0, .g = 255, .b = 255}, // cyan
    {.r =   0, .g =   0, .b = 255}, // blue
    {.r = 255, .g =   0, .b = 255}  // magenta
};

// static const rgb_t gameTouchColorMap[TOUCH_SENSOR_NUM_BUTTONS] = 
// {
//     {.r = 255, .g =   0, .b = 0  },
//     {.r =   0, .g = 255, .b = 0  },
//     {.r =   0, .g =   0, .b = 255},
//     {.r = 255, .g = 255, .b = 0  },
//     {.r = 255, .g =   0, .b = 255},
//     {.r =   0, .g = 255, .b = 255},
//     {.r = 128, .g = 128, .b = 255},
//     {.r = 128, .g = 255, .b = 128}
// };

esp_err_t LedControl_Init(LedControl *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings, BatterySensor *pBatterySensor, GameState *pGameState, uint32_t batteryIndicatorHoldTime)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    // initialize class object to all 0
    memset(this, 0, sizeof(*this));
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pUserSettings = pUserSettings;
    this->pBatterySensor = pBatterySensor;
    this->pGameState = pGameState;
    this->ledControlModeSettings.innerLedState = INNER_LED_STATE_LED_SEQUENCE;
    this->ledControlModeSettings.outerLedState = OUTER_LED_STATE_LED_SEQUENCE;
    this->batteryIndicatorRuntimeInfo.holdTime = batteryIndicatorHoldTime;

    // How To Make Colors with LED Lights (src: https://www.springtree.net/audio-visual-blog/rgb-led-color-mixing/)
    //     White: All colors at full brightness
    //     Orange: Red at full brightness, green just a little under half way, and no blue
    //     Light Blue: Red at around 30 percent of brightness, green up high at 80 percent, and blue at full brightness
    //     Magenta: Red and Blue at full brightness.
    //     Hot Pink: Red at 90 percent brightness, no green, and blue at 40 percent of brightness
    //     Yellow: Red and Green at full brightness.
    //     Light Pink: Red at 90 percent of its intensity, green and blue at 50 percent
    //     Cyan: Green and Blue at full brightness.
    //     Purple: Red and blue are all the way up, with green at 60 percent
    //     Pink Purple: Red at full brightness, green at 30, and blue at 90
    //     Brown: Too hard, don’t even try, plus why would you want to make a brown? 
    this->batteryIndicatorRuntimeInfo.initColor.r = 0; // off
    this->batteryIndicatorRuntimeInfo.initColor.g = 0;
    this->batteryIndicatorRuntimeInfo.initColor.b = 0;
    this->batteryIndicatorRuntimeInfo.greatColor.r = 0;
    this->batteryIndicatorRuntimeInfo.greatColor.g = 0;
    this->batteryIndicatorRuntimeInfo.greatColor.b = 200;
    this->batteryIndicatorRuntimeInfo.goodColor.r = 0;
    this->batteryIndicatorRuntimeInfo.goodColor.g = 200;
    this->batteryIndicatorRuntimeInfo.goodColor.b = 0;
    this->batteryIndicatorRuntimeInfo.warnColor.r = 211;
    this->batteryIndicatorRuntimeInfo.warnColor.g = 117;
    this->batteryIndicatorRuntimeInfo.warnColor.b = 6;
    this->batteryIndicatorRuntimeInfo.badColor.r = 200;
    this->batteryIndicatorRuntimeInfo.badColor.g = 0;
    this->batteryIndicatorRuntimeInfo.badColor.b = 0;
    this->touchModeRuntimeInfo.touchColor.r = 0;
    this->touchModeRuntimeInfo.touchColor.g = 0;
    this->touchModeRuntimeInfo.touchColor.b = 128;
    this->touchModeRuntimeInfo.shortPressColor.r = 0;
    this->touchModeRuntimeInfo.shortPressColor.g = 0;
    this->touchModeRuntimeInfo.shortPressColor.b = 255;
    this->touchModeRuntimeInfo.longPressColor.r = 0;
    this->touchModeRuntimeInfo.longPressColor.g = 255;
    this->touchModeRuntimeInfo.longPressColor.b = 255;
    this->touchModeRuntimeInfo.veryLongPressColor.r = 255;
    this->touchModeRuntimeInfo.veryLongPressColor.g = 255;
    this->touchModeRuntimeInfo.veryLongPressColor.b = 255;
    this->touchModeRuntimeInfo.updatePeriod = 100; // 100 ms

    // this->ledStatusIndicatorRuntimeInfo.statusIndicator = LED_STATUS_INDICATOR_NONE;
    this->ledStatusIndicatorRuntimeInfo.initColor = this->batteryIndicatorRuntimeInfo.initColor;
    this->ledStatusIndicatorRuntimeInfo.errorColor.r = 255;
    this->ledStatusIndicatorRuntimeInfo.errorColor.g = 0;
    this->ledStatusIndicatorRuntimeInfo.errorColor.b = 0;
    this->ledStatusIndicatorRuntimeInfo.bleServiceEnabledColor.r = 255;
    this->ledStatusIndicatorRuntimeInfo.bleServiceEnabledColor.g = 0;
    this->ledStatusIndicatorRuntimeInfo.bleServiceEnabledColor.b = 255;
    this->ledStatusIndicatorRuntimeInfo.bleReconnectingColor.r = 255;
    this->ledStatusIndicatorRuntimeInfo.bleReconnectingColor.g = 118; // orange?
    this->ledStatusIndicatorRuntimeInfo.bleReconnectingColor.b = 0;
    this->ledStatusIndicatorRuntimeInfo.bleConnectedColor.r = 0;
    this->ledStatusIndicatorRuntimeInfo.bleConnectedColor.g = 0;
    this->ledStatusIndicatorRuntimeInfo.bleConnectedColor.b = 255;
    this->ledStatusIndicatorRuntimeInfo.otaUpdateSuccessColor.r = 0;
    this->ledStatusIndicatorRuntimeInfo.otaUpdateSuccessColor.g = 255;
    this->ledStatusIndicatorRuntimeInfo.otaUpdateSuccessColor.b = 0;
    this->ledStatusIndicatorRuntimeInfo.otaUpdateInProgressColor.r = 255;
    this->ledStatusIndicatorRuntimeInfo.otaUpdateInProgressColor.g = 255;
    this->ledStatusIndicatorRuntimeInfo.otaUpdateInProgressColor.b = 0;
    this->ledStatusIndicatorRuntimeInfo.networkTestSuccessColor.r = 0;
    this->ledStatusIndicatorRuntimeInfo.networkTestSuccessColor.g = 255;
    this->ledStatusIndicatorRuntimeInfo.networkTestSuccessColor.b = 0;

    this->ledStatusIndicatorRuntimeInfo.innerLedWidth = 3;
    this->ledStatusIndicatorRuntimeInfo.outerLedWidth = 3;
    this->ledStatusIndicatorRuntimeInfo.curInnerPosition = 0;
    this->ledStatusIndicatorRuntimeInfo.curOuterPosition = 0;
    this->ledStatusIndicatorRuntimeInfo.revolutionsPerSecond = 1;
    this->ledStatusIndicatorRuntimeInfo.nextInnerDrawTime = 0;
    this->ledStatusIndicatorRuntimeInfo.nextOuterDrawTime = 0;

    this->bleFileTransferPercentRuntimeInfo.color.r = 255;
    this->bleFileTransferPercentRuntimeInfo.color.g = 255;
    this->bleFileTransferPercentRuntimeInfo.color.b = 0;
    this->bleFileTransferPercentRuntimeInfo.percentComplete = 0;
    this->bleFileTransferPercentRuntimeInfo.prevPercentComplete = 100;

    this->gameEventRuntimeInfo.initColor = this->batteryIndicatorRuntimeInfo.initColor;
    this->gameEventRuntimeInfo.updatePeriod = 50;
    this->gameEventRuntimeInfo.maxEventPulsesPerSecond = 10.0;
    this->gameEventRuntimeInfo.minEventPulsesPerSecond = 0.25;
    this->gameEventRuntimeInfo.nextInnerDrawTime = 0;
    this->gameEventRuntimeInfo.nextOuterDrawTime = 0;
    this->gameEventRuntimeInfo.outerLedWidth = 2;
    this->gameEventRuntimeInfo.curOuterPosition = 0;
    this->gameEventRuntimeInfo.revolutionsPerSecond = 1;
    
    this->networkTestRuntimeInfo.color.r = 0;
    this->networkTestRuntimeInfo.color.g = 0;
    this->networkTestRuntimeInfo.color.b = 255;
    this->networkTestRuntimeInfo.success = false;

    // Initialize LED strip
    this->ledStrip.strip_gpio_num = LED_STRIP_GPIO;
    this->ledStrip.max_leds = LED_STRIP_LEN;
    this->ledStrip.led_model = LED_TYPE;
    this->ledStrip.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    this->ledStrip.flags.invert_out = false;

    // Configuration for RMT driver
// #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
//     this->ledStripRmt.rmt_channel = 0;
// #else
//     this->ledStripRmt.clk_src           = RMT_CLK_SRC_DEFAULT;
//     this->ledStripRmt.resolution_hz     = LED_STRIP_RMT_RES_HZ;
//     this->ledStripRmt.flags.with_dma    = false;                   // DMA feature is available on ESP target like ESP32-S3;
// #endif

//     ESP_ERROR_CHECK(led_strip_new_rmt_device(&this->ledStrip, &this->ledStripRmt, &this->ledStripHandle));


    // Configuration for SPI
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0, 0)
    this->ledStripSpi.clk_src = SPI_CLK_SRC_DEFAULT;    // different clock source can lead to different power consumption
    this->ledStripSpi.flags.with_dma = true;            // Using DMA can improve performance and help drive more LEDs
    this->ledStripSpi.spi_bus = SPI2_HOST;              // SPI bus ID
#endif
    ESP_ERROR_CHECK(led_strip_new_spi_device(&this->ledStrip, &this->ledStripSpi, &this->ledStripHandle));

    this->ledControlModeSettings.innerLedState = INNER_LED_STATE_LED_SEQUENCE;
    this->ledControlModeSettings.outerLedState = OUTER_LED_STATE_LED_SEQUENCE;

    this->jsonMutex = xSemaphoreCreateMutex();
    assert(this->jsonMutex);

    this->ledLoadedIndex = 0xffffffff; // this is to be sure that the first time we run the service draw Json routine, the led sequence is loaded properly

    this->selectedIndex = pUserSettings->settings.selectedIndex;

    LedControl_SetCurrentLedSequenceIndex(this, this->selectedIndex);

    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_GAME_EVENT_JOINED, &LedControl_GameStatusNotificationHandler, this));
    // ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &LedControl_TouchSensorNotificationHandler, this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_SERVICE_PERCENT_CHANGED, &LedControl_BleControlPercentChangedNotificationHandler, this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_SONG_NOTE_ACTION, &LedControl_SongNoteActionNotificationHandler, this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_INTERACTIVE_GAME_ACTION, &LedControl_InteractiveGameActionNotificationHandler, this));

    assert(xTaskCreatePinnedToCore(LedControlTask, "LedControlTask", configMINIMAL_STACK_SIZE * 2, this, LED_CONTROL_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
    return ret;
}

esp_err_t LedControl_SetInnerLedState(LedControl *this, InnerLedState state)
{
    esp_err_t ret = ESP_FAIL;

    if (state >= INNER_LED_STATE_OFF && state < NUM_INNER_LED_STATES)
    {
        this->ledControlModeSettings.innerLedState = state;
        switch (state)
        {
        case INNER_LED_STATE_OFF:
            this->drawLedNoneUpdateRequired = true;
            break;
        default:
            break;
        }
        ret = ESP_OK;
        ESP_LOGD(TAG, "Setting inner led state to %d", state);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid inner led state");
    }
    return ret;
}

esp_err_t LedControl_SetOuterLedState(LedControl *this, OuterLedState state)
{
    esp_err_t ret = ESP_FAIL;

    if (state >= OUTER_LED_STATE_OFF && state < NUM_OUTER_LED_STATES)
    {
        this->ledControlModeSettings.outerLedState = state;
        switch (state)
        {
        case OUTER_LED_STATE_OFF:
            this->drawLedNoneUpdateRequired = true;
            break;
        default:
            break;
        }
        ret = ESP_OK;
        ESP_LOGD(TAG, "Setting outer led state to %d", state);
    }
    else
    {
        ESP_LOGE(TAG, "Invalid outer led state");
    }
    return ret;
}

static void LedControlTask(void *pvParameters)
{
    LedControl * this = (LedControl *)pvParameters;
    assert(this);
    while (true)
    {
        LedControl_ServiceDrawNoneSequence             ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_OFF,                   this->ledControlModeSettings.innerLedState == INNER_LED_STATE_OFF              );
        LedControl_ServiceDrawJsonLedSequence          ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_LED_SEQUENCE,          this->ledControlModeSettings.innerLedState == INNER_LED_STATE_LED_SEQUENCE     );
        LedControl_ServiceDrawBatteryIndicatorSequence ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_BATTERY_STATUS,        this->ledControlModeSettings.innerLedState == INNER_LED_STATE_BATTERY_STATUS   );
        LedControl_ServiceDrawPercentCompleteSequence  ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_BLE_FILE_XFER_PCNT,     this->ledControlModeSettings.innerLedState == INNER_LED_MODE_BLE_FILE_XFER_PCNT);
        LedControl_ServiceDrawGameStatusSequence       ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_GAME_STATUS,           this->ledControlModeSettings.innerLedState == INNER_LED_STATE_GAME_STATUS      );
        LedControl_ServiceDrawGameEventSequence        ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_GAME_EVENT,            this->ledControlModeSettings.innerLedState == INNER_LED_STATE_GAME_EVENT       );
        LedControl_ServiceDrawGameInteractiveSequence  ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_GAME_INTERACTIVE,      false                                                                          );
        LedControl_ServiceDrawTouchLightingSequence    ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_TOUCH_LIGHTING,        this->ledControlModeSettings.innerLedState == INNER_LED_STATE_TOUCH_LIGHTING   );
        LedControl_ServiceDrawBleEnabledSequence       ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_BLE_SERVICE_ENABLE,    false                                                                          );
        LedControl_ServiceDrawBleConnectedSequence     ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_BLE_SERVICE_CONNECTED, false                                                                          );
        LedControl_ServiceDrawBleReconnectingSequence  ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_BLE_RECONNECTING,      false                                                                          );
        LedControl_ServiceDrawOtaDownloadInProgSequence( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_OTA_DOWNLOAD_IP,       false                                                                          );
        LedControl_ServiceDrawNetworkTestSequence      ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_NETWORK_TEST,          this->ledControlModeSettings.innerLedState == INNER_LED_STATE_NETWORK_TEST     );
        LedControl_ServiceDrawSongModeSequence         ( this, this->ledControlModeSettings.outerLedState == OUTER_LED_STATE_SONG_MODE,             false                                                                          );
        LedControl_FlushLedStrip(this);
        vTaskDelay(pdMS_TO_TICKS(LED_CONTROL_TASK_PERIOD));
    }
}

static esp_err_t LedControl_FlushLedStrip(LedControl *this)
{
    esp_err_t ret = ESP_OK;
    if (this->flushNeeded)
    {
        this->flushNeeded = false;
        // ESP_LOGI(TAG, "Refreshing led strip");
        ret = led_strip_refresh(this->ledStripHandle);
    }
    return ret;
}

static esp_err_t LedControl_ServiceDrawJsonLedSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if (!allowDrawOuterRing && !allowDrawInnerRing)
    {
        return ret;
    }

    if (this->loadRequired)
    {
        this->loadRequired = false;
        ESP_LOGI(TAG, "Loading json sequence %lu", this->selectedIndex);
        if (LedControl_LoadJsonLedSequence(this) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to load json sequence %lu", this->selectedIndex);
        }
        UserSettings_SetSelectedIndex(this->pUserSettings, this->selectedIndex);
        this->ledLoadedIndex = this->selectedIndex;
    }

    // if (xSemaphoreTake(this->jsonMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        if (this->jsonSequenceRuntimeInfo.valid && this->jsonSequenceRuntimeInfo.root != NULL)
        {
            if (TimeUtils_IsTimeExpired(this->jsonSequenceRuntimeInfo.nextFrameDrawTime))
            {
                cJSON *frame = cJSON_GetArrayItem(this->jsonSequenceRuntimeInfo.frames,this->jsonSequenceRuntimeInfo.curFrameIndex);
                if(!frame)
                {
                    ESP_LOGE(TAG, "cJSON failed to get frame");
                    return ESP_FAIL;
                }
                if (cJSON_GetObjectItem(frame,"h") == NULL)
                {
                    ESP_LOGE(TAG, "frame index=%d is corrupt. hold time \"h\" not found", this->jsonSequenceRuntimeInfo.curFrameIndex);
                    return ESP_FAIL;
                }
                if (cJSON_GetObjectItem(frame,"p") == NULL)
                {
                    ESP_LOGE(TAG, "frame index=%d is corrupt. pixel array \"p\" not found", this->jsonSequenceRuntimeInfo.curFrameIndex);
                    return ESP_FAIL;
                }

                uint32_t hold_time = MAX(0, cJSON_GetObjectItem(frame,"h")->valueint);
                this->jsonSequenceRuntimeInfo.nextFrameDrawTime = TimeUtils_GetFutureTimeTicks(hold_time);
                this->flushNeeded = true;

                cJSON *pixelArray = cJSON_GetObjectItem(frame,"p");
                int pixelsArraySize = cJSON_GetArraySize(pixelArray);

                if(this->jsonSequenceRuntimeInfo.curFrameIndex <= 2)
                {
                    ESP_LOGD(TAG, "Starting sequence. hold_time=%lu, pixels_array_size=%d, frame_idx=%d", hold_time,
                            pixelsArraySize, this->jsonSequenceRuntimeInfo.curFrameIndex);
                }

                for (int pixelIndex=0; pixelIndex < pixelsArraySize; pixelIndex++)
                {
                    cJSON *pixel = cJSON_GetArrayItem(pixelArray, pixelIndex);
                    if(!pixel)
                    {
                        ESP_LOGE(TAG, "cJSON failed to get pixel");
                        continue;
                    }
                    cJSON *n1JSON = cJSON_GetObjectItem(pixel,"n1");
                    cJSON *n2JSON = cJSON_GetObjectItem(pixel,"n2");

                    if (n1JSON == NULL && n2JSON == NULL)
                    {
                        ESP_LOGE(TAG, "frame index=%d is corrupt. \"n1\" and \"n2\" not present. one of them must be set", this->jsonSequenceRuntimeInfo.curFrameIndex);
                        continue;
                    }

                    int n1 = -2;
                    if (n1JSON != NULL)
                    {
                        n1 = MAX(-1, MIN(LED_STRIP_LEN-1, n1JSON->valueint));
                    }
                    
                    int n2 = -2;
                    if (n2JSON != NULL)
                    {
                        n2 = MAX(-1, MIN(LED_STRIP_LEN-1, n2JSON->valueint));
                    }

                    cJSON * rJSON = cJSON_GetObjectItem(pixel,"r");
                    cJSON * gJSON = cJSON_GetObjectItem(pixel,"g");
                    cJSON * bJSON = cJSON_GetObjectItem(pixel,"b");
                    cJSON * iJSON = cJSON_GetObjectItem(pixel,"i");

                    if(this->jsonSequenceRuntimeInfo.curFrameIndex == 0)
                    {
                        ESP_LOGD(TAG, "pixel=%d, n1=%d, n2=%d, r=%d, g=%d, b=%d, i=%d", pixelIndex, n1, n2, rJSON->valueint, gJSON->valueint, bJSON->valueint, iJSON->valueint);
                    }
                    if (rJSON == NULL && 
                        gJSON == NULL &&
                        bJSON == NULL &&
                        iJSON == NULL)
                    {
                        ESP_LOGE(TAG, "frame index=%d is corrupt. one of the following must be specified: \"r\", \"g\", \"b\", \"i\" not found", 
                                this->jsonSequenceRuntimeInfo.curFrameIndex);
                        continue;
                    }

                    bool changeDetected = false;
                    // if n1 is set to fixed index, but n2 is not, only set pixel at fixed index
                    if (n1 >= 0 && n2 == -2)
                    {
                        if ((LedControl_IndexIsOuterRing(n1) && allowDrawOuterRing) || (LedControl_IndexIsInnerRing(n1) && allowDrawInnerRing))
                        {
                            LedControl_SetPixelFromJson(this, n1, rJSON, gJSON, bJSON, iJSON, &changeDetected);
                        }
                    }
                    // if n2 is set to fixed index, but n1 is not, only set pixel at fixed index
                    else if (n1 == -2 && n2 >= 0)
                    {
                        if ((LedControl_IndexIsOuterRing(n2) && allowDrawOuterRing) || (LedControl_IndexIsInnerRing(n2) && allowDrawInnerRing))
                        {
                            LedControl_SetPixelFromJson(this, n2, rJSON, gJSON, bJSON, iJSON, &changeDetected);
                        }
                    }
                    // if n1 and n2 are set to fixed indexes, use range of indexes
                    else if (n1 >= 0 || n2 >= 0)
                    {
                        int min = MAX(0,MIN(n1, n2));
                        int max = MIN(LED_STRIP_LEN-1, MAX(n1, n2));
                        for (int pixelItr = min; pixelItr <= max; pixelItr++)
                        {
                            if ((LedControl_IndexIsOuterRing(pixelItr) && allowDrawOuterRing) || (LedControl_IndexIsInnerRing(pixelItr) && allowDrawInnerRing))
                            {
                                LedControl_SetPixelFromJson(this, pixelItr, rJSON, gJSON, bJSON, iJSON, &changeDetected);
                            }
                        }
                    }
                    // if n1 or n2 are set to -1, the -1 takes precedence and the other value is ignored
                    else if (n1 == -1 || n2 == -1)
                    {
                        for (int pixelItr = 0; pixelItr < LED_STRIP_LEN; pixelItr++)
                        {
                            if ((LedControl_IndexIsOuterRing(pixelItr) && allowDrawOuterRing) || (LedControl_IndexIsInnerRing(pixelItr) && allowDrawInnerRing))
                            {
                                LedControl_SetPixelFromJson(this, pixelItr, rJSON, gJSON, bJSON, iJSON, &changeDetected);
                            }
                        }
                    }
                    else
                    {
                        ESP_LOGE(TAG, "frame index=%d is contains unhandled pixel indexes. n1=%d n2=%d", this->jsonSequenceRuntimeInfo.curFrameIndex, n1, n2);
                    }
                } // pixel iteration within a frame
                this->jsonSequenceRuntimeInfo.curFrameIndex = (this->jsonSequenceRuntimeInfo.curFrameIndex + 1) % this->jsonSequenceRuntimeInfo.numFrames;
            } // frame iteration within a json
        }
        // if (xSemaphoreGive(this->jsonMutex) != pdTRUE)
        // {
        //     ESP_LOGE(TAG, "Unable to give json mutex in ServiceDrawingJsonLedSequence");
        // }
    }
    // else
    // {
    //     ESP_LOGE(TAG, "Unable to take json mutex");
    // }
    return ret;
}

static esp_err_t LedControl_InitServiceDrawBatteryIndicatorSequence(LedControl *this)
{
    assert(this);
    int batteryPercent = BatterySensor_GetBatteryPercent(this->pBatterySensor);
    this->batteryIndicatorRuntimeInfo.startDrawTime = xTaskGetTickCount();
    this->batteryIndicatorRuntimeInfo.numOuterLeds = MAX(1, (int)(OUTER_RING_LED_COUNT * batteryPercent / 100.0));
    this->batteryIndicatorRuntimeInfo.numInnerLeds = MAX(1, (int)(INNER_RING_LED_COUNT * batteryPercent / 100.0));
    ESP_LOGI(TAG, "perc=%d numOuterLeds=%d numInnerLeds=%d", BatterySensor_GetBatteryPercent(this->pBatterySensor), 
             this->batteryIndicatorRuntimeInfo.numOuterLeds, this->batteryIndicatorRuntimeInfo.numInnerLeds);
    this->batteryIndicatorRuntimeInfo.color = this->batteryIndicatorRuntimeInfo.greatColor;
    if (batteryPercent >= 90)
    {
        this->batteryIndicatorRuntimeInfo.color = this->batteryIndicatorRuntimeInfo.greatColor;
    }
    else if ((batteryPercent < 90) && (batteryPercent >= 50))
    {
        this->batteryIndicatorRuntimeInfo.color = this->batteryIndicatorRuntimeInfo.goodColor;
    }
    else if ((batteryPercent < 50) && (batteryPercent >= 25))
    {
        this->batteryIndicatorRuntimeInfo.color = this->batteryIndicatorRuntimeInfo.warnColor;
    }
    else if (batteryPercent < 25)
    {
        this->batteryIndicatorRuntimeInfo.color = this->batteryIndicatorRuntimeInfo.badColor;
    }
    this->batteryIndicatorRuntimeInfo.innerLedsDrawIterator = 0;
    this->batteryIndicatorRuntimeInfo.outerLedsDrawIterator = 0;
    this->batteryIndicatorRuntimeInfo.nextOuterFrameDrawTime = this->batteryIndicatorRuntimeInfo.startDrawTime;
    this->batteryIndicatorRuntimeInfo.nextInnerFrameDrawTime = this->batteryIndicatorRuntimeInfo.startDrawTime;

    memset(this->pixelColorState, 0, sizeof(this->pixelColorState));
    ESP_ERROR_CHECK(LedControll_FillPixels(this, this->batteryIndicatorRuntimeInfo.initColor, OUTER_RING_LED_OFFSET, OUTER_RING_LED_COUNT));
    ESP_ERROR_CHECK(LedControll_FillPixels(this, this->batteryIndicatorRuntimeInfo.initColor, INNER_RING_LED_OFFSET, INNER_RING_LED_COUNT));
    return ESP_OK;
}

static esp_err_t LedControl_ServiceDrawBatteryIndicatorSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    assert(this);
    color_t color = {.r=this->batteryIndicatorRuntimeInfo.color.r,
                        .g=this->batteryIndicatorRuntimeInfo.color.g,
                        .b=this->batteryIndicatorRuntimeInfo.color.b,
                        .i=100};
    if (allowDrawOuterRing && TimeUtils_IsTimeExpired(this->batteryIndicatorRuntimeInfo.nextOuterFrameDrawTime))
    {
        if (this->batteryIndicatorRuntimeInfo.outerLedsDrawIterator < this->batteryIndicatorRuntimeInfo.numOuterLeds)
        {
            this->flushNeeded = true;
            int ledsToSet = 1;
            int holdTime = this->batteryIndicatorRuntimeInfo.holdTime / OUTER_RING_LED_COUNT;
            double holdTimeDbl = this->batteryIndicatorRuntimeInfo.holdTime / (double)OUTER_RING_LED_COUNT;
            if (holdTimeDbl < (double)LED_CONTROL_TASK_PERIOD)
            {
                int ledsRemaining = this->batteryIndicatorRuntimeInfo.numOuterLeds - this->batteryIndicatorRuntimeInfo.outerLedsDrawIterator;
                ledsToSet = MIN((int)LED_CONTROL_TASK_PERIOD/holdTimeDbl, ledsRemaining);
                holdTime = 0; // led control task has a min period of 50ms, if less than 50ms, ensure we run the next frame by setting next time to 0
            }
            
            this->batteryIndicatorRuntimeInfo.nextOuterFrameDrawTime = TimeUtils_GetFutureTimeTicks(holdTime);
            for (int i = 0; i < ledsToSet; i++)
            {
                ESP_ERROR_CHECK(LedControl_SetPixel(this, color, correctedPixelOffset[this->batteryIndicatorRuntimeInfo.outerLedsDrawIterator+OUTER_RING_LED_OFFSET]));
                this->batteryIndicatorRuntimeInfo.outerLedsDrawIterator++;
            }
        }
    }
    if (allowDrawInnerRing && TimeUtils_IsTimeExpired(this->batteryIndicatorRuntimeInfo.nextInnerFrameDrawTime))
    {
        if (this->batteryIndicatorRuntimeInfo.innerLedsDrawIterator < this->batteryIndicatorRuntimeInfo.numInnerLeds)
        {
            this->flushNeeded = true;
            int ledsToSet = 1;
            int holdTime = this->batteryIndicatorRuntimeInfo.holdTime / OUTER_RING_LED_COUNT;
            double holdTimeDbl = this->batteryIndicatorRuntimeInfo.holdTime / (double)INNER_RING_LED_COUNT;
            if (holdTimeDbl < (double)LED_CONTROL_TASK_PERIOD)
            {
                int ledsRemaining = this->batteryIndicatorRuntimeInfo.numOuterLeds - this->batteryIndicatorRuntimeInfo.outerLedsDrawIterator;
                ledsToSet = MIN((int)LED_CONTROL_TASK_PERIOD/holdTimeDbl, ledsRemaining);
                holdTime = 0; // led control task has a min period of 50ms, if less than 50ms, ensure we run the next frame by setting next time to 0
            }

            this->batteryIndicatorRuntimeInfo.nextInnerFrameDrawTime = TimeUtils_GetFutureTimeTicks(holdTime);
            for (int i = 0; i < ledsToSet; i++)
            {
                ESP_ERROR_CHECK(LedControl_SetPixel(this, color, correctedPixelOffset[this->batteryIndicatorRuntimeInfo.innerLedsDrawIterator+INNER_RING_LED_OFFSET]));
                this->batteryIndicatorRuntimeInfo.innerLedsDrawIterator++;
            }
        }
    }

    return ESP_OK;
}


static esp_err_t LedControl_ServiceDrawPercentCompleteSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    assert(this);
    rgb_t initColor = { .r=0, .g=0, .b=0 };
    if ((allowDrawOuterRing || allowDrawInnerRing) && this->bleFileTransferPercentRuntimeInfo.prevPercentComplete != this->bleFileTransferPercentRuntimeInfo.percentComplete)
    {
        int numOuterLeds = MAX(1, (int)(OUTER_RING_LED_COUNT * this->bleFileTransferPercentRuntimeInfo.percentComplete / 100.0));
        int numInnerLeds = MAX(1, (int)(INNER_RING_LED_COUNT * this->bleFileTransferPercentRuntimeInfo.percentComplete / 100.0));
        ESP_LOGD(TAG, "perc=%lu numOuterLeds=%d numInnerLeds=%d", this->bleFileTransferPercentRuntimeInfo.percentComplete, numOuterLeds, numInnerLeds);
        this->bleFileTransferPercentRuntimeInfo.prevPercentComplete = this->bleFileTransferPercentRuntimeInfo.percentComplete;

        if (allowDrawOuterRing)
        {
            this->flushNeeded = true;
            LedControll_FillPixelsWithIntensity(this, initColor, 0, OUTER_RING_LED_OFFSET+numOuterLeds, OUTER_RING_LED_COUNT-numOuterLeds);
            LedControll_FillPixelsWithIntensity(this, this->bleFileTransferPercentRuntimeInfo.color, 100, OUTER_RING_LED_OFFSET, numOuterLeds);
        }
        if (allowDrawInnerRing)
        {
            this->flushNeeded = true;
            LedControll_FillPixelsWithIntensity(this, initColor, 0, INNER_RING_LED_OFFSET+numInnerLeds, INNER_RING_LED_COUNT-numInnerLeds);
            LedControll_FillPixelsWithIntensity(this, this->bleFileTransferPercentRuntimeInfo.color, 100, INNER_RING_LED_OFFSET, numInnerLeds);
        }
    }

    return ESP_OK;
}

static esp_err_t LedControl_DrawStatusIndicator(LedControl *this, color_t color, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    esp_err_t ret = ESP_OK;
    if (allowDrawOuterRing && TimeUtils_IsTimeExpired(this->ledStatusIndicatorRuntimeInfo.nextOuterDrawTime))
    {
        this->flushNeeded = true;
        uint32_t holdTime = 1000 / (OUTER_RING_LED_COUNT * this->ledStatusIndicatorRuntimeInfo.revolutionsPerSecond);
        this->ledStatusIndicatorRuntimeInfo.nextOuterDrawTime = TimeUtils_GetFutureTimeTicks(holdTime);

        LedControll_FillPixelsWithIntensity(this, this->ledStatusIndicatorRuntimeInfo.initColor, 100, OUTER_RING_LED_OFFSET, OUTER_RING_LED_COUNT);
        for (int i = 0; i < this->ledStatusIndicatorRuntimeInfo.outerLedWidth; i++)
        {
            int pixelIndex = ((this->ledStatusIndicatorRuntimeInfo.curOuterPosition+i) % OUTER_RING_LED_COUNT);
            ret = LedControl_SetPixel(this, color, correctedPixelOffset[pixelIndex+OUTER_RING_LED_OFFSET]);
        }

        this->ledStatusIndicatorRuntimeInfo.curOuterPosition = (this->ledStatusIndicatorRuntimeInfo.curOuterPosition+1) % OUTER_RING_LED_COUNT;
    }
    if (allowDrawInnerRing && TimeUtils_IsTimeExpired(this->ledStatusIndicatorRuntimeInfo.nextInnerDrawTime))
    {
        this->flushNeeded = true;
        int holdTime = 1000 / (INNER_RING_LED_COUNT * this->ledStatusIndicatorRuntimeInfo.revolutionsPerSecond);
        this->ledStatusIndicatorRuntimeInfo.nextInnerDrawTime = TimeUtils_GetFutureTimeTicks(holdTime);
        LedControll_FillPixelsWithIntensity(this, this->ledStatusIndicatorRuntimeInfo.initColor, 100, INNER_RING_LED_OFFSET, INNER_RING_LED_COUNT);

        for (int i = 0; i < this->ledStatusIndicatorRuntimeInfo.innerLedWidth; i++)
        {
            int pixelIndex = ((this->ledStatusIndicatorRuntimeInfo.curInnerPosition+i) % INNER_RING_LED_COUNT);
            ret = LedControl_SetPixel(this, color, correctedPixelOffset[pixelIndex+INNER_RING_LED_OFFSET]);
        }

        this->ledStatusIndicatorRuntimeInfo.curInnerPosition = (this->ledStatusIndicatorRuntimeInfo.curInnerPosition+1) % INNER_RING_LED_COUNT;
    }
    return ret;
}

static esp_err_t LedControl_ServiceDrawBleEnabledSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    assert(this);
    color_t color = {.r=this->ledStatusIndicatorRuntimeInfo.bleServiceEnabledColor.r, .g=this->ledStatusIndicatorRuntimeInfo.bleServiceEnabledColor.g, .b=this->ledStatusIndicatorRuntimeInfo.bleServiceEnabledColor.b, .i=100};
    return LedControl_DrawStatusIndicator(this, color, allowDrawOuterRing, allowDrawInnerRing);
}

static esp_err_t LedControl_ServiceDrawBleReconnectingSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    assert(this);
    color_t color = {.r=this->ledStatusIndicatorRuntimeInfo.bleReconnectingColor.r, .g=this->ledStatusIndicatorRuntimeInfo.bleReconnectingColor.g, .b=this->ledStatusIndicatorRuntimeInfo.bleReconnectingColor.b, .i=100};
    return LedControl_DrawStatusIndicator(this, color, allowDrawOuterRing, allowDrawInnerRing);
}

static esp_err_t LedControl_ServiceDrawBleConnectedSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    assert(this);
    color_t color = {.r=this->ledStatusIndicatorRuntimeInfo.bleConnectedColor.r, .g=this->ledStatusIndicatorRuntimeInfo.bleConnectedColor.g, .b=this->ledStatusIndicatorRuntimeInfo.bleConnectedColor.b, .i=100};
    return LedControl_DrawStatusIndicator(this, color, allowDrawOuterRing, allowDrawInnerRing);
}

static esp_err_t LedControl_ServiceDrawOtaDownloadInProgSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    assert(this);
    color_t color = {.r=this->ledStatusIndicatorRuntimeInfo.otaUpdateInProgressColor.r, .g=this->ledStatusIndicatorRuntimeInfo.otaUpdateInProgressColor.g, .b=this->ledStatusIndicatorRuntimeInfo.otaUpdateInProgressColor.b, .i=100};
    return LedControl_DrawStatusIndicator(this, color, allowDrawOuterRing, allowDrawInnerRing);
}

static esp_err_t LedControl_ServiceDrawGameStatusSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if (allowDrawOuterRing || allowDrawInnerRing)
    {
        if (this->gameStatusRuntimeInfo.updateNeeded)
        {
            this->gameStatusRuntimeInfo.updateNeeded = false;
            for (int stoneIndex = 0; stoneIndex < NUM_GAMESTATE_EVENTCOLORS; stoneIndex++)
            {
                color_t color = { .r = stoneColorMap[stoneIndex].r, .g = stoneColorMap[stoneIndex].g, .b = stoneColorMap[stoneIndex].b, .i = 100};
                for (int ledIndexIter = 0; ledIndexIter < gameStatusMap[stoneIndex].numIndexes; ledIndexIter++)
                {
                //     if (allowDrawOuterRing)
                //     {
                //         ret = LedControl_SetPixel(this, color, correctedPixelOffset[gameStatusMap[stoneIndex].indexes[ledIndexIter]+OUTER_RING_LED_OFFSET]);
                //     }
                    if (allowDrawInnerRing)
                    {
                        ret = LedControl_SetPixel(this, color, correctedPixelOffset[gameStatusMap[stoneIndex].indexes[ledIndexIter]+INNER_RING_LED_OFFSET]);
                    }
                }
            }
        }
        this->flushNeeded = true;
        return ret;
    }

    return ret;
}


static esp_err_t LedControl_InitDrawGameEventSequence(LedControl * this)
{
    this->gameEventRuntimeInfo.nextOuterDrawTime = TimeUtils_GetFutureTimeTicks(100);
    this->gameEventRuntimeInfo.nextInnerDrawTime = TimeUtils_GetFutureTimeTicks(100);
    this->gameEventRuntimeInfo.curOuterPosition = 0;
    this->gameEventRuntimeInfo.curPulseDirection = 1;
    this->gameEventRuntimeInfo.curIntensity = 0;
    ESP_LOGI(TAG, "Init Draw Game Event. %lu %lu %lu", xTaskGetTickCount(), this->gameEventRuntimeInfo.nextOuterDrawTime, this->gameEventRuntimeInfo.nextInnerDrawTime);

    return ESP_OK;
}

static esp_err_t LedControl_ServiceDrawGameEventSequence(LedControl * this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if (allowDrawOuterRing || allowDrawInnerRing)
    {
        if (allowDrawOuterRing && TimeUtils_IsTimeExpired(this->gameEventRuntimeInfo.nextOuterDrawTime))
        {
            color_t color = { .r = stoneColorMap[this->pGameState->gameStateData.status.eventData.currentEventColor].r, 
                              .g = stoneColorMap[this->pGameState->gameStateData.status.eventData.currentEventColor].g,
                              .b = stoneColorMap[this->pGameState->gameStateData.status.eventData.currentEventColor].b, 
                              .i = 100 };
            this->flushNeeded = true;
            int holdTime = 1000 / (OUTER_RING_LED_COUNT * this->gameEventRuntimeInfo.revolutionsPerSecond);
            this->gameEventRuntimeInfo.nextOuterDrawTime = TimeUtils_GetFutureTimeTicks(holdTime);

            LedControll_FillPixelsWithIntensity(this, this->gameEventRuntimeInfo.initColor, 100, OUTER_RING_LED_OFFSET, OUTER_RING_LED_COUNT);
            for (int i = 0; i < this->gameEventRuntimeInfo.outerLedWidth; i++)
            {
                int pixelIndex = ((this->gameEventRuntimeInfo.curOuterPosition+i) % OUTER_RING_LED_COUNT);
                ret = LedControl_SetPixel(this, color, correctedPixelOffset[pixelIndex+OUTER_RING_LED_OFFSET]);
                pixelIndex = ((this->gameEventRuntimeInfo.curOuterPosition+i + ((1 * OUTER_RING_LED_COUNT)/4)) % OUTER_RING_LED_COUNT);
                ret = LedControl_SetPixel(this, color, correctedPixelOffset[pixelIndex+OUTER_RING_LED_OFFSET]);
                pixelIndex = ((this->gameEventRuntimeInfo.curOuterPosition+i + ((2 * OUTER_RING_LED_COUNT)/4)) % OUTER_RING_LED_COUNT);
                ret = LedControl_SetPixel(this, color, correctedPixelOffset[pixelIndex+OUTER_RING_LED_OFFSET]);
                pixelIndex = ((this->gameEventRuntimeInfo.curOuterPosition+i + ((3 * OUTER_RING_LED_COUNT)/4)) % OUTER_RING_LED_COUNT);
                ret = LedControl_SetPixel(this, color, correctedPixelOffset[pixelIndex+OUTER_RING_LED_OFFSET]);
            }

            this->gameEventRuntimeInfo.curOuterPosition = (this->gameEventRuntimeInfo.curOuterPosition+1) % OUTER_RING_LED_COUNT;
        }

        if (allowDrawInnerRing && TimeUtils_IsTimeExpired(this->gameEventRuntimeInfo.nextInnerDrawTime))
        {
            rgb_t color = stoneColorMap[this->pGameState->gameStateData.status.eventData.currentEventColor];
            this->flushNeeded = true;
            this->gameEventRuntimeInfo.nextInnerDrawTime = TimeUtils_GetFutureTimeTicks(this->gameEventRuntimeInfo.updatePeriod);
            uint32_t numInnerLeds = (uint32_t)((INNER_RING_LED_COUNT * this->pGameState->gameStateData.status.eventData.powerLevel)/100.0);
            double pulsesPerSecond = this->gameEventRuntimeInfo.minEventPulsesPerSecond + ((this->gameEventRuntimeInfo.maxEventPulsesPerSecond - this->gameEventRuntimeInfo.minEventPulsesPerSecond) * (1-(this->pGameState->gameStateData.status.eventData.mSecRemaining/(double)MAX_EVENT_TIME_MSEC)));
            double updatesPerSecond = (1000.0 / this->gameEventRuntimeInfo.updatePeriod);
            double totalIncrementsPerSecond = 200 * pulsesPerSecond;
            double increment = totalIncrementsPerSecond/updatesPerSecond;

            this->gameEventRuntimeInfo.curIntensity += (this->gameEventRuntimeInfo.curPulseDirection * increment);

            if (this->gameEventRuntimeInfo.curIntensity > 100.0)
            {
                this->gameEventRuntimeInfo.curIntensity = 100.0;
                this->gameEventRuntimeInfo.curPulseDirection = -1;
            }
            else if (this->gameEventRuntimeInfo.curIntensity < 0)
            {
                this->gameEventRuntimeInfo.curIntensity = 0.0;
                this->gameEventRuntimeInfo.curPulseDirection = 1;
            }

            LedControll_FillPixelsWithIntensity(this, this->gameEventRuntimeInfo.initColor, 0, INNER_RING_LED_OFFSET, INNER_RING_LED_COUNT);
            LedControll_FillPixelsWithIntensity(this, color, (uint32_t)this->gameEventRuntimeInfo.curIntensity, INNER_RING_LED_OFFSET, numInnerLeds);
        }
        return ret;
    }

    return ret;
}

static esp_err_t LedControl_ServiceDrawNetworkTestSequence(LedControl* this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    assert(this);
    rgb_t rgb;
    if (this->networkTestRuntimeInfo.success)
    {
        rgb = this->ledStatusIndicatorRuntimeInfo.otaUpdateSuccessColor;
    }
    else
    {
        rgb = this->ledStatusIndicatorRuntimeInfo.errorColor;
    }

    color_t color = {.r=rgb.r, .g=rgb.g, .b=rgb.b, .i=100};
    return LedControl_DrawStatusIndicator(this, color, allowDrawOuterRing, allowDrawInnerRing);
}


static esp_err_t LedControl_ServiceDrawGameInteractiveSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if (allowDrawOuterRing)
    {
        if (this->interactiveGameModeRuntimeSettings.updateNeeded)
        {
            this->interactiveGameModeRuntimeSettings.updateNeeded = false;
            this->flushNeeded = true;
            LedControll_FillPixelsWithIntensity(this, this->touchModeRuntimeInfo.initColor, 100, OUTER_RING_LED_OFFSET, OUTER_RING_LED_COUNT);

            for (int i = 0; i < TOUCH_SENSOR_NUM_BUTTONS; i++)
            {
                if (this->interactiveGameModeRuntimeSettings.touchSensorsToLightBits.u & (1 << i))
                {
                    while (true)
                    {
                        color_t random_color = {.r = GetRandomNumber(0, 256), .g = GetRandomNumber(0, 256), .b = GetRandomNumber(0, 256), .i = 100};
                        
                        // ensure random color has enough light to be visible
                        if (random_color.r >= 128 || random_color.g >= 128 || random_color.b >= 128)
                        {
                            color_t fail = { .r = 255, .g = 0, .b = 0, .i = 100 };
                            color_t color = this->interactiveGameModeRuntimeSettings.touchSensorsToLightBits.s.lastFailed? fail : random_color;
                            ESP_LOGD(TAG, "LedControl_ServiceDrawGameInteractiveSequence: using color r=%d g=%d b=%d", color.r, color.g, color.b);

                            for (int ledIndexIter = 0; ledIndexIter < touchMap[i].numIndexes; ledIndexIter++)
                            {
                                ret = LedControl_SetPixel(this, color, correctedPixelOffset[touchMap[i].indexes[ledIndexIter]]);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    if (allowDrawInnerRing)
    {
        ESP_LOGI(TAG, "LedControl_ServiceDrawGameInteractiveSequence does not support drawing inner ring");
    }
    return ret;
}


static esp_err_t LedControl_ServiceDrawSongModeSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if (allowDrawOuterRing)
    {
        if (this->songModeRuntimeInfo.updateNeeded)
        {
            this->songModeRuntimeInfo.updateNeeded = false;
            this->flushNeeded = true;
            LedControll_FillPixelsWithIntensity(this, this->touchModeRuntimeInfo.initColor, 100, OUTER_RING_LED_OFFSET, OUTER_RING_LED_COUNT);

            if (this->songModeRuntimeInfo.lastSongNoteChangeEventNotificationData.action == SONG_NOTE_CHANGE_TYPE_TONE_START)
            {
                NoteParts parts = GetNoteParts(this->songModeRuntimeInfo.lastSongNoteChangeEventNotificationData.note);
                if (parts.base != NOTE_BASE_NONE && parts.octave != NOTE_OCTAVE_NONE)
                {
                    int mapIndex = (parts.base + TOUCH_NOTE_OFFSET + (parts.octave * NUM_BASE_NOTES)) % NUM_LED_NOTES;
                    for (int ledIndexIter = 0; ledIndexIter < songMap[mapIndex].numIndexes; ledIndexIter++)
                    {
                        color_t color = songColorMap[parts.octave];
                        ret = LedControl_SetPixel(this, color, correctedPixelOffset[songMap[mapIndex].indexes[ledIndexIter]]);
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "Note Parts for %d are None. base: %d  octave: %d", 
                             this->songModeRuntimeInfo.lastSongNoteChangeEventNotificationData.note,
                             parts.base, parts.octave);
                }
            }
        }
    }
    return ret;
}

static esp_err_t LedControl_ServiceDrawNoneSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if ((allowDrawOuterRing || allowDrawInnerRing) && this->drawLedNoneUpdateRequired)
    {
        this->drawLedNoneUpdateRequired = false;
        rgb_t offColor = { .r = 0, .g = 0, .b = 0 };
        if (allowDrawOuterRing)
        {
            ret = LedControll_FillPixels(this, offColor, OUTER_RING_LED_OFFSET, OUTER_RING_LED_COUNT);
        }
        if (allowDrawInnerRing)
        {
            ret = LedControll_FillPixels(this, offColor, INNER_RING_LED_OFFSET, INNER_RING_LED_COUNT);
        }
    }
    return ret;
}

static esp_err_t LedControl_ServiceDrawTouchLightingSequence(LedControl *this, bool allowDrawOuterRing, bool allowDrawInnerRing)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if (allowDrawOuterRing || allowDrawInnerRing)
    {
        if (allowDrawOuterRing && TimeUtils_IsTimeExpired(this->touchModeRuntimeInfo.nextOuterDrawTime))
        {
            this->flushNeeded = true;
            LedControll_FillPixelsWithIntensity(this, this->touchModeRuntimeInfo.initColor, 100, OUTER_RING_LED_OFFSET, OUTER_RING_LED_COUNT);
        }
        if (allowDrawInnerRing && TimeUtils_IsTimeExpired(this->touchModeRuntimeInfo.nextInnerDrawTime))
        {
            this->flushNeeded = true;
            LedControll_FillPixelsWithIntensity(this, this->touchModeRuntimeInfo.initColor, 100, INNER_RING_LED_OFFSET, INNER_RING_LED_COUNT);
        }

        for (int touchIndex = 0; touchIndex < TOUCH_SENSOR_NUM_BUTTONS; touchIndex++)
        {
            rgb_t rbg = this->touchModeRuntimeInfo.initColor;
            switch(this->touchModeRuntimeInfo.touchSensorValue[touchIndex])
            {
                case TOUCH_SENSOR_EVENT_RELEASED:
                    break;
                case TOUCH_SENSOR_EVENT_TOUCHED:
                    rbg = this->touchModeRuntimeInfo.touchColor;
                    break;
                case TOUCH_SENSOR_EVENT_SHORT_PRESSED:
                    rbg = this->touchModeRuntimeInfo.shortPressColor;
                    break;
                case TOUCH_SENSOR_EVENT_LONG_PRESSED:
                    rbg = this->touchModeRuntimeInfo.longPressColor;
                    break;
                case TOUCH_SENSOR_EVENT_VERY_LONG_PRESSED:
                    rbg = this->touchModeRuntimeInfo.veryLongPressColor;
                    break;
                default:
                    ESP_LOGE(TAG, "Invalid touch sensor value %d", this->touchModeRuntimeInfo.touchSensorValue[touchIndex]);
                    return ESP_FAIL;
                    break;
            }
            for (int ledIndexIter = 0; ledIndexIter < touchMap[touchIndex].numIndexes; ledIndexIter++)
            {
                color_t color = { .r = rbg.r, .g = rbg.g, .b = rbg.b, .i = 100 };
                if (allowDrawOuterRing && TimeUtils_IsTimeExpired(this->touchModeRuntimeInfo.nextOuterDrawTime))
                {
                    ret = LedControl_SetPixel(this, color, correctedPixelOffset[touchMap[touchIndex].indexes[ledIndexIter]]);
                }
                // if (allowDrawInnerRing && TimeUtils_IsTimeExpired(this->touchModeRuntimeInfo.nextInnerDrawTime))
                // {
                //     ret = LedControl_SetPixel(this, color, correctedPixelOffset[touchMap[touchIndex].indexes[ledIndexIter]+INNER_RING_LED_OFFSET]);
                // }
            }
        }
        if (allowDrawOuterRing && TimeUtils_IsTimeExpired(this->touchModeRuntimeInfo.nextOuterDrawTime))
        {
            this->touchModeRuntimeInfo.nextOuterDrawTime = TimeUtils_GetFutureTimeTicks(this->touchModeRuntimeInfo.updatePeriod);
        }
        if (allowDrawInnerRing && TimeUtils_IsTimeExpired(this->touchModeRuntimeInfo.nextInnerDrawTime))
        {
            this->touchModeRuntimeInfo.nextInnerDrawTime = TimeUtils_GetFutureTimeTicks(this->touchModeRuntimeInfo.updatePeriod);
        }
    }
    return ret;
}

esp_err_t LedControl_SetCurrentLedSequenceIndex(LedControl *this, int newLedSequenceIndex)
{
    assert(this);
    ESP_LOGI(TAG, "Switching to led sequence %d", newLedSequenceIndex);
    int numSequences = LedSequences_GetNumLedSequences();
    if (newLedSequenceIndex >= numSequences)
    {
        ESP_LOGI(TAG, "Index %d >= %d", newLedSequenceIndex, numSequences);
        return ESP_FAIL;
    }
    this->selectedIndex = newLedSequenceIndex;
    this->loadRequired = true;
    ESP_LOGI(TAG, "this->selectedIndex = %lu", this->selectedIndex);
    
    return ESP_OK;
}

esp_err_t LedControl_CycleSelectedLedSequence(LedControl *this, bool direction)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    int intDir = direction ? 1 : -1;
    uint8_t nextIndex = 0;
    uint8_t curIndex = LedControl_GetCurrentLedSequenceIndex(this);
    int numSequences = LedSequences_GetNumLedSequences();
    for (int i = 1; i <= numSequences; i++)
    {
        nextIndex = ((uint8_t)(curIndex + (i*intDir)) % numSequences);
        ESP_LOGI(TAG, "curIndex = %d\t(i*intDir)=%d\tnumSequences = %d\tnextIndex = %d", curIndex, (i*intDir), numSequences, nextIndex);
        const char *json = LedSequences_GetLedSequenceJson(nextIndex);
        if (json != NULL && JsonUtils_ValidateJson(json))
        {
            ESP_LOGI(TAG, "direction %d", intDir);
            break;
        }
        ESP_LOGI(TAG, "sequence %d is invalid. skipping", nextIndex);
    }
    ret = LedControl_SetCurrentLedSequenceIndex(this, nextIndex);
    
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "LedControl_SetCurrentLedSequenceIndex fail. error = %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t LedControl_SetLedCustomSequence(LedControl *this, int newCustomIndex)
{
    assert(this);
    ESP_LOGI(TAG, "Switching to custom led sequence %d", newCustomIndex);
    if ((newCustomIndex >= LedSequences_GetNumCustomLedSequences()) || 
        (LedControl_SetCurrentLedSequenceIndex(this, newCustomIndex + LedSequences_GetCustomLedSequencesOffset()) != ESP_OK))
    {
        ESP_LOGE(TAG, "LedControl_SetLedCustomSequence fail");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "LedControl_SetLedCustomSequence success");
    return ESP_OK;
}

int LedControl_GetCurrentLedSequenceIndex(LedControl *this)
{
    assert(this);
    return this->selectedIndex;
}

static esp_err_t LedControll_FillPixels(LedControl *this, rgb_t rgb, int ledStartIndex, int numLedsToFill)
{
    esp_err_t ret = ESP_OK;
    color_t color = { .r = rgb.r, .g = rgb.g, .b = rgb.b, .i = 100 };
    for (int i = 0; i < numLedsToFill; i++)
    {
        if (LedControl_SetPixel(this, color, correctedPixelOffset[i+ledStartIndex]) != ESP_OK)
        {
            ESP_LOGE(TAG, "LedControl_SetPixel failed for index %d", i);
            ret = ESP_FAIL;
        }
    }
    return ret;
}

static esp_err_t LedControll_FillPixelsWithIntensity(LedControl *this, rgb_t rgb, int intensity, int ledStartIndex, int numLedsToFill)
{
    esp_err_t ret = ESP_OK;
    color_t color = { .r = rgb.r, .g = rgb.g, .b = rgb.b, .i = intensity };
    for (int i = 0; i < numLedsToFill; i++)
    {
        if (LedControl_SetPixel(this, color, correctedPixelOffset[i+ledStartIndex]) != ESP_OK)
        {
            ESP_LOGE(TAG, "LedControl_SetPixel failed for index %d", i);
            ret = ESP_FAIL;
        }
    }
    return ret;
}

static esp_err_t LedControl_SetPixel(LedControl *this, color_t in_color, int pix_num)
{
    assert(this);
    rgb_t color = { .r = (int)(in_color.r * (in_color.i / 100.0)), .g = (int)(in_color.g * (in_color.i / 100.0)), .b = (int)(in_color.b * (in_color.i / 100.0)) };
    color.r = (int)(color.r * BRIGHTNESS_NORMAL / 255.0);
    color.g = (int)(color.g * BRIGHTNESS_NORMAL / 255.0);
    color.b = (int)(color.b * BRIGHTNESS_NORMAL / 255.0);
    esp_err_t ret = led_strip_set_pixel(this->ledStripHandle, pix_num, color.red, color.green, color.blue);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "led_strip_set_pixel failed. pixnum= %d, error = %s", pix_num, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t LedControl_SetPixelFromJson(LedControl * this, int n, cJSON *r, cJSON *g, cJSON *b, cJSON *i, bool *pChangeDetected)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    bool changeDetected = false;
    if ((n < 0) || (n >= LED_STRIP_LEN))
    {
        ESP_LOGE(TAG, "LedControl_SetPixelFromJson was provided invalid n=%d", n);
        return false;
    }

    changeDetected = false;
    if (r != NULL)
    {
        int newR = MAX(0, MIN(255, r->valueint));
        if (newR != this->pixelColorState[n].r)
        {
            this->pixelColorState[n].r = newR;
            changeDetected = true;
        }
    }
    
    if (g != NULL)
    {
        int newG = MAX(0, MIN(255, g->valueint));
        if (newG != this->pixelColorState[n].g)
        {
            this->pixelColorState[n].g = newG;
            changeDetected = true;
        }
    }

    if (b != NULL)
    {
        int newB = MAX(0, MIN(255, b->valueint));
        if (newB != this->pixelColorState[n].b)
        {
            this->pixelColorState[n].b = newB;
            changeDetected = true;
        }
    }

    if (i != NULL)
    {
        int newI = MAX(0, MIN(100, i->valueint));
        if (newI != this->pixelColorState[n].i)
        {
            this->pixelColorState[n].i = newI;
            changeDetected = true;
        }
    }

    if (changeDetected)
    {
        ret = LedControl_SetPixel(this, this->pixelColorState[n], n);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "LedControl_SetPixelFromJson failed. error = %s", esp_err_to_name(ret));
        }
    }

    if (pChangeDetected != NULL)
    {
        *pChangeDetected = changeDetected;
    }
    return ret;
}

static esp_err_t LedControl_LoadJsonLedSequence(LedControl * this)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    const char *json = LedSequences_GetLedSequenceJson(this->selectedIndex);

    if (xSemaphoreTake(this->jsonMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        do
        {
            if (json == 0)
            {
                ESP_LOGE(TAG, "JSON null");
                ret = ESP_FAIL;
                break;
            }

            if (this->jsonSequenceRuntimeInfo.valid)
            {
                ESP_LOGI(TAG, "Deleting old json object");
                cJSON_Delete(this->jsonSequenceRuntimeInfo.root);
            }

            memset(&this->jsonSequenceRuntimeInfo, 0, sizeof(this->jsonSequenceRuntimeInfo));

            this->jsonSequenceRuntimeInfo.root = cJSON_Parse(json);
            if (this->jsonSequenceRuntimeInfo.root == NULL)
            {
                ESP_LOGE(TAG, "JSON parse failed. json = \"%s\"", json);
                ret = ESP_FAIL;
                break;
            }

            this->jsonSequenceRuntimeInfo.frames = cJSON_GetObjectItem(this->jsonSequenceRuntimeInfo.root,"f");
            if (this->jsonSequenceRuntimeInfo.frames == NULL)
            {
                ESP_LOGE(TAG, "frames not found in root json");
                cJSON_Delete(this->jsonSequenceRuntimeInfo.root);
                this->jsonSequenceRuntimeInfo.root = NULL;
                ret = ESP_FAIL;
                break;
            }

            this->jsonSequenceRuntimeInfo.numFrames = cJSON_GetArraySize(this->jsonSequenceRuntimeInfo.frames);
            if (this->jsonSequenceRuntimeInfo.numFrames <= 0)
            {
                ESP_LOGE(TAG, "unable to find frames array size");
                cJSON_Delete(this->jsonSequenceRuntimeInfo.root);
                this->jsonSequenceRuntimeInfo.root = NULL;
                ret = ESP_FAIL;
                break;
            }
            this->jsonSequenceRuntimeInfo.valid = true;
            ret = ESP_OK;
        } while (0);
        if (xSemaphoreGive(this->jsonMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give json mutex in LoadJsonLedSequence");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to lock json mutex");
    }
    
    return ret;
}

static bool LedControl_IndexIsInnerRing(int pixelIndex)
{
    return (pixelIndex >= INNER_RING_LED_OFFSET && pixelIndex < INNER_RING_LED_OFFSET+INNER_RING_LED_COUNT);
}

static bool LedControl_IndexIsOuterRing(int pixelIndex)
{
    return (pixelIndex >= OUTER_RING_LED_OFFSET && pixelIndex < OUTER_RING_LED_OFFSET+OUTER_RING_LED_COUNT);
}

void LedControl_SetTouchSensorUpdate(LedControl *this, TouchSensorEvent touchSensorEvent, int touchSensorIdx)
{
    this->touchModeRuntimeInfo.touchSensorValue[touchSensorIdx] = touchSensorEvent;
    ESP_LOGD(TAG, "Touch Sensor Update. %d: %d", touchSensorIdx, touchSensorEvent);
}

esp_err_t LedControl_SetLedMode(LedControl *this, LedMode mode)
{
    esp_err_t ret = ESP_OK;
    esp_err_t innerRet = ESP_OK, outerRet = ESP_OK, initRet = ESP_OK;
    assert(this);
    BadgeType badgeType = GetBadgeType();
    switch(mode)
    {
        case LED_MODE_SEQUENCE:
            ESP_LOGD(TAG, "Setting LED mode to sequence");
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_LED_SEQUENCE);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            break;
        case LED_MODE_SONG:
            ESP_LOGD(TAG, "Setting LED mode to song mode");
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_SONG_MODE);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            break;
        case LED_MODE_TOUCH:
            ESP_LOGD(TAG, "Setting LED mode to touch");
            this->touchModeRuntimeInfo.nextOuterDrawTime = TimeUtils_GetCurTimeTicks();
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_TOUCH_LIGHTING);
            innerRet = LedControl_SetInnerLedState(this, OUTER_LED_STATE_OFF);
            // if (badgeType == BADGE_TYPE_TRON || badgeType == BADGE_TYPE_REACTOR)
            // {
            //     this->touchModeRuntimeInfo.nextInnerDrawTime = TimeUtils_GetCurTimeTicks();
            //     innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_TOUCH_LIGHTING);
            // }
            // else
            // {
            //     innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            // }
            break;
        case LED_MODE_BATTERY:
            ESP_LOGD(TAG, "Setting LED mode to battery");
            initRet = LedControl_InitServiceDrawBatteryIndicatorSequence(this);
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_BATTERY_STATUS);
            if (badgeType == BADGE_TYPE_TRON || badgeType == BADGE_TYPE_REACTOR)
            {
                innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_BATTERY_STATUS);
            }
            else
            {
                innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            }
            break;
        case LED_MODE_BLE_FILE_TRANSFER_PERCENT:
            ESP_LOGD(TAG, "Setting LED mode to ble file transfer percent complete");
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_BLE_FILE_XFER_PCNT);
            if (badgeType == BADGE_TYPE_TRON || badgeType == BADGE_TYPE_REACTOR)
            {
                innerRet = LedControl_SetInnerLedState(this, INNER_LED_MODE_BLE_FILE_XFER_PCNT);
            }
            else
            {
                innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            }
            break;
        case LED_MODE_NETWORK_TEST:
            ESP_LOGD(TAG, "Setting LED mode to network test");
            this->networkTestRuntimeInfo.success = false;
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_NETWORK_TEST);
            if (badgeType == BADGE_TYPE_TRON || badgeType == BADGE_TYPE_REACTOR)
            {
                innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_NETWORK_TEST);
            }
            else
            {
                innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            }
            break;
        case LED_MODE_EVENT:
            ESP_LOGD(TAG, "Setting LED mode to event");
            initRet = LedControl_InitDrawGameEventSequence(this);
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_GAME_EVENT);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_GAME_EVENT);
            break;
        case LED_MODE_GAME_STATUS:
            ESP_LOGD(TAG, "Setting LED mode to game status");
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_LED_SEQUENCE);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_GAME_STATUS);
            break;
        case LED_MODE_BLE_FILE_TRANSFER_ENABLED:
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_BLE_SERVICE_ENABLE);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            break;
        case LED_MODE_BLE_FILE_TRANSFER_CONNECTED:
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_BLE_SERVICE_CONNECTED);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            break;
        case LED_MODE_OTA_DOWNLOAD_IP:
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_OTA_DOWNLOAD_IP);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            break;
        case LED_MODE_INTERACTIVE_GAME:
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_GAME_INTERACTIVE);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_GAME_EVENT);
            break;
        case LED_MODE_BLE_RECONNECTING:
            outerRet = LedControl_SetOuterLedState(this, OUTER_LED_STATE_BLE_RECONNECTING);
            innerRet = LedControl_SetInnerLedState(this, INNER_LED_STATE_LED_SEQUENCE);
            break;
        default:
            ESP_LOGE(TAG, "Invalid mode: %d", mode);
            ret = ESP_FAIL;
            break;
    }
    if (ret != ESP_OK || innerRet != ESP_OK || outerRet != ESP_OK || initRet != ESP_OK )
    {
        ret = ESP_FAIL;
    }
    return ret;
}

static void LedControl_BleControlPercentChangedNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling BLE Xfer Percent Changed Notification %lu", *((uint32_t *) notificationData));

    LedControl *this = (LedControl *)pObj;
    assert(this);
    this->bleFileTransferPercentRuntimeInfo.percentComplete = *((uint32_t *) notificationData);
}

static void LedControl_GameStatusNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Game State Notification %lu", *((uint32_t *) notificationData));

    LedControl *this = (LedControl *)pObj;
    assert(this);
    switch (notificationEvent)
    {
        case NOTIFICATION_EVENTS_GAME_EVENT_JOINED:
            this->gameStatusRuntimeInfo.updateNeeded = true;
            break;
        default:
            ESP_LOGE(TAG, "Invalid notification event: %lu", notificationEvent);
            break;
    }
}


static void LedControl_SongNoteActionNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Song Note Action Notification");
    LedControl *this = (LedControl *)pObj;
    assert(this);
    assert(notificationData);
    SongNoteChangeEventNotificationData data = *(SongNoteChangeEventNotificationData *)notificationData;

    if (data.action == SONG_NOTE_CHANGE_TYPE_TONE_START || 
        data.action == SONG_NOTE_CHANGE_TYPE_TONE_STOP)
    {
        this->songModeRuntimeInfo.lastSongNoteChangeEventNotificationData = data;
        this->songModeRuntimeInfo.updateNeeded = true;
    }
}


static void LedControl_InteractiveGameActionNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    LedControl *this = (LedControl *)pObj;
    assert(this);
    assert(notificationData);
    InteractiveGameData touchSensorsBits = *(InteractiveGameData *)notificationData;
    ESP_LOGD(TAG, "Handling Interactive Game Action Notification. 0x%02x", touchSensorsBits.u);

    this->interactiveGameModeRuntimeSettings.touchSensorsToLightBits.u = touchSensorsBits.u;
    this->interactiveGameModeRuntimeSettings.updateNeeded = true;
}


