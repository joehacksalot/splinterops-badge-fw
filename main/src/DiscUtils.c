
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"

#include "DiscUtils.h"

#define TAG  "FS"

esp_err_t DiscUtils_InitNvs(void)
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

esp_err_t DiscUtils_InitFs(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config =
    {
            .max_files = 4,
            .allocation_unit_size = 0, // Allocation unit set to sector size
            .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
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