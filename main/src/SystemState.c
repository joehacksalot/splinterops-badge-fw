
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "mbedtls/base64.h"

#include "BadgeStats.h"
#include "BleControl.h"
#include "Console.h"
#include "DiscUtils.h"
#include "LedModing.h"
#include "LedSequences.h"
#include "NotificationDispatcher.h"
#include "OtaUpdate.h"
#include "Song.h"
#include "SystemState.h"
#include "TaskPriorities.h"
#include "TimeUtils.h"
#include "TouchSensor.h"
#include "TouchActions.h"
#include "UserSettings.h"
#include "WifiClient.h"

#define LED_GAME_STATUS_TOGGLE_DURATION_MSEC (5000)
#define LED_PREVIEW_DRAW_DURATION_MSEC       (2000)
#define STATUS_INDICATOR_DRAW_DURATION_MSEC  (3000)
#define NETWORK_TEST_DRAW_DURATION_MSEC      (10000)
#define NETWORK_TEST_SUCCESS_DRAW_DURATION_MSEC   (2000)
#define TOUCH_ACTIVE_TIMEOUT_DURATION_MSEC   (5000)
#define BATTERY_SEQUENCE_DRAW_DURATION_MSEC  (3000)

#if defined(TRON_BADGE) || defined(REACTOR_BADGE)
#define BATTERY_SEQUENCE_HOLD_DURATION_MSEC  (2000)
#elif defined(CREST_BADGE)
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

static void SystemState_StatusIndicatorActiveTimerCallback(TimerHandle_t xTimer);
static esp_err_t SystemState_ResetStatusIndicatorActiveTimer(SystemState *this);
static esp_err_t SystemState_StopStatusIndicatorActiveTimer(SystemState *this);
static esp_err_t SystemState_StatusIndicatorInactiveTimerExpired(SystemState *this);

static void SystemState_NetworkTestActiveTimerCallback(TimerHandle_t xTimer);
static esp_err_t SystemState_ResetNetworkTestActiveTimer(SystemState *this);
static esp_err_t SystemState_StopNetworkTestActiveTimer(SystemState *this);
static esp_err_t SystemState_NetworkTestInactiveTimerExpired(SystemState *this);

static esp_err_t SystemState_LedSequencePreviewInactiveTimerExpired(SystemState *this);
static void SystemState_LedSequencePreviewActiveTimerCallback(TimerHandle_t xTimer);
static esp_err_t SystemState_ResetLedSequencePreviewActiveTimer(SystemState *this);

static esp_err_t SystemState_LedGameStatusToggleTimerExpired(SystemState *this);
static void SystemState_LedGameStatusToggleTimerCallback(TimerHandle_t xTimer);

static void SystemState_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);
static void SystemState_TouchActionNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);
static void SystemState_BleNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);
static void SystemState_GameEventNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);
static void SystemState_NetworkTestNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);

static void SystemStateTask(void *pvParameters);

// Internal Constants
static const char * TAG = "SYS";

// Internal Variables
static SystemState *pSystemState = NULL;


SystemState* SystemState_GetInstance(void)
{
    if (pSystemState == NULL)
    {
        pSystemState = (SystemState*)malloc(sizeof(SystemState));
    }
    return pSystemState;
}

esp_err_t SystemState_Init(SystemState *this)
{
    esp_err_t ret= ESP_OK;
    assert(this);
    memset(this, 0, sizeof(*this));

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

    this->drawStatusIndicatorTimer = xTimerCreate("StatusIndicatorActiveTimer", 
                                                  pdMS_TO_TICKS(STATUS_INDICATOR_DRAW_DURATION_MSEC),
                                                  pdFALSE, 0, SystemState_StatusIndicatorActiveTimerCallback);
    if (this->drawStatusIndicatorTimer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create status indicator active timer");
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

    // Initialize flash fat filesystem
    ret = DiscUtils_InitNvs();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS. error code = %s", esp_err_to_name(ret));
    }
    ret = DiscUtils_InitFs();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize FATFS. error code = %s", esp_err_to_name(ret));
    }

    this->pBleControl = BleControl_GetInstance();
    memset(this->pBleControl, 0, sizeof(*this->pBleControl));

    ESP_ERROR_CHECK(Console_Init());
    ESP_ERROR_CHECK(BadgeStats_Init(&this->badgeStats));
    ESP_ERROR_CHECK(GpioControl_Init(&this->gpioControl));
    ESP_ERROR_CHECK(NotificationDispatcher_Init(&this->notificationDispatcher));
    ESP_ERROR_CHECK(UserSettings_Init(&this->userSettings)); // uses bootloader random enable logic

    // ESP_ERROR_CHECK(AudioPlayer_Init(&this->audioPlayer, &this->notificationDispatcher));
    ESP_ERROR_CHECK(BatterySensor_Init(&this->batterySensor, &this->notificationDispatcher));
    LedSequences_Init(&this->batterySensor);
    UserSettings_RegisterBatterySensor(&this->userSettings, &this->batterySensor);
    BadgeStats_RegisterBatterySensor(&this->badgeStats, &this->batterySensor);
    ESP_ERROR_CHECK(GameState_Init(&this->gameState, &this->notificationDispatcher, &this->badgeStats, &this->userSettings));
    ESP_ERROR_CHECK(LedControl_Init(&this->ledControl, &this->notificationDispatcher, &this->userSettings, &this->batterySensor, &this->gameState, BATTERY_SEQUENCE_HOLD_DURATION_MSEC));
    ESP_ERROR_CHECK(LedModing_Init(&this->ledModing, &this->ledControl));
#if defined(REACTOR_BADGE) || defined(CREST_BADGE)
    ESP_ERROR_CHECK(SynthMode_Init(&this->synthMode, &this->notificationDispatcher, &this->userSettings));
#endif
    ESP_ERROR_CHECK(TouchSensor_Init(&this->touchSensor, &this->notificationDispatcher));
    ESP_ERROR_CHECK(TouchActions_Init(&this->touchActions, &this->notificationDispatcher));
    ESP_ERROR_CHECK(BleControl_Init(this->pBleControl, &this->notificationDispatcher, &this->userSettings, &this->gameState));
    ESP_ERROR_CHECK(WifiClient_Init(&this->wifiClient, &this->notificationDispatcher));
    ESP_ERROR_CHECK(OtaUpdate_Init(&this->otaUpdate, &this->wifiClient, &this->notificationDispatcher));
    ESP_ERROR_CHECK(HTTPGameClient_Init(&this->httpGameClient, &this->wifiClient, &this->notificationDispatcher, &this->batterySensor));

    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_ACTION_CMD,            &SystemState_TouchActionNotificationHandler, this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION,          &SystemState_TouchSensorNotificationHandler, this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_ENABLED,                 &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_DISABLED,                &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_REQ_RECV,           &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_ENABLED,            &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_DISABLED,           &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_CONNECTED,          &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_DISCONNECTED,       &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_COMPLETE,           &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_FAILED,             &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_NEW_CUSTOM_RECV,    &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_PERCENT_CHANGED,    &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_NEW_SETTINGS_RECV,  &SystemState_BleNotificationHandler,         this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_GAME_EVENT_JOINED,           &SystemState_GameEventNotificationHandler,   this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_GAME_EVENT_ENDED,            &SystemState_GameEventNotificationHandler,   this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(&this->notificationDispatcher, NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE,       &SystemState_NetworkTestNotificationHandler, this));

    GpioControl_Control(&this->gpioControl, GPIO_FEATURE_LEFT_EYE, true, 0);
    GpioControl_Control(&this->gpioControl, GPIO_FEATURE_RIGHT_EYE, true, 0);

    xTaskCreate(SystemStateTask, "SystemStateTask", configMINIMAL_STACK_SIZE * 5, this, SYSTEM_STATE_TASK_PRIORITY, NULL);
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

    if (this->touchActionCmdClearRequired == false)
    {
#if defined (REACTOR_BADGE) || defined (CREST_BADGE)
        if (!this->touchActive)
        {
            if (touchCmd == TOUCH_ACTIONS_CMD_ENABLE_TOUCH)
            {
                ESP_LOGI(TAG, "Touch Enabled. Clear Required");
                this->touchActionCmdClearRequired = true;
                this->touchActive = true;
                NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_ENABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                PlaySongEventNotificationData playSongNotificationData;
                playSongNotificationData.song = SONG_EPONAS_SONG2;
                NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &playSongNotificationData.song, sizeof(playSongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                
                SystemState_ResetTouchActiveTimer(this);
                GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500); // TODO: Make these components use the notification dispatcher instead of these functions
                LedModing_SetTouchActive(&this->ledModing, true);      // TODO: Make these components use the notification dispatcher instead of these functions
                TouchSensor_SetTouchEnabled(&this->touchSensor, true); // TODO: Make these components use the notification dispatcher instead of these functions
                SynthMode_SetEnabled(&this->synthMode, true);          // TODO: Make these components use the notification dispatcher instead of these functions
                cmdProcessed = true;
            }
        }
        else if (touchCmd == TOUCH_ACTIONS_CMD_DISABLE_TOUCH)
        {
            ESP_LOGI(TAG, "Touch Disabled");
            this->touchActive = false;
            NotificationDispatcher_NotifyEvent(&this->notificationDispatcher, NOTIFICATION_EVENTS_TOUCH_DISABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
            SystemState_StopTouchActiveTimer(this);
            GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500); // TODO: Make these components use the notification dispatcher instead of these functions
            LedModing_SetTouchActive(&this->ledModing, false);      // TODO: Make these components use the notification dispatcher instead of these functions
            TouchSensor_SetTouchEnabled(&this->touchSensor, false); // TODO: Make these components use the notification dispatcher instead of these functions
            SynthMode_SetEnabled(&this->synthMode, false);          // TODO: Make these components use the notification dispatcher instead of these functions
            cmdProcessed = true;
        }
        else
        {
#endif 
            switch(touchCmd)
            {
                case TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE:
                    ESP_LOGI(TAG, "Next LED Sequence");
                    GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500); // TODO: Make these components use the notification dispatcher instead of these functions
                    // GpioControl_Control(&this->gpioControl, GPIO_FEATURE_PIEZO, true, 500);
                    LedModing_CycleSelectedLedSequence(&this->ledModing, true);    // TODO: Make these components use the notification dispatcher instead of these functions
                    LedModing_SetLedSequencePreviewActive(&this->ledModing, true); // TODO: Make these components use the notification dispatcher instead of these functions
                    SystemState_ResetLedSequencePreviewActiveTimer(this);
                    BadgeStats_IncrementNumLedCycles(&this->badgeStats);           // TODO: Make these components use the notification dispatcher instead of these functions
                    cmdProcessed = true;
                    break;
                case TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE:
                    ESP_LOGI(TAG, "Previous LED Sequence");
                    GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
                    LedModing_CycleSelectedLedSequence(&this->ledModing, false);
                    LedModing_SetLedSequencePreviewActive(&this->ledModing, true);
                    SystemState_ResetLedSequencePreviewActiveTimer(this);
                    BadgeStats_IncrementNumLedCycles(&this->badgeStats);
                    cmdProcessed = true;
                    break;
                case TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER:
                    ESP_LOGI(TAG, "Displaying Voltage Meter");
                    GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
                    LedModing_SetBatteryIndicatorActive(&this->ledModing, true);
                    SystemState_ResetBatteryIndicatorActiveTimer(this);
                    BadgeStats_IncrementNumBatteryChecks(&this->badgeStats);
                    cmdProcessed = true;
                    this->batteryIndicatorActive = true;
                    break;
                case TOUCH_ACTIONS_CMD_ENABLE_BLE_XFER:
                    ESP_LOGI(TAG, "Enabling BLE Xfer");
                    GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
                    // GpioControl_Control(&this->gpioControl, GPIO_FEATURE_PIEZO, true, 500);
                    BleControl_EnableBleXfer(this->pBleControl, true);
                    BadgeStats_IncrementNumBleEnables(&this->badgeStats);
                    cmdProcessed = true;
                    break;
                case TOUCH_ACTIONS_CMD_DISABLE_BLE_XFER:
                    ESP_LOGI(TAG, "Disabling BLE Xfer");
                    GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
                    // GpioControl_Control(&this->gpioControl, GPIO_FEATURE_PIEZO, true, 500);
                    BleControl_DisableBleXfer(this->pBleControl);
                    BadgeStats_IncrementNumBleDisables(&this->badgeStats);
                    cmdProcessed = true;
                    break;
                case TOUCH_ACTIONS_CMD_NETWORK_TEST:
                    ESP_LOGI(TAG, "Enabling Network Test");
                    GpioControl_Control(&this->gpioControl, GPIO_FEATURE_VIBRATION, true, 500);
                    // GpioControl_Control(&this->gpioControl, GPIO_FEATURE_PIEZO, true, 500);
                    LedModing_SetNetworkTestActive(&this->ledModing, true);
                    SystemState_ResetNetworkTestActiveTimer(this);
                    BadgeStats_IncrementNumNetworkTests(&this->badgeStats);
                    cmdProcessed = true;
                    break;
                case TOUCH_ACTIONS_CMD_CLEAR:
                case TOUCH_ACTIONS_CMD_ENABLE_TOUCH:
                case TOUCH_ACTIONS_CMD_UNKNOWN:
                default:
                    break;
            }
        }
#if defined(REACTOR_BADGE) || defined (CREST_BADGE)
    }
#endif
    if (cmdProcessed)
    {
        BadgeStats_IncrementNumTouchCmds(&this->badgeStats);
    }
}

static void SystemState_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");

    SystemState *this = (SystemState *)pObj;
    assert(this);
    BadgeStats_IncrementNumTouches(&this->badgeStats);
    if (this->touchActive)
    {
        SystemState_ResetTouchActiveTimer(this);
    }
}

static void SystemState_TouchActionNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Action Notification");

    SystemState *this = (SystemState *)pObj;
    TouchActionsCmd touchCmd = *((TouchActionsCmd*)notificationData);
    assert(this);
    SystemState_ProcessTouchActionCmd(this, touchCmd);
}

static void SystemState_BleNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    SystemState *this = (SystemState *)pObj;
    assert(this);
    ESP_LOGD(TAG, "Handling BLE Event");
    switch (notificationEvent)
    {
    case NOTIFICATION_EVENTS_BLE_ENABLED:
        ESP_LOGI(TAG, "BLE Enabled");
        if (this->pBleControl->bleXferPairMode)
        {
            ESP_LOGI(TAG, "In pair mode, enabling BLE xfer with pair name");
            BleControl_EnableBleXfer(this->pBleControl, false);
            this->pBleControl->bleXferRefreshInProgress = false;
        }

        break;
    case NOTIFICATION_EVENTS_BLE_DISABLED:
        ESP_LOGI(TAG, "BLE Disabled");
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_REQ_RECV:
        ESP_LOGD(TAG, "BLE Xfer Requested");
        this->pBleControl->bleXferRefreshInProgress = true;
        if (!this->pBleControl->bleXferPairMode)
        {
            this->pBleControl->bleXferPairMode = true;
            BleControl_DisableBleXfer(this->pBleControl);
        }
        else
        {
            ESP_LOGI(TAG, "Already in BLE Xfer pair mode, skipping");
        }

        break;
    case NOTIFICATION_EVENTS_BLE_XFER_ENABLED:
        ESP_LOGI(TAG, "BLE Xfer Enabled");
        if (LedModing_SetBleXferEnableActive(&this->ledModing, true) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Enable Active");
        }
        if (SystemState_ResetStatusIndicatorActiveTimer(this) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to reset status indicator active timer");
        }
        this->ledStatusIndicatorActive = true;
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_DISABLED:
        ESP_LOGI(TAG, "BLE Xfer Disabled");
        this->ledStatusIndicatorActive = false;
        if (LedModing_SetBleXferActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Active");
        }
        if (LedModing_SetBleXferEnableActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Enable Active");
        }
        if (SystemState_StopStatusIndicatorActiveTimer(this) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to stop status indicator active timer");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_CONNECTED:
        ESP_LOGI(TAG, "BLE Xfer Connected");
        if (LedModing_SetBleConnectedActive(&this->ledModing, true) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Connected Active");
        }
        if (SystemState_ResetStatusIndicatorActiveTimer(this) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to reset status indicator active timer");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_DISCONNECTED:
        ESP_LOGI(TAG, "BLE Xfer Disconnected");
        if (LedModing_SetBleXferActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Active");
        }
        if (LedModing_SetBleConnectedActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Connected Active");
        }
        if (SystemState_ResetStatusIndicatorActiveTimer(this) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to reset status indicator active timer");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_PERCENT_CHANGED:
        if (LedModing_SetBleXferActive(&this->ledModing, true) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Active");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_COMPLETE:
        ESP_LOGI(TAG, "BLE Xfer Complete");
        if (BleControl_DisableBleXfer(this->pBleControl) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to disable BLE Xfer");
        }
        if (LedModing_SetBleXferActive(&this->ledModing, false) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set BLE Xfer Active");
        }
        if (SystemState_ResetStatusIndicatorActiveTimer(this) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to reset status indicator active timer");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_FAILED:
        ESP_LOGI(TAG, "BLE Xfer Failed");
        if (SystemState_ResetStatusIndicatorActiveTimer(this) != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to reset status indicator active timer");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_NEW_SETTINGS_RECV:
        ESP_LOGI(TAG, "BLE Xfer New Settings Recv");
        if (notificationData != NULL)
        {
            UserSettingsFile *pUserSettingsFile = (UserSettingsFile*)notificationData;
            if (UserSettings_UpdateJson(&this->userSettings, (char*)pUserSettingsFile) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to update user settings");
            }
        }
        else
        {
            ESP_LOGE(TAG, "BLE Xfer New Settings Recv. Notification Data is NULL");
        }
        break;
    case NOTIFICATION_EVENTS_BLE_XFER_NEW_CUSTOM_RECV:
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
            BadgeStats_IncrementNumBleDisables(&this->badgeStats);
        }
        else
        {
            ESP_LOGE(TAG, "BLE Xfer New Custom Recv. Notification Data is NULL");
        }
        
        break;
    default:
        break;
    }
}

static void SystemState_GameEventNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
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
            BleControl_UpdateEventId(this->pBleControl, eventIdB64);
            break;
        }
        case NOTIFICATION_EVENTS_GAME_EVENT_JOINED:
        {
            ESP_LOGI(TAG, "Game event joined notification");
            this->gameState.nextHeartBeatTime = TimeUtils_GetFutureTimeTicks(EVENT_HEARTBEAT_INTERVAL_MS);
            BleControl_UpdateEventId(this->pBleControl, (char*)notificationData);
            LedModing_SetGameEventActive(&this->ledModing, true);
            break;
        }
        default:
            ESP_LOGE(TAG, "Invalid Notification");
    };
}

static esp_err_t SystemState_StatusIndicatorInactiveTimerExpired(SystemState *this)
{
    assert(this);
    ESP_LOGI(TAG, "Status Indicator Inactive Timer Expired");
    return LedModing_SetStatusIndicator(&this->ledModing, LED_STATUS_INDICATOR_NONE);
}

static esp_err_t SystemState_ResetStatusIndicatorActiveTimer(SystemState *this)
{
    assert(this);
    assert(this->drawStatusIndicatorTimer);
    esp_err_t ret = ESP_FAIL;

    if (xTimerReset(this->drawStatusIndicatorTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t SystemState_StopStatusIndicatorActiveTimer(SystemState *this)
{
    assert(this);
    assert(this->drawStatusIndicatorTimer);
    esp_err_t ret = ESP_FAIL;

    if (xTimerStop(this->drawStatusIndicatorTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
    }
    return ret;
}

static void SystemState_StatusIndicatorActiveTimerCallback(TimerHandle_t xTimer)
{
    SystemState* systemState = SystemState_GetInstance();
    SystemState_StatusIndicatorInactiveTimerExpired(systemState);
}

static esp_err_t SystemState_NetworkTestInactiveTimerExpired(SystemState *this)
{
    assert(this);
    ESP_LOGI(TAG, "Network Test Inactive Timer Expired");
    this->networkTestActive = false;
    return LedModing_SetNetworkTestActive(&this->ledModing, false);
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
#if defined(REACTOR_BADGE) || defined(CREST_BADGE)
    SynthMode_SetEnabled(&this->synthMode, false);
#endif
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
    esp_err_t ret = ESP_FAIL;
    assert(this);
    assert(this->touchActiveTimer);
    
    if (xTimerStop(this->touchActiveTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
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
    this->ledSequencePreviewActive = false;
    return LedModing_SetLedSequencePreviewActive(&this->ledModing, false);
}

static esp_err_t SystemState_ResetLedSequencePreviewActiveTimer(SystemState *this)
{
    assert(this);
    assert(this->ledSequencePreviewTimer);
    esp_err_t ret = ESP_FAIL;

    this->ledSequencePreviewActive = true;
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
    esp_err_t ret = ESP_FAIL;
    this->ledGameStatusActive = !this->ledGameStatusActive;
    if (xTimerReset(this->ledGameStatusToggleTimer, 0) == pdPASS)
    {
        ret = ESP_OK;
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

static void SystemState_NetworkTestNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGI(TAG, "Handling Network Test Notification: %d", *((bool *) notificationData));

    SystemState *this = (SystemState *)pObj;
    assert(this);
    this->ledControl.networkTestRuntimeInfo.success = *((bool *) notificationData);
    SystemState_StopNetworkTestActiveTimer(this);
    SystemState_ResetNetworkTestSuccessTimer(this);
}