
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"

#include "DiskUtilities.h"

#define TAG  "FS"

#define BATTERY_NO_FLASH_WRITE_THRESHOLD (10)

esp_err_t DiskUtilities_InitNvs(void)
{
    static bool initialized = false;
    esp_err_t ret = ESP_ERR_INVALID_STATE;

    if (!initialized)
    {
        initialized = true;
        
        //Initialize NVS for applications
        ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
        {
            ret = nvs_flash_erase();
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to erase NVS flash. error code = %s", esp_err_to_name(ret));
            }
            ret = nvs_flash_init();
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to initialize NVS flash. error code = %s", esp_err_to_name(ret));
            }
        }
    }
    return ret;
}

esp_err_t DiskUtilities_InitFs(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config =
    {
            .max_files = 4,
            .allocation_unit_size = 0, // Allocation unit set to sector size
            .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(CONFIG_MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Mounted data FATFS");
    }
    return ret;
}

esp_err_t ReadFileFromDisk(char *filename, char *buffer, int bufferSize, int * pBytesRead, int expectedFileSize)
{
    esp_err_t ret = ESP_FAIL;
    assert(filename);
    assert(buffer);

    // Read current position file from flash filesystem
    ESP_LOGI(TAG, "Reading %s file", filename);
    FILE * fp = fopen(filename, "rb");
    if (fp != 0)
    {
        // Check file size matches
        fseek(fp, 0L, SEEK_END);
        ssize_t fileSize = ftell(fp);
        if (expectedFileSize > 0 && fileSize == expectedFileSize) // expectedFileSize of > 0 signals enforce expected file size check
        {
            int bytesToRead = MAX(bufferSize, fileSize);
            int bytesRead = 0;
            // File size matches expected, read data into offset
            fseek(fp, 0, SEEK_SET);
            bytesRead = fread(buffer, 1, bytesToRead, fp);
            if (bytesRead == bytesToRead)
            {
                ret = ESP_OK;
                if (pBytesRead != NULL)
                {
                    *pBytesRead = bytesRead;
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
            ESP_LOGE(TAG, "ERROR: Actual: %d, Expected: %d", fileSize, expectedFileSize);
        }
        fclose(fp);
    }
    return ret;
}

esp_err_t WriteFileToDisk(BatterySensor * pBatterySensor, char *filename, char *buffer, int bufferSize)
{
    esp_err_t ret = ESP_FAIL;
    assert(pBatterySensor);
    assert(filename);
    assert(buffer);
    if (pBatterySensor != NULL && BatterySensor_GetBatteryPercent(pBatterySensor) > BATTERY_NO_FLASH_WRITE_THRESHOLD)
    {
        int status = remove(filename);
        if(status != 0)
        {
            ESP_LOGE(TAG, "Error: unable to remove the file(%s): %d", filename, status);
        }

        FILE * fp = fopen(filename, "wb");
        if (fp != 0)
        {
            ssize_t bytesWritten = fwrite(buffer, 1, bufferSize, fp);
            if (bytesWritten == bufferSize)
            {
                ESP_LOGI(TAG, "Write completed for %s", filename);
                ret = ESP_OK;
            }
            else
            {
                ESP_LOGE(TAG, "Write failed for %s of size %d", filename, bufferSize);
            }
            fclose(fp);
        }
        else
        {
            ESP_LOGE(TAG, "Creation of %s failed", filename);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Battery level too low to write to flash");
    }
    return ret;
}