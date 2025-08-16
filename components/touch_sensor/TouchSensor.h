#ifndef TOUCH_SENSOR_H_
#define TOUCH_SENSOR_H_

#include "esp_err.h"
#include "esp_event.h"
#include "NotificationDispatcher.h"
#include "BadgeHwProfile.h"

#define DELTA_VALUE_HISTORY_SIZE 1

typedef enum TouchSensorEvent_t
{
    TOUCH_SENSOR_EVENT_RELEASED = 0,
    TOUCH_SENSOR_EVENT_TOUCHED,
    TOUCH_SENSOR_EVENT_SHORT_PRESSED,
    TOUCH_SENSOR_EVENT_LONG_PRESSED,
    TOUCH_SENSOR_EVENT_VERY_LONG_PRESSED
} TouchSensorEvent;

typedef struct TouchSensorEventNotificationData_t
{
    TouchSensorEvent touchSensorEvent;
    int touchSensorIdx;
} TouchSensorEventNotificationData;

typedef struct TouchSensor_t
{
    NotificationDispatcher *pNotificationDispatcher;
    int prevDeltaValueCtr[TOUCH_SENSOR_NUM_BUTTONS];
    int prevDeltaValues[TOUCH_SENSOR_NUM_BUTTONS][DELTA_VALUE_HISTORY_SIZE];
    int prevTouchValue[TOUCH_SENSOR_NUM_BUTTONS];
    int touchSensorActive[TOUCH_SENSOR_NUM_BUTTONS];
    TickType_t touchSensorActiveTimeStamp[TOUCH_SENSOR_NUM_BUTTONS];
    esp_event_loop_handle_t touch_notify_loop_task;
    int touchSensortNotificationEvent;
    bool touchEnabled;
} TouchSensor;

esp_err_t TouchSensor_Init(TouchSensor *this, NotificationDispatcher *pNotificationDispatcher, int taskPriority, int touchSensortNotificationEvent);
int TouchSensor_GetTouchSensorActive(TouchSensor *this, int pad_num);
esp_err_t TouchSensor_SetTouchEnabled(TouchSensor *this, bool enabled);

#endif // TOUCH_SENSOR_H_