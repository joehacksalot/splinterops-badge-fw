
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_check.h"
#include "esp_log.h"

static const char * TAG = "JSON";

bool JsonUtils_ValidateJson(const char * json)
{
    if (json == NULL)
    {
        return false;
    }
    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
    {
        return false;
    }
    cJSON_Delete(root);
    return true;
}

esp_err_t GetSharecodeFromJson(char * custom_led_sequence, char * share_code, int share_code_size)
{
    memset(custom_led_sequence, 0, share_code_size);
    cJSON *root = cJSON_Parse(custom_led_sequence);
    if (root == NULL)
    {
        ESP_LOGI(TAG, "JSON parse failed. custom_led_sequence = \"%s\"", custom_led_sequence);
        return ESP_FAIL;
    }
    
    cJSON *sharecodeJSON = cJSON_GetObjectItem(root,"c");
    if (sharecodeJSON == NULL)
    {
        ESP_LOGI(TAG, "frames not found in root json");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    snprintf(share_code, share_code_size, "%s", sharecodeJSON->valuestring);
    cJSON_Delete(root);
    return ESP_OK;
}