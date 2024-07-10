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
#define OCARINA_NUM_SONGS 12

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


typedef struct OcarinaSongStatus_t
{
    bool unlocked;
} OcarinaSongStatus;


typedef struct Ocarina_t
{
    bool initialized;
    bool enabled;
    CircularBuffer ocarinaKeys;
    OcarinaSongStatus songStatus[OCARINA_NUM_SONGS];
    NotificationDispatcher* pNotificationDispatcher;
} Ocarina;


esp_err_t Ocarina_Init(Ocarina* this, NotificationDispatcher* pNotificationDispatcher);
esp_err_t Ocarina_SetModeEnabled(Ocarina *this, bool enabled);

#endif // OCARINA_H_