#ifndef TOUCH_ACTIONS_CMD_H_
#define TOUCH_ACTIONS_CMD_H_

#include "TouchSensor.h"
#include "NotificationDispatcher.h"

typedef enum TouchActionsCmd_e
{
    TOUCH_ACTIONS_CMD_UNKNOWN,
    TOUCH_ACTIONS_CMD_CLEAR,
    TOUCH_ACTIONS_CMD_ENABLE_TOUCH,
    TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE,
    TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE,
    TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER,
    TOUCH_ACTIONS_CMD_ENABLE_BLE_XFER,
    TOUCH_ACTIONS_CMD_DISABLE_BLE_XFER,
    TOUCH_ACTIONS_CMD_ENABLE_AUDIO_PLAYER,
    TOUCH_ACTIONS_CMD_DISABLE_AUDIO_PLAYER,
    TOUCH_ACTIONS_CMD_NETWORK_TEST
} TouchActionsCmd;

typedef struct TouchActions_t
{
    TouchSensorEvent touchSensorValue[TOUCH_SENSOR_NUM_BUTTONS];
    NotificationDispatcher *pNotificationDispatcher;
} TouchActions;

esp_err_t TouchActions_Init(TouchActions *this, NotificationDispatcher *pNotificationDispatcher);

#endif // TOUCH_ACTIONS_CMD_H_