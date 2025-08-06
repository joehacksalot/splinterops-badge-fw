/**
 * @file GameTypes.h
 * @brief Common data types and constants for multi-badge gaming system
 * 
 * This module defines the core data structures and constants used throughout
 * the interactive gaming system including:
 * - Badge identification and authentication types
 * - Peer communication data structures
 * - Game event color coding and bit shifting
 * - Network protocol constants and limits
 * - Hash map types for peer tracking
 * - Base64 encoding size definitions
 * 
 * These types enable secure multi-badge communication and game coordination
 * across the badge network with proper identification and event tracking.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef GAME_TYPES_H_
#define GAME_TYPES_H_

#include "hashmap.h"
#include "Utilities.h"

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
#define RED_SHIFT                     (0)
#define YELLOW_SHIFT                  (1)
#define BLUE_SHIFT                    (2)
#define CYAN_SHIFT                    (3)
#define MAGENTA_SHIFT                 (4)
#define GREEN_SHIFT                   (5)

typedef enum GameState_EventColor_e
{
    GAMESTATE_EVENTCOLOR_RED,
    GAMESTATE_EVENTCOLOR_YELLOW,
    GAMESTATE_EVENTCOLOR_GREEN,
    GAMESTATE_EVENTCOLOR_CYAN,
    GAMESTATE_EVENTCOLOR_BLUE,
    GAMESTATE_EVENTCOLOR_MAGENTA,
    NUM_GAMESTATE_EVENTCOLORS
} GameState_EventColor;

typedef struct GameEventData_t
{
    char currentEventIdB64[EVENT_ID_B64_SIZE];
    GameState_EventColor currentEventColor;
    uint8_t powerLevel;
    uint32_t mSecRemaining;
} GameEventData;

typedef struct GameStatusData_t
{
    uint8_t stoneBits; // Red, Yellow, Blue, Cyan, Magenta, Green
    uint16_t songUnlockedBits;
} GameStatusData;

typedef struct GameStatus_t
{
    GameStatusData statusData;
    GameEventData eventData;
} GameStatus;

typedef struct PeerReport_t // Sent from BleControl 
{
    char badgeIdB64[BADGE_ID_B64_SIZE];
    char eventIdB64[EVENT_ID_B64_SIZE];
    int16_t peakRssi;
    BadgeType badgeType;
} PeerReport;

typedef HASHMAP(char, int) PeerMap_t;

typedef struct GameStateData_t
{
    GameStatus status;
} GameStateData;

#endif // GAME_TYPES_H_