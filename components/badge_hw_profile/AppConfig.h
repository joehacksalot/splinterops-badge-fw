
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>

typedef struct AppConfig_t
{
    bool touchActionCommandEnabled;
    bool buzzerPresent;
    bool eyeGpioLedsPresent;
    bool vibrationMotorPresent;
} AppConfig;

#endif // APP_CONFIG_H