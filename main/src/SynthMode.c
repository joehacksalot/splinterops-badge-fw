#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "AudioUtils.h"
#include "NotificationDispatcher.h"
#include "TaskPriorities.h"
#include "TouchSensor.h"
#include "SynthMode.h"
#include "TouchSensor.h"
#include "UserSettings.h"

#define SPEAKER_GPIO_NUM GPIO_NUM_18

#define DEFAULT_LEDC_TIMER LEDC_TIMER_0
#define DEFAULT_LEDC_CHANNEL LEDC_CHANNEL_0
#define DEFAULT_LEDC_DUTY_RESOLUTION LEDC_TIMER_2_BIT
#define DEFAULT_LEDC_SPEED_MODE LEDC_HIGH_SPEED_MODE
#define DEFAULT_LEDC_DUTY_OFF 0
#define DEFAULT_LEDC_DUTY_ON 3
#define DEFAULT_LEDC_FREQ 440
#define DEFAULT_LEDC_DURATION 400

static const char * TAG = "SYN";

// middle C - major scale
static int touchFrequencyMapping[TOUCH_SENSOR_NUM_BUTTONS] = 
#if defined (REACTOR_BADGE) || defined (TRON_BADGE)
{
    262, // TOUCH_SENSOR_12_OCLOCK
    294, // TOUCH_SENSOR_1_OCLOCK
    330, // TOUCH_SENSOR_2_OCLOCK
    349, // TOUCH_SENSOR_4_OCLOCK
    392, // TOUCH_SENSOR_5_OCLOCK
    440, // TOUCH_SENSOR_7_OCLOCK
    494, // TOUCH_SENSOR_8_OCLOCK
    523, // TOUCH_SENSOR_10_OCLOCK
    587  // TOUCH_SENSOR_11_OCLOCK
};
#elif defined (CREST_BADGE)
{
    262, // TOUCH_SENSOR_RIGHT_WING_FEATHER_1 = 0, // 0
    294, // TOUCH_SENSOR_RIGHT_WING_FEATHER_2,     // 1
    330, // TOUCH_SENSOR_RIGHT_WING_FEATHER_3,     // 2
    349, // TOUCH_SENSOR_RIGHT_WING_FEATHER_4,     // 3
    392, // TOUCH_SENSOR_TAIL_FEATHER,             // 4
    440, // TOUCH_SENSOR_LEFT_WING_FEATHER_4,      // 5
    494, // TOUCH_SENSOR_LEFT_WING_FEATHER_3,      // 6
    523, // TOUCH_SENSOR_LEFT_WING_FEATHER_2,      // 7
    587  // TOUCH_SENSOR_LEFT_WING_FEATHER_1,      // 8
};
#endif

static void SynthMode_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);
esp_err_t SynthMode_PlayTone(SynthMode* this, int frequency);
esp_err_t SynthMode_StopTone(SynthMode* this);


esp_err_t SynthMode_ConfigurePWM(SynthMode *this)
{
    esp_err_t ret = ESP_FAIL;

    ledc_timer_config_t ledc_timer =
    {
        .duty_resolution = DEFAULT_LEDC_DUTY_RESOLUTION, // resolution of PWM duty
        .freq_hz = DEFAULT_LEDC_FREQ,                    // frequency of PWM signal
        .speed_mode = DEFAULT_LEDC_SPEED_MODE,           // timer mode
        .timer_num = DEFAULT_LEDC_TIMER,                 // timer number
        .clk_cfg = LEDC_AUTO_CLK                         // timer index
    };

    ret = ledc_timer_config(&ledc_timer);

    if (ret == ESP_FAIL || ret == ESP_ERR_INVALID_ARG)
    {
        ESP_LOGE(TAG, "PWM LEDC timer failed to init");
        return ret;
    }

    ledc_channel_config_t ledc_channel =
    {
        .channel    = DEFAULT_LEDC_CHANNEL,
        .duty       = DEFAULT_LEDC_DUTY_OFF,
        .gpio_num   = SPEAKER_GPIO_NUM,
        .speed_mode = DEFAULT_LEDC_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = DEFAULT_LEDC_TIMER
    };

    ret = ledc_channel_config(&ledc_channel);

    if (ret == ESP_FAIL)
    {
        ESP_LOGE(TAG, "PWM LEDC channel failed to init");
        return ret;
    }

    return ret;

}


esp_err_t SynthMode_SetEnabled(SynthMode *this, bool enabled)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    if (this->initialized)
    {
        ESP_LOGD(TAG, "Setting audio enabled to %s", enabled ? "true" : "false");
        this->audioEnabled = enabled;
        ret = ESP_OK;
    }

    return ret;
}


esp_err_t SynthMode_Init(SynthMode *this, NotificationDispatcher *pNotificationDispatcher, UserSettings* pUserSettings)
{
    assert(this);

    if (this->initialized == false)
    {
        this->pNotificationDispatcher = pNotificationDispatcher;
        this->pUserSettings = pUserSettings;
        this->audioEnabled = false;
        esp_err_t ret = SynthMode_ConfigurePWM(this);
        if (ret == ESP_OK)
        {
            this->initialized = true;
            ESP_LOGI(TAG, "Synth Mode succesfully initialized");
            return NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &SynthMode_TouchSensorNotificationHandler, this);
        }
    }

    return ESP_FAIL;
}


esp_err_t SynthMode_PlayTone(SynthMode* this, int frequency)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    if (this->initialized)
    {
        ESP_LOGD(TAG, "Starting tone at %d", frequency);
        ledc_set_freq(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_TIMER, frequency);
        ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_ON);
        ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);
        ret = ESP_OK;
    }

    return ret;
}


esp_err_t SynthMode_StopTone(SynthMode* this)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;
    if (this->initialized)
    {
        ESP_LOGD(TAG, "Stopping tone");
        ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_OFF);
        ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);
        ret = ESP_OK;
    }

    return ret;
}


static void SynthMode_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");
    SynthMode *this = (SynthMode *)pObj;
    assert(this);
    assert(notificationData);
    TouchSensorEventNotificationData touchNotificationData = *(TouchSensorEventNotificationData *)notificationData;
    
    // global sound disabled
    if (this->pUserSettings->settings.soundEnabled == false)
    {
        ESP_LOGD(TAG, "Sound disabled, not playing audio");
        return;
    }
    // touch mode audio disabled
    if (this->audioEnabled == false)
    {
        ESP_LOGD(TAG, "Touch mode audio disabled, not playing audio");
        return;
    }

    if (touchNotificationData.touchSensorEvent != TOUCH_SENSOR_EVENT_RELEASED)
    {
        ESP_LOGD(TAG, "Playing audio for touch sensor %d  frequency %d", touchNotificationData.touchSensorIdx, touchFrequencyMapping[touchNotificationData.touchSensorIdx]);
        SynthMode_PlayTone(this, touchFrequencyMapping[touchNotificationData.touchSensorIdx]);
        return;
    }
    SynthMode_StopTone(this);
}
