/**
  * @file CircularBuffer.c
  * @brief Generic fixed-size circular buffer utilities for arbitrary element types.
  *
  * This module implements a simple circular buffer that stores a fixed capacity
  * of elements of uniform size. Operations provided include initialization,
  * push-back, pop-front, clear, count, and sequence matching of the last N
  * elements. Memory for the buffer is allocated at initialization and freed
  * during cleanup.
  *
  * Notes
  * - Not thread-safe. Protect with a mutex if accessed from multiple tasks.
  * - Buffer is considered full when count == capacity; further pushes fail.
  * - All operations are O(1) except sequence matching which is O(N).
  * - Logging uses ESP-IDF logging macros.
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CircularBuffer.h"

static const char *TAG = "CBUF";

/**
 * Initializes a circular pBuffer.
 *
 * @param cb Pointer to the circular pBuffer structure to be initialized.
 * @param capacity The maximum number of elements that the circular pBuffer can hold.
 * @param size The size of each element in the circular pBuffer.
 *
 * @return ESP_OK if the circular pBuffer is successfully initialized, ESP_FAIL otherwise.
 *
 * @throws None
 */
esp_err_t CircularBuffer_Init(CircularBuffer *cb, size_t capacity, size_t size)
{
  cb->pBuffer = malloc(capacity * size);
  if(cb->pBuffer == NULL)
  {
    ESP_LOGE(TAG, "Could not allocate circular pBuffer");
    return ESP_FAIL;
  }
  cb->pBufferEnd = (char *)cb->pBuffer + capacity * size;
  cb->capacity = capacity;
  cb->count = 0;
  cb->size = size;
  cb->pHead = cb->pBuffer;
  cb->pTail = cb->pBuffer;
  return ESP_OK;
}

/**
 * Frees the memory allocated for the circular pBuffer.
 *
 * @param cb Pointer to the circular pBuffer structure.
 *
 * @throws None
 */
void CircularBuffer_Free(CircularBuffer *cb)
{
  free(cb->pBuffer);
  memset(cb, 0, sizeof(*cb));
}

/**
 * Clears the circular buffer and resets size back to 0.
 *
 * @param cb Pointer to the circular buffer structure.
 *
 * @throws None
 */
void CircularBuffer_Clear(CircularBuffer *cb)
{
  cb->count = 0;
  cb->pHead = cb->pBuffer;
  cb->pTail = cb->pBuffer;
}

/**
 * Returns the number of elements in the circular buffer.
 *
 * @param cb Pointer to the circular buffer structure.
 *
 * @return The number of elements in the circular buffer.
 *
 * @throws None
 */
int CircularBuffer_Count(CircularBuffer *cb)
{
  return cb->count;
}

/**
 * Pushes an item to the back of the circular pBuffer.
 * 
 * @param cb Pointer to the circular pBuffer structure.
 * @param item Pointer to the item to be pushed into the circular pBuffer.
 * 
 * @return ESP_OK if the item was successfully pushed, ESP_FAIL if the circular pBuffer is full.
 * 
 * @throws None
 */
esp_err_t CircularBuffer_PushBack(CircularBuffer *cb, const void *item)
{
  if(cb->count == cb->capacity)
  {
    ESP_LOGE(TAG, "Circular pBuffer is full");
    return ESP_FAIL;
  }

  memcpy(cb->pHead, item, cb->size);
  cb->pHead = (char*)cb->pHead + cb->size;
  if(cb->pHead == cb->pBufferEnd)
  {
    cb->pHead = cb->pBuffer;
  }

  cb->count++;
  return ESP_OK;
}

/**
 * Pop an item from the front of the circular pBuffer.
 *
 * @param cb Pointer to the circular pBuffer structure.
 * @param item Pointer to the destination where the popped item will be copied.
 *
 * @return ESP_OK if the item was successfully popped, ESP_FAIL if the circular pBuffer is empty.
 *
 * @throws None.
 */
esp_err_t CircularBuffer_PopFront(CircularBuffer *cb, void *item)
{
  if(cb->count == 0)
  {
    ESP_LOGE(TAG, "Circular pBuffer is empty");
    return ESP_FAIL;
  }

  memcpy(item, cb->pTail, cb->size);
  cb->pTail = (char*)cb->pTail + cb->size;
  if(cb->pTail == cb->pBufferEnd)
  {
    cb->pTail = cb->pBuffer;
  }

  cb->count--;
  return ESP_OK;
}

/**
 * Matches the last N elements of the sequence in the circular buffer.
 * 
 * @param cb Pointer to the circular buffer structure.
 * @param sequence Pointer to the sequence to be matched.
 * @param sequence_length Length of the sequence.
 * 
 * @return ESP_OK if the sequence is found at the end, ESP_FAIL otherwise.
 */
esp_err_t CircularBuffer_MatchSequence(CircularBuffer *cb, const void *sequence, size_t sequence_length)
{
  if(sequence_length > cb->count)
  {
    ESP_LOGE(TAG, "Sequence length is greater than the number of elements in the circular buffer");
    return ESP_FAIL;
  }

  // Pointer to the current position in the circular buffer
  const char *current = (const char *)cb->pTail + (cb->count - sequence_length) * cb->size;
  const char *seq = (const char *)sequence;

  // Wrap around if we reach the end of the buffer
  if(current >= (const char *)cb->pBufferEnd)
  {
    current -= (cb->pBufferEnd - cb->pBuffer);
  }

  for(size_t i = 0; i < sequence_length; i++)
  {
    if(memcmp(current, seq, cb->size) != 0)
    {
      return ESP_FAIL;
    }

    current = (const char *)current + cb->size;
    seq += cb->size;

    // Wrap around if we reach the end of the buffer
    if(current == cb->pBufferEnd)
    {
      current = cb->pBuffer;
    }
  }

  return ESP_OK;
}