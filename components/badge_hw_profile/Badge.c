
#include <string.h>
#include "Badge.h"
#include "BadgeHwProfile.h"
#include "esp_log.h"

static Badge instance;

extern esp_err_t Badge_GenerateBadgeIdAndKey(Badge *this);

Badge *Badge_GetInstance(void)
{
    return &instance;
}

esp_err_t Badge_Init(Badge *this)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    this->badgeName = BADGE_PROFILE_NAME;
    this->badgeType = BADGE_TYPE;
    assert(Badge_GenerateBadgeIdAndKey(this) == ESP_OK);
    return ESP_OK;
}
