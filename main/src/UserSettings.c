
#include <string.h>
#include "mbedtls/base64.h"
#include "sha/sha_parallel_engine.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "cJSON.h"

#include "BatterySensor.h"
#include "DiskUtilities.h"
#include "TaskPriorities.h"
#include "UserSettings.h"
#include "Utilities.h"

#define USER_SETTINGS_WRITE_PERIOD_MS (60 * 1000)
#define SETTINGS_FILE_NAME CONFIG_MOUNT_PATH "/settings"
#define MUTEX_MAX_WAIT_MS (50)
#define SHA_INPUT_SIZE 12
#define SHA2_256_BYTES 32
#define SALT_SIZE 4
#define BASE_MAC_BUFFER_SIZE 8

uint8_t BADGE_ID_SALT[SALT_SIZE] = { 0x90, 0xDE, 0xCA, 0xFF };
uint8_t KEY_SALT[SALT_SIZE] = { 0x14, 0x73, 0xC0, 0xDE };

static const char *TAG = "SET";

static void UserSettings_Task(void *pvParameters);
static esp_err_t UserSettings_ReadUserSettingsFileFromDisk(UserSettings *this);
static esp_err_t UserSettings_WriteUserSettingsFileToDisk(UserSettings *this);

esp_err_t UserSettings_Init(UserSettings *this, BatterySensor * pBatterySensor)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    this->pBatterySensor = pBatterySensor;
    this->settings.soundEnabled = 1;
    this->settings.vibrationEnabled = 1;
    this->mutex = xSemaphoreCreateMutex();
    assert(this->mutex);
    // Set badge id and key
    uint8_t baseMac[BASE_MAC_BUFFER_SIZE];
    memset(baseMac, 0, BASE_MAC_BUFFER_SIZE);
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(baseMac));
    ESP_LOGD(TAG, "Generating badge id from base MAC");
    uint8_t shaInput[SHA_INPUT_SIZE];
    memcpy(shaInput, BADGE_ID_SALT, 4);
    memcpy(shaInput + 4, baseMac, BASE_MAC_BUFFER_SIZE);
    uint8_t shaOutput[SHA2_256_BYTES];
    esp_sha(SHA2_256, shaInput, SHA_INPUT_SIZE, shaOutput);
    memcpy(this->badgeId, shaOutput, BADGE_ID_SIZE);
    size_t b64Outlen;
    mbedtls_base64_encode((unsigned char*)this->badgeIdB64, BADGE_ID_B64_SIZE, &b64Outlen, this->badgeId, BADGE_ID_SIZE);
    ESP_LOGI(TAG, "BadgeId [B64]: %s", this->badgeIdB64);
    ESP_LOGD(TAG, "Generating badge key from base MAC");
    memcpy(shaInput, KEY_SALT, SALT_SIZE);
    esp_sha(SHA2_256, shaInput, SHA_INPUT_SIZE, shaOutput);
    mbedtls_base64_encode((unsigned char*)this->keyB64, KEY_B64_SIZE, &b64Outlen, shaOutput, 8);
    memcpy(this->key, shaOutput, KEY_SIZE);
    mbedtls_base64_encode((unsigned char*)this->keyB64, KEY_B64_SIZE, &b64Outlen, this->key, KEY_SIZE);
    ESP_LOGI(TAG, "Key B64: %s", this->keyB64);
    if (UserSettings_ReadUserSettingsFileFromDisk(this) != ESP_OK)
    {
        UserSettings_WriteUserSettingsFileToDisk(this);
    }

    assert(xTaskCreatePinnedToCore(UserSettings_Task, "UserSettingsTask", configMINIMAL_STACK_SIZE * 2, this, USER_SETTINGS_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
    return ESP_OK;
}

static void UserSettings_Task(void *pvParameters)
{
    UserSettings *this = (UserSettings *)pvParameters;
    assert(this);
    while (true)
    {
        if (this->updateNeeded)
        {
            ESP_LOGI(TAG, "Writing Settings File");
            UserSettings_WriteUserSettingsFileToDisk(this);
            this->updateNeeded = false;
        }
        vTaskDelay(pdMS_TO_TICKS(USER_SETTINGS_WRITE_PERIOD_MS));
    }
}

esp_err_t UserSettings_SetSelectedIndex(UserSettings *this, uint32_t selectedIndex)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        ESP_LOGI(TAG, "Updating selected index to %lu", selectedIndex);
        this->settings.selectedIndex = selectedIndex;
        this->updateNeeded = true;
        ret = ESP_OK;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
    return ret;
}

esp_err_t UserSettings_SetPairId(UserSettings *this, uint8_t * pairId)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        if (pairId == NULL)
        {
            ESP_LOGI(TAG, "Clearing pair id");
            memset(this->settings.pairId, 0, sizeof(this->settings.pairId));
        }
        else
        {
            ESP_LOGI(TAG, "Updating pair id");
            ESP_LOG_BUFFER_HEX(TAG, pairId, sizeof(this->settings.pairId));
            memcpy(this->settings.pairId, pairId, sizeof(this->settings.pairId));
        }
        this->updateNeeded = true;
        ret = ESP_OK;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
    return ret;
}

esp_err_t UserSettings_UpdateFromJson(UserSettings *this, uint8_t *pSettingsJson)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    if (pSettingsJson != 0)
    {
        cJSON * root = cJSON_Parse((char*)pSettingsJson);
        if (root != NULL)
        {
            UserSettingsFile tmpSettings;
            if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
            {
                tmpSettings = this->settings;
                if (xSemaphoreGive(this->mutex) != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
                }
            }
            if (cJSON_GetObjectItem(root,"vibrations") != NULL)
            {
                tmpSettings.vibrationEnabled = MAX(0, cJSON_GetObjectItem(root,"vibrations")->valueint);
            }
            if (cJSON_GetObjectItem(root,"sounds") != NULL)
            {
                tmpSettings.soundEnabled = MAX(0, cJSON_GetObjectItem(root,"sounds")->valueint);
            }
            if (cJSON_GetObjectItem(root,"ssid") != NULL)
            {
                char * ssid = cJSON_GetObjectItem(root,"ssid")->valuestring;
                strncpy((char*)tmpSettings.wifiSettings.ssid, ssid, sizeof(tmpSettings.wifiSettings.ssid));
            }
            if (cJSON_GetObjectItem(root,"pass") != NULL)
            {
                char * pass = cJSON_GetObjectItem(root,"pass")->valuestring;
                strncpy((char*)tmpSettings.wifiSettings.password, pass, sizeof(tmpSettings.wifiSettings.password));
            }
            cJSON_Delete(root);
            this->settings = tmpSettings;
            ret = UserSettings_WriteUserSettingsFileToDisk(this);
        }
        else
        {
            ESP_LOGE(TAG, "JSON parse failed. json = \"%s\"", pSettingsJson);
        }
    }
    else
    {
        ESP_LOGE(TAG, "JSON null");
    }
    return ret;
}

static esp_err_t UserSettings_ReadUserSettingsFileFromDisk(UserSettings *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);

    // Read current position file from flash filesystem
    ESP_LOGI(TAG, "Reading user settings file");
    FILE * fp = fopen(SETTINGS_FILE_NAME, "rb");
    if (fp != 0)
    {
        // Check file size matches
        fseek(fp, 0L, SEEK_END);
        ssize_t fileSize = ftell(fp);
        if (fileSize == sizeof(this->settings))
        {
            // File size matches expected, read data into offset
            fseek(fp, 0, SEEK_SET);
            UserSettingsFile tmpSettings;
            if (fread(&tmpSettings, 1, sizeof(tmpSettings), fp) == sizeof(tmpSettings))
            {
                if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
                {
                    this->settings = tmpSettings;
                    if (xSemaphoreGive(this->mutex) != pdTRUE)
                    {
                        ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
                    }

                    ESP_LOGI(TAG, "Settings: %d, %d, %s, %s", this->settings.soundEnabled, this->settings.vibrationEnabled, this->settings.wifiSettings.ssid, this->settings.wifiSettings.password);
                    ESP_LOGI(TAG, "Settings file found and read");
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
            ESP_LOGE(TAG, "ERROR: Actual: %d, Expected: %d", fileSize, sizeof(this->settings));
        }
        fclose(fp);
    }
    return ret;
}
static esp_err_t UserSettings_WriteUserSettingsFileToDisk(UserSettings *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    assert(this->pBatterySensor);

    if (xSemaphoreTake(this->mutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        UserSettingsFile tmpSettings = this->settings;
        if (xSemaphoreGive(this->mutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }

        ret = WriteFileToDisk(this->pBatterySensor, SETTINGS_FILE_NAME, (char*)&tmpSettings, sizeof(tmpSettings));
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write user settings file");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
    return ret;
}