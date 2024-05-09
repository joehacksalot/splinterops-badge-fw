#ifndef JSONUTILS_H
#define JSONUTILS_H

#include "esp_check.h"
#include "esp_log.h"

esp_err_t GetSharecodeFromJson(char * custom_led_sequence, char * share_code, int share_code_size);

bool JsonUtils_ValidateJson(const char * json);


#endif // JSONUTILS_H