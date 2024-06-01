#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "AudioUtils.h"
#include "LedControl.h"
#include "TaskPriorities.h"
#include "SynthMode.h"
#include "NotificationDispatcher.h"
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
static int touchFrequencyMapping[TOUCH_SENSOR_NUM_BUTTONS] = {
    262, // TOUCH_SENSOR_12_OCLOCK
    294, // TOUCH_SENSOR_1_OCLOCK
    330, // TOUCH_SENSOR_2_OCLOCK
    349, // TOUCH_SENSOR_4_OCLOCK
    392, // TOUCH_SENSOR_5_OCLOCK
    440, // TOUCH_SENSOR_7_OCLOCK
    494, // TOUCH_SENSOR_8_OCLOCK
    523, // TOUCH_SENSOR_10_OCLOCK
    587 // TOUCH_SENSOR_11_OCLOCK
};

static esp_err_t SynthMode_ServicePlayTouchSound(SynthMode* this);
static void SynthModeTask(void* pvParameters);


esp_err_t SynthMode_ConfigurePWM(SynthMode *this) {

    esp_err_t ret = ESP_FAIL;

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = DEFAULT_LEDC_DUTY_RESOLUTION, // resolution of PWM duty
        .freq_hz = DEFAULT_LEDC_FREQ,          // frequency of PWM signal
        .speed_mode = DEFAULT_LEDC_SPEED_MODE,    // timer mode
        .timer_num = DEFAULT_LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK          // timer index
    };

    ret = ledc_timer_config(&ledc_timer);

    if (ret == ESP_FAIL || ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "PWM LEDC timer failed to init");
        return ret;
    }

    ledc_channel_config_t ledc_channel = {
        .channel    = DEFAULT_LEDC_CHANNEL,
        .duty       = DEFAULT_LEDC_DUTY_OFF,
        .gpio_num   = SPEAKER_GPIO_NUM,
        .speed_mode = DEFAULT_LEDC_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = DEFAULT_LEDC_TIMER
    };

    ret = ledc_channel_config(&ledc_channel);

    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "PWM LEDC channel failed to init");
        return ret;
    }

    return ret;

}

esp_err_t SynthMode_SetEnabled(SynthMode *this, bool enabled) {

    assert(this);

    esp_err_t ret = ESP_FAIL;

    if (this->initialized) {
        this->audioEnabled = enabled;
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t SynthMode_Init(SynthMode *this, NotificationDispatcher *pNotificationDispatcher, LedControl* ledControl, UserSettings* userSettings) {

    esp_err_t ret = ESP_FAIL;
    assert(this);

    if (this->initialized == false) {

        this->pNotificationDispatcher = pNotificationDispatcher;

        this->ledControl = ledControl;
        this->userSettings = userSettings;

        this->audioEnabled = false;

        this->procSyncMutex = xSemaphoreCreateMutex();
        assert(this->procSyncMutex);

        ret = SynthMode_ConfigurePWM(this);

        if (ret == ESP_OK) {
            this->initialized = true;
            xTaskCreate(SynthModeTask, "SynthModeTask", 1024, this, SYNTH_MODE_TASK_PRIORITY, NULL);
            ESP_LOGI(TAG, "Synth Mode succesfully initialized");
        }
    }

    return ret;
}

esp_err_t SynthMode_PlayTone(SynthMode* this, int frequency) {

    assert(this);

    esp_err_t ret = ESP_FAIL;

    if (this->initialized) {
        ledc_set_freq(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_TIMER, frequency);
        ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_ON);
        ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);
        // ledc_set_freq(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_TIMER, frequency);
        vTaskDelay(pdMS_TO_TICKS(DEFAULT_LEDC_DURATION));
        ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_OFF);
        ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);

        ret = ESP_OK;
    }

    return ret;
}

static void SynthModeTask(void* pvParameters) {
    SynthMode* this = (SynthMode*)pvParameters;
    assert(this);
    while(true) {
        SynthMode_ServicePlayTouchSound(this);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static esp_err_t SynthMode_ServicePlayTouchSound(SynthMode* this) {
    esp_err_t ret = ESP_FAIL;
    assert(this);
    // global sound disabled
    if (this->userSettings->settings.soundEnabled == false) {
        return ESP_OK;
    }
    // touch mode audio disabled
    if (this->audioEnabled == false) {
        return ESP_OK;
    }

    for (int touchIndex = 0; touchIndex < TOUCH_SENSOR_NUM_BUTTONS; touchIndex++) {
        if (this->ledControl->touchModeRuntimeInfo.touchSensorValue[touchIndex] == TOUCH_SENSOR_EVENT_TOUCHED) {

            ret = SynthMode_PlayTone(this, touchFrequencyMapping[touchIndex]);
            break;
        }
    }

    return ret;
}
