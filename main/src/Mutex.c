/**
 * @file Mutex.c
 * @brief FreeRTOS mutex wrapper for thread synchronization
 * 
 * This module provides a simplified wrapper around FreeRTOS mutexes,
 * offering a consistent API for creating, locking, and unlocking
 * binary semaphores for thread-safe access to shared resources.
 * 
 * ## Key Features:
 * - **Thread-safe access**: Ensures exclusive access to critical sections
 * - **Resource protection**: Prevents data corruption in multi-threaded environments
 * - **Timeout support**: Configurable wait times for mutex acquisition
 * - **Error handling**: Robust error reporting for mutex operations
 * - **Simplified API**: Abstracts FreeRTOS mutex complexities
 * 
 * ## Implementation Details:
 * - Uses FreeRTOS `xSemaphoreCreateMutex()` for mutex creation
 * - Employs `xSemaphoreTake()` for locking with optional timeouts
 * - Utilizes `xSemaphoreGive()` for unlocking the mutex
 * - Provides `vSemaphoreDelete()` for proper resource cleanup
 * - Integrates ESP-IDF logging for operational status and errors
 * 
 * ## Usage Patterns:
 * - Call `Mutex_Create()` once to initialize a mutex before use.
 * - Use `Mutex_Lock()` before accessing shared resources.
 * - Call `Mutex_Unlock()` after finishing with shared resources.
 * - Call `Mutex_Free()` to release mutex resources when no longer needed.
 * - Always check return values for error handling.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"

#include "Mutex.h"

static const char * TAG = "MUT";

/**
 * @brief Creates a FreeRTOS mutex.
 * 
 * This function attempts to create a FreeRTOS mutex. If the mutex pointer
 * is already non-NULL, it indicates the mutex has already been created,
 * and an error is returned. Otherwise, a new mutex is created.
 * 
 * @param pMutex Pointer to a `SemaphoreHandle_t` where the created mutex handle will be stored.
 * @return `ESP_OK` if the mutex was successfully created.
 * @return `ESP_FAIL` if the mutex could not be created (e.g., out of memory) or if it already exists.
 */
esp_err_t Mutex_Create(SemaphoreHandle_t *pMutex)
{
    if(*pMutex == NULL)
    {
        *pMutex = xSemaphoreCreateMutex();

        if(*pMutex)
        {
            ESP_LOGI(TAG, "Mutex created");
            return ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "Mutex failed to create");
            return ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Mutex already created");
        return ESP_FAIL;
    }
}

/**
 * @brief Deletes a FreeRTOS mutex.
 * 
 * This function attempts to delete an existing FreeRTOS mutex and frees
 * any associated memory. If the mutex pointer is NULL, it indicates the
 * mutex has not been created or has already been freed.
 * 
 * @param pMutex Pointer to a `SemaphoreHandle_t` holding the mutex handle to be deleted.
 * @return `ESP_OK` if the mutex was successfully deleted.
 * @return `ESP_FAIL` if the mutex could not be deleted (e.g., not created).
 */
esp_err_t Mutex_Free(SemaphoreHandle_t *pMutex)
{
    if(*pMutex != NULL)
    {
        vSemaphoreDelete(*pMutex);
        *pMutex = NULL;
        ESP_LOGI(TAG, "Mutex deleted");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Mutex unable to free. not created");
    }
    return ESP_FAIL;
}


/**
 * @brief Locks a FreeRTOS mutex.
 * 
 * This function attempts to obtain a lock on the specified mutex. If the mutex
 * is not available, the calling task will block for a maximum of `xTicksToWait`
 * FreeRTOS ticks. If `xTicksToWait` is 0, the function will return immediately
 * if the mutex is not available.
 * 
 * @param pMutex Pointer to a `SemaphoreHandle_t` holding the mutex handle to lock.
 * @param xTicksToWait The maximum number of ticks to wait for the mutex to become available.
 *                     Use `portMAX_DELAY` to wait indefinitely.
 * @return `ESP_OK` if the mutex was successfully locked.
 * @return `ESP_FAIL` if the mutex could not be locked within the specified timeout.
 */
esp_err_t Mutex_Lock(SemaphoreHandle_t *pMutex, TickType_t xTicksToWait)
{
    if (*pMutex != NULL)
    {
        if (xSemaphoreTake(*pMutex, xTicksToWait) == pdTRUE) 
        {
            ESP_LOGI(TAG, "Mutex locked");
            return ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "Mutex failed to lock");
            return ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Mutex unable to lock. not created");
        return ESP_FAIL;
    }
}

/**
 * @brief Unlocks a FreeRTOS mutex.
 * 
 * This function releases a lock on the specified mutex, making it available
 * for other tasks to acquire. It should only be called by the task that
 * currently holds the mutex.
 * 
 * @param pMutex Pointer to a `SemaphoreHandle_t` holding the mutex handle to unlock.
 * @return `ESP_OK` if the mutex was successfully unlocked.
 * @return `ESP_FAIL` if the mutex could not be unlocked (e.g., not held by the calling task).
 */
esp_err_t Mutex_Unlock(SemaphoreHandle_t *pMutex)
{
    if (xSemaphoreGive(*pMutex) == pdTRUE) 
    {
        ESP_LOGI(TAG, "Mutex unlocked");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Mutex failed to unlock");
        return ESP_FAIL;
    }
}
