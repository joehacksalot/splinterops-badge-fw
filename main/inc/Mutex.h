/**
 * @file Mutex.h
 * @brief Thread-safe mutex operations wrapper for FreeRTOS semaphores
 * 
 * This module provides a simplified interface for mutex operations including:
 * - Mutex creation and initialization
 * - Thread-safe mutex locking with timeout support
 * - Mutex unlocking operations
 * - Proper mutex cleanup and resource management
 * - ESP-IDF error code integration
 * - FreeRTOS semaphore abstraction layer
 * 
 * The mutex wrapper ensures consistent error handling and provides
 * a clean API for thread synchronization across the badge system.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef MUTEX_JOSE_H_
#define MUTEX_JOSE_H_

/**
 * @brief Create and initialize a new mutex
 * 
 * Creates a FreeRTOS mutex semaphore for thread synchronization.
 * The mutex must be freed using Mutex_Free() when no longer needed.
 * 
 * @param pMutex Pointer to store the created mutex handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t Mutex_Create(SemaphoreHandle_t *pMutex);

/**
 * @brief Free and cleanup a mutex
 * 
 * Releases all resources associated with the mutex. The mutex
 * should not be used after calling this function.
 * 
 * @param pMutex Pointer to the mutex handle to free
 * @return ESP_OK on success, error code on failure
 */
esp_err_t Mutex_Free(SemaphoreHandle_t *pMutex);

/**
 * @brief Lock a mutex with timeout
 * 
 * Attempts to acquire the mutex lock. If the mutex is already locked
 * by another task, this function will block until the timeout expires
 * or the mutex becomes available.
 * 
 * @param pMutex Pointer to the mutex handle to lock
 * @param xTicksToWait Maximum time to wait for the mutex (in FreeRTOS ticks)
 * @return ESP_OK on successful lock, error code on timeout or failure
 */
esp_err_t Mutex_Lock(SemaphoreHandle_t *pMutex, TickType_t xTicksToWait);

/**
 * @brief Unlock a previously locked mutex
 * 
 * Releases the mutex lock, allowing other waiting tasks to acquire it.
 * This function should only be called by the task that currently holds the lock.
 * 
 * @param pMutex Pointer to the mutex handle to unlock
 * @return ESP_OK on success, error code on failure
 */
esp_err_t Mutex_Unlock(SemaphoreHandle_t *pMutex);

#endif // MUTEX_JOSE_H_
