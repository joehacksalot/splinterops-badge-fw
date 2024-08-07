
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

esp_err_t GameState_Init(GameState *this, NotificationDispatcher *pNotificationDispatcher, BadgeStats *pBadgeStats, UserSettings *pUserSettings, BatterySensor *pBatterySensor);
void GameState_SetEventId(GameState *this, char *newEventIdB64);
void GameState_SendHeartBeat(GameState *this, uint32_t waitTimeMs);

#endif // GAME_STATE_H_