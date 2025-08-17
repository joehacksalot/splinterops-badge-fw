#include "BadgeHwProfile.h"
#include "crest/BadgeProfile.h"
#include "AppConfig.h"

// Concrete app configuration for the CREST badge
const AppConfig APP_CONFIG = {
    .touchActionCommandEnabled = true,
    .buzzerPresent = true,
    .eyeGpioLedsPresent = false,
    .vibrationMotorPresent = true,
};

const int TOUCH_BUTTON_MAP[TOUCH_SENSOR_NUM_BUTTONS] =  {0,2,3,4,5,6,7,8,9};

