
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "BadgeStats.h"
#include "BatterySensor.h"
#include "DiscUtils.h"
#include "TaskPriorities.h"

#define STATS_FILE_NAME       MOUNT_PATH "/stats"
#define BADGE_WRITE_PERIOD_MS (15 * 60 * 1000)
#define MUTEX_MAX_WAIT_MS     (50)
static const char *TAG = "STA";

static void BadgeStatsTask(void *pvParameters);
static esp_err_t BadgeStats_ReadBadgeStatsFileFromDisk(BadgeStats *this);
static esp_err_t BadgeStats_WriteBadgeStatsFileToDisk(BadgeStats *this);

esp_err_t BadgeStats_Init(BadgeStats *this)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    this->pBatterySensor = NULL;
    this->mutex = xSemaphoreCreateMutex();
    assert(this->mutex);
    if (BadgeStats_ReadBadgeStatsFileFromDisk(this) != ESP_OK)
    {
        BadgeStats_WriteBadgeStatsFileToDisk(this);
    }
    BadgeStats_IncrementNumPowerOns(this);

    // xTaskCreate(BadgeStatsTask, "BadgeStatsTask", configMINIMAL_STACK_SIZE * 5, this, BADGE_STAT_TASK_PRIORITY, NULL);
    return ESP_OK;
}

esp_err_t BadgeStats_RegisterBatterySensor(BadgeStats *this, BatterySensor *pBatterySensor)
{
    assert(this);
    assert(pBatterySensor);
    this->pBatterySensor = pBatterySensor;
    return ESP_OK;
}


static void BadgeStatsTask(void *pvParameters)
{
    BadgeStats *this = (BadgeStats *)pvParameters;
    assert(this);
    while (true)
    {
        if (this->updateNeeded)
        {
            ESP_LOGI(TAG, "Writing Stats File");
            BadgeStats_WriteBadgeStatsFileToDisk(this);
            this->updateNeeded = false;
        }
        vTaskDelay(pdMS_TO_TICKS(BADGE_WRITE_PERIOD_MS));
    }
}

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

static esp_err_t BadgeStats_ReadBadgeStatsFileFromDisk(BadgeStats *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);

    // Read current position file from flash filesystem
    ESP_LOGI(TAG, "Reading badge stats file");
    FILE * fp = fopen(STATS_FILE_NAME, "rb");
    if (fp != 0)
    {
        // Check file size matches
        fseek(fp, 0L, SEEK_END);
        ESP_LOGD(TAG, "Checking file size");
        ssize_t fileSize = ftell(fp);
        if (fileSize == sizeof(this->badgeStats))
        {
            // File size matches expected, read data into offset
            ESP_LOGD(TAG, "File size matches expected, reading data");
            fseek(fp, 0, SEEK_SET);
            BadgeStatsFile tmpStats;
            if (fread(&tmpStats, 1, sizeof(tmpStats), fp) == sizeof(tmpStats))
            {
                if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
                {
                    this->badgeStats = tmpStats;
                    if (xSemaphoreGive(this->mutex) != pdTRUE)
                    {
                        ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
                    }

                    ESP_LOGI(TAG, "Stats file found and read");
                    ret = ESP_OK;
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Partial read completed");
            }
        }
        else
        {
            // File size doesn't match expected
            ESP_LOGE(TAG, "ERROR: Stats file size not as expected. Actual: %d, Expected: %d", fileSize, sizeof(this->badgeStats));
        }

        fclose(fp);
    }
    else
    {
        ESP_LOGI(TAG, "Stats file %s does not exist", STATS_FILE_NAME);
    }

    return ret;
}

static esp_err_t BadgeStats_WriteBadgeStatsFileToDisk(BadgeStats *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);

    BadgeStatsFile tmpStats;
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        tmpStats = this->badgeStats;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }

        if (this->pBatterySensor != NULL && BatterySensor_GetBatteryPercent(this->pBatterySensor) > BATTERY_NO_FLASH_WRITE_THRESHOLD)
        {
            int status = remove(STATS_FILE_NAME);
            if(status != 0)
            {
                printf("Error: unable to remove the file. %s", STATS_FILE_NAME);
            }
            FILE * fp = fopen(STATS_FILE_NAME, "wb");
            if (fp != 0)
            {
                ssize_t bytesWritten = fwrite(&tmpStats, 1, sizeof(tmpStats), fp);
                if (bytesWritten == sizeof(tmpStats))
                {
                    ESP_LOGI(TAG, "Write completed for %s", STATS_FILE_NAME);
                    ret = ESP_OK;
                }
                else
                {
                    ESP_LOGE(TAG, "Write of selected sequence index file failed. fwrite return did not match byte write size. expected=%d, actual=%d\n", sizeof(tmpStats), bytesWritten);
                }
                fclose(fp);
            }
            else
            {
                ESP_LOGE(TAG, "Creation of %s failed", STATS_FILE_NAME);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Battery level too low to write stats file");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
        return ESP_FAIL;
    }
    return ret;
}