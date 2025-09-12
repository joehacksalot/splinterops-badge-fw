
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
 * @return void
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
 * @return void
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

/**
 * Initializes an iterator for the circular buffer.
 *
 * @param cb Pointer to the circular buffer structure.
 * @param iterator Pointer to the iterator structure to be initialized.
 *
 * @return void
 *
 * @throws None
 */
void CircularBuffer_InitIterator(CircularBuffer *cb, CircularBufferIterator_t *iterator)
{
  assert(cb != NULL);
  assert(iterator != NULL);
  
  iterator->pBuffer = cb;
  iterator->pCurrent = cb->pTail;
  iterator->index = 0;
}

/**
 * Checks if there are more elements to iterate over.
 *
 * @param iterator Pointer to the iterator structure.
 *
 * @return true if there are more elements, false otherwise.
 *
 * @throws None
 */
bool CircularBuffer_HasNext(CircularBufferIterator_t *iterator)
{
  assert(iterator != NULL);
  assert(iterator->pBuffer != NULL);
  
  return iterator->index < iterator->pBuffer->count;
}

/**
 * Gets the next element from the iterator and advances the iterator.
 *
 * @param iterator Pointer to the iterator structure.
 * @param item Pointer to the destination where the item will be copied.
 *
 * @return ESP_OK if the item was successfully retrieved, ESP_FAIL if there are no more items.
 *
 * @throws None
 */
esp_err_t CircularBuffer_GetNext(CircularBufferIterator_t *iterator, void *item)
{
  assert(iterator != NULL);
  assert(iterator->pBuffer != NULL);
  assert(item != NULL);
  
  if (!CircularBuffer_HasNext(iterator))
  {
    ESP_LOGE(TAG, "No more elements in iterator");
    return ESP_FAIL;
  }
  
  // Copy the current item
  memcpy(item, iterator->pCurrent, iterator->pBuffer->size);
  
  // Advance the iterator
  iterator->pCurrent = (char*)iterator->pCurrent + iterator->pBuffer->size;
  if (iterator->pCurrent == iterator->pBuffer->pBufferEnd)
  {
    iterator->pCurrent = iterator->pBuffer->pBuffer;
  }
  
  iterator->index++;
  return ESP_OK;
}

/**
 * Peeks at an element at a specific index in the circular buffer without removing it.
 *
 * @param cb Pointer to the circular buffer structure.
 * @param index Index of the element to peek at (0 is the oldest element).
 * @param item Pointer to the destination where the item will be copied.
 *
 * @return ESP_OK if the item was successfully retrieved, ESP_FAIL if the index is out of bounds.
 *
 * @throws None
 */
esp_err_t CircularBuffer_PeekAt(CircularBuffer *cb, size_t index, void *item)
{
  assert(cb != NULL);
  assert(item != NULL);
  
  if (index >= cb->count)
  {
    ESP_LOGE(TAG, "Index %zu out of bounds (count: %zu)", index, cb->count);
    return ESP_FAIL;
  }
  
  // Calculate the position of the element
  void *position = (char*)cb->pTail + (index * cb->size);
  
  // Wrap around if necessary
  if (position >= cb->pBufferEnd)
  {
    position = (char*)cb->pBuffer + ((char*)position - (char*)cb->pBufferEnd);
  }
  
  // Copy the item
  memcpy(item, position, cb->size);
  return ESP_OK;
}