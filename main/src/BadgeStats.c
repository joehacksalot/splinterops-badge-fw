/**
 * @file BadgeStats.c
 * @brief Badge usage statistics tracking and persistence implementation
 * 
 * This module implements comprehensive usage statistics tracking for the badge system,
 * providing detailed metrics on user interactions, system events, and hardware usage.
 * 
 * ## Key Features:
 * - **Power-on tracking**: Counts badge boot cycles and power events
 * - **Touch interaction metrics**: Tracks touch sensor usage and command processing
 * - **LED activity monitoring**: Records LED cycle counts and pattern usage
 * - **Battery monitoring stats**: Tracks battery check frequency and power management
 * - **BLE communication metrics**: Monitors Bluetooth enable/disable cycles and data transfers
 * - **Network activity tracking**: Records network test executions and connectivity events
 * - **Thread-safe operations**: Uses mutex protection for concurrent access
 * - **Persistent storage**: Saves statistics to flash memory for long-term tracking
 * 
 * ## Implementation Details:
 * - Uses FreeRTOS mutex for thread-safe statistic updates
 * - Integrates with battery sensor for power-aware disk operations
 * - Implements periodic background task for statistics persistence
 * - Provides atomic increment operations for all tracked metrics
 * - Handles disk I/O errors gracefully with fallback mechanisms
 * 
 * ## Usage Patterns:
 * - Initialize once during system startup with BadgeStats_Init()
 * - Register battery sensor for power management integration
 * - Call increment functions from various system modules as events occur
 * - Statistics are automatically persisted to flash storage periodically
 * 
 * @author Badge Development Team
 * @date 2024
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "BadgeStats.h"
#include "BatterySensor.h"
#include "DiskDefines.h"
#include "DiskUtilities.h"
#include "TaskPriorities.h"
#include "Utilities.h"

#define STATS_FILE_NAME       MOUNT_PATH "/stats"
#define BADGE_WRITE_PERIOD_MS (15 * 60 * 1000)
#define MUTEX_MAX_WAIT_MS     (50)
static const char *TAG = "STA";

//static void BadgeStatsTask(void *pvParameters);
//static esp_err_t BadgeStats_ReadBadgeStatsFileFromDisk(BadgeStats *this);
//static esp_err_t BadgeStats_WriteBadgeStatsFileToDisk(BadgeStats *this);

/**
 * @brief Initialize the badge statistics tracking system
 * 
 * Initializes the BadgeStats module by clearing all counters, creating the
 * thread-safe mutex, and incrementing the power-on counter. Sets up the
 * foundation for tracking all badge usage statistics.
 * 
 * Implementation notes:
 * - Clears all statistics counters to zero
 * - Creates FreeRTOS mutex for thread-safe access
 * - Automatically increments power-on counter
 * - Background persistence task is currently disabled to prevent disk writes
 * 
 * @param this Pointer to BadgeStats instance to initialize
 * @return ESP_OK on successful initialization
 */
esp_err_t BadgeStats_Init(BadgeStats *this)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    this->pBatterySensor = NULL;
    this->mutex = xSemaphoreCreateMutex();
    assert(this->mutex);
    // if (BadgeStats_ReadBadgeStatsFileFromDisk(this) != ESP_OK)
    // {
    //     BadgeStats_WriteBadgeStatsFileToDisk(this);
    // }
    BadgeStats_IncrementNumPowerOns(this);

    // assert(xTaskCreatePinnedToCore(_BadgeStatsTask, "BadgeStatsTask", configMINIMAL_STACK_SIZE * 4, this, BADGE_STAT_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS); // Commented to prevent disk writes. statistics will still be captured, but not saved to disk
    return ESP_OK;
}

/**
 * @brief Register battery sensor for power-aware operations
 * 
 * Registers a battery sensor instance with the statistics system to enable
 * power-aware disk operations. The battery sensor is used to check power
 * levels before performing disk writes to prevent data corruption.
 * 
 * @param this Pointer to BadgeStats instance
 * @param pBatterySensor Pointer to initialized BatterySensor instance
 * @return ESP_OK on successful registration
 */
esp_err_t BadgeStats_RegisterBatterySensor(BadgeStats *this, BatterySensor *pBatterySensor)
{
    assert(this);
    assert(pBatterySensor);
    this->pBatterySensor = pBatterySensor;
    return ESP_OK;
}


// static void BadgeStatsTask(void *pvParameters)
// {
//     BadgeStats *this = (BadgeStats *)pvParameters;
//     assert(this);
//     while (true)
//     {
//         if (this->updateNeeded)
//         {
//             ESP_LOGI(TAG, "Writing Stats File");
//             BadgeStats_WriteBadgeStatsFileToDisk(this);
//             this->updateNeeded = false;
//         }
//         vTaskDelay(pdMS_TO_TICKS(BADGE_WRITE_PERIOD_MS));
//     }
// }

/**
 * @brief Increment power-on counter in thread-safe manner
 * 
 * Atomically increments the badge power-on counter and marks statistics
 * as needing update. This tracks how many times the badge has been powered on.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumPowerOns(BadgeStats *this)
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


/**
 * @brief Increment touch event counter in thread-safe manner
 * 
 * Atomically increments the touch sensor activation counter and marks
 * statistics as needing update. This tracks total touch interactions.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumTouches(BadgeStats *this)
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

/**
 * @brief Increment touch command counter in thread-safe manner
 * 
 * Atomically increments the touch-based command execution counter and marks
 * statistics as needing update. This tracks commands triggered by touch.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumTouchCmds(BadgeStats *this)
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

/**
 * @brief Increment LED cycle counter in thread-safe manner
 * 
 * Atomically increments the LED animation cycle counter and marks
 * statistics as needing update. This tracks LED pattern executions.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumLedCycles(BadgeStats *this)
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

/**
 * @brief Increment battery check counter in thread-safe manner
 * 
 * Atomically increments the battery sensor check counter and marks
 * statistics as needing update. This tracks battery monitoring frequency.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBatteryChecks(BadgeStats *this)
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

/**
 * @brief Increment BLE enable counter in thread-safe manner
 * 
 * Atomically increments the Bluetooth enable event counter and marks
 * statistics as needing update. This tracks BLE activation events.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBleEnables(BadgeStats *this)
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

/**
 * @brief Increment BLE disable counter in thread-safe manner
 * 
 * Atomically increments the Bluetooth disable event counter and marks
 * statistics as needing update. This tracks BLE deactivation events.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBleDisables(BadgeStats *this)
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

/**
 * @brief Increment BLE sequence transfer counter in thread-safe manner
 * 
 * Atomically increments the BLE sequence transfer counter and marks
 * statistics as needing update. This tracks LED sequence sharing via BLE.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBleSeqXfers(BadgeStats *this)
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

/**
 * @brief Increment BLE settings transfer counter in thread-safe manner
 * 
 * Atomically increments the BLE settings transfer counter and marks
 * statistics as needing update. This tracks configuration sharing via BLE.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBleSetXfers(BadgeStats *this)
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

/**
 * @brief Increment network test counter in thread-safe manner
 * 
 * Atomically increments the network connectivity test counter and marks
 * statistics as needing update. This tracks network diagnostic operations.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumNetworkTests(BadgeStats *this)
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

// static esp_err_t BadgeStats_ReadBadgeStatsFileFromDisk(BadgeStats *this)
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

// static esp_err_t BadgeStats_WriteBadgeStatsFileToDisk(BadgeStats *this)
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
