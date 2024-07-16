#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include <string.h>
#include "Utilities.h"

BadgeType GetBadgeType(void)
{
#if defined(TRON_BADGE)
    return BADGE_TYPE_TRON;
#elif defined(REACTOR_BADGE)
    return BADGE_TYPE_REACTOR;
#elif defined(CREST_BADGE)
    return BADGE_TYPE_CREST;
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
        default:
            return BADGE_TYPE_UNKNOWN;
    }
}
