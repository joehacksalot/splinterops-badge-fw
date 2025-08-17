
#include <string.h>
#include "mbedtls/base64.h"
#include "sha/sha_parallel_engine.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_efuse.h"
#include "Badge.h"

#define SHA_INPUT_SIZE 12
#define SHA2_256_BYTES 32
#define SALT_SIZE 4
#define BASE_MAC_BUFFER_SIZE 8

uint8_t BADGE_ID_SALT[SALT_SIZE] = { 0x90, 0xDE, 0xCA, 0xFF };
uint8_t KEY_SALT[SALT_SIZE] = { 0x14, 0x73, 0xC0, 0xDE };

const char *TAG = "BDGE";

esp_err_t Badge_GenerateBadgeIdAndKey(Badge *this)
{
    assert(this);
    
    // Set badge uuid and unique key
    uint8_t baseMac[BASE_MAC_BUFFER_SIZE];
    memset(baseMac, 0, BASE_MAC_BUFFER_SIZE);
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(baseMac));
    ESP_LOGD(TAG, "Generating badge uuid from base MAC");
    uint8_t shaInput[SHA_INPUT_SIZE];
    memcpy(shaInput, BADGE_ID_SALT, 4);
    memcpy(shaInput + 4, baseMac, BASE_MAC_BUFFER_SIZE);
    uint8_t shaOutput[SHA2_256_BYTES];
    esp_sha(SHA2_256, shaInput, SHA_INPUT_SIZE, shaOutput);
    memcpy(this->uuid, shaOutput, BADGE_UUID_SIZE);
    size_t b64Outlen;
    mbedtls_base64_encode((unsigned char*)this->uuidB64, BADGE_UUID_B64_SIZE, &b64Outlen, this->uuid, BADGE_UUID_SIZE);
    ESP_LOGI(TAG, "BadgeId [B64]: %s", this->uuidB64);
    ESP_LOGD(TAG, "Generating badge uniqueKey from base MAC");
    memcpy(shaInput, KEY_SALT, SALT_SIZE);
    esp_sha(SHA2_256, shaInput, SHA_INPUT_SIZE, shaOutput);
    mbedtls_base64_encode((unsigned char*)this->uniqueKeyB64, BADGE_UNIQUE_KEY_B64_SIZE, &b64Outlen, shaOutput, 8);
    memcpy(this->uniqueKey, shaOutput, BADGE_UNIQUE_KEY_SIZE);
    mbedtls_base64_encode((unsigned char*)this->uniqueKeyB64, BADGE_UNIQUE_KEY_B64_SIZE, &b64Outlen, this->uniqueKey, BADGE_UNIQUE_KEY_SIZE);
    ESP_LOGI(TAG, "Key B64: %s", this->uniqueKeyB64);

    return ESP_OK;
}