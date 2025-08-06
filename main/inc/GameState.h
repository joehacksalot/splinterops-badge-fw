/**
 * @file GameState.h
 * @brief Interactive game state management and peer communication system
 * 
 * This module provides the core game functionality for multi-badge interactions including:
 * - Game state synchronization across multiple badges
 * - Peer discovery and tracking via heartbeat system
 * - Event-based game coordination
 * - Badge statistics and performance tracking
 * - HTTP-based game server communication
 * - Peer report generation and processing
 * - Game status management and validation
 * 
 * The system enables complex multi-badge gaming experiences with real-time
 * peer communication and centralized game state management through a server backend.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef GAME_STATE_H_
#define GAME_STATE_H_

#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "BadgeStats.h"
#include "BatterySensor.h"
#include "GameTypes.h"
#include "NotificationDispatcher.h"
#include "Ocarina.h"
#include "UserSettings.h"

#define EVENT_HEARTBEAT_INTERVAL_MS (60 * 1000)

typedef HASHMAP(char, char) SeenEventMap_t;

typedef struct HeartBeatRequest_t
{
    GameStateData gameStateData;
    BadgeStatsFile badgeStats;
    PeerReport peerReports[MAX_PEER_MAP_DEPTH];
    uint32_t numPeerReports;
    char badgeIdB64[BADGE_ID_B64_SIZE];
    char keyB64[KEY_B64_SIZE];
    uint32_t waitTimeMs;
} HeartBeatRequest;

typedef struct HeartBeatResponse_t
{
    GameStatus status;
} HeartBeatResponse;

typedef struct GameState_t
{
    SemaphoreHandle_t gameStateDataMutex;
    TickType_t nextHeartBeatTime;
    TickType_t eventEndTime;
    bool sendHeartbeatImmediately;
    bool gameStatusDataUpdated;
    GameStateData gameStateData;
    SeenEventMap_t seenEventMap;
    PeerMap_t peerMap;
    PeerReport peerReports[MAX_PEER_MAP_DEPTH];
    uint32_t numPeerReports;
    NotificationDispatcher *pNotificationDispatcher;
    BadgeStats *pBadgeStats;
    UserSettings *pUserSettings;
    BatterySensor *pBatterySensor;
} GameState;

/**
 * @brief Initialize the game state management system
 * 
 * Initializes the multi-badge game state system including peer discovery,
 * event tracking, heartbeat management, and integration with badge statistics.
 * Sets up mutexes for thread-safe game state access.
 * 
 * @param this Pointer to GameState instance to initialize
 * @param pNotificationDispatcher Notification system for game events
 * @param pBadgeStats Badge statistics for game tracking
 * @param pUserSettings User settings for game preferences
 * @param pBatterySensor Battery sensor for power-aware gaming
 * @return ESP_OK on success, error code on failure
 */
esp_err_t GameState_Init(GameState *this, NotificationDispatcher *pNotificationDispatcher, BadgeStats *pBadgeStats, UserSettings *pUserSettings, BatterySensor *pBatterySensor);

/**
 * @brief Set the current event identifier
 * 
 * Updates the event ID used for badge-to-badge game interactions.
 * This allows badges to participate in event-specific games and
 * ensures proper peer discovery within the same event.
 * 
 * @param this Pointer to GameState instance
 * @param newEventIdB64 New event ID in Base64 encoding
 */
void GameState_SetEventId(GameState *this, char *newEventIdB64);

/**
 * @brief Send a heartbeat to maintain game session
 * 
 * Sends a heartbeat message to maintain active game sessions and
 * update peer badges with current game status. Used for real-time
 * multiplayer game coordination.
 * 
 * @param this Pointer to GameState instance
 * @param waitTimeMs Maximum time to wait for heartbeat completion
 */
void GameState_SendHeartBeat(GameState *this, uint32_t waitTimeMs);

#endif // GAME_STATE_H_