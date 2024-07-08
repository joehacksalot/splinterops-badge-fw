#ifndef OCARINA_H_
#define OCARINA_H_

#include "freertos/semphr.h"
#include "esp_err.h"

#include "CircularBuffer.h"
#include "NotificationDispatcher.h"
#include "Song.h"
#include "UserSettings.h"

#define OCARINA_SONG_MAX_NAME_LENGTH 32
#define OCARINA_MAX_SONG_KEYS 8

typedef enum OcarinaKey_t
{
  OCARINA_KEY_L = 0,  // D3
  UNUSED_KEY_E3,
  OCARINA_KEY_R,      // F3
  UNUSED_KEY_G3,
  OCARINA_KEY_Y,      // A4
  OCARINA_KEY_X,      // B4
  UNUSED_KEY_C4,
  OCARINA_KEY_A,      // D4
  UNUSED_KEY_E4,
} OcarinaKey;


typedef struct OcarinaKeySet_t {
  const char Name[OCARINA_SONG_MAX_NAME_LENGTH];
  int NumKeys;
  Song Song;
  OcarinaKey Keys[OCARINA_MAX_SONG_KEYS];
} OcarinaKeySet;


typedef struct Ocarina_t
{
    bool initialized;
    CircularBuffer ocarinaKeys;
    NotificationDispatcher* pNotificationDispatcher;
    UserSettings* pUserSettings;
} Ocarina;

esp_err_t Ocarina_Init(Ocarina* this, NotificationDispatcher* pNotificationDispatcher, UserSettings* userSettings);

#endif // OCARINA_H_