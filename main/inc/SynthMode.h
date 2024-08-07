#ifndef SYNTH_MODE_H_
#define SYNTH_MODE_H_

#include "freertos/semphr.h"
#include "esp_err.h"

#include "CircularBuffer.h"
#include "LedControl.h"
#include "NotificationDispatcher.h"
#include "Song.h"
#include "SynthModeNotifications.h"
#include "UserSettings.h"
#include "Utilities.h"
#include "TouchSensor.h"

typedef struct SynthMode_t
{
    bool initialized;
    bool touchSoundEnabled;
    int octaveShift;
    Song selectedSong;
    int currentNoteIdx;
    CircularBuffer songQueue;
    SemaphoreHandle_t queueMutex;
    SemaphoreHandle_t toneMutex;
    TickType_t nextNotePlayTime;
    NotificationDispatcher* pNotificationDispatcher;
    UserSettings* pUserSettings;
} SynthMode;

esp_err_t SynthMode_Init(SynthMode* this, NotificationDispatcher* pNotificationDispatcher, UserSettings* userSettings);
esp_err_t SynthMode_SetTouchSoundEnabled(SynthMode *this, bool enabled, int octaveShift);
bool SynthMode_GetTouchSoundEnabled(SynthMode *this);


#endif // SYNTH_MODE_H_