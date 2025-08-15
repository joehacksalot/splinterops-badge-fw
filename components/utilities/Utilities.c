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

uint32_t GetRandomNumber(uint32_t min, uint32_t max)
{
    return (esp_random() % (max - min + 1)) + min;
}

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
