#include "BadgeHwProfile.h"
#include "reactor/BadgeProfile.h"
#include "AppConfig.h"

// Concrete app configuration for the CREST badge
const AppConfig APP_CONFIG = {
    .touchActionCommandEnabled = true,
    .buzzerPresent = true,
    .eyeGpioLedsPresent = true,
    .vibrationMotorPresent = true,
};

const int TOUCH_BUTTON_MAP[TOUCH_SENSOR_NUM_BUTTONS] =  {7,6,4,3,2,5,0,9,8};
