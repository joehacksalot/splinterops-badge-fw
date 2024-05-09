#ifndef SYNTH_MODE_H_
#define SYNTH_MODE_H_

#include "esp_err.h"

#include "LedControl.h"
#include "NotificationDispatcher.h"
#include "UserSettings.h"
typedef struct SynthMode_t {
    bool initialized;
    bool audioEnabled;
    SemaphoreHandle_t procSyncMutex;
    NotificationDispatcher* pNotificationDispatcher;
    LedControl* ledControl;
    UserSettings* userSettings;
} SynthMode;

esp_err_t SynthMode_PlayTone(SynthMode *this, int frequency);
esp_err_t SynthMode_Init(SynthMode* this, NotificationDispatcher* pNotificationDispatcher, LedControl* ledControl, UserSettings* userSettings);
esp_err_t SynthMode_SetEnabled(SynthMode *this, bool enabled);


#endif // SYNTH_MODE_H_