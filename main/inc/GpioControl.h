/**
 * @file GpioControl.h
 * @brief GPIO control system for badge hardware features
 * 
 * This module provides GPIO control functionality for badge hardware features including:
 * - Left and right eye LED control
 * - Vibration motor control
 * - Timer-based GPIO state management
 * - Duration-based feature activation
 * - Hardware abstraction for different badge variants
 * - Thread-safe GPIO operations
 * 
 * The GPIO control system manages physical hardware features that provide
 * visual and tactile feedback to enhance the badge user experience.
 * 
 * @author Badge Development Team
 * @date 2024
 */

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

/**
 * @brief Initialize the GPIO control system
 * 
 * Initializes GPIO pins and hardware features including vibration motor,
 * buzzer, and other badge hardware components. Sets up ESP timers for
 * timed GPIO operations and configures pin modes.
 * 
 * @param this Pointer to GpioControl instance to initialize
 * @return ESP_OK on success, error code on failure
 */
esp_err_t GpioControl_Init(GpioControl *this);

/**
 * @brief Control GPIO-based hardware features
 * 
 * Controls various GPIO-based hardware features with optional duration.
 * Supports features like vibration motor, buzzer, and other actuators
 * with precise timing control using ESP timers.
 * 
 * @param this Pointer to GpioControl instance
 * @param feature GPIO feature to control (vibration, buzzer, etc.)
 * @param state True to enable, false to disable the feature
 * @param durationMs Duration in milliseconds (0 for indefinite)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t GpioControl_Control(GpioControl *this, GpioFeatures feature, bool state, uint32_t durationMs);


#endif // GPIOCONTROL_H_
