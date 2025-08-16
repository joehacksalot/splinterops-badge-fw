
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "mbedtls/base64.h"

#include "BadgeMetrics.h"
#include "BleControl.h"
#include "BleControl_Service.h"
#include "BleControl_ServiceChar_FileTransfer.h"
#include "Console.h"
#include "DiskUtilities.h"
#include "LedModing.h"
#include "LedSequences.h"
#include "NotificationDispatcher.h"
#include "NotificationEvents.h"
#include "Ocarina.h"
#include "OtaUpdate.h"
#include "SynthMode.h"
#include "SystemState.h"
#include "TaskPriorities.h"
#include "TimeUtils.h"
#include "TouchSensor.h"
#include "TouchActions.h"
#include "UserSettings.h"
#include "Utilities.h"
#include "WifiClient.h"

#define BATTERY_SENSOR_ADC_CHANNEL (ADC_CHANNEL_7)

#define PEER_RSSID_SONG_THRESHOLD_TRON              (-50)
#define PEER_RSSID_SONG_THRESHOLD_REACTOR           (-50)
#define PEER_RSSID_SONG_THRESHOLD_CREST             (-58)
#define PEER_RSSID_SONG_THRESHOLD_FMAN25            (-58)

#define NOTIFICATION_QUEUE_SIZE (100)

#define LED_GAME_STATUS_TOGGLE_DURATION_MSEC (5000)
#define PEER_SONG_COOLDOWN_DURATION_MSEC     (3*60*1000) // 3 min
#define LED_PREVIEW_DRAW_DURATION_MSEC       (2000)
#define STATUS_INDICATOR_DRAW_DURATION_MSEC  (3000)
#define NETWORK_TEST_DRAW_DURATION_MSEC      (10000)
#define NETWORK_TEST_SUCCESS_DRAW_DURATION_MSEC   (2000)
#define TOUCH_ACTIVE_TIMEOUT_DURATION_MSEC   (5000)
#define BATTERY_SEQUENCE_DRAW_DURATION_MSEC  (3000)
#define BLE_SERVICE_DISABLE_TIMEOUT_AFTER_GAME_INTERRUPTION (10 * 1000 * 1000)
#define FIRSTBOOT_FILE_NAME CONFIG_MOUNT_PATH "/fb"

#if defined(TRON_BADGE) || defined(REACTOR_BADGE)
#define BATTERY_SEQUENCE_HOLD_DURATION_MSEC  (2000)
#elif defined(CREST_BADGE) || defined(FMAN25_BADGE)
#define BATTERY_SEQUENCE_HOLD_DURATION_MSEC  (1000)
#endif

// Internal Function Declarations
static void SystemState_TouchActiveTimerCallback(TimerHandle_t xTimer);
static esp_err_t SystemState_ResetTouchActiveTimer(SystemState *this);
static esp_err_t SystemState_TouchInactiveTimerExpired(SystemState *this);
static esp_err_t SystemState_StopTouchActiveTimer(SystemState *this);

static void SystemState_BatteryIndicatorActiveTimerCallback(TimerHandle_t xTimer);
static esp_err_t SystemState_ResetBatteryIndicatorActiveTimer(SystemState *this);
static esp_err_t SystemState_BatteryIndicatorInactiveTimerExpired(SystemState *this);

// static void SystemState_StatusIndicatorActiveTimerCallback(TimerHandle_t xTimer);
// static esp_err_t SystemState_ResetStatusIndicatorActiveTimer(SystemState *this);
// static esp_err_t SystemState_StopStatusIndicatorActiveTimer(SystemState *this);
// static esp_err_t SystemState_StatusIndicatorInactiveTimerExpired(SystemState *this);

static esp_err_t SystemState_ResetPeerSongCooldownTimer(SystemState *this);
static void SystemState_NetworkTestActiveTimerCallback(TimerHandle_t xTimer);
static esp_err_t SystemState_ResetNetworkTestActiveTimer(SystemState *this);
static esp_err_t SystemState_StopNetworkTestActiveTimer(SystemState *this);
static esp_err_t SystemState_NetworkTestInactiveTimerExpired(SystemState *this);

static esp_err_t SystemState_LedSequencePreviewInactiveTimerExpired(SystemState *this);
static void SystemState_LedSequencePreviewActiveTimerCallback(TimerHandle_t xTimer);
static esp_err_t SystemState_ResetLedSequencePreviewActiveTimer(SystemState *this);

static esp_err_t SystemState_LedGameStatusToggleTimerExpired(SystemState *this);
static void SystemState_LedGameStatusToggleTimerCallback(TimerHandle_t xTimer);
static void SystemState_PeerSongCooldownTimerCallback(TimerHandle_t xTimer);

static void SystemState_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void SystemState_TouchActionNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void SystemState_BleNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void SystemState_GameEventNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void SystemState_NetworkTestNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void SystemState_SongNoteChangeNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void SystemState_PeerHeartbeatNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void SystemState_InteractiveGameNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);

static void SystemStateTask(void *pvParameters);

// Internal Constants
static const char * TAG = "SYS";

// Internal Variables
static SystemState *pSystemState = NULL;


SystemState* SystemState_GetInstance(void)
{
    if (pSystemState == NULL)
    {
        pSystemState = malloc(sizeof(SystemState));
    }
    return pSystemState;
}

esp_err_t SystemState_Init(SystemState *this)
{
    esp_err_t ret= ESP_OK;
    assert(this);
    memset(this, 0, sizeof(*this));

    // Initialize ESP Timers
    esp_timer_init(); // JER: Something seems to be initializing the esp timers somewhere else, this prints an error message

    // esp_log_level_set("*", ESP_LOG_WARN);
    // esp_log_level_set("BLE", ESP_LOG_INFO);
    // esp_log_level_set("LED", ESP_LOG_INFO);
    // esp_log_level_set("HGC", ESP_LOG_INFO);
    // esp_log_level_set("MOD", ESP_LOG_INFO);
    // esp_log_level_set("SYS", ESP_LOG_INFO);

    BadgeType badgeType = GetBadgeType();
    switch (badgeType)
    {
        case BADGE_TYPE_TRON:
            break;
        case BADGE_TYPE_REACTOR:
            this->appConfig.buzzerPresent = true;
            this->appConfig.touchActionCommandEnabled = true;
            this->appConfig.eyeGpioLedsPresent = true;
            break;
        case BADGE_TYPE_CREST:
        case BADGE_TYPE_FMAN25:
            this->appConfig.buzzerPresent = true;
            this->appConfig.touchActionCommandEnabled = true;
        default:
            break;
    }

    this->touchActiveTimer = xTimerCreate("TouchActiveTimer", pdMS_TO_TICKS(TOUCH_ACTIVE_TIMEOUT_DURATION_MSEC), pdFALSE, 0, SystemState_TouchActiveTimerCallback);
    if (this->touchActiveTimer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create touch active timer");
        ret = ESP_FAIL;
    }

    this->drawBatteryIndicatorActiveTimer = xTimerCreate("BatteryIndicatorActiveTimer", 
                                                         pdMS_TO_TICKS(BATTERY_SEQUENCE_DRAW_DURATION_MSEC+BATTERY_SEQUENCE_HOLD_DURATION_MSEC),
                                                         pdFALSE, 0, SystemState_BatteryIndicatorActiveTimerCallback);
    if (this->drawBatteryIndicatorActiveTimer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create battery indicator active timer");
        ret = ESP_FAIL;
    }

    this->drawNetworkTestTimer = xTimerCreate("NetworkTestActiveTimer", 
                                                  pdMS_TO_TICKS(NETWORK_TEST_DRAW_DURATION_MSEC),
                                                  pdFALSE, 0, SystemState_NetworkTestActiveTimerCallback);
    if (this->drawNetworkTestTimer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create network test active timer");
        ret = ESP_FAIL;
    }

    this->drawNetworkTestSuccessTimer = xTimerCreate("NetworkTestSuccessTimer", 
                                                  pdMS_TO_TICKS(NETWORK_TEST_SUCCESS_DRAW_DURATION_MSEC),
                                                  pdFALSE, 0, SystemState_NetworkTestActiveTimerCallback);
    if (this->drawNetworkTestSuccessTimer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create network test success timer");
        ret = ESP_FAIL;
    }

    this->ledSequencePreviewTimer = xTimerCreate("LedPreviewActiveTimer", 
                                                 pdMS_TO_TICKS(LED_PREVIEW_DRAW_DURATION_MSEC),
                                                 pdFALSE, 0, SystemState_LedSequencePreviewActiveTimerCallback);
    if (this->ledSequencePreviewTimer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create led sequence preview timer");
        ret = ESP_FAIL;
    }

    this->ledGameStatusToggleTimer = xTimerCreate("LedGameStatusToggleTimer", 
                                                 pdMS_TO_TICKS(LED_GAME_STATUS_TOGGLE_DURATION_MSEC),
                                                 pdFALSE, 0, SystemState_LedGameStatusToggleTimerCallback);
    if (this->ledSequencePreviewTimer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create led sequence preview timer");
        ret = ESP_FAIL;
    }

    this->peerSongCooldownTimer = xTimerCreate("PeerSongCooldownTimer", 
                                                 pdMS_TO_TICKS(PEER_SONG_COOLDOWN_DURATION_MSEC),
                                                 pdFALSE, 0, SystemState_PeerSongCooldownTimerCallback);
    if (this->peerSongCooldownTimer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create led sequence preview timer");
        ret = ESP_FAIL;
    }

    // Initialize flash fat filesystem
    ret = DiskUtilities_InitNvs();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS. error code = %s", esp_err_to_name(ret));
    }
    ret = DiskUtilities_InitFs();
    bool fsInitialized = ret == ESP_OK;
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize FATFS. error code = %s", esp_err_to_name(ret));
    }

    // Initialize cJSON before any library uses it
    cJSON_Hooks memoryHook;
    memoryHook.malloc_fn = &malloc;
    memoryHook.free_fn = &free;
    cJSON_InitHooks(&memoryHook);

    ESP_ERROR_CHECK(Console_Init(CONSOLE_TASK_PRIORITY));
    ESP_ERROR_CHECK(NotificationDispatcher_Init(&this->notificationDispatcher, NOTIFICATION_QUEUE_SIZE, NOTIFICATIONS_TASK_PRIORITY, APP_CPU_NUM));
    ESP_ERROR_CHECK(BatterySensor_Init(&this->batterySensor, &this->notificationDispatcher, BATTERY_SENSOR_ADC_CHANNEL, BATT_SENSE_TASK_PRIORITY, APP_CPU_NUM));
    ESP_ERROR_CHECK(BadgeMetrics_Init(&this->badgeStats));
    ESP_ERROR_CHECK(GpioControl_Init(&this->gpioControl));
    ESP_ERROR_CHECK(UserSettings_Init(&this->userSettings, &this->batterySensor, USER_SETTINGS_TASK_PRIORITY)); // uses bootloader random enable logic

    ESP_ERROR_CHECK(LedSequences_Init(&this->batterySensor));
    // ESP_ERROR_CHECK(BadgeMetrics_RegisterBatterySensor(&this->badgeStats, &this->batterySensor));
    ESP_ERROR_CHECK(GameState_Init(&this->gameState, &this->notificationDispatcher, &this->badgeStats, &this->userSettings, &this->batterySensor));
    ESP_ERROR_CHECK(LedControl_Init(&this->ledControl, &this->notificationDispatcher, &this->userSettings, &this->batterySensor, &this->gameState, BATTERY_SEQUENCE_HOLD_DURATION_MSEC));
    ESP_ERROR_CHECK(LedModing_Init(&this->ledModing, &this->ledControl));
    if (this->appConfig.buzzerPresent)
    {
        ESP_ERROR_CHECK(SynthMode_Init(&this->synthMode, &this->notificationDispatcher, &this->userSettings));
        ESP_ERROR_CHECK(Ocarina_Init(&this->ocarina, &this->notificationDispatcher));
    }

    ESP_ERROR_CHECK(TouchSensor_Init(&this->touchSensor, &this->notificationDispatcher, TOUCH_SENSOR_TASK_PRIORITY, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION));
    ESP_ERROR_CHECK(TouchActions_Init(&this->touchActions, &this->notificationDispatcher));
    ESP_ERROR_CHECK(BleControl_Init(&this->bleControl, &this->notificationDispatcher, &this->userSettings, &this->gameState));
    ESP_ERROR_CHECK(WifiClient_Init(&this->wifiClient, &this->notificationDispatcher, &this->userSettings));
    ESP_ERROR_CHECK(OtaUpdate_Init(&this->otaUpdate, &this->wifiClient, &this->notificationDispatcher));
    ESP_ERROR_CHECK(HTTPGameClient_Init(&this->httpGameClient, &this->wifiClient, &this->notificationDispatcher, &this->batterySensor));

    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_ACTION_CMD,                 &SystemState_TouchActionNotificationHandler,    this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION,               &SystemState_TouchSensorNotificationHandler,    this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_SERVICE_ENABLED,              &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_SERVICE_DISABLED,             &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_DROPPED,                      &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_SERVICE_CONNECTED,       &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_SERVICE_DISCONNECTED,         &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_COMPLETE,                &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_FAILED,                  &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_LEDJSON_RECVD,           &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_NEW_PAIR_RECV,                &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_SERVICE_PERCENT_CHANGED, &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_FILE_SETTINGS_RECVD,          &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_GAME_EVENT_JOINED,                &SystemState_GameEventNotificationHandler,      this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_GAME_EVENT_ENDED,                 &SystemState_GameEventNotificationHandler,      this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE,            &SystemState_NetworkTestNotificationHandler,    this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED,      &SystemState_PeerHeartbeatNotificationHandler,  this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_INTERACTIVE_GAME_ACTION,          &SystemState_InteractiveGameNotificationHandler,this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_OTA_DOWNLOAD_INITIATED,           &SystemState_BleNotificationHandler,            this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_OTA_DOWNLOAD_COMPLETE,            &SystemState_BleNotificationHandler,            this));
    
    if (this->appConfig.buzzerPresent)
    {
       ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_SONG_NOTE_ACTION,              &SystemState_SongNoteChangeNotificationHandler, this));
    }

    if (this->appConfig.eyeGpioLedsPresent)
    {
        GpioControl_Control(&this->gpioControl, GPIO_FEATURE_LEFT_EYE, true, 0);
        GpioControl_Control(&this->gpioControl, GPIO_FEATURE_RIGHT_EYE, true, 0);
    }

    assert(xTaskCreatePinnedToCore(SystemStateTask, "SystemStateTask", configMINIMAL_STACK_SIZE * 2, this, SYSTEM_STATE_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
    bool firstBoot = false;
    if (fsInitialized) {
        ESP_LOGI(TAG, "Checking for first boot file %s", FIRSTBOOT_FILE_NAME);
        uint8_t firstBootByte = 0;
        ret = ReadFileFromDisk(FIRSTBOOT_FILE_NAME, (char *)&firstBootByte, sizeof(firstBootByte), NULL, sizeof(firstBootByte));
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "First boot file found, checking to see if first boot byte is set");
            if (firstBootByte)
            {
                ESP_LOGI(TAG, "First boot byte is set, not first boot");
            }
            else
            {
                ESP_LOGI(TAG, "First boot byte is not set, first boot");
                firstBoot = true;
            }
        }
        else
        {
            ESP_LOGI(TAG, "First boot byte not found, setting up default settings");
            firstBoot = true;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize filesystem, skipping first boot byte check");
    }

    if (firstBoot)
    {
        PlaySongEventNotificationData firstBootPlaySongNotificationData;
        switch (badgeType)
        {
            case BADGE_TYPE_REACTOR:
                firstBootPlaySongNotificationData.song = SONG_BONUS;
                break;
            case BADGE_TYPE_CREST:
                firstBootPlaySongNotificationData.song = SONG_ZELDA_OPENING;
                break;
            case BADGE_TYPE_FMAN25:
                firstBootPlaySongNotificationData.song = SONG_MARGARITAVILLE;
                break;
            case BADGE_TYPE_TRON:
            default:
                firstBootPlaySongNotificationData.song = SONG_BONUS_BONUS;
        }
        NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &firstBootPlaySongNotificationData, sizeof(firstBootPlaySongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
    }
    else
    {
        if (badgeType == BADGE_TYPE_FMAN25)
        {
            uint32_t rnd = esp_random();
            if ((rnd % 5) == 0)
            {
                PlaySongEventNotificationData randomBootPlaySongNotificationData;
                randomBootPlaySongNotificationData.song = SONG_BONUS_BONUS;
                NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &randomBootPlaySongNotificationData, sizeof(randomBootPlaySongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
            }
        }
    }

    return ret;
}

static void SystemStateTask(void *pvParameters)
{
    SystemState * this = (SystemState *)pvParameters;
    assert(this);
    bool prevNetworkTestActive = this->networkTestActive;
    while (true)
    {
        if (prevNetworkTestActive != this->networkTestActive && this->networkTestActive == true)
        {
            WifiClient_TestConnect(&this->wifiClient);
        }

        prevNetworkTestActive = this->networkTestActive;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static bool SystemState_ProcessTouchModeEnabledModeCmd(SystemState *this, TouchActionsCmd touchCmd)
{
    bool cmdProcessed = false;
    switch(touchCmd)
    {
        case TOUCH_ACTIONS_CMD_ENABLE_TOUCH:
            ESP_LOGI(TAG, "Touch Enabled. Clear Required");
            this->touchActionCmdClearRequired = true;
            this->touchActive = true;
            NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_ENABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
            SystemState_ResetTouchActiveTimer(this);
            GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500); // TODO: Make these components use the notification dispatcher instead of these functions
            LedModing_SetTouchActive(&this->ledModing, true);      // TODO: Make these components use the notification dispatcher instead of these functions
            TouchSensor_SetTouchEnabled(&this->touchSensor, true); // TODO: Make these components use the notification dispatcher instead of these functions
            cmdProcessed = true;
        default:
            break;
    }
    return cmdProcessed;
}


static bool SystemState_ProcessSynthModeCmd(SystemState *this, TouchActionsCmd touchCmd)
{
    bool cmdProcessed = false;
    switch(touchCmd)
    {
        case TOUCH_ACTIONS_CMD_TOGGLE_SYNTH_MODE_ENABLE:
            if (this->appConfig.buzzerPresent)
            {
                ESP_LOGI(TAG, "Disabling Synth Mode");
                GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
                SynthMode_SetTouchSoundEnabled(&this->synthMode, false, 0);
                Ocarina_SetModeEnabled(&this->ocarina, false);
                cmdProcessed = true;
            }
            break;
        default:
            break;
    }
    return cmdProcessed;
}

static bool SystemState_ProcessMenuCmd(SystemState *this, TouchActionsCmd touchCmd)
{
    bool cmdProcessed = false;
    switch(touchCmd)
    {
        case TOUCH_ACTIONS_CMD_DISABLE_TOUCH:
            if (this->appConfig.touchActionCommandEnabled && this->touchActive)
            {
                ESP_LOGI(TAG, "Touch Disabled");
                this->touchActive = false;
                NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_DISABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                SystemState_StopTouchActiveTimer(this);
                GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500); // TODO: Make these components use the notification dispatcher instead of these functions
                LedModing_SetTouchActive(&this->ledModing, false);      // TODO: Make these components use the notification dispatcher instead of these functions
                TouchSensor_SetTouchEnabled(&this->touchSensor, false); // TODO: Make these components use the notification dispatcher instead of these functions
                if (this->appConfig.buzzerPresent)
                {
                    SynthMode_SetTouchSoundEnabled(&this->synthMode, false, 0);          // TODO: Make these components use the notification dispatcher instead of these functions
                    Ocarina_SetModeEnabled(&this->ocarina, false);          // TODO: Make these components use the notification dispatcher instead of these functions
                }
                cmdProcessed = true;
            }
            break;
        case TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE:
            ESP_LOGI(TAG, "Next LED Sequence");
            GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500); // TODO: Make these components use the notification dispatcher instead of these functions
            LedModing_CycleSelectedLedSequence(&this->ledModing, true);
            LedModing_SetLedSequencePreviewActive(&this->ledModing, true);
            SystemState_ResetLedSequencePreviewActiveTimer(this);
            BadgeMetrics_IncrementNumLedCycles(&this->badgeStats);
            cmdProcessed = true;
            break;
        case TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE:
            ESP_LOGI(TAG, "Previous LED Sequence");
            GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
            LedModing_CycleSelectedLedSequence(&this->ledModing, false);
            LedModing_SetLedSequencePreviewActive(&this->ledModing, true);
            SystemState_ResetLedSequencePreviewActiveTimer(this);
            BadgeMetrics_IncrementNumLedCycles(&this->badgeStats);
            cmdProcessed = true;
            break;
        case TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER:
            ESP_LOGI(TAG, "Displaying Voltage Meter");
            GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
            LedModing_SetBatteryIndicatorActive(&this->ledModing, true);
            SystemState_ResetBatteryIndicatorActiveTimer(this);
            BadgeMetrics_IncrementNumBatteryChecks(&this->badgeStats);
            cmdProcessed = true;
            this->batteryIndicatorActive = true;
            break;
        case TOUCH_ACTIONS_CMD_ENABLE_BLE_PAIRING:
            ESP_LOGI(TAG, "Enabling BLE Service");
            GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
            if (BleControl_EnableBleService(&this->bleControl, true, 0) == ESP_OK)
            {
                if (LedModing_SetBleServiceEnableActive(&this->ledModing, true) != ESP_OK)
                {
                    ESP_LOGW(TAG, "Failed to set BLE Service Enable Active");
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to enable BLE Service");
            }
            BadgeMetrics_IncrementNumBleEnables(&this->badgeStats);
            cmdProcessed = true;
            break;
        case TOUCH_ACTIONS_CMD_DISABLE_BLE_PAIRING:
            ESP_LOGI(TAG, "Disabling BLE Service");
            GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
            BleControl_DisableBleService(&this->bleControl, false);
            if (LedModing_SetBleFileTransferIPActive(&this->ledModing, false) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to set BLE Xfer Active");
            }
            if (LedModing_SetInteractiveGameActive(&this->ledModing, false) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to set BLE Connected Active");
            }
            if (LedModing_SetBleConnectedActive(&this->ledModing, false) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to set BLE Connected Active false");
            }
            if (LedModing_SetBleServiceEnableActive(&this->ledModing, false) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to set BLE Service Enable Active false");
            }
            BadgeMetrics_IncrementNumBleDisables(&this->badgeStats);
            cmdProcessed = true;
            break;
        case TOUCH_ACTIONS_CMD_NETWORK_TEST:
            ESP_LOGI(TAG, "Enabling Network Test");
            GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
            LedModing_SetNetworkTestActive(&this->ledModing, true);
            SystemState_ResetNetworkTestActiveTimer(this);
            BadgeMetrics_IncrementNumNetworkTests(&this->badgeStats);
            cmdProcessed = true;
            break;
        case TOUCH_ACTIONS_CMD_TOGGLE_SYNTH_MODE_ENABLE:
            if (this->appConfig.buzzerPresent)
            {
                ESP_LOGI(TAG, "Enabling Synth Mode");
                GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
                SynthMode_SetTouchSoundEnabled(&this->synthMode, true, 0);
                Ocarina_SetModeEnabled(&this->ocarina, true);
                cmdProcessed = true;
            }
            break;
        case TOUCH_ACTIONS_CMD_CLEAR:
        case TOUCH_ACTIONS_CMD_ENABLE_TOUCH:
        case TOUCH_ACTIONS_CMD_UNKNOWN:
        default:
            break;
    }
    return cmdProcessed;
}

static void SystemState_ProcessTouchActionCmd(SystemState *this, TouchActionsCmd touchCmd)
{
    ESP_LOGD(TAG, "Touch Action: %d", touchCmd);

    assert(this);
    bool cmdProcessed = false;
    
    if ((this->touchActionCmdClearRequired) && (touchCmd == TOUCH_ACTIONS_CMD_CLEAR))
    {
        this->touchActionCmdClearRequired = false;
        ESP_LOGI(TAG, "Touch Cleared");
    }

    if (this->interactiveGameTouchSensorsToLightBits.s.active)
    {
        ESP_LOGI(TAG, "Interactive Game in progress, ignoring touch command %d", touchCmd);
        return;
    }

    if (this->touchActionCmdClearRequired)
    {
        ESP_LOGI(TAG, "Touch Action Cmd Clear is required, ignoring touch command %d", touchCmd);
        return;
    }

    // Process touch command
    if (this->appConfig.touchActionCommandEnabled && !this->touchActive)
    {
        cmdProcessed = SystemState_ProcessTouchModeEnabledModeCmd(this, touchCmd);
    }
    else if (this->appConfig.buzzerPresent && this->synthMode.touchSoundEnabled)
    {
        cmdProcessed = SystemState_ProcessSynthModeCmd(this, touchCmd);
    }
    else
    {
        cmdProcessed = SystemState_ProcessMenuCmd(this, touchCmd);
    }

    if (cmdProcessed)
    {
        BadgeMetrics_IncrementNumTouchCmds(&this->badgeStats);
    }
}

static void SystemState_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");

    SystemState *this = (SystemState *)pObj;
    assert(this);
    TouchSensorEventNotificationData touchSensorEventNotificationData = *(TouchSensorEventNotificationData *)notificationData;
    bool active = touchSensorEventNotificationData.touchSensorEvent != TOUCH_SENSOR_EVENT_RELEASED;
    BleControl_SetTouchSensorActive(&this->bleControl, touchSensorEventNotificationData.touchSensorIdx, active);
    LedControl_SetTouchSensorUpdate(&this->ledControl, touchSensorEventNotificationData.touchSensorEvent, touchSensorEventNotificationData.touchSensorIdx);
    BadgeMetrics_IncrementNumTouches(&this->badgeStats);
    if (this->interactiveGameTouchSensorsToLightBits.s.active)
    {
        GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 250);
    }
    if (this->touchActive)
    {
        SystemState_ResetTouchActiveTimer(this);
    }
}

static void SystemState_TouchActionNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Action Notification");

    SystemState *this = (SystemState *)pObj;
    TouchActionsCmd touchCmd = *((TouchActionsCmd*)notificationData);
    assert(this);
    SystemState_ProcessTouchActionCmd(this, touchCmd);
}

static void SystemState_BleNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    SystemState *this = (SystemState *)pObj;
    assert(this);
    ESP_LOGD(TAG, "Handling BLE Event");
    switch (notificationEvent)
    {
    case NOTIFICATION_EVENTS_BLE_DROPPED:
        ESP_LOGI(TAG, "BLE Dropped");
        this->bleReconnecting = true;
        if (LedModing_SetBleReconnectingActive(&this->ledModing, true) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Connected Active false");
        }
        break;
    case NOTIFICATION_EVENTS_OTA_DOWNLOAD_INITIATED:
        ESP_LOGI(TAG, "OTA Download Initiated");
        if (LedModing_SetOtaDownloadInitiatedActive(&this->ledModing, true) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set OTA Download Initiated Enable Active true");
        }
        break;
    case NOTIFICATION_EVENTS_OTA_DOWNLOAD_COMPLETE:
        ESP_LOGI(TAG, "OTA Download Initiated");
        if (LedModing_SetOtaDownloadInitiatedActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set OTA Download Initiated Enable Active false");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_SERVICE_ENABLED:
        ESP_LOGI(TAG, "BLE Service Enabled");
        break;
    case NOTIFICATION_EVENTS_BLE_SERVICE_DISABLED:
        ESP_LOGI(TAG, "BLE Service Disabled");
        if (LedModing_SetBleFileTransferIPActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Active");
        }
        if (LedModing_SetInteractiveGameActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Connected Active");
        }
        if (LedModing_SetBleServiceEnableActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Service Enable Active");
        }
        if (LedModing_SetBleConnectedActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Connected Active false");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_SERVICE_CONNECTED:
        ESP_LOGI(TAG, "BLE Service Connected");
        if (LedModing_SetBleConnectedActive(&this->ledModing, true) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Connected Active");
        }
        {
            PlaySongEventNotificationData playSongNotificationData;
            playSongNotificationData.song = SONG_SUCCESS_SOUND;
            NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &playSongNotificationData.song, sizeof(playSongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
        }
        if (this->bleReconnecting)
        {
            this->bleReconnecting = false;
            if (LedModing_SetBleReconnectingActive(&this->ledModing, false) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to set BLE Connected Active false");
            }
        }
        break;
    case NOTIFICATION_EVENTS_BLE_SERVICE_DISCONNECTED:
        ESP_LOGI(TAG, "BLE Service Disconnected");
        if (this->bleReconnecting)
        {
            this->bleReconnecting = false;
            if (LedModing_SetBleReconnectingActive(&this->ledModing, false) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to set BLE Connected Active false");
            }
        }
        break;
    case NOTIFICATION_EVENTS_BLE_FILE_SERVICE_PERCENT_CHANGED:
        if (LedModing_SetBleFileTransferIPActive(&this->ledModing, true) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Active");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_FILE_COMPLETE:
        ESP_LOGI(TAG, "BLE Xfer Complete");
        if (LedModing_SetBleFileTransferIPActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Active");
        }
        // if (SystemState_ResetStatusIndicatorActiveTimer(this) != ESP_OK)
        // {
        //     ESP_LOGW(TAG, "Failed to reset status indicator active timer");
        // }
        break;
    case NOTIFICATION_EVENTS_BLE_FILE_FAILED:
        ESP_LOGI(TAG, "BLE Xfer Failed");
        // if (SystemState_ResetStatusIndicatorActiveTimer(this) != ESP_OK)
        // {
        //     ESP_LOGW(TAG, "Failed to reset status indicator active timer");
        // }
        break;
    case NOTIFICATION_EVENTS_BLE_FILE_SETTINGS_RECVD:
        ESP_LOGI(TAG, "BLE Xfer New Settings Recv");
        if (notificationData != NULL)
        {
            UserSettingsFile *pUserSettingsFile = (UserSettingsFile*)notificationData;
            if (UserSettings_UpdateFromJson(&this->userSettings, (uint8_t*)pUserSettingsFile) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to update user settings");
            }
            WifiClient_Disconnect(&this->wifiClient);
            GameState_SendHeartBeat(&this->gameState, 0);
        }
        else
        {
            ESP_LOGE(TAG, "BLE Xfer New Settings Recv. Notification Data is NULL");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_FILE_LEDJSON_RECVD:
        if (notificationData != NULL)
        {
            int customLedIndex = *((int*)notificationData);
            ESP_LOGI(TAG, "BLE Xfer New Custom Recv. Custom Index: %d", customLedIndex);
            if (LedModing_SetLedCustomSequence(&this->ledModing, customLedIndex) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to set custom LED sequence");
            }
            if (LedModing_SetLedSequencePreviewActive(&this->ledModing, true) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to set LED sequence preview active");
            }
            if (SystemState_ResetLedSequencePreviewActiveTimer(this) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to reset LED sequence preview active timer");
            }
            BadgeMetrics_IncrementNumBleDisables(&this->badgeStats);
        }
        else
        {
            ESP_LOGE(TAG, "BLE Xfer New Custom Recv. Notification Data is NULL");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_NEW_PAIR_RECV:
        LedModing_SetLedSequencePreviewActive(&this->ledModing, false);
        // just picked this one with false to flow to normal (idk how to do it properly)
        // tbh this might be a problem with LedModding not refreshing automatically once state is changed
        // also broken for Settings, TouchMode->Ble Timeout, and TouchMode->Disable Ble
        break;
    default:
        break;
    }
}

static void SystemState_GameEventNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGI(TAG, "Handling Game Event Notification");
    SystemState *this = (SystemState *)pObj;
    assert(this);
    // assert(notificationData);
    // GameStatus *pStatus = (GameStatus *)notificationData;

    switch(notificationEvent)
    {
        case NOTIFICATION_EVENTS_GAME_EVENT_ENDED:
        {
            ESP_LOGI(TAG, "Game event ended notification");
            char eventIdB64[EVENT_ID_B64_SIZE];
            memset(&eventIdB64, 0, EVENT_ID_B64_SIZE);
            uint8_t eventId[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            size_t outlen;

            // Initialize current event id to blank
            mbedtls_base64_encode((uint8_t *)eventIdB64, EVENT_ID_B64_SIZE, &outlen, eventId, EVENT_ID_SIZE);
            LedModing_SetGameEventActive(&this->ledModing, false);
            BleControl_UpdateEventId(&this->bleControl, eventIdB64);
            break;
        }
        case NOTIFICATION_EVENTS_GAME_EVENT_JOINED:
        {
            ESP_LOGI(TAG, "Game event joined notification");
            this->gameState.nextHeartBeatTime = TimeUtils_GetFutureTimeTicks(EVENT_HEARTBEAT_INTERVAL_MS);
            BleControl_UpdateEventId(&this->bleControl, (char*)notificationData);
            LedModing_SetGameEventActive(&this->ledModing, true);
            break;
        }
        default:
            ESP_LOGE(TAG, "Invalid Notification");
    };
}

static esp_err_t SystemState_NetworkTestInactiveTimerExpired(SystemState *this)
{
    assert(this);
    ESP_LOGI(TAG, "Network Test Inactive Timer Expired");
    this->networkTestActive = false;
    return LedModing_SetNetworkTestActive(&this->ledModing, false);
}

static esp_err_t SystemState_ResetPeerSongCooldownTimer(SystemState *this)
{
    assert(this);
    assert(this->peerSongCooldownTimer);
    esp_err_t ret = ESP_FAIL;
    if (xTimerReset(this->peerSongCooldownTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t SystemState_ResetNetworkTestActiveTimer(SystemState *this)
{
    assert(this);
    assert(this->drawNetworkTestTimer);
    esp_err_t ret = ESP_FAIL;
    this->networkTestActive = true;
    if (xTimerReset(this->drawNetworkTestTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t SystemState_ResetNetworkTestSuccessTimer(SystemState *this)
{
    assert(this);
    assert(this->drawNetworkTestSuccessTimer);
    esp_err_t ret = ESP_FAIL;
    if (xTimerReset(this->drawNetworkTestSuccessTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t SystemState_StopNetworkTestActiveTimer(SystemState *this)
{
    assert(this);
    assert(this->drawNetworkTestTimer);
    esp_err_t ret = ESP_FAIL;
    if (xTimerStop(this->drawNetworkTestTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    return ret;
}

static void SystemState_NetworkTestActiveTimerCallback(TimerHandle_t xTimer)
{
    SystemState* systemState = SystemState_GetInstance();
    SystemState_NetworkTestInactiveTimerExpired(systemState);
    SystemState_StopTouchActiveTimer(systemState);
}

static esp_err_t SystemState_TouchInactiveTimerExpired(SystemState *this)
{
    assert(this);
    this->touchActive = false;
    ESP_LOGI(TAG, "Touch Disabled");
    LedModing_SetTouchActive(&this->ledModing, false);
    TouchSensor_SetTouchEnabled(&this->touchSensor, false);
    if (this->appConfig.buzzerPresent)
    {
        SynthMode_SetTouchSoundEnabled(&this->synthMode, false, 0);
        Ocarina_SetModeEnabled(&this->ocarina, false);
    }
    return NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_DISABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
}

static esp_err_t SystemState_ResetTouchActiveTimer(SystemState *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    assert(this->touchActiveTimer);
    
    if (xTimerReset(this->touchActiveTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t SystemState_StopTouchActiveTimer(SystemState *this)
{
    // esp_err_t ret = ESP_FAIL;
    assert(this);
    assert(this->touchActiveTimer);
    
    if (xTimerStop(this->touchActiveTimer, 0) == pdPASS)
    {
        // ret = ESP_OK;
    }
    return SystemState_TouchInactiveTimerExpired(this);
}

static void SystemState_TouchActiveTimerCallback(TimerHandle_t xTimer)
{
    SystemState* systemState = SystemState_GetInstance();
    SystemState_TouchInactiveTimerExpired(systemState);
}

static esp_err_t SystemState_BatteryIndicatorInactiveTimerExpired(SystemState *this)
{
    assert(this);
    ESP_LOGI(TAG, "Battery Indicator Inactive Timer Expired");
    this->batteryIndicatorActive = false;
    return LedModing_SetBatteryIndicatorActive(&this->ledModing, false);
}

static esp_err_t SystemState_ResetBatteryIndicatorActiveTimer(SystemState *this)
{
    assert(this);
    assert(this->drawBatteryIndicatorActiveTimer);
    esp_err_t ret = ESP_FAIL;

    if (xTimerReset(this->drawBatteryIndicatorActiveTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to reset battery indicator active timer");
    }
    return ret;
}

static void SystemState_BatteryIndicatorActiveTimerCallback(TimerHandle_t xTimer)
{
    SystemState* systemState = SystemState_GetInstance();
    SystemState_BatteryIndicatorInactiveTimerExpired(systemState);
    SystemState_StopTouchActiveTimer(systemState);
}

static esp_err_t SystemState_LedSequencePreviewInactiveTimerExpired(SystemState *this)
{
    assert(this);
    ESP_LOGI(TAG, "Led Preview Timer Expired");
    return LedModing_SetLedSequencePreviewActive(&this->ledModing, false);
}

static esp_err_t SystemState_ResetLedSequencePreviewActiveTimer(SystemState *this)
{
    assert(this);
    assert(this->ledSequencePreviewTimer);
    esp_err_t ret = ESP_FAIL;

    if (xTimerReset(this->ledSequencePreviewTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to Reset Led Sequence Preview Timer");
    }
    return ret;
}

static void SystemState_LedSequencePreviewActiveTimerCallback(TimerHandle_t xTimer)
{
    SystemState* systemState = SystemState_GetInstance();
    SystemState_LedSequencePreviewInactiveTimerExpired(systemState);
}

static esp_err_t SystemState_LedGameStatusToggleTimerExpired(SystemState *this)
{
    assert(this);
    ESP_LOGI(TAG, "Led Game Status Timer Expired");
    // esp_err_t ret = ESP_FAIL;
    this->ledGameStatusActive = !this->ledGameStatusActive;
    if (xTimerReset(this->ledGameStatusToggleTimer, 0) == pdPASS)
    {
        // ret = ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to Reset Led Game Status Toggle Timer");
    }
    return LedModing_SetGameStatusActive(&this->ledModing, this->ledGameStatusActive);
}

static void SystemState_LedGameStatusToggleTimerCallback(TimerHandle_t xTimer)
{
    SystemState* systemState = SystemState_GetInstance();
    SystemState_LedGameStatusToggleTimerExpired(systemState);
}

static void SystemState_PeerSongCooldownTimerCallback(TimerHandle_t xTimer)
{
    SystemState* systemState = SystemState_GetInstance();
    systemState->peerSongWaitingCooldown = false;
}

static void SystemState_NetworkTestNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGI(TAG, "Handling Network Test Notification: %d", *((bool *) notificationData));

    SystemState *this = (SystemState *)pObj;
    assert(this);
    this->ledControl.networkTestRuntimeInfo.success = *((bool *) notificationData);
    SystemState_StopNetworkTestActiveTimer(this);
    SystemState_ResetNetworkTestSuccessTimer(this);
}

static void SystemState_SongNoteChangeNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Song Note Change Notification: %d", *((bool *) notificationData));

    SystemState *this = (SystemState *)pObj;
    assert(this);
    SongNoteChangeEventNotificationData data = *((SongNoteChangeEventNotificationData *) notificationData);
    switch(data.action)
    {
        case SONG_NOTE_CHANGE_TYPE_SONG_START:
            ESP_LOGI(TAG, "Song Start Notification Received");
            LedModing_SetSongActiveStatusActive(&this->ledModing, true);
            break;
        case SONG_NOTE_CHANGE_TYPE_SONG_STOP:
            ESP_LOGI(TAG, "Song Stop Notification Received");
            LedModing_SetSongActiveStatusActive(&this->ledModing, false);
            if (this->peerSongPlaying)
            {
                this->peerSongPlaying = false;
                this->peerSongWaitingCooldown = true;
                if (SystemState_ResetPeerSongCooldownTimer(this) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to reset peer song cooldown timer");
                }
            }
            if (data.song == SONG_ZELDA_OPENING)
            {
                ESP_LOGI(TAG, "First boot song complete, setting first boot byte");
                uint8_t firstBootByte = 0xFF;
                esp_err_t ret = WriteFileToDisk(&this->batterySensor, FIRSTBOOT_FILE_NAME, (char *)&firstBootByte, sizeof(firstBootByte));
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to write first boot byte to disk. error code = %s", esp_err_to_name(ret));
                }
            }
            break;
        default:
            break;
    }
}

static void SystemState_InteractiveGameNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    SystemState *this = (SystemState *)pObj;
    assert(this);
    InteractiveGameData touchSensorsToLightBits = *((InteractiveGameData *) notificationData);
    if (this->interactiveGameTouchSensorsToLightBits.s.active == false && touchSensorsToLightBits.s.active)
    {
        LedModing_SetInteractiveGameActive(&this->ledModing, true);
        SynthMode_SetTouchSoundEnabled(&this->synthMode, true, 2);
    }
    else if (this->interactiveGameTouchSensorsToLightBits.s.active && touchSensorsToLightBits.s.active == false)
    {
        LedModing_SetInteractiveGameActive(&this->ledModing, false);
        SynthMode_SetTouchSoundEnabled(&this->synthMode, false, 0);
    }
    this->interactiveGameTouchSensorsToLightBits.u = touchSensorsToLightBits.u;
}

static void SystemState_PeerHeartbeatNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    SystemState *this = (SystemState *)pObj;
    assert(this);
    switch (notificationEvent)
    {
        case NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED:
            if (notificationData != NULL)
            {
                PeerReport peerReport = *((PeerReport *)notificationData);
                ESP_LOGD(TAG, "NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED event with badge id [B64] %s   peakrssi %d    badgeType %d", peerReport.badgeIdB64, peerReport.peakRssi, peerReport.badgeType);
                if (!this->appConfig.buzzerPresent)
                {
                    break;
                }

                bool *pSeen = hashmap_get(&this->httpGameClient.siblingMap, peerReport.badgeIdB64);
                if (pSeen != NULL)
                {
                    if (*pSeen)
                    {
                        ESP_LOGD(TAG, "Subling already seen, skipping song play");
                        break;
                    }

                    *pSeen = true;
                }

                int rssiThreshold = -60;
                PlaySongEventNotificationData successPlaySongNotificationData;
                switch(peerReport.badgeType)
                {
                    case BADGE_TYPE_TRON:
                        rssiThreshold = PEER_RSSID_SONG_THRESHOLD_TRON;
                        successPlaySongNotificationData.song = SONG_BONUS_BONUS;
                        break;
                    case BADGE_TYPE_REACTOR:
                        rssiThreshold = PEER_RSSID_SONG_THRESHOLD_REACTOR;
                        successPlaySongNotificationData.song = SONG_BONUS;
                        break;
                    case BADGE_TYPE_CREST:
                        rssiThreshold = PEER_RSSID_SONG_THRESHOLD_CREST;
                        successPlaySongNotificationData.song = SONG_ZELDA_OPENING;
                        break;
                    case BADGE_TYPE_FMAN25:
                        rssiThreshold = PEER_RSSID_SONG_THRESHOLD_FMAN25;
                        successPlaySongNotificationData.song = SONG_RIGHT_ROUND;
                        break;
                    default:
                        successPlaySongNotificationData.song = SONG_BONUS_BONUS;
                        break;
                }

                if (this->peerSongPlaying == false && peerReport.peakRssi > rssiThreshold && this->peerSongWaitingCooldown == false)
                {
                    if (peerReport.badgeType != BADGE_TYPE_UNKNOWN)
                    {
                        ESP_LOGI(TAG, "Playing Peer Song for badge type %d", peerReport.badgeType);
                        this->peerSongPlaying = true;
                        NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &successPlaySongNotificationData, sizeof(successPlaySongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Peer Badge type unknown, skipping song play");
                    }
                }
            }

            break;
    }
}
