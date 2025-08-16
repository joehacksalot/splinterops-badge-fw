#ifndef BADGE_PROFILE_TRON_H_
#define BADGE_PROFILE_TRON_H_

#include "BadgeType.h"

#define BADGE_TYPE BADGE_TYPE_TRON
#define BADGE_PROFILE_NAME "TRON"
#define BLE_DEVICE_NAME "IWCv1"

typedef enum TouchSensorNames_t
{
    TOUCH_SENSOR_12_OCLOCK = 0, // 0
    TOUCH_SENSOR_1_OCLOCK,  // 1
    TOUCH_SENSOR_2_OCLOCK,  // 2
    TOUCH_SENSOR_4_OCLOCK,  // 3
    TOUCH_SENSOR_5_OCLOCK,  // 4
    TOUCH_SENSOR_7_OCLOCK,  // 5
    TOUCH_SENSOR_8_OCLOCK,  // 6
    TOUCH_SENSOR_10_OCLOCK, // 7
    TOUCH_SENSOR_11_OCLOCK, // 8
    TOUCH_SENSOR_NUM_BUTTONS // 9
} TouchSensorNames;

// Reactor Pin Map Notes:
// touch pin 1 -> no response
// touch pin 7 -> 12,6 o' clock -> 0 idx
// touch pin 6 -> 1 o' clock    -> 1 idx
// touch pin 4 -> 2 o' clock    -> 2 idx
// touch pin 3 -> 4 o' clock    -> 3 idx
// touch pin 2 -> 5 o' clock    -> 4 idx
// touch pin 5 -> 7 o' clock    -> 5 idx
// touch pin 0 -> 8 o' clock    -> 6 idx
// touch pin 9 -> 10 o' clock   -> 7 idx
// touch pin 8 -> 11 o' clock   -> 8 idx

#endif // BADGE_PROFILE_TRON_H_
