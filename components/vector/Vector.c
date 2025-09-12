#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "esp_log.h"
#include "Vector.h"

static const char *TAG = "VEC";

// Helper function to calculate new capacity when growing
static uint32_t _Vector_CalculateGrowCapacity(uint32_t currentCapacity)
{
    return (currentCapacity == 0) ? 1 : currentCapacity * 2;
}

// Helper function to resize the internal buffer
static esp_err_t _Vector_Reallocate(Vector* this, uint32_t newCapacity)
{
    assert(this);
    
    if (this->isStatic)
    {
        if (newCapacity > this->capacity)
        {
            ESP_LOGE(TAG, "Cannot resize static vector beyond capacity %lu", this->capacity);
            return ESP_ERR_INVALID_STATE;
        }
        return ESP_OK;
    }
    else
    {
        void* newData = NULL;
        if (newCapacity > 0)
        {
            newData = malloc(newCapacity * this->elementSize);
            if (!newData)
            {
                ESP_LOGE(TAG, "Failed to allocate memory for vector");
                return ESP_ERR_NO_MEM;
            }
        
            // Copy existing elements if any
            if (this->size > 0 && this->pData)
            {
                uint32_t copySize = (this->size < newCapacity) ? this->size : newCapacity;
                memcpy(newData, this->pData, copySize * this->elementSize);
            }
        }
        this->pData = newData;
        this->capacity = newCapacity;
    }
    
    // Free old buffer if it was dynamically allocated
    if (this->pData && !this->isStatic)
    {
        free(this->pData);
    }
    
    // Adjust size if new capacity is smaller than current size
    if (this->size > newCapacity)
    {
        this->size = newCapacity;
    }
    
    return ESP_OK;
}

esp_err_t Vector_Init(Vector* this, uint32_t elementSize, uint32_t initialCapacity)
{
    assert(this);
    assert(elementSize > 0);
    
    memset(this, 0, sizeof(*this));
    this->elementSize = elementSize;
    this->isStatic = false;
    
    if (initialCapacity > 0)
    {
        return _Vector_Reallocate(this, initialCapacity);
    }
    
    return ESP_OK;
}

esp_err_t Vector_InitStatic(Vector* this, uint32_t elementSize, void* staticBuffer, uint32_t staticCapacity)
{
    assert(this);
    assert(elementSize > 0);
    assert(staticBuffer);
    assert(staticCapacity > 0);
    
    memset(this, 0, sizeof(*this));
    this->elementSize = elementSize;
    this->capacity = staticCapacity;
    this->isStatic = true;
    this->pData = staticBuffer;
    
    return ESP_OK;
}

void Vector_Deinit(Vector* this)
{
    assert(this);
    
    if (this->pData && !this->isStatic)
    {
        free(this->pData);
    }
    
    memset(this, 0, sizeof(*this));
}

uint32_t Vector_Size(const Vector* this)
{
    assert(this);
    return this->size;
}

uint32_t Vector_Capacity(const Vector* this)
{
    assert(this);
    return this->capacity;
}

bool Vector_Empty(const Vector* this)
{
    assert(this);
    return this->size == 0;
}

esp_err_t Vector_Reserve(Vector* this, uint32_t newCapacity)
{
    assert(this);
    
    if (newCapacity <= this->capacity)
    {
        return ESP_OK;
    }
    
    return _Vector_Reallocate(this, newCapacity);
}

esp_err_t Vector_Resize(Vector* this, uint32_t count)
{
    assert(this);
    
    if (count > this->capacity)
    {
        esp_err_t err = _Vector_Reallocate(this, count);
        if (err != ESP_OK)
        {
            return err;
        }
    }
    
    // If growing, initialize new elements to zero
    if (count > this->size)
    {
        memset((uint8_t*)this->pData + (this->size * this->elementSize), 0, 
               (count - this->size) * this->elementSize);
    }
    
    this->size = count;
    return ESP_OK;
}

esp_err_t Vector_ShrinkToFit(Vector* this)
{
    assert(this);
    
    if (this->isStatic || this->size == this->capacity)
    {
        return ESP_OK;
    }
    
    return _Vector_Reallocate(this, this->size);
}

void* Vector_At(const Vector* this, uint32_t index)
{
    assert(this);
    
    if (index >= this->size)
    {
        ESP_LOGE(TAG, "Index %lu out of bounds (size: %lu)", index, this->size);
        return NULL;
    }
    
    return (uint8_t*)this->pData + (index * this->elementSize);
}

void* Vector_Front(const Vector* this)
{
    assert(this);
    
    if (this->size == 0)
    {
        ESP_LOGE(TAG, "Cannot get front of empty vector");
        return NULL;
    }
    
    return this->pData;
}

void* Vector_Back(const Vector* this)
{
    assert(this);
    
    if (this->size == 0)
    {
        ESP_LOGE(TAG, "Cannot get back of empty vector");
        return NULL;
    }
    
    return (uint8_t*)this->pData + ((this->size - 1) * this->elementSize);
}

void* Vector_Data(const Vector* this)
{
    assert(this);
    return this->pData;
}

esp_err_t Vector_PushBack(Vector* this, const void* element)
{
    assert(this);
    assert(element);
    
    // Check if we need to grow the vector
    if (this->size == this->capacity)
    {
        uint32_t newCapacity = _Vector_CalculateGrowCapacity(this->capacity);
        esp_err_t err = _Vector_Reallocate(this, newCapacity);
        if (err != ESP_OK)
        {
            return err;
        }
    }
    
    // Copy the element to the end of the vector
    memcpy((uint8_t*)this->pData + (this->size * this->elementSize), element, this->elementSize);
    this->size++;
    
    return ESP_OK;
}

esp_err_t Vector_PopBack(Vector* this)
{
    assert(this);
    
    if (this->size == 0)
    {
        ESP_LOGE(TAG, "Cannot pop from empty vector");
        return ESP_ERR_INVALID_STATE;
    }
    
    this->size--;
    return ESP_OK;
}

esp_err_t Vector_PopFront(Vector* this, void* pElement, uint32_t elementSize)
{
    assert(this);
    
    if (this->size == 0)
    {
        ESP_LOGE(TAG, "Cannot pop from empty vector");
        return ESP_ERR_INVALID_STATE;
    }
    
    memmove((uint8_t*)pElement, (uint8_t*)this->pData, elementSize);
    memmove((uint8_t*)this->pData, (uint8_t*)this->pData + elementSize, (this->size - 1) * elementSize);
    this->size--;
    return ESP_OK;
}

esp_err_t Vector_RemoveAt(Vector* this, uint32_t index)
{
    assert(this);
    
    if (index >= this->size)
    {
        ESP_LOGE(TAG, "RemoveAt index %lu out of bounds (size: %lu)", index, this->size);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Shift elements to fill the gap
    uint32_t remainingElements = this->size - (index + 1);
    if (remainingElements > 0)
    {
        memmove(
            (uint8_t*)this->pData + (index * this->elementSize),
            (uint8_t*)this->pData + ((index + 1) * this->elementSize),
            remainingElements * this->elementSize
        );
    }
    
    this->size--;
    return ESP_OK;
}

esp_err_t Vector_Remove(Vector* this, const void* element, VectorEqualityFn equalityFn)
{
    assert(this);
    assert(element);
    assert(equalityFn);
    
    // Find the element
    for (uint32_t i = 0; i < this->size; i++)
    {
        void* currentElement = (uint8_t*)this->pData + (i * this->elementSize);
        if (equalityFn(currentElement, element))
        {
            // Found it, now remove it
            return Vector_RemoveAt(this, i);
        }
    }
    
    // Element not found
    return ESP_ERR_NOT_FOUND;
}

esp_err_t Vector_Insert(Vector* this, uint32_t index, const void* element)
{
    assert(this);
    assert(element);
    
    if (index > this->size)
    {
        ESP_LOGE(TAG, "Insert index %lu out of bounds (size: %lu)", index, this->size);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if we need to grow the vector
    if (this->size == this->capacity)
    {
        uint32_t newCapacity = _Vector_CalculateGrowCapacity(this->capacity);
        esp_err_t err = _Vector_Reallocate(this, newCapacity);
        if (err != ESP_OK)
        {
            return err;
        }
    }
    
    // Shift elements to make space for the new element
    if (index < this->size)
    {
        memmove(
            (uint8_t*)this->pData + ((index + 1) * this->elementSize),
            (uint8_t*)this->pData + (index * this->elementSize),
            (this->size - index) * this->elementSize
        );
    }
    
    // Insert the new element
    memcpy((uint8_t*)this->pData + (index * this->elementSize), element, this->elementSize);
    this->size++;
    
    return ESP_OK;
}

esp_err_t Vector_Erase(Vector* this, uint32_t index)
{
    assert(this);
    
    if (index >= this->size)
    {
        ESP_LOGE(TAG, "Erase index %lu out of bounds (size: %lu)", index, this->size);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Shift elements to fill the gap
    uint32_t remainingElements = this->size - (index + 1);
    if (remainingElements > 0)
    {
        memmove(
            (uint8_t*)this->pData + (index * this->elementSize),
            (uint8_t*)this->pData + ((index + 1) * this->elementSize),
            remainingElements * this->elementSize
        );
    }
    
    this->size--;
    return ESP_OK;
}

void Vector_Clear(Vector* this)
{
    assert(this);
    this->size = 0;
}

esp_err_t Vector_Swap(Vector* this, Vector* other)
{
    assert(this);
    assert(other);
    
    if (this->elementSize != other->elementSize)
{
        ESP_LOGE(TAG, "Cannot swap vectors with different element sizes");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Swap all fields
    void* tempData = this->pData;
    uint32_t tempSize = this->size;
    uint32_t tempCapacity = this->capacity;
    bool tempIsStatic = this->isStatic;
    
    this->pData = other->pData;
    this->size = other->size;
    this->capacity = other->capacity;
    this->isStatic = other->isStatic;
    
    other->pData = tempData;
    other->size = tempSize;
    other->capacity = tempCapacity;
    other->isStatic = tempIsStatic;
    
    return ESP_OK;
}

esp_err_t Vector_Assign(Vector* this, uint32_t count, const void* element)
{
    assert(this);
    assert(element || count == 0);
    
    // Resize the vector to the requested count
    esp_err_t err = Vector_Resize(this, count);
    if (err != ESP_OK)
{
        return err;
    }
    
    // Fill with copies of the element
    for (uint32_t i = 0; i < count; i++)
{
        memcpy((uint8_t*)this->pData + (i * this->elementSize), element, this->elementSize);
    }
    
    return ESP_OK;
}

esp_err_t Vector_EmplaceBack(Vector* this, const void* element)
{
    // In C, emplace_back is essentially the same as push_back since we don't have constructors
    return Vector_PushBack(this, element);
}
