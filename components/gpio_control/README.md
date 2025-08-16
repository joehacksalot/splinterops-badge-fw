# GPIO Control Component

A GPIO control component for managing badge hardware features with timer-based automatic shutoff functionality.

## Overview

This component provides centralized control for various GPIO-based hardware features on the badge, including LED eyes and vibration motor. It includes automatic timeout functionality to prevent hardware from being left on indefinitely.

## Features

- **LED Eye Control**: Independent control of left and right eye LEDs
- **Vibration Motor Control**: Control of haptic feedback motor
- **Timer-based Auto-shutoff**: Automatic GPIO state reset after specified duration
- **State Management**: Tracks previous states and timer status for each feature
- **Thread-safe Timers**: Uses ESP-IDF timer system for reliable timeout handling

## Hardware Configuration

The component is configured for the following GPIO pins:

```c
#define GPIO_LEFT_EYE      22    // Left eye LED
#define GPIO_RIGHT_EYE     21    // Right eye LED  
#define GPIO_VIBRATION     19    // Vibration motor
```

## Files

- `GpioControl.h` - Component interface and data structures
- `GpioControl.c` - Implementation with timer management and GPIO control

## Usage

### Initialization

```c
#include "GpioControl.h"

GpioControl gpioControl;

// Initialize the GPIO control system
esp_err_t ret = GpioControl_Init(&gpioControl);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize GPIO control");
}
```

### Controlling GPIO Features

```c
// Turn on left eye for 5 seconds
GpioControl_Control(&gpioControl, GPIO_FEATURE_LEFT_EYE, true, 5000);

// Turn on right eye for 3 seconds
GpioControl_Control(&gpioControl, GPIO_FEATURE_RIGHT_EYE, true, 3000);

// Activate vibration for 1 second
GpioControl_Control(&gpioControl, GPIO_FEATURE_VIBRATION, true, 1000);

// Turn off immediately (duration = 0)
GpioControl_Control(&gpioControl, GPIO_FEATURE_LEFT_EYE, false, 0);
```

### Available GPIO Features

```c
typedef enum GpioFeatures_e {
    GPIO_FEATURE_LEFT_EYE = 0,    // Left eye LED
    GPIO_FEATURE_RIGHT_EYE,       // Right eye LED
    GPIO_FEATURE_VIBRATION,       // Vibration motor
    NUM_GPIO_FEATURES
} GpioFeatures;
```

## API Reference

### Functions

#### `GpioControl_Init(GpioControl *this)`
Initializes the GPIO control system, configures GPIO pins, and creates timers for each feature.

**Parameters:**
- `this`: Pointer to GpioControl structure

**Returns:** 
- `ESP_OK` on success
- ESP error code on failure

#### `GpioControl_Control(GpioControl *this, GpioFeatures feature, bool state, uint32_t durationMs)`
Controls a specific GPIO feature with optional automatic timeout.

**Parameters:**
- `this`: Pointer to GpioControl structure
- `feature`: GPIO feature to control (from GpioFeatures enum)
- `state`: Target state (true = on, false = off)
- `durationMs`: Duration in milliseconds (0 = immediate, >0 = auto-shutoff after duration)

**Returns:**
- `ESP_OK` on success
- ESP error code on failure

### Data Structures

#### `GpioControl`
Main control structure containing:
- `previousState[]`: Previous state for each GPIO feature
- `timerRunning[]`: Timer status for each feature
- `timerHandleArgs[]`: Timer configuration for each feature
- `timers[]`: ESP timer handles for each feature

## Timer Behavior

- When `durationMs > 0`: GPIO is set to the specified state and automatically reverted after the timeout
- When `durationMs = 0`: GPIO is set immediately without timeout
- Timers are automatically managed and cleaned up when features are turned off
- Each feature has its own independent timer to allow concurrent control

## Dependencies

- ESP-IDF `driver` component for GPIO control
- ESP-IDF `esp_timer` component for timeout functionality  
- ESP-IDF `log` component for logging

## Integration

To use this component in your project, include it in your main component's CMakeLists.txt:

```cmake
idf_component_register(
    # ... your sources
    REQUIRES gpio_control
)
```

Then include the header in your code:

```c
#include "GpioControl.h"
```

## Thread Safety

The component uses ESP-IDF timers which are thread-safe. However, the GpioControl structure itself is not protected by mutexes, so external synchronization may be needed in multi-threaded applications.
