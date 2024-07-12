
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
    BADGE_TYPE_CREST = 3
} BadgeType;


extern void registerCurrentTaskInfo(void);
extern void displayTaskInfoArray(void);
extern BadgeType GetBadgeType(void);
extern BadgeType ParseBadgeType(int badgeTypeNum);


#endif // UTILITIES_H_