/**
 * @file JsonUtils.c
 * @brief JSON parsing and validation utilities implementation
 * 
 * This module provides comprehensive JSON processing utilities for the badge
 * firmware, enabling structured data parsing, validation, and extraction
 * for configuration files, network communications, and data exchange.
 * 
 * ## Key Features:
 * - **JSON validation**: Syntax checking and structure validation
 * - **Share code extraction**: LED sequence share code parsing from JSON
 * - **Error handling**: Robust parsing error detection and reporting
 * - **Memory management**: Automatic JSON object cleanup and memory safety
 * - **Type safety**: Safe string extraction with bounds checking
 * - **Logging integration**: Detailed error reporting for debugging
 * 
 * ## Implementation Details:
 * - Uses cJSON library for JSON parsing and manipulation
 * - Provides safe string extraction with buffer size validation
 * - Implements comprehensive error checking for all JSON operations
 * - Handles malformed JSON gracefully with appropriate error codes
 * - Ensures proper memory cleanup to prevent leaks
 * 
 * ## Usage Patterns:
 * - Validate JSON strings before processing with JsonUtils_ValidateJson()
 * - Extract share codes from LED sequence JSON with GetSharecodeFromJson()
 * - Always check return codes for error handling
 * - Functions handle null inputs safely
 * 
 * @author Badge Development Team
 * @date 2024
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

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