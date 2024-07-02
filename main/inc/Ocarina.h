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


const OcarinaKeySet OcarinaSongKeySets[] =
{
  {"Zelda's Lullaby",    6, SONG_ZELDAS_LULLABY,        {OCARINA_KEY_X,  OCARINA_KEY_A,  OCARINA_KEY_Y,  OCARINA_KEY_X,  OCARINA_KEY_A,  OCARINA_KEY_Y}},
  {"Epona's Song",       6, SONG_EPONAS_SONG,           {OCARINA_KEY_A,  OCARINA_KEY_X,  OCARINA_KEY_Y,  OCARINA_KEY_A,  OCARINA_KEY_X,  OCARINA_KEY_Y}},
  {"Saria's Song",       6, SONG_SARIAS_SONG,           {OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_X,  OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_X}},
  {"Song of Storms",     6, SONG_SONG_OF_STORMS,        {OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_A,  OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_A}},
  {"Sun's Song",         6, SONG_SUNS_SONG,             {OCARINA_KEY_Y,  OCARINA_KEY_R,  OCARINA_KEY_A,  OCARINA_KEY_Y,  OCARINA_KEY_R,  OCARINA_KEY_A}},
  {"Song of Time",       6, SONG_SONG_OF_TIME,          {OCARINA_KEY_Y,  OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_L,  OCARINA_KEY_R}},
  {"Minuet of Forest",   6, SONG_MINUET_OF_FOREST,      {OCARINA_KEY_L,  OCARINA_KEY_A,  OCARINA_KEY_X,  OCARINA_KEY_Y,  OCARINA_KEY_X,  OCARINA_KEY_Y}},
  {"Bolero of Fire",     8, SONG_BOLERO_OF_FIRE,        {OCARINA_KEY_R,  OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_L,  OCARINA_KEY_Y,  OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_R}},
  {"Serenade of Water",  5, SONG_SERENADE_OF_WATER,     {OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_Y,  OCARINA_KEY_Y,  OCARINA_KEY_X}},
  {"Requiem of Spirit",  6, SONG_REQUIEM_OF_SPIRIT,     {OCARINA_KEY_L,  OCARINA_KEY_R,  OCARINA_KEY_L,  OCARINA_KEY_Y,  OCARINA_KEY_R,  OCARINA_KEY_L}},
  {"Nocturne of Shadow", 7, SONG_NOCTURNE_OF_SHADOW,    {OCARINA_KEY_X,  OCARINA_KEY_Y,  OCARINA_KEY_Y,  OCARINA_KEY_L,  OCARINA_KEY_X,  OCARINA_KEY_Y,  OCARINA_KEY_R}},
  {"Prelude of Light",   6, SONG_PRELUDE_OF_LIGHT,      {OCARINA_KEY_A,  OCARINA_KEY_Y,  OCARINA_KEY_A,  OCARINA_KEY_Y,  OCARINA_KEY_X,  OCARINA_KEY_A}}
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