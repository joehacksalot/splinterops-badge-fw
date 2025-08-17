
#include "BadgeType.h"
#include "BadgeHwProfile.h"

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