
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

// Pushes an item to the back of the circular pBuffer.
// 
// @param cb Pointer to the circular pBuffer structure.
// @param item Pointer to the item to be pushed into the circular pBuffer.
// 
// @return ESP_OK if the item was successfully pushed, ESP_FAIL if the circular pBuffer is full.
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