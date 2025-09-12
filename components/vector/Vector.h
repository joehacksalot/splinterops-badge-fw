#ifndef VECTOR_H
#define VECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief Function type for comparing vector elements for equality
 * 
 * @param a First element to compare
 * @param b Second element to compare
 * @return bool true if elements are equal, false otherwise
 */
typedef bool (*VectorEqualityFn)(const void* a, const void* b);

/**
 * @brief Maximum capacity for statically allocated vectors
 */
#define VECTOR_MAX_STATIC_CAPACITY 64

/**
 * @brief Vector structure that mimics std::vector functionality
 * 
 * This implementation supports both static and dynamic allocation modes.
 * In static mode, the vector uses a pre-allocated buffer with a fixed capacity.
 * In dynamic mode, the vector uses heap allocation and can grow as needed.
 */
typedef struct Vector_t
{
    void* pData;                  // Pointer to the data storage
    uint32_t elementSize;         // Size of each element in bytes
    uint32_t size;                // Current number of elements
    uint32_t capacity;            // Maximum number of elements
    bool isStatic;                // Whether using static or dynamic allocation
    uint8_t staticBuffer[];       // Flexible array member for static allocation
} Vector;

/**
 * @brief Initialize a vector with dynamic memory allocation
 * 
 * @param this Pointer to the vector to initialize
 * @param elementSize Size of each element in bytes
 * @param initialCapacity Initial capacity of the vector
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t Vector_Init(Vector* this, uint32_t elementSize, uint32_t initialCapacity);

/**
 * @brief Initialize a vector with static memory allocation
 * 
 * @param this Pointer to the vector to initialize
 * @param elementSize Size of each element in bytes
 * @param staticBuffer Pointer to the static buffer
 * @param staticCapacity Capacity of the static buffer
 * @return esp_err_t ESP_OK on success
 */
esp_err_t Vector_InitStatic(Vector* this, uint32_t elementSize, void* staticBuffer, uint32_t staticCapacity);

/**
 * @brief Free resources used by the vector
 * 
 * @param this Pointer to the vector
 */
void Vector_Deinit(Vector* this);

/**
 * @brief Get the current size of the vector
 * 
 * @param this Pointer to the vector
 * @return uint32_t Number of elements in the vector
 */
uint32_t Vector_Size(const Vector* this);

/**
 * @brief Get the current capacity of the vector
 * 
 * @param this Pointer to the vector
 * @return uint32_t Maximum number of elements the vector can hold without resizing
 */
uint32_t Vector_Capacity(const Vector* this);

/**
 * @brief Check if the vector is empty
 * 
 * @param this Pointer to the vector
 * @return bool true if empty, false otherwise
 */
bool Vector_Empty(const Vector* this);

/**
 * @brief Reserve capacity for at least the specified number of elements
 * 
 * @param this Pointer to the vector
 * @param newCapacity The new capacity
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if memory allocation fails,
 *                   ESP_ERR_INVALID_STATE if vector is static and newCapacity exceeds static capacity
 */
esp_err_t Vector_Reserve(Vector* this, uint32_t newCapacity);

/**
 * @brief Resize the vector to contain count elements
 * 
 * @param this Pointer to the vector
 * @param count New size of the vector
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if memory allocation fails,
 *                   ESP_ERR_INVALID_STATE if vector is static and count exceeds static capacity
 */
esp_err_t Vector_Resize(Vector* this, uint32_t count);

/**
 * @brief Shrink the capacity to fit the size
 * 
 * @param this Pointer to the vector
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t Vector_ShrinkToFit(Vector* this);

/**
 * @brief Get a pointer to the element at the specified index
 * 
 * @param this Pointer to the vector
 * @param index Index of the element
 * @return void* Pointer to the element, NULL if index is out of bounds
 */
void* Vector_At(const Vector* this, uint32_t index);

/**
 * @brief Get a pointer to the first element
 * 
 * @param this Pointer to the vector
 * @return void* Pointer to the first element, NULL if vector is empty
 */
void* Vector_Front(const Vector* this);

/**
 * @brief Get a pointer to the last element
 * 
 * @param this Pointer to the vector
 * @return void* Pointer to the last element, NULL if vector is empty
 */
void* Vector_Back(const Vector* this);

/**
 * @brief Get a pointer to the underlying data array
 * 
 * @param this Pointer to the vector
 * @return void* Pointer to the data array
 */
void* Vector_Data(const Vector* this);

/**
 * @brief Add an element to the end of the vector
 * 
 * @param this Pointer to the vector
 * @param element Pointer to the element to add
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if memory allocation fails,
 *                   ESP_ERR_INVALID_STATE if vector is static and at capacity
 */
esp_err_t Vector_PushBack(Vector* this, const void* element);

/**
 * @brief Remove the last element from the vector
 * 
 * @param this Pointer to the vector
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_STATE if vector is empty
 */
esp_err_t Vector_PopBack(Vector* this);

/**
 * @brief Remove the first element from the vector and copy it to the provided buffer
 * 
 * @param this Pointer to the vector
 * @param pElement Pointer to buffer where the element will be copied
 * @param elementSize Size of the element in bytes
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_STATE if vector is empty
 */
esp_err_t Vector_PopFront(Vector* this, void* pElement, uint32_t elementSize);

/**
 * @brief Insert an element at the specified position
 * 
 * @param this Pointer to the vector
 * @param index Position where the element should be inserted
 * @param element Pointer to the element to insert
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if memory allocation fails,
 *                   ESP_ERR_INVALID_ARG if index is out of bounds,
 *                   ESP_ERR_INVALID_STATE if vector is static and at capacity
 */
esp_err_t Vector_Insert(Vector* this, uint32_t index, const void* element);

/**
 * @brief Erase an element at the specified position
 * 
 * @param this Pointer to the vector
 * @param index Position of the element to erase
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if index is out of bounds
 */
esp_err_t Vector_Erase(Vector* this, uint32_t index);

/**
 * @brief Remove an element at the specified index
 * 
 * @param this Pointer to the vector
 * @param index Index of the element to remove
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if index is out of bounds
 */
esp_err_t Vector_RemoveAt(Vector* this, uint32_t index);

/**
 * @brief Remove the first element that matches the provided element
 * 
 * @param this Pointer to the vector
 * @param element Pointer to the element to match against
 * @param equalityFn Function to compare elements for equality
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if no matching element is found
 */
esp_err_t Vector_Remove(Vector* this, const void* element, VectorEqualityFn equalityFn);

/**
 * @brief Clear all elements from the vector
 * 
 * @param this Pointer to the vector
 */
void Vector_Clear(Vector* this);

/**
 * @brief Swap the contents of two vectors
 * 
 * @param this First vector
 * @param other Second vector
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if element sizes don't match
 */
esp_err_t Vector_Swap(Vector* this, Vector* other);

/**
 * @brief Assign count copies of element to the vector
 * 
 * @param this Pointer to the vector
 * @param count Number of copies
 * @param element Pointer to the element to copy
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if memory allocation fails,
 *                   ESP_ERR_INVALID_STATE if vector is static and count exceeds static capacity
 */
esp_err_t Vector_Assign(Vector* this, uint32_t count, const void* element);

/**
 * @brief Emplace an element at the end of the vector
 * This is similar to push_back but constructs the element in-place
 * 
 * @param this Pointer to the vector
 * @param element Pointer to the element to emplace
 * @return esp_err_t ESP_OK on success, ESP_ERR_NO_MEM if memory allocation fails,
 *                   ESP_ERR_INVALID_STATE if vector is static and at capacity
 */
esp_err_t Vector_EmplaceBack(Vector* this, const void* element);

#endif // VECTOR_H
