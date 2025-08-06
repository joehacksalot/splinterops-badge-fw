/**
 * @file Utilities.c
 * @brief Miscellaneous helpers for randomness and badge identification.
 *
 * Provides small utility functions used across the firmware:
 * - Pseudo-random number generation using esp_random().
 * - Badge type detection based on compile-time flags.
 * - Parsing numeric badge types.
 * - Mapping badge type to a BLE device name string.
 *
 * Notes
 * - Badge selection is controlled via TRON_BADGE, REACTOR_BADGE, CREST_BADGE,
 *   or FMAN25_BADGE compile-time defines.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_random.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "Utilities.h"

static const char BLE_DEVICE_NAME_TRON[]    = "IWCv1";
static const char BLE_DEVICE_NAME_REACTOR[] = "IWCv2";
static const char BLE_DEVICE_NAME_CREST[]   = "IWCv3";
static const char BLE_DEVICE_NAME_FMAN25[]  = "IWCv4";

/**
 * @brief Get a random integer in the inclusive range [min, max].
 *
 * Uses esp_random() modulo arithmetic to constrain the result.
 *
 * @param min Minimum inclusive bound.
 * @param max Maximum inclusive bound (must be >= min).
 * @return Random integer between min and max inclusive.
 */
uint32_t GetRandomNumber(uint32_t min, uint32_t max)
{
    return (esp_random() % (max - min + 1)) + min;
}

/**
 * @brief Determine badge type from compile-time configuration.
 *
 * Evaluates compile-time defines to return the active badge variant.
 *
 * @return BadgeType matching the selected build, or BADGE_TYPE_UNKNOWN.
 */
BadgeType GetBadgeType(void)
{
#if defined(TRON_BADGE)
    return BADGE_TYPE_TRON;
#elif defined(REACTOR_BADGE)
    return BADGE_TYPE_REACTOR;
#elif defined(CREST_BADGE)
    return BADGE_TYPE_CREST;
#elif defined(FMAN25_BADGE)
    return BADGE_TYPE_FMAN25;
#else
    return BADGE_TYPE_UNKNOWN;
#endif
}

/**
 * @brief Convert numeric badge type value to BadgeType enum.
 *
 * @param badgeTypeNum Integer code: 1=TRON, 2=REACTOR, 3=CREST, 4=FMAN25.
 * @return Corresponding BadgeType, or BADGE_TYPE_UNKNOWN if unmapped.
 */
BadgeType ParseBadgeType(int badgeTypeNum)
{
    switch (badgeTypeNum)
    {
        case 1:
            return BADGE_TYPE_TRON;
        case 2:
            return BADGE_TYPE_REACTOR;
        case 3:
            return BADGE_TYPE_CREST;
        case 4:
            return BADGE_TYPE_FMAN25;
        default:
            return BADGE_TYPE_UNKNOWN;
    }
}

/**
 * @brief Write the BLE device name for the current badge type.
 *
 * Writes up to bufferSize-1 bytes to the provided buffer. Caller should ensure
 * the buffer is zero-initialized or explicitly add a terminator if needed.
 *
 * @param buffer Destination character buffer.
 * @param bufferSize Size of destination buffer in bytes.
 */
void GetBadgeBleDeviceName(char * buffer, uint32_t bufferSize)
{
    const char * deviceName = "Unknown";
    switch(GetBadgeType())
    {
        case BADGE_TYPE_TRON:
            deviceName = BLE_DEVICE_NAME_TRON;
            break;
        case BADGE_TYPE_REACTOR:
            deviceName = BLE_DEVICE_NAME_REACTOR;
            break;
        case BADGE_TYPE_CREST:
            deviceName = BLE_DEVICE_NAME_CREST;
            break;
        case BADGE_TYPE_FMAN25:
            deviceName = BLE_DEVICE_NAME_FMAN25;
            break;
        default:
            break;
    }
    strncpy(buffer, (char *)deviceName, bufferSize - 1);
}
