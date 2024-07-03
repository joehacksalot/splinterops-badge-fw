
#ifndef OCARINA_SONG_KEYS_H_
#define OCARINA_SONG_KEYS_H_

#define OCARINA_SONG_MAX_NAME_LENGTH 32
#define OCARINA_MAX_SONG_KEYS 8
#define OCARINA_NUM_SONGS 

typedef enum OcarinaKey_t
{
  OCARINA_KEY_A_L,      // D3
  OCARINA_KEY_DOWN_R,   // F3
  OCARINA_KEY_RIGHT_Y,  // A4
  OCARINA_KEY_LEFT_X,   // B4
  OCARINA_KEY_UP_A = 0, // D4
} OcarinaKey;


typedef struct OcarinaKeySet_t {
  const char Name[OCARINA_SONG_MAX_NAME_LENGTH];
  int NumKeys;
  OcarinaKey Keys[OCARINA_MAX_SONG_KEYS];
} OcarinaKeySet;


const OcarinaKeySet OcarinaSongKeySets[] =
{
  {"Zelda's Lullaby",    6, {OCARINA_KEY_LEFT_X,  OCARINA_KEY_UP_A,    OCARINA_KEY_RIGHT_Y, OCARINA_KEY_LEFT_X,   OCARINA_KEY_UP_A,    OCARINA_KEY_RIGHT_Y                                            }},
  {"Epona's Song",       6, {OCARINA_KEY_UP_A,    OCARINA_KEY_LEFT_X,  OCARINA_KEY_RIGHT_Y, OCARINA_KEY_UP_A,     OCARINA_KEY_LEFT_X,  OCARINA_KEY_RIGHT_Y                                            }},
  {"Saria's Song",       6, {OCARINA_KEY_DOWN_R,  OCARINA_KEY_RIGHT_Y, OCARINA_KEY_LEFT_X,  OCARINA_KEY_DOWN_R,   OCARINA_KEY_RIGHT_Y, OCARINA_KEY_LEFT_X                                             }},
  {"Song of Storms",     6, {OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R,  OCARINA_KEY_UP_A,    OCARINA_KEY_A_L,      OCARINA_KEY_DOWN_R,  OCARINA_KEY_UP_A                                               }},
  {"Sun's Song",         6, {OCARINA_KEY_RIGHT_Y, OCARINA_KEY_DOWN_R,  OCARINA_KEY_UP_A,    OCARINA_KEY_RIGHT_Y,  OCARINA_KEY_DOWN_R,  OCARINA_KEY_UP_A                                               }},
  {"Song of Time",       6, {OCARINA_KEY_RIGHT_Y, OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R,  OCARINA_KEY_RIGHT_Y,  OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R                                             }},
  {"Minuet of Forest",   6, {OCARINA_KEY_A_L,     OCARINA_KEY_UP_A,    OCARINA_KEY_LEFT_X,  OCARINA_KEY_RIGHT_Y,  OCARINA_KEY_LEFT_X,  OCARINA_KEY_RIGHT_Y                                            }},
  {"Bolero of Fire",     8, {OCARINA_KEY_DOWN_R,  OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R,  OCARINA_KEY_A_L,      OCARINA_KEY_RIGHT_Y, OCARINA_KEY_DOWN_R,  OCARINA_KEY_RIGHT_Y,  OCARINA_KEY_DOWN_R  }},
  {"Serenade of Water",  5, {OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R,  OCARINA_KEY_RIGHT_Y, OCARINA_KEY_RIGHT_Y,  OCARINA_KEY_LEFT_X                                                                  }},
  {"Requiem of Spirit",  6, {OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R,  OCARINA_KEY_A_L,     OCARINA_KEY_RIGHT_Y,  OCARINA_KEY_DOWN_R,  OCARINA_KEY_A_L                                                }},
  {"Nocturne of Shadow", 7, {OCARINA_KEY_LEFT_X,  OCARINA_KEY_RIGHT_Y, OCARINA_KEY_RIGHT_Y, OCARINA_KEY_A_L,      OCARINA_KEY_LEFT_X,  OCARINA_KEY_RIGHT_Y, OCARINA_KEY_DOWN_R                        }},
  {"Prelude of Light",   6, {OCARINA_KEY_UP_A,    OCARINA_KEY_RIGHT_Y, OCARINA_KEY_UP_A,    OCARINA_KEY_RIGHT_Y,  OCARINA_KEY_LEFT_X,  OCARINA_KEY_UP_A                                               }}
  // {"Scarecrow's Song",   8, {OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R,  OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R,   OCARINA_KEY_A_L,     OCARINA_KEY_DOWN_R,  OCARINA_KEY_A_L,      OCARINA_KEY_DOWN_R  }}
};

#endif // OCARINA_SONG_KEYS_H_
