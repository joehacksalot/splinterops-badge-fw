
/**
 * @file TouchActions.c
 * @brief Translates raw touch sensor events into high-level action commands.
 *
 * This module subscribes to touch sensor notifications via the
 * NotificationDispatcher and converts combinations and press patterns into
 * actionable commands defined in TouchActions.h (e.g., enable/disable touch,
 * toggle synth mode, change LED sequence, network test).
 *
 * Badge variants supported via compile-time defines:
 * - TRON_BADGE
 * - REACTOR_BADGE
 * - CREST_BADGE
 * - FMAN25_BADGE
 *
 * Threading
 * - Handlers are invoked from the NotificationDispatcher's context; no internal
 *   mutexing is performed. If multiple tasks access shared state based on these
 *   commands, coordinate with external synchronization.
 *
 * Notes
 * - No dynamic allocation aside from what the dispatcher may use.
 * - All logic is purely combinational based on last-known sensor states.
 */
#include <string.h>
#include "esp_log.h"

#include "TouchActions.h"
#include "NotificationDispatcher.h"

// Internal Function Declarations
static esp_err_t CommandDetected(TouchActions *this, TouchActionsCmd touchActionCmd);
static void TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void ReportTouchActionCommands(TouchActions *this);

// Internal Constants
static const char * TAG = "ACT";

/**
 * @brief Initialize the TouchActions module and subscribe for touch events.
 *
 * Resets internal state, stores the dispatcher pointer, initializes all
 * cached sensor values to RELEASED, and registers a notification handler for
 * NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION.
 *
 * @param this Pointer to TouchActions instance to initialize.
 * @param pNotificationDispatcher Dispatcher used to receive touch notifications.
 * @return ESP_OK on success, error from NotificationDispatcher on failure.
 */
esp_err_t TouchActions_Init(TouchActions *this, NotificationDispatcher *pNotificationDispatcher)
{
    memset(this, 0, sizeof(*this));

    this->pNotificationDispatcher = pNotificationDispatcher;
    for (int i = 0; i < TOUCH_SENSOR_NUM_BUTTONS; i++)
    {
        this->touchSensorValue[i] = TOUCH_SENSOR_EVENT_RELEASED;
    }

    return NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &TouchSensorNotificationHandler, this);
}

/**
 * @brief Internal helper to emit a detected command notification.
 *
 * Packages and forwards the specified command through the NotificationDispatcher
 * as NOTIFICATION_EVENTS_TOUCH_ACTION_CMD.
 *
 * @param this TouchActions instance.
 * @param touchActionCmd Command to emit.
 * @return esp_err_t result of NotificationDispatcher_NotifyEvent.
 */
static esp_err_t CommandDetected(TouchActions *this, TouchActionsCmd touchActionCmd)
{
    ESP_LOGD(TAG, "Command Detected: %d", touchActionCmd);
    esp_err_t ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_ACTION_CMD, &touchActionCmd, sizeof(touchActionCmd), DEFAULT_NOTIFY_WAIT_DURATION);

    return ret;
}

/**
 * @brief Evaluate current touch states and map to action commands.
 *
 * Reads the cached sensor states and, depending on the badge variant, matches
 * specific button combinations and press durations to high-level commands. Any
 * detected command is forwarded via CommandDetected().
 *
 * This function does not block and should be called whenever touch state
 * changes are received.
 *
 * @param this TouchActions instance.
 */
static void ReportTouchActionCommands(TouchActions *this)
{
#if defined(TRON_BADGE)
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_CLEAR);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_ENABLE_BLE_PAIRING);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISABLE_BLE_PAIRING);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        && 
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE);
    }
#elif defined(REACTOR_BADGE)
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_CLEAR);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  >= TOUCH_SENSOR_EVENT_SHORT_PRESSED) &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  >= TOUCH_SENSOR_EVENT_SHORT_PRESSED) &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_SHORT_PRESSED) &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] >= TOUCH_SENSOR_EVENT_SHORT_PRESSED) &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_ENABLE_TOUCH);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_TOUCHED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        && 
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_ENABLE_BLE_PAIRING);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISABLE_BLE_PAIRING);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_TOGGLE_SYNTH_MODE_ENABLE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_NETWORK_TEST);
    }
#elif defined(CREST_BADGE)
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_CLEAR);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_SHORT_PRESSED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_ENABLE_TOUCH);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] >= TOUCH_SENSOR_EVENT_SHORT_PRESSED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] >= TOUCH_SENSOR_EVENT_SHORT_PRESSED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] >= TOUCH_SENSOR_EVENT_SHORT_PRESSED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISABLE_TOUCH);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_TOUCHED)        && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  >= TOUCH_SENSOR_EVENT_TOUCHED)        && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_ENABLE_BLE_PAIRING);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISABLE_BLE_PAIRING);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)      && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  >= TOUCH_SENSOR_EVENT_TOUCHED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] >= TOUCH_SENSOR_EVENT_TOUCHED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED)
        )
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_TOGGLE_SYNTH_MODE_ENABLE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_NETWORK_TEST);
    }
#elif defined(FMAN25_BADGE)
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_CLEAR);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  >= TOUCH_SENSOR_EVENT_SHORT_PRESSED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_ENABLE_TOUCH);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_SHORT_PRESSED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  == TOUCH_SENSOR_EVENT_SHORT_PRESSED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_SHORT_PRESSED)  &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISABLE_TOUCH);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_TOUCHED)        && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_ENABLE_BLE_PAIRING);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_DISABLE_BLE_PAIRING);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_TOUCHED)       && 
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)      &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_TOUCHED)
        )
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_TOGGLE_SYNTH_MODE_ENABLE);
    }
    if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_1]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_2]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_LEFT_TOUCH_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_CENTER_TOUCH]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
        (this->touchSensorValue[TOUCH_SENSOR_RIGHT_TOUCH_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        CommandDetected(this, TOUCH_ACTIONS_CMD_NETWORK_TEST);
    }
#endif
}

/**
 * @brief Notification handler for touch sensor events.
 *
 * Updates the cached sensor value for the reported sensor index, logs the
 * event, and invokes ReportTouchActionCommands() to translate state to
 * commands.
 *
 * @param pObj Pointer to TouchActions instance (as void*).
 * @param eventBase Event base identifier (unused here, for interface parity).
 * @param notificationEvent Specific touch event code (unused; data carries details).
 * @param notificationData Pointer to TouchSensorEventNotificationData.
 */
static void TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");
    TouchActions *this = (TouchActions *)pObj;
    assert(this);
    assert(notificationData);
    TouchSensorEventNotificationData touchNotificationData = *(TouchSensorEventNotificationData *)notificationData;
    
    this->touchSensorValue[touchNotificationData.touchSensorIdx] = touchNotificationData.touchSensorEvent;
    ESP_LOGD(TAG, "Touch Sensor Notification. %d: %d", touchNotificationData.touchSensorIdx, touchNotificationData.touchSensorEvent);

    ReportTouchActionCommands(this);
}
