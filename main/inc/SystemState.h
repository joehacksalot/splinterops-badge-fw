
#ifndef SYSTEM_STATE_H_
#define SYSTEM_STATE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "BadgeMetrics.h"
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

typedef struct SystemState_t
{
    bool touchActive;
    bool batteryIndicatorActive;
    bool gameEventActive;
    bool touchActionCmdClearRequired;
    bool ledGameStatusActive;
    bool networkTestActive;
    bool peerSongPlaying;
    bool peerSongWaitingCooldown;
    bool bleReconnecting;
    InteractiveGameData interactiveGameTouchSensorsToLightBits;
    TimerHandle_t touchActiveTimer;
    TimerHandle_t drawBatteryIndicatorActiveTimer;
    TimerHandle_t drawNetworkTestTimer;
    TimerHandle_t drawNetworkTestSuccessTimer;
    TimerHandle_t ledSequencePreviewTimer;
    TimerHandle_t ledGameStatusToggleTimer;
    TimerHandle_t peerSongCooldownTimer;
    AppConfig appConfig;
    BadgeMetrics badgeStats;
    BatterySensor batterySensor;
    BleControl bleControl;
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
    SynthMode synthMode;
    Ocarina ocarina;
} SystemState;

SystemState *SystemState_GetInstance();
esp_err_t SystemState_Init(SystemState *this);

#endif // SYSTEM_STATE_H_
