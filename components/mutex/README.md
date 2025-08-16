# Mutex Component

A FreeRTOS mutex wrapper component providing simplified mutex operations for thread synchronization.

## Overview

This component provides a clean wrapper around FreeRTOS mutex functionality, offering simplified creation, locking, unlocking, and cleanup operations with proper error handling and logging.

## Features

- **Simplified API**: Easy-to-use wrapper functions for FreeRTOS mutex operations
- **Error Handling**: Proper ESP error code returns for all operations
- **Logging**: Built-in logging for mutex operations and error conditions
- **Thread Safety**: Full FreeRTOS mutex functionality for critical section protection

## Files

- `Mutex.h` - Component interface and function declarations
- `Mutex.c` - Implementation with FreeRTOS mutex wrapper functions

## Usage

### Include Header

```c
#include "Mutex.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
```

### Creating a Mutex

```c
SemaphoreHandle_t myMutex = NULL;

// Create the mutex
esp_err_t ret = Mutex_Create(&myMutex);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create mutex");
}
```

### Locking and Unlocking

```c
// Lock the mutex (wait indefinitely)
esp_err_t ret = Mutex_Lock(&myMutex, portMAX_DELAY);
if (ret == ESP_OK) {
    // Critical section - protected code here
    
    // Unlock the mutex
    Mutex_Unlock(&myMutex);
}

// Lock with timeout
ret = Mutex_Lock(&myMutex, pdMS_TO_TICKS(1000)); // 1 second timeout
if (ret == ESP_OK) {
    // Got the lock within timeout
    Mutex_Unlock(&myMutex);
} else {
    ESP_LOGW(TAG, "Failed to acquire mutex within timeout");
}
```

### Cleanup

```c
// Free the mutex when done
esp_err_t ret = Mutex_Free(&myMutex);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to free mutex");
}
```

## API Reference

### Functions

#### `Mutex_Create(SemaphoreHandle_t *pMutex)`
Creates a new FreeRTOS mutex.

**Parameters:**
- `pMutex`: Pointer to mutex handle (should be NULL initially)

**Returns:**
- `ESP_OK` on success
- `ESP_FAIL` if creation failed
- `ESP_ERR_INVALID_STATE` if mutex already exists

#### `Mutex_Lock(SemaphoreHandle_t *pMutex, TickType_t xTicksToWait)`
Attempts to acquire the mutex lock.

**Parameters:**
- `pMutex`: Pointer to mutex handle
- `xTicksToWait`: Maximum time to wait for the mutex (use `portMAX_DELAY` to wait indefinitely)

**Returns:**
- `ESP_OK` on successful lock acquisition
- `ESP_FAIL` if lock acquisition failed or timed out
- `ESP_ERR_INVALID_ARG` if mutex handle is NULL

#### `Mutex_Unlock(SemaphoreHandle_t *pMutex)`
Releases the mutex lock.

**Parameters:**
- `pMutex`: Pointer to mutex handle

**Returns:**
- `ESP_OK` on successful unlock
- `ESP_FAIL` if unlock failed
- `ESP_ERR_INVALID_ARG` if mutex handle is NULL

#### `Mutex_Free(SemaphoreHandle_t *pMutex)`
Deletes the mutex and frees associated resources.

**Parameters:**
- `pMutex`: Pointer to mutex handle

**Returns:**
- `ESP_OK` on successful deletion
- `ESP_ERR_INVALID_ARG` if mutex handle is NULL

## Usage Patterns

### Basic Critical Section Protection

```c
SemaphoreHandle_t resourceMutex = NULL;
Mutex_Create(&resourceMutex);

void protected_function() {
    if (Mutex_Lock(&resourceMutex, portMAX_DELAY) == ESP_OK) {
        // Access shared resource safely
        shared_resource++;
        
        Mutex_Unlock(&resourceMutex);
    }
}
```

### Timeout-based Locking

```c
void non_blocking_function() {
    if (Mutex_Lock(&resourceMutex, 0) == ESP_OK) {
        // Got lock immediately
        // Do quick operation
        Mutex_Unlock(&resourceMutex);
    } else {
        // Couldn't get lock, do alternative action
        ESP_LOGW(TAG, "Resource busy, skipping operation");
    }
}
```

## Dependencies

- ESP-IDF `freertos` component for mutex functionality
- ESP-IDF `log` component for logging

## Integration

To use this component in your project, include it in your component's CMakeLists.txt:

```cmake
idf_component_register(
    # ... your sources
    REQUIRES mutex
)
```

## Thread Safety

This component provides thread-safe mutex operations as it wraps FreeRTOS mutex functionality. The mutex operations themselves are atomic and thread-safe.
