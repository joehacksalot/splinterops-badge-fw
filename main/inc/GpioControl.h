#ifndef GPIOCONTROL_H_
#define GPIOCONTROL_H_

#include "esp_err.h"
#include "esp_timer.h"

typedef enum GpioFeatures_e
{
    GPIO_FEATURE_LEFT_EYE = 0,
    GPIO_FEATURE_RIGHT_EYE,
    GPIO_FEATURE_VIBRATION,
    // GPIO_FEATURE_PIEZO,
    NUM_GPIO_FEATURES
} GpioFeatures;

typedef struct GpioControl_t
{
    int previousState[NUM_GPIO_FEATURES];
    int timerRunning[NUM_GPIO_FEATURES];
    esp_timer_create_args_t timerHandleArgs[NUM_GPIO_FEATURES];
    esp_timer_handle_t timers[NUM_GPIO_FEATURES];
} GpioControl;

esp_err_t GpioControl_Init(GpioControl *this);
esp_err_t GpioControl_Control(GpioControl *this, GpioFeatures feature, bool state, uint32_t durationMs);


#endif // GPIOCONTROL_H_
