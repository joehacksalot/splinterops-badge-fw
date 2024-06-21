#ifndef SYNTH_MODE_H_
#define SYNTH_MODE_H_

#include "esp_err.h"

#include "LedControl.h"
#include "NotificationDispatcher.h"
#include "Song.h"
#include "UserSettings.h"

typedef struct SynthMode_t
{
    bool initialized;
    bool audioEnabled;
    Song selectedSong;
    int currentNoteIdx;
    TickType_t nextNotePlayTime;
    SemaphoreHandle_t procSyncMutex;
    NotificationDispatcher* pNotificationDispatcher;
    UserSettings* pUserSettings;
} SynthMode;

typedef struct PlaySongEventNotificationData_t
{
    Song song;
} PlaySongEventNotificationData;

esp_err_t SynthMode_Init(SynthMode* this, NotificationDispatcher* pNotificationDispatcher, UserSettings* userSettings);
esp_err_t SynthMode_SetEnabled(SynthMode *this, bool enabled);


#endif // SYNTH_MODE_H_