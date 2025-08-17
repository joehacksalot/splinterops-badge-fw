#ifndef BADGE_TYPE_H_
#define BADGE_TYPE_H_

typedef enum BadgeType_e
{
    BADGE_TYPE_UNKNOWN = 0,
    BADGE_TYPE_TRON = 1,
    BADGE_TYPE_REACTOR = 2,
    BADGE_TYPE_CREST = 3,
    BADGE_TYPE_FMAN25 = 4
} BadgeType;

extern BadgeType ParseBadgeType(int badgeTypeNum);

#endif // BADGE_TYPE_H_