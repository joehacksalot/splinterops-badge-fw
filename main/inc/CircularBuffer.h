/**
 * @file CircularBuffer.h
 * @brief Generic circular buffer data structure implementation
 * 
 * This module provides a thread-safe circular buffer (ring buffer) implementation
 * that supports:
 * - Dynamic memory allocation with configurable capacity
 * - Generic data storage (any data type via void pointers)
 * - FIFO (First In, First Out) operations
 * - Sequence pattern matching for data analysis
 * - Overflow handling with automatic overwriting
 * - Memory management with proper cleanup
 * 
 * Used throughout the badge system for buffering touch sensor data,
 * audio samples, and other streaming data that requires efficient
 * circular storage with bounded memory usage.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

typedef struct CircularBuffer_t
{
    void *pBuffer;     // data pBuffer
    void *pBufferEnd;  // end of data pBuffer
    size_t capacity;   // maximum number of items in the pBuffer
    size_t count;      // number of items in the pBuffer
    size_t size;       // size of each item in the pBuffer
    void *pHead;       // pointer to head
    void *pTail;       // pointer to tail
} CircularBuffer;

/**
 * @brief Initialize a circular buffer
 * 
 * Allocates memory and initializes a circular buffer with the specified
 * capacity and element size. The buffer is thread-safe and supports
 * generic data types.
 * 
 * @param cb Pointer to CircularBuffer instance to initialize
 * @param capacity Maximum number of elements the buffer can hold
 * @param size Size in bytes of each element
 * @return ESP_OK on success, ESP_ERR_NO_MEM on allocation failure
 */
esp_err_t CircularBuffer_Init(CircularBuffer *cb, size_t capacity, size_t size);

/**
 * @brief Free circular buffer memory
 * 
 * Releases all memory allocated for the circular buffer and resets
 * the buffer structure to an uninitialized state.
 * 
 * @param cb Pointer to CircularBuffer instance to free
 */
void CircularBuffer_Free(CircularBuffer *cb);

/**
 * @brief Clear all elements from the circular buffer
 * 
 * Removes all elements from the buffer without deallocating memory.
 * The buffer capacity remains unchanged and can be reused.
 * 
 * @param cb Pointer to CircularBuffer instance to clear
 */
void CircularBuffer_Clear(CircularBuffer *cb);

/**
 * @brief Get the current number of elements in the buffer
 * 
 * Returns the current count of elements stored in the circular buffer.
 * This is a thread-safe operation.
 * 
 * @param cb Pointer to CircularBuffer instance
 * @return Number of elements currently in the buffer
 */
int CircularBuffer_Count(CircularBuffer *cb);

/**
 * @brief Add an element to the back of the buffer
 * 
 * Adds a new element to the tail of the circular buffer. If the buffer
 * is full, the oldest element is overwritten (FIFO behavior).
 * 
 * @param cb Pointer to CircularBuffer instance
 * @param item Pointer to the item to add
 * @return ESP_OK on success, error code on failure
 */
esp_err_t CircularBuffer_PushBack(CircularBuffer *cb, const void *item);

/**
 * @brief Remove an element from the front of the buffer
 * 
 * Removes and returns the oldest element from the head of the circular
 * buffer. The element is copied to the provided output buffer.
 * 
 * @param cb Pointer to CircularBuffer instance
 * @param item Pointer to buffer where the removed item will be copied
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if buffer is empty
 */
esp_err_t CircularBuffer_PopFront(CircularBuffer *cb, void *item);

/**
 * @brief Match a sequence pattern in the buffer
 * 
 * Searches for a specific sequence pattern within the circular buffer.
 * Used for pattern recognition in data streams such as musical sequences
 * or command patterns.
 * 
 * @param cb Pointer to CircularBuffer instance
 * @param sequence Pointer to the sequence pattern to match
 * @param sequenceLength Length of the sequence pattern in elements
 * @return ESP_OK if sequence found, ESP_ERR_NOT_FOUND if not found
 */
esp_err_t CircularBuffer_MatchSequence(CircularBuffer *cb, const void *sequence, size_t sequenceLength);

#endif // CIRCULAR_BUFFER_H_