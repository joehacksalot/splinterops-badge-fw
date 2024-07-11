
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "GpioControl.h"

// Configure output pin
#define GPIO_LEFT_EYE      22
#define GPIO_RIGHT_EYE     21
#define GPIO_VIBRATION     19
// #define GPIO_PIEZO         18
// 1ULL << GPIO_PIEZO
#define GPIO_OUTPUT_PIN_SEL     (1ULL << GPIO_LEFT_EYE |  1ULL << GPIO_RIGHT_EYE | 1ULL << GPIO_VIBRATION)


// Internal Function Declarations
// static esp_err_t GpioControl_StopTimer(GpioControl *this, GpioFeatures feature);
static esp_err_t GpioControl_ResetTimer(GpioControl *this, GpioFeatures feature, uint32_t durationMs);
static esp_err_t GpioControl_TimeoutEventHandlerAction(GpioControl *this, GpioFeatures feature);
static void GpioControl_LeftEyeTimeoutEventHandler(void* arg);
static void GpioControl_RightEyeTimeoutEventHandler(void* arg);
static void GpioControl_VibrationTimeoutEventHandler(void* arg);
// static void GpioControl_PiezoTimeoutEventHandler(void* arg);

// Internal Constants
static const char * TAG = "GPIO";

esp_err_t GpioControl_Init(GpioControl *this)
{
    esp_err_t ret = ESP_OK;
    assert(this);

    this->timerHandleArgs[GPIO_FEATURE_LEFT_EYE].callback = &GpioControl_LeftEyeTimeoutEventHandler;
    this->timerHandleArgs[GPIO_FEATURE_LEFT_EYE].arg = (void*)(this);
    this->timerHandleArgs[GPIO_FEATURE_LEFT_EYE].name = "left-eye-timeout";
    ESP_ERROR_CHECK(esp_timer_create(&this->timerHandleArgs[GPIO_FEATURE_LEFT_EYE], &this->timers[GPIO_FEATURE_LEFT_EYE]));

    this->timerHandleArgs[GPIO_FEATURE_RIGHT_EYE].callback = &GpioControl_RightEyeTimeoutEventHandler;
    this->timerHandleArgs[GPIO_FEATURE_RIGHT_EYE].arg = (void*)(this);
    this->timerHandleArgs[GPIO_FEATURE_RIGHT_EYE].name = "right-eye-timeout";
    ESP_ERROR_CHECK(esp_timer_create(&this->timerHandleArgs[GPIO_FEATURE_RIGHT_EYE], &this->timers[GPIO_FEATURE_RIGHT_EYE]));

    this->timerHandleArgs[GPIO_FEATURE_VIBRATION].callback = &GpioControl_VibrationTimeoutEventHandler;
    this->timerHandleArgs[GPIO_FEATURE_VIBRATION].arg = (void*)(this);
    this->timerHandleArgs[GPIO_FEATURE_VIBRATION].name = "vibration-timeout";
    ESP_ERROR_CHECK(esp_timer_create(&this->timerHandleArgs[GPIO_FEATURE_VIBRATION], &this->timers[GPIO_FEATURE_VIBRATION]));

    // this->timerHandleArgs[GPIO_FEATURE_PIEZO].callback = &GpioControl_PiezoTimeoutEventHandler;
    // this->timerHandleArgs[GPIO_FEATURE_PIEZO].arg = (void*)(this);
    // this->timerHandleArgs[GPIO_FEATURE_PIEZO].name = "piezo-timeout";
    // ESP_ERROR_CHECK(esp_timer_create(&this->timerHandleArgs[GPIO_FEATURE_PIEZO], &this->timers[GPIO_FEATURE_PIEZO]));


    gpio_config_t config = {};
    config.intr_type = GPIO_INTR_DISABLE;
    config.mode = GPIO_MODE_OUTPUT;
    config.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    config.pull_down_en = 0;
    config.pull_up_en = 0;
    ret = (gpio_config(&config));
    ESP_ERROR_CHECK(ret);
    return ret;
}

esp_err_t GpioControl_Control(GpioControl *this, GpioFeatures feature, bool state, uint32_t durationMs)
{
    int gpioPinNumber = -1;

    switch (feature)
    {
      case GPIO_FEATURE_LEFT_EYE:
          gpioPinNumber = GPIO_LEFT_EYE;
          break;
      case GPIO_FEATURE_RIGHT_EYE:
          gpioPinNumber = GPIO_RIGHT_EYE;
          break;
      case GPIO_FEATURE_VIBRATION:
          gpioPinNumber = GPIO_VIBRATION;
          break;
    //   case GPIO_FEATURE_PIEZO:
    //       gpioPinNumber = GPIO_PIEZO;
    //       break;
      default:
          return ESP_FAIL;
    }

    int curState = gpio_get_level(gpioPinNumber);
    
    if (durationMs > 0)
    {
        if (this->timerRunning[feature] == 0)
        {
            this->previousState[feature] = curState;
        }
        GpioControl_ResetTimer(this, feature, durationMs);
    }

    ESP_ERROR_CHECK(gpio_set_level(gpioPinNumber, state? 1 : 0));
    ESP_LOGI(TAG, "Gpio feature %d set to %d for %lu ms", feature, state, durationMs);

    return ESP_OK;
}

// static esp_err_t GpioControl_StopTimer(GpioControl *this, GpioFeatures feature)
// {
//     assert(this);
//     assert(feature >= 0);
//     assert(feature < NUM_GPIO_FEATURES);

//     this->timerRunning[feature] = 0;
//     return esp_timer_stop(this->timers[feature]);
// }

static esp_err_t GpioControl_ResetTimer(GpioControl *this, GpioFeatures feature, uint32_t durationMs)
{
    assert(this);
    assert(feature >= 0);
    assert(feature < NUM_GPIO_FEATURES);

    this->timerRunning[feature] = 1;
    esp_timer_stop(this->timers[feature]);
    return esp_timer_start_once(this->timers[feature], durationMs*1000);
}

static esp_err_t GpioControl_TimeoutEventHandlerAction(GpioControl *this, GpioFeatures feature)
{
    assert(this);
    assert(feature >= 0);
    assert(feature < NUM_GPIO_FEATURES);

    return GpioControl_Control(this, feature, this->previousState[feature], 0);
}

static void GpioControl_LeftEyeTimeoutEventHandler(void* arg)
{
    GpioControl *this = (GpioControl*)arg;
    assert(this);
    GpioControl_TimeoutEventHandlerAction(this, GPIO_FEATURE_LEFT_EYE);
}

static void GpioControl_RightEyeTimeoutEventHandler(void* arg)
{
    GpioControl *this = (GpioControl*)arg;
    assert(this);
    GpioControl_TimeoutEventHandlerAction(this, GPIO_FEATURE_RIGHT_EYE);
}

static void GpioControl_VibrationTimeoutEventHandler(void* arg)
{
    GpioControl *this = (GpioControl*)arg;
    assert(this);
    GpioControl_TimeoutEventHandlerAction(this, GPIO_FEATURE_VIBRATION);
}

// static void GpioControl_PiezoTimeoutEventHandler(void* arg)
// {
//     GpioControl *this = (GpioControl*)arg;
//     assert(this);
//     GpioControl_TimeoutEventHandlerAction(this, GPIO_FEATURE_PIEZO);
// }
