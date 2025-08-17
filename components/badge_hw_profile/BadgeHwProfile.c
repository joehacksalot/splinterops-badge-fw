
#include <string.h>
#include "BadgeHwProfile.h"

extern const int TOUCH_BUTTON_MAP[TOUCH_SENSOR_NUM_BUTTONS];
extern const AppConfig APP_CONFIG;

const int* BadgeHwProfile_TouchButtonMap(void)
{
    return TOUCH_BUTTON_MAP;
}

int BadgeHwProfile_TouchButtonMapSize(void)
{
    return TOUCH_SENSOR_NUM_BUTTONS;
}

void GetBadgeBleDeviceName(char * buffer, uint32_t bufferSize)
{
    strncpy(buffer, (char *)BLE_DEVICE_NAME, bufferSize - 1);
}

const AppConfig * GetBadgeAppConfig(void)
{
    return &APP_CONFIG;
}

