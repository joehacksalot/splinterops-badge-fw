#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/touch_sensor.h"
#include "TimeUtils.h"

#include "NotificationDispatcher.h"
#include "TouchSensor.h"
#include "TaskPriorities.h"
#include "Utilities.h"

// Touch pad settings
#define TOUCH_THRESHOLD_NONE 0
#define TOUCH_FILTER_MODE_EN 1
#define TOUCH_FILTER_PERIOD_MS 50                   // Changing to meet CPU0 watchdog

// Touch sensor settings
#define TOUCH_ACTIVE_DELTA_THRESHOLD         (150)
#define TOUCH_SAMPLE_PERIOD_MS               (100)
#define TOUCH_SHORT_PRESS_THRESHOLD          (1000)
#define TOUCH_LONG_PRESS_THRESHOLD           (3000)
#define TOUCH_SUPER_LONG_PRESS_THRESHOLD     (5000)
#define TOUCH_STUCK_RELEASE_THRESHOLD        (7000)

// Internal Function Declarations
static void TouchSensorTask(void *pvParameters);
static esp_err_t MonitorTouchSensors(TouchSensor *this);

// Internal Constants
static const char * TOUCH_TAG = "TCH";

#if defined(TRON_BADGE)
  static const int TOUCH_BUTTON_MAP[TOUCH_SENSOR_NUM_BUTTONS] =  {0,2,3,4,5,6,7,8,9};
#elif defined(REACTOR_BADGE)
  static const int TOUCH_BUTTON_MAP[TOUCH_SENSOR_NUM_BUTTONS] =  {7,6,4,3,2,5,0,9,8};
#elif defined(CREST_BADGE)
  static const int TOUCH_BUTTON_MAP[TOUCH_SENSOR_NUM_BUTTONS] =  {0,2,3,4,5,6,7,8,9};
#endif

esp_err_t TouchSensor_Init(TouchSensor *this, NotificationDispatcher *pNotificationDispatcher)
{
    assert(this);
    assert(pNotificationDispatcher);
    esp_err_t ret = ESP_FAIL;

    memset(this, 0, sizeof(*this));

    this->pNotificationDispatcher = pNotificationDispatcher;

    // Defaults to software trigger mode
    ret = touch_pad_init();
    ESP_ERROR_CHECK(ret);

    // Set reference voltage for charging/discharging
    // High will be 2.7V
    // Low will be 0.5V
    // High reference voltage attenuation will be 1V
    ret = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    ESP_ERROR_CHECK(ret);

    // Index 0 (First touchpad)
    for (int i = 0; i < TOUCH_SENSOR_NUM_BUTTONS; ++i)
    {
        ret = touch_pad_config(TOUCH_BUTTON_MAP[i], TOUCH_THRESHOLD_NONE);
        ESP_ERROR_CHECK(ret);
    }

    // Filter uses FreeRTOS time with priority of 1 (Low) on CPU0. Quality of data will be affected if
    // higher priority tasks take lots of CPU time
    ret = touch_pad_filter_start(TOUCH_FILTER_PERIOD_MS);
    ESP_ERROR_CHECK(ret);

    xTaskCreate(TouchSensorTask, "TouchSensorTask", configMINIMAL_STACK_SIZE * 10, this, TOUCH_SENSOR_TASK_PRIORITY, NULL);
    return ret;
}

static void TouchSensorTask(void *pvParameters)
{
    TouchSensor *this = (TouchSensor *)pvParameters;
    assert(this);
    registerCurrentTaskInfo();
    while (true)
    {
        MonitorTouchSensors(this);
        vTaskDelay(pdMS_TO_TICKS(TOUCH_SAMPLE_PERIOD_MS));
    }
}

esp_err_t TouchSensor_SetTouchEnabled(TouchSensor *this, bool enabled)
{
    assert(this);
    this->touchEnabled = enabled;
    return ESP_OK;
}

static esp_err_t MonitorTouchSensors(TouchSensor *this)
{
    esp_err_t ret = ESP_OK;
    uint16_t touchSensorValue = 0;
    uint16_t touchFilterValue = 0;

    assert(this);

    for (int i = 0; i < TOUCH_SENSOR_NUM_BUTTONS; i++)
    {
        // reach touch pad raw and filtered values for mapped touch pad index
        uint16_t mappedTouchButtonIndex = TOUCH_BUTTON_MAP[i];
        ret = touch_pad_read_raw_data(mappedTouchButtonIndex, &touchSensorValue);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TOUCH_TAG, "touch_pad_read_raw_data error %s", esp_err_to_name(ret));
        }
        ret = touch_pad_read_filtered(mappedTouchButtonIndex, &touchFilterValue);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TOUCH_TAG, "touch_pad_read_filtered error %s", esp_err_to_name(ret));
        }

        // compute and store delta value for the current index
        int delta = 0;
        if (this->prevTouchValue[i] > 0)
        {
            delta = touchSensorValue - this->prevTouchValue[i];
        }
        this->prevDeltaValues[i][this->prevDeltaValueCtr[i]] = delta;
        this->prevDeltaValueCtr[i] = ((this->prevDeltaValueCtr[i] + 1) % DELTA_VALUE_HISTORY_SIZE);

        // Sum all the previous values for the current index
        int curTouchSensorValuesSum = 0;
        for (int prev_delta_values_idx = 0; prev_delta_values_idx < DELTA_VALUE_HISTORY_SIZE; prev_delta_values_idx++)
        {
            curTouchSensorValuesSum += this->prevDeltaValues[i][prev_delta_values_idx];
        }

        // compute absolute value of the 
        int absDeltaSum = abs(curTouchSensorValuesSum);

        TickType_t curTime = xTaskGetTickCount();

        // if the absolute value of the sum of the previous delta values is greater than the threshold, then it is considered a touch action (either touch or release)
        if (absDeltaSum > TOUCH_ACTIVE_DELTA_THRESHOLD)
        {
            // negative delta means touch
            if (delta < 0)
            {
                if (this->touchSensorActive[i] == TOUCH_SENSOR_EVENT_RELEASED)
                {
                    ESP_LOGI(TOUCH_TAG, "Touch %d Pressed", i);
                    this->touchSensorActive[i] = TOUCH_SENSOR_EVENT_TOUCHED;
                    this->touchSensorActiveTimeStamp[i] = curTime;

                    TouchSensorEventNotificationData notificationData = { .touchSensorIdx = i, .touchSensorEvent = TOUCH_SENSOR_EVENT_TOUCHED };
                    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &notificationData, sizeof(notificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TOUCH_TAG, "NotificationDispatcher_NotifyEvent for TOUCH_SENSOR_EVENT_TOUCHED event error %d", ret);
                    }
                }
            }
            // positive delta means release
            else
            {
                if (this->touchSensorActive[i] != TOUCH_SENSOR_EVENT_RELEASED)
                {
                    ESP_LOGD(TOUCH_TAG, "Touch %d Released", i);
                    this->touchSensorActive[i] = TOUCH_SENSOR_EVENT_RELEASED;
                    this->touchSensorActiveTimeStamp[i] = curTime;

                    TouchSensorEventNotificationData notificationData = { .touchSensorIdx = i, .touchSensorEvent = TOUCH_SENSOR_EVENT_RELEASED };
                    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &notificationData, sizeof(notificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TOUCH_TAG, "NotificationDispatcher_NotifyEvent for TOUCH_SENSOR_EVENT_RELEASED event error %d", ret);
                    }
                }
            }
            ESP_LOGD(TOUCH_TAG, "Abs Delta: %d, Delta: %d, touchSensorValue: %d, touch_value_filtered: %d, sum: %d", absDeltaSum, delta, touchSensorValue, touchFilterValue, curTouchSensorValuesSum);
        }
        // if no touch action has been detected, check for short or long press on active touch pads
        else
        {
            // if the touch pad is active, check for long press
            if (this->touchSensorActive[i] != TOUCH_SENSOR_EVENT_RELEASED)
            {
                long elapsed_time_msec = TimeUtils_GetElapsedTimeMSec(this->touchSensorActiveTimeStamp[i]);
                // if the touchpad has been aftive for longer than the short press threshold, then it is considered a short press
                if (this->touchSensorActive[i] == TOUCH_SENSOR_EVENT_TOUCHED && 
                    elapsed_time_msec > TOUCH_SHORT_PRESS_THRESHOLD)
                {
                    ESP_LOGD(TOUCH_TAG, "Touch %d Short Pressed", i);
                    this->touchSensorActive[i] = TOUCH_SENSOR_EVENT_SHORT_PRESSED;
                    TouchSensorEventNotificationData notificationData = { .touchSensorIdx = i, .touchSensorEvent = TOUCH_SENSOR_EVENT_SHORT_PRESSED };
                    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &notificationData, sizeof(notificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TOUCH_TAG, "NotificationDispatcher_NotifyEvent for TOUCH_SENSOR_EVENT_SHORT_PRESSED event error %d", ret);
                    }
                }
                // if the touch pad has been active for longer than the long press threshold, then it is considered a long press
                else if (this->touchSensorActive[i] == TOUCH_SENSOR_EVENT_SHORT_PRESSED && 
                         elapsed_time_msec > TOUCH_LONG_PRESS_THRESHOLD)
                {
                    ESP_LOGD(TOUCH_TAG, "Touch %d Long Pressed", i);
                    this->touchSensorActive[i] = TOUCH_SENSOR_EVENT_LONG_PRESSED;
                    TouchSensorEventNotificationData notificationData = { .touchSensorIdx = i, .touchSensorEvent = TOUCH_SENSOR_EVENT_LONG_PRESSED };
                    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &notificationData, sizeof(notificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TOUCH_TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BUTTON_LONG_PRESSED event error %d", ret);
                    }
                }// if the touch pad has been active for longer than the super long press threshold, then it is considered a super long press
                else if (this->touchSensorActive[i] == TOUCH_SENSOR_EVENT_LONG_PRESSED && 
                         elapsed_time_msec > TOUCH_SUPER_LONG_PRESS_THRESHOLD)
                {
                    ESP_LOGD(TOUCH_TAG, "Touch %d Super Long Pressed", i);
                    this->touchSensorActive[i] = TOUCH_SENSOR_EVENT_VERY_LONG_PRESSED;
                    TouchSensorEventNotificationData notificationData = { .touchSensorIdx = i, .touchSensorEvent = TOUCH_SENSOR_EVENT_VERY_LONG_PRESSED };
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &notificationData, sizeof(notificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TOUCH_TAG, "NotificationDispatcher_NotifyEvent for TOUCH_SENSOR_EVENT_VERY_LONG_PRESSED event error %d", ret);
                    }
                }
                else if (this->touchSensorActive[i] == TOUCH_SENSOR_EVENT_VERY_LONG_PRESSED && 
                         elapsed_time_msec > TOUCH_STUCK_RELEASE_THRESHOLD &&
                         this->touchEnabled == false)
                {
                    ESP_LOGD(TOUCH_TAG, "Touch %d Released Unstuck", i);
                    this->touchSensorActive[i] = TOUCH_SENSOR_EVENT_RELEASED;
                    this->touchSensorActiveTimeStamp[i] = curTime;

                    TouchSensorEventNotificationData notificationData = { .touchSensorIdx = i, .touchSensorEvent = TOUCH_SENSOR_EVENT_RELEASED };
                    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &notificationData, sizeof(notificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TOUCH_TAG, "NotificationDispatcher_NotifyEvent for TOUCH_SENSOR_EVENT_RELEASED event error %d", ret);
                    }
                }
            }
        }

        this->prevTouchValue[i] = touchSensorValue;
    }
    return ret;
}

int TouchSensor_GetTouchSensorActive(TouchSensor *this, int touchSensorIdx)
{
    assert(this);
    if (touchSensorIdx >= TOUCH_SENSOR_NUM_BUTTONS)
    {
        ESP_LOGE(TOUCH_TAG, "TouchSensor_GetTouchSensorActive called with invalid pad num. %d \n", touchSensorIdx);
        touchSensorIdx = 0;
    }
    return this->touchSensorActiveTimeStamp[touchSensorIdx];
}