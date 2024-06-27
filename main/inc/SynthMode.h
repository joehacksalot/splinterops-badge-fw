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

typedef struct SynthMode_t
{
    bool initialized;
    bool touchSoundEnabled;
    Song selectedSong;
    int currentNoteIdx;
    CircularBuffer songQueue;
    SemaphoreHandle_t queueMutex;
    SemaphoreHandle_t toneMutex;
    TickType_t nextNotePlayTime;
    SemaphoreHandle_t procSyncMutex;
    NotificationDispatcher* pNotificationDispatcher;
    UserSettings* pUserSettings;
} SynthMode;

esp_err_t SynthMode_Init(SynthMode* this, NotificationDispatcher* pNotificationDispatcher, UserSettings* userSettings);
esp_err_t SynthMode_SetTouchSoundEnabled(SynthMode *this, bool enabled);


#endif // SYNTH_MODE_H_