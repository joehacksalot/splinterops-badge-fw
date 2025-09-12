#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
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

typedef struct CircularBufferIterator_t
{
    CircularBuffer *pBuffer;  // pointer to the circular buffer
    void *pCurrent;           // pointer to current item
    size_t index;             // current index in iteration
} CircularBufferIterator_t;

esp_err_t CircularBuffer_Init(CircularBuffer *cb, size_t capacity, size_t size);
void CircularBuffer_Free(CircularBuffer *cb);
void CircularBuffer_Clear(CircularBuffer *cb);
int CircularBuffer_Count(CircularBuffer *cb);
esp_err_t CircularBuffer_PushBack(CircularBuffer *cb, const void *item);
esp_err_t CircularBuffer_PopFront(CircularBuffer *cb, void *item);
esp_err_t CircularBuffer_MatchSequence(CircularBuffer *cb, const void *sequence, size_t sequenceLength);

/* Iterator functions */
void CircularBuffer_InitIterator(CircularBuffer *cb, CircularBufferIterator_t *iterator);
bool CircularBuffer_HasNext(CircularBufferIterator_t *iterator);
esp_err_t CircularBuffer_GetNext(CircularBufferIterator_t *iterator, void *item);
esp_err_t CircularBuffer_PeekAt(CircularBuffer *cb, size_t index, void *item);

#endif // CIRCULAR_BUFFER_H_