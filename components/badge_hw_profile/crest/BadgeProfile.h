#ifndef BADGE_PROFILE_CREST_H_
#define BADGE_PROFILE_CREST_H_

#include "BadgeType.h"

#define BADGE_TYPE BADGE_TYPE_CREST
#define BADGE_PROFILE_NAME "CREST"
#define BLE_DEVICE_NAME "IWCv3"

typedef enum TouchSensorCrestNames_t
{
    TOUCH_SENSOR_RIGHT_WING_FEATHER_1 = 0, // 0
    TOUCH_SENSOR_RIGHT_WING_FEATHER_2,     // 1
    TOUCH_SENSOR_RIGHT_WING_FEATHER_3,     // 2
    TOUCH_SENSOR_RIGHT_WING_FEATHER_4,     // 3
    TOUCH_SENSOR_TAIL_FEATHER,             // 4
    TOUCH_SENSOR_LEFT_WING_FEATHER_4,      // 5
    TOUCH_SENSOR_LEFT_WING_FEATHER_3,      // 6
    TOUCH_SENSOR_LEFT_WING_FEATHER_2,      // 7
    TOUCH_SENSOR_LEFT_WING_FEATHER_1,      // 8
    
    TOUCH_SENSOR_NUM_BUTTONS // 9
} TouchSensorNames;

// Crest Pin Map Notes:
// touch pin 0 -> 0 idx
// touch pin 1 -> no response
// touch pin 2 -> 1 idx
// touch pin 3 -> 2 idx
// touch pin 4 -> 3 idx
// touch pin 5 -> 4 idx
// touch pin 6 -> 5 idx
// touch pin 7 -> 6 idx
// touch pin 8 -> 7 idx
// touch pin 9 -> 8 idx

#endif // BADGE_PROFILE_CREST_H_
