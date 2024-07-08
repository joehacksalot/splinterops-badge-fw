
#include <string.h>
#include "esp_log.h"

#include "TouchActions.h"
#include "NotificationDispatcher.h"

// Internal Function Declarations
static esp_err_t CommandDetected(TouchActions *this, TouchActionsCmd touchActionCmd);
static void TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);
static TouchActionsCmd GetTouchAction(TouchActions *this);

// Internal Constants
static const char * TAG = "ACT";


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

static esp_err_t CommandDetected(TouchActions *this, TouchActionsCmd touchActionCmd)
{
    ESP_LOGD(TAG, "Command Detected: %d", touchActionCmd);
    esp_err_t ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_ACTION_CMD, &touchActionCmd, sizeof(touchActionCmd), DEFAULT_NOTIFY_WAIT_DURATION);

    return ret;
}

static TouchActionsCmd GetTouchAction(TouchActions *this)
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
        return TOUCH_ACTIONS_CMD_CLEAR;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        return TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_ENABLE_BLE_XFER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        return TOUCH_ACTIONS_CMD_DISABLE_BLE_XFER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        && 
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE;
    }
    else
    {
        return TOUCH_ACTIONS_CMD_UNKNOWN;
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
        return TOUCH_ACTIONS_CMD_CLEAR;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)      &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  >= TOUCH_SENSOR_EVENT_SHORT_PRESSED) &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  >= TOUCH_SENSOR_EVENT_SHORT_PRESSED) &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)      &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_SHORT_PRESSED) &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] >= TOUCH_SENSOR_EVENT_SHORT_PRESSED) &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_ENABLE_TOUCH;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_TOUCHED))
    {
        return TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        && 
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_ENABLE_BLE_XFER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_DISABLE_BLE_XFER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_ENABLE_SYNTH_MODE;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_DISABLE_SYNTH_MODE;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_12_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_1_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_2_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_4_OCLOCK]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_5_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_7_OCLOCK]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_8_OCLOCK]  >= TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_10_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_11_OCLOCK] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_NETWORK_TEST;
    }
    else
    {
        return TOUCH_ACTIONS_CMD_UNKNOWN;
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
        return TOUCH_ACTIONS_CMD_CLEAR;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_SHORT_PRESSED)  &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_ENABLE_TOUCH;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_DISABLE_TOUCH;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        return TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_TOUCHED)        && 
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        return TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] >= TOUCH_SENSOR_EVENT_TOUCHED))
    {
        return TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  >= TOUCH_SENSOR_EVENT_TOUCHED)        && 
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_ENABLE_BLE_XFER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_DISABLE_BLE_XFER;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_ENABLE_SYNTH_MODE;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_DISABLE_SYNTH_MODE;
    }
    else if ((this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_1]  == TOUCH_SENSOR_EVENT_RELEASED)       && 
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_2]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_3]  == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_LEFT_WING_FEATHER_4]  >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_TAIL_FEATHER]         >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_4] >= TOUCH_SENSOR_EVENT_TOUCHED)        &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_3] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_2] == TOUCH_SENSOR_EVENT_RELEASED)       &&
             (this->touchSensorValue[TOUCH_SENSOR_RIGHT_WING_FEATHER_1] == TOUCH_SENSOR_EVENT_RELEASED))
    {
        return TOUCH_ACTIONS_CMD_NETWORK_TEST;
    }
    else
    {
        return TOUCH_ACTIONS_CMD_UNKNOWN;
    }
#endif
}

static void TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");
    TouchActions *this = (TouchActions *)pObj;
    assert(this);
    assert(notificationData);
    TouchSensorEventNotificationData touchNotificationData = *(TouchSensorEventNotificationData *)notificationData;
    
    this->touchSensorValue[touchNotificationData.touchSensorIdx] = touchNotificationData.touchSensorEvent;
    ESP_LOGD(TAG, "Touch Sensor Notification. %d: %d", touchNotificationData.touchSensorIdx, touchNotificationData.touchSensorEvent);

    TouchActionsCmd touchActionCmd = GetTouchAction(this);
    if (touchActionCmd != TOUCH_ACTIONS_CMD_UNKNOWN)
    {
        CommandDetected(this, touchActionCmd);
    }
}
