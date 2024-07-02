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
  OcarinaKey Keys[OCARINA_MAX_SONG_KEYS];
  Song Song;
} OcarinaKeySet;


const OcarinaKeySet OcarinaSongKeySets[] =
{
  {"Zelda's Lullaby",    6, [OCARINA_KEY_X,  OCARINA_KEY_A,  OCARINA_KEY_Y,  OCARINA_KEY_X,  OCARINA_KEY_A,  OCARINA_KEY_Y]}, SONG_ZELDAS_LULLABY,
  {"Epona's Song",       6, [OCARINA_KEY_A,  OCARINA_KEY_X,  OCARINA_KEY_Y,  OCARINA_KEY_A,  OCARINA_KEY_X,  OCARINA_KEY_Y]}, SONG_EPONAS_SONG,
  {"Saria's Song",       6, [OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_X,  OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_X]}, SONG_SARIAS_SONG,
  {"Song of Storms",     6, [OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_A,  OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_A]}, SONG_SONG_OF_STORMS,
  {"Sun's Song",         6, [OCARINA_KEY_Y,  OCARINA_KEY_R,  OCARINA_KEY_A,  OCARINA_KEY_Y,  OCARINA_KEY_R,  OCARINA_KEY_A]}, SONG_SUNS_SONG,
  {"Song of Time",       6, [OCARINA_KEY_Y,  OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_L,  OCARINA_KEY_R]}, SONG_SONG_OF_TIME,
  {"Minuet of Forest",   6, [OCARINA_KEY_L,  OCARINA_KEY_A,  OCARINA_KEY_X,  OCARINA_KEY_Y,  OCARINA_KEY_X,  OCARINA_KEY_Y]}, SONG_MINUET_OF_FOREST,
  {"Bolero of Fire",     8, [OCARINA_KEY_R,  OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_L,  OCARINA_KEY_Y,  OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_R]}, SONG_BOLERO_OF_FIRE,
  {"Serenade of Water",  5, [OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_Y,  OCARINA_KEY_X]}, SONG_SERENADE_OF_WATER,
  {"Requiem of Spirit",  6, [OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_L,  OCARINA_KEY_Y,  OCARINA_KEY_R,  OCARINA_KEY_L]}, SONG_REQUIEM_OF_SPIRIT,
  {"Nocturne of Shadow", 7, [OCARINA_KEY_X,  OCARINA_KEY_Y,  OCARINA_KEY_Y,  OCARINA_KEY_L,  OCARINA_KEY_X,  OCARINA_KEY_Y,  OCARINA_KEY_R]}, SONG_NOCTURNE_OF_SHADOW,
  {"Prelude of Light",   6, [OCARINA_KEY_A,  OCARINA_KEY_Y,  OCARINA_KEY_A,  OCARINA_KEY_Y,  OCARINA_KEY_X,  OCARINA_KEY_A]}, SONG_PRELUDE_OF_LIGHT
};

typedef struct Ocarina_t
{
    bool initialized;
    CircularBuffer ocarinaKeys;
    NotificationDispatcher* pNotificationDispatcher;
    UserSettings* pUserSettings;
} Ocarina;

esp_err_t Ocarina_Init(Ocarina* this, NotificationDispatcher* pNotificationDispatcher, UserSettings* userSettings);

#endif // OCARINA_H_