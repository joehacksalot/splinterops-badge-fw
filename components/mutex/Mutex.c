#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"

#include "Mutex.h"

static const char * TAG = "MUT";

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
