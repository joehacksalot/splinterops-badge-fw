#ifndef BADGE_HW_PROFILE_H_
#define BADGE_HW_PROFILE_H_

#include "sdkconfig.h"
#include <stdint.h>
#include "AppConfig.h"

// Conditionally include the per-badge profile header
#if CONFIG_BADGE_TYPE_TRON
#  include "tron/BadgeProfile.h"
#elif CONFIG_BADGE_TYPE_REACTOR
#  include "reactor/BadgeProfile.h"
#elif CONFIG_BADGE_TYPE_CREST
#  include "crest/BadgeProfile.h"
#elif CONFIG_BADGE_TYPE_FMAN25
#  include "fman25/BadgeProfile.h"
#else
#  include "fman25/BadgeProfile.h" // Default profile
#endif

extern void GetBadgeBleDeviceName(char * buffer, uint32_t bufferSize);

// Returns the touch sensor button map for the active hardware profile
extern const int* BadgeHwProfile_TouchButtonMap(void);

// Returns the size of the touch sensor button map for the active hardware profile
extern int BadgeHwProfile_TouchButtonMapSize(void);

// Returns the active badge's application configuration (per-profile constant)
extern const AppConfig * GetBadgeAppConfig(void);

#endif // BADGE_HW_PROFILE_H_
