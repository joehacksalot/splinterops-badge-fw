/**
 * @file JsonUtils.h
 * @brief JSON parsing and validation utilities
 * 
 * This module provides JSON processing utilities for the badge system including:
 * - JSON validation and syntax checking
 * - Share code extraction from JSON data
 * - LED sequence configuration parsing
 * - Error handling for malformed JSON
 * - Integration with cJSON library for parsing
 * 
 * Used throughout the badge system for processing configuration data,
 * BLE file transfers, and user settings in JSON format.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef JSONUTILS_H
#define JSONUTILS_H

#include "esp_check.h"
#include "esp_log.h"

/**
 * @brief Extract share code from LED sequence JSON
 * 
 * Parses a custom LED sequence JSON string to extract the embedded
 * share code used for badge-to-badge sequence sharing. The share code
 * is a unique identifier that allows sequences to be synchronized.
 * 
 * @param custom_led_sequence JSON string containing LED sequence data
 * @param share_code Output buffer to store the extracted share code
 * @param share_code_size Size of the share code output buffer
 * @return ESP_OK on success, error code on parsing failure or missing share code
 */
esp_err_t GetSharecodeFromJson(char * custom_led_sequence, char * share_code, int share_code_size);

/**
 * @brief Validate JSON syntax and structure
 * 
 * Validates that a JSON string has correct syntax and can be parsed
 * successfully. Used to verify configuration data, BLE transfers,
 * and user settings before processing.
 * 
 * @param json JSON string to validate
 * @return true if JSON is valid and parseable, false otherwise
 */
bool JsonUtils_ValidateJson(const char * json);


#endif // JSONUTILS_H