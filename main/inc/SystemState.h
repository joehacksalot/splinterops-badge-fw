
#ifndef SYSTEM_STATE_H_
#define SYSTEM_STATE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "BadgeStats.h"
#include "BatterySensor.h"
#include "BleControl.h"
#include "GameState.h"
#include "GpioControl.h"
#include "LedModing.h"
#include "NotificationDispatcher.h"
#include "OtaUpdate.h"
#include "TouchSensor.h"
#include "TouchActions.h"
#include "UserSettings.h"
#include "WifiClient.h"
#include "HTTPGameClient.h"

#if defined(REACTOR_BADGE) || defined(CREST_BADGE)
    #include "SynthMode.h"
    #include "Ocarina.h"
#endif

typedef struct SystemState_t
{
    bool touchActive;
    bool batteryIndicatorActive;
    bool gameEventActive;
    bool touchActionCmdClearRequired;
    bool ledStatusIndicatorActive;
    bool ledSequencePreviewActive;
    bool ledGameStatusActive;
    bool networkTestActive;
    TimerHandle_t touchActiveTimer;
    TimerHandle_t drawBatteryIndicatorActiveTimer;
    TimerHandle_t drawStatusIndicatorTimer;
    TimerHandle_t drawNetworkTestTimer;
    TimerHandle_t drawNetworkTestSuccessTimer;
    TimerHandle_t ledSequencePreviewTimer;
    TimerHandle_t ledGameStatusToggleTimer;
    BadgeStats badgeStats;
    BatterySensor batterySensor;
    BleControl *pBleControl;
    // BleCameControl *pBleGameControl;
    GameState gameState;
    GpioControl gpioControl;
    LedControl ledControl;
    LedModing ledModing;
    NotificationDispatcher notificationDispatcher;
    OtaUpdate otaUpdate;
    TouchSensor touchSensor;
    TouchActions touchActions;
    UserSettings userSettings;
    WifiClient wifiClient;
    HTTPGameClient httpGameClient;

#if defined(REACTOR_BADGE) || defined(CREST_BADGE)
    SynthMode synthMode;
    Ocarina ocarina;
#endif
} SystemState;

SystemState *SystemState_GetInstance();
esp_err_t SystemState_Init(SystemState *this);

#endif // SYSTEM_STATE_H_
