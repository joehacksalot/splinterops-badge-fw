/**
 * @file TouchActions.h
 * @brief Touch sensor action processing and command dispatch system
 * 
 * This module provides high-level touch action processing including:
 * - Touch gesture recognition and command mapping
 * - Multi-touch sequence processing
 * - System command dispatch (LED control, BLE pairing, synth mode)
 * - Touch sensor state tracking and validation
 * - Integration with notification dispatcher for action events
 * - User interface navigation through touch interactions
 * 
 * The touch actions system translates raw touch sensor events into
 * meaningful system commands and user interface interactions.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef TOUCH_ACTIONS_CMD_H_
#define TOUCH_ACTIONS_CMD_H_

#include "TouchSensor.h"
#include "NotificationDispatcher.h"

typedef enum TouchActionsCmd_e
{
    TOUCH_ACTIONS_CMD_UNKNOWN,
    TOUCH_ACTIONS_CMD_CLEAR,
    TOUCH_ACTIONS_CMD_ENABLE_TOUCH,
    TOUCH_ACTIONS_CMD_DISABLE_TOUCH,
    TOUCH_ACTIONS_CMD_NEXT_LED_SEQUENCE,
    TOUCH_ACTIONS_CMD_PREV_LED_SEQUENCE,
    TOUCH_ACTIONS_CMD_DISPLAY_VOLTAGE_METER,
    TOUCH_ACTIONS_CMD_ENABLE_BLE_PAIRING,
    TOUCH_ACTIONS_CMD_DISABLE_BLE_PAIRING,
    TOUCH_ACTIONS_CMD_TOGGLE_SYNTH_MODE_ENABLE,
    TOUCH_ACTIONS_CMD_NETWORK_TEST
} TouchActionsCmd;

typedef struct TouchActions_t
{
    TouchSensorEvent touchSensorValue[TOUCH_SENSOR_NUM_BUTTONS];
    NotificationDispatcher *pNotificationDispatcher;
} TouchActions;

/**
 * @brief Initialize the touch actions processing system
 * 
 * Initializes the touch action processor that converts low-level touch
 * sensor events into high-level actions and commands. Integrates with
 * the notification dispatcher to handle touch-based interactions.
 * 
 * @param this Pointer to TouchActions instance to initialize
 * @param pNotificationDispatcher Notification system for touch action events
 * @return ESP_OK on success, error code on failure
 */
esp_err_t TouchActions_Init(TouchActions *this, NotificationDispatcher *pNotificationDispatcher);

#endif // TOUCH_ACTIONS_CMD_H_