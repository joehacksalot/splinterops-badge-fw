#ifndef GAME_TYPES_H_
#define GAME_TYPES_H_

#include "hashmap.h"

#define BADGE_ID_SIZE                 (8)
#define BADGE_ID_B64_SIZE             (13)
#define KEY_SIZE                      (8)
#define KEY_B64_SIZE                  (13)
#define PAIR_ID_SIZE                  (8)
#define PAIR_ID_B64_SIZE              (13)
#define MAX_PEER_MAP_DEPTH            (25)
#define MAX_OBSERVED_EVENT_QUEUE_SIZE (10)
#define EVENT_ID_SIZE                 (8)
#define EVENT_ID_B64_SIZE             (13)

typedef enum GameState_EventColor_e
{
    GAMESTATE_EVENTCOLOR_RED,
    GAMESTATE_EVENTCOLOR_GREEN,
    GAMESTATE_EVENTCOLOR_YELLOW,
    GAMESTATE_EVENTCOLOR_MAGENTA,
    GAMESTATE_EVENTCOLOR_BLUE,
    GAMESTATE_EVENTCOLOR_CYAN,
    NUM_GAMESTATE_EVENTCOLORS
} GameState_EventColor;

typedef struct GameStatus_t
{
    uint8_t red;
    uint8_t yellow;
    uint8_t blue;
    uint8_t cyan;
    uint8_t magenta;
    uint8_t green;
    char currentEventIdB64[EVENT_ID_B64_SIZE];
    GameState_EventColor currentEventColor;
    uint8_t powerLevel;
    uint32_t mSecRemaining;
} GameStatus;

typedef struct PeerReport_t // Sent from BleControl 
{
    char badgeIdB64[BADGE_ID_B64_SIZE];
    char eventIdB64[EVENT_ID_B64_SIZE];
    int16_t peakRssi;
} PeerReport;

typedef HASHMAP(char, int) PeerMap_t;

typedef struct GameStateData_t
{
    GameStatus status;
} GameStateData;

#endif // GAME_TYPES_H_