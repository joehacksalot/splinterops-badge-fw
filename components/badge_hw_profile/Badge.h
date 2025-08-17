
#ifndef BADGE_H_
#define BADGE_H_

#include "BadgeType.h"
#include <stdint.h>
#include <esp_err.h>

#define BADGE_UUID_SIZE                 (8)
#define BADGE_UUID_B64_SIZE             (13)
#define BADGE_UNIQUE_KEY_SIZE           (8)
#define BADGE_UNIQUE_KEY_B64_SIZE       (13)

typedef struct Badge_t
{
    BadgeType badgeType;
    const char* badgeName;
    uint8_t uuid[BADGE_UUID_SIZE];
    uint8_t uuidB64[BADGE_UUID_B64_SIZE];
    uint8_t uniqueKey[BADGE_UNIQUE_KEY_SIZE];
    uint8_t uniqueKeyB64[BADGE_UNIQUE_KEY_B64_SIZE];
} Badge;

Badge *Badge_GetInstance(void);

esp_err_t Badge_Init(Badge *this);

#endif // BADGE_H_