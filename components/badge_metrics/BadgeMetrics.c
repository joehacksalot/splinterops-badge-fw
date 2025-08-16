
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "BadgeMetrics.h"
#include "DiskUtilities.h"
#include "Utilities.h"

#define STATS_FILE_NAME       CONFIG_MOUNT_PATH "/stats"
#define BADGE_WRITE_PERIOD_MS (15 * 60 * 1000)
#define MUTEX_MAX_WAIT_MS     (50)
static const char *TAG = "STA";

//static void BadgeStatsTask(void *pvParameters);
//static esp_err_t BadgeMetrics_ReadBadgeStatsFileFromDisk(BadgeMetrics *this);
//static esp_err_t BadgeMetrics_WriteBadgeStatsFileToDisk(BadgeMetrics *this);

esp_err_t BadgeMetrics_Init(BadgeMetrics *this)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    // this->pBatterySensor = NULL;
    this->mutex = xSemaphoreCreateMutex();
    assert(this->mutex);
    // if (BadgeMetrics_ReadBadgeStatsFileFromDisk(this) != ESP_OK)
    // {
    //     BadgeMetrics_WriteBadgeStatsFileToDisk(this);
    // }
    BadgeMetrics_IncrementNumPowerOns(this);

    // assert(xTaskCreatePinnedToCore(_BadgeStatsTask, "BadgeStatsTask", configMINIMAL_STACK_SIZE * 4, this, BADGE_STAT_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS); // Commented to prevent disk writes. statistics will still be captured, but not saved to disk
    return ESP_OK;
}

// esp_err_t BadgeMetrics_RegisterBatterySensor(BadgeMetrics *this, BatterySensor *pBatterySensor)
// {
//     assert(this);
//     assert(pBatterySensor);
//     this->pBatterySensor = pBatterySensor;
//     return ESP_OK;
// }


// static void BadgeStatsTask(void *pvParameters)
// {
//     BadgeMetrics *this = (BadgeMetrics *)pvParameters;
//     assert(this);
//     while (true)
//     {
//         if (this->updateNeeded)
//         {
//             ESP_LOGI(TAG, "Writing Stats File");
//             BadgeMetrics_WriteBadgeStatsFileToDisk(this);
//             this->updateNeeded = false;
//         }
//         vTaskDelay(pdMS_TO_TICKS(BADGE_WRITE_PERIOD_MS));
//     }
// }

void BadgeMetrics_IncrementNumPowerOns(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numPowerOns++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}


void BadgeMetrics_IncrementNumTouches(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numTouches++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

void BadgeMetrics_IncrementNumTouchCmds(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numTouchCmds++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

void BadgeMetrics_IncrementNumLedCycles(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numLedCycles++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

void BadgeMetrics_IncrementNumBatteryChecks(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numBattChecks++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

void BadgeMetrics_IncrementNumBleEnables(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numBleEnables++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

void BadgeMetrics_IncrementNumBleDisables(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numBleDisables++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

void BadgeMetrics_IncrementNumBleSeqXfers(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numBleSeqXfers++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

void BadgeMetrics_IncrementNumBleSetXfers(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numBleSetXfers++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

void BadgeMetrics_IncrementNumNetworkTests(BadgeMetrics *this)
{
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        this->badgeStats.numNetworkTests++;
        this->updateNeeded = true;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

// static esp_err_t BadgeMetrics_ReadBadgeStatsFileFromDisk(BadgeMetrics *this)
// {
//     esp_err_t ret = ESP_FAIL;
//     assert(this);

//     BadgeStatsFile badgeStatsFile;
//     if (ReadFileFromDisk(STATS_FILE_NAME, (char *)&badgeStatsFile, sizeof(badgeStatsFile), NULL, sizeof(badgeStatsFile)) == ESP_OK)
//     {
//         if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
//         {
//             this->badgeStats = badgeStatsFile;
//             ret = ESP_OK;
//             if (xSemaphoreGive(this->mutex) != pdTRUE)
//             {
//                 ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
//             }
//         }
//         else
//         {
//             ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
//         }
//     }
//     else
//     {
//         ESP_LOGE(TAG, "Failed to read game status file");
//     }
//     return ret;
// }

// static esp_err_t BadgeMetrics_WriteBadgeStatsFileToDisk(BadgeMetrics *this)
// {
//     esp_err_t ret = ESP_FAIL;
//     assert(this);
//     if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
//     {
//         BadgeStatsFile badgeStatsFile = this->badgeStats;
//         if (xSemaphoreGive(this->mutex) != pdTRUE)
//         {
//             ESP_LOGE(TAG, "Failed to give mutex in %s", __FUNCTION__);
//         }
//         ret = WriteFileToDisk(this->pBatterySensor, STATS_FILE_NAME, (char *)&badgeStatsFile, sizeof(badgeStatsFile));
//         if (ret != ESP_OK)
//         {
//             ESP_LOGE(TAG, "Failed to write badge stats file");
//         }
//     }
//     else
//     {
//         ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
//     }
//     return ret;
// }
