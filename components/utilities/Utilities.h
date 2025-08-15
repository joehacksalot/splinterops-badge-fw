
#ifndef UTILITIES_H_
#define UTILITIES_H_

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

typedef enum BadgeType_e
{
    BADGE_TYPE_UNKNOWN = 0,
    BADGE_TYPE_TRON = 1,
    BADGE_TYPE_REACTOR = 2,
    BADGE_TYPE_CREST = 3,
    BADGE_TYPE_FMAN25 = 4
} BadgeType;

extern BadgeType GetBadgeType(void);
extern BadgeType ParseBadgeType(int badgeTypeNum);
extern uint32_t GetRandomNumber(uint32_t min, uint32_t max);
extern void GetBadgeBleDeviceName(char * buffer, uint32_t bufferSize);

#endif // UTILITIES_H_