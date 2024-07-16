
#include "string.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

#include "DiskDefines.h"
#include "DiskUtilities.h"
#include "GameState.h"
#include "NotificationDispatcher.h"
#include "SynthModeNotifications.h"
#include "TaskPriorities.h"
#include "TimeUtils.h"
#include "Utilities.h"

#define GAME_STATUS_FILE_NAME MOUNT_PATH "/game"
#define MUTEX_MAX_WAIT_MS                (50)
#define GAME_HEARTBEAT_INTERVAL_MS       (5*60*1000)
#define GAME_TASK_DELAY_MS               (100)
#define FIRST_HEARTBEAT_POWERON_DELAY_MS (5000)

static const char *TAG = "GME";
static int mapIndices[MAX_PEER_MAP_DEPTH];

static void _GameState_Task(void *pvParameters);
static void _GameState_NotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void _GameState_SendHeartbeatHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static esp_err_t GameState_AddPeerReport(GameState *this, PeerReport *peerReport);
static bool _GameState_IsCurrentEvent(GameState *this);
static bool _GameState_CheckEventIdChanged(GameState *this, char *eventIdB64);
static void _GameState_SetEventId(GameState *this, char *newEventIdB64);
static void _GameState_ResetEventId(GameState *this);
static bool _GameState_TryAddSeenEventId(GameState *this, char *newEventIdB64);
static bool _GameState_IsBlankEvent(char *eventIdB64);
static esp_err_t _GameState_ReadGameStatusDataFileFromDisk(GameState *this);
static esp_err_t _GameState_WriteGameStatusDataFileToDisk(GameState *this);

esp_err_t GameState_Init(GameState *this, NotificationDispatcher *pNotificationDispatcher, BadgeStats *pBadgeStats, UserSettings *pUserSettings, BatterySensor *pBatterySensor)
{
    assert(this);
    memset(this, 0, sizeof(*this));
    for (int i = 0; i < MAX_PEER_MAP_DEPTH; ++i)
    {
        mapIndices[i] = i;
    }

    hashmap_init(&this->peerMap, hashmap_hash_string, strcmp);
    hashmap_init(&this->seenEventMap, hashmap_hash_string, strcmp);
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pBadgeStats = pBadgeStats;
    this->pUserSettings = pUserSettings;
    this->pBatterySensor = pBatterySensor;
    this->gameStateDataMutex = xSemaphoreCreateMutex();
    this->nextHeartBeatTime = TimeUtils_GetFutureTimeTicks(FIRST_HEARTBEAT_POWERON_DELAY_MS);
    this->sendHeartbeatImmediately = false;
    this->gameStatusDataUpdated = false;
    _GameState_ResetEventId(this);
    ESP_LOGI(TAG, "Initialized event id: %s", this->gameStateData.status.eventData.currentEventIdB64);
    assert(this->gameStateDataMutex);
    _GameState_ReadGameStatusDataFileFromDisk(this);
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED, &_GameState_NotificationHandler, this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV, &_GameState_NotificationHandler, this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_SEND_HEARTBEAT, &_GameState_SendHeartbeatHandler, this));
    ESP_ERROR_CHECK(NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_OCARINA_SONG_MATCHED, &_GameState_NotificationHandler, this));
    assert(xTaskCreatePinnedToCore(_GameState_Task, "GameStateTask", configMINIMAL_STACK_SIZE * 3, this, GAME_STATE_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
    return ESP_OK;
}

void GameState_SendHeartBeat(GameState *this, uint32_t waitTimeMs)
{
    ESP_LOGI(TAG, "Current heartbeat time %lu", waitTimeMs);
    this->nextHeartBeatTime = TimeUtils_GetFutureTimeTicks(waitTimeMs);
    this->sendHeartbeatImmediately = false;
    if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        HeartBeatRequest heartBeatRequest = { .gameStateData = this->gameStateData, .badgeStats = this->pBadgeStats->badgeStats, .waitTimeMs = 0, .numPeerReports = this->numPeerReports };
        memcpy(heartBeatRequest.badgeIdB64, this->pUserSettings->badgeIdB64, sizeof(heartBeatRequest.badgeIdB64));
        memcpy(heartBeatRequest.keyB64, this->pUserSettings->keyB64, sizeof(heartBeatRequest.keyB64));
        memcpy(heartBeatRequest.peerReports, this->peerReports, sizeof(heartBeatRequest.peerReports));
        NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_WIFI_HEARTBEAT_READY_TO_SEND, &heartBeatRequest, sizeof(heartBeatRequest), DEFAULT_NOTIFY_WAIT_DURATION);
        hashmap_clear(&this->peerMap);
        memset(this->peerReports, 0, sizeof(this->peerReports));
        if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

static void _GameState_Task(void *pvParameters)
{
    GameState *this = (GameState *)pvParameters;
    assert(this);

    // TEST CODE
    // vTaskDelay(pdMS_TO_TICKS(20000));
    // HeartBeatRequest request = { .badgeIdB64 = "badgeId12345", .keyB64 = "badgekey1234", .gameStateData = { .status = {.currentEventColor = GAMESTATE_EVENTCOLOR_RED, .currentEventIdB64 = "eventId12345", .mSecRemaining = 15*60*1000, .powerLevel = 75}}};
    // NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV, &request, sizeof(request), DEFAULT_NOTIFY_WAIT_DURATION);
    // END TEST CODE

    while(true)
    {
        if (_GameState_IsCurrentEvent(this))
        {
            TickType_t endTime = 0;
            if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
            {
                endTime = this->eventEndTime;
                if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
                }
                if (TimeUtils_IsTimeExpired(endTime))
                {
                    ESP_LOGI(TAG, "Current event ended");
                    _GameState_ResetEventId(this);
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_GAME_EVENT_ENDED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
            }
        }

        if (TimeUtils_IsTimeExpired(this->nextHeartBeatTime) || this->sendHeartbeatImmediately)
        {
            uint32_t waitTimeMs;
            if (_GameState_IsCurrentEvent(this))
            {
                waitTimeMs = EVENT_HEARTBEAT_INTERVAL_MS;
            }
            else
            {
                waitTimeMs = GAME_HEARTBEAT_INTERVAL_MS;
            }

            GameState_SendHeartBeat(this, waitTimeMs);
        }

        if (this->gameStatusDataUpdated)
        {
            this->gameStatusDataUpdated = false;
            _GameState_WriteGameStatusDataFileToDisk(this);
        }

        vTaskDelay(pdMS_TO_TICKS(GAME_TASK_DELAY_MS));
    }
}

static void _GameState_SetEventId(GameState *this, char *newEventIdB64)
{
    if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        memcpy(this->gameStateData.status.eventData.currentEventIdB64, newEventIdB64, EVENT_ID_B64_SIZE);
        if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

static void _GameState_ResetEventId(GameState *this)
{
    if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        uint8_t eventId[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        size_t outlen;

        // Initialize current event id to blank
        mbedtls_base64_encode((uint8_t *)this->gameStateData.status.eventData.currentEventIdB64, EVENT_ID_B64_SIZE, &outlen, eventId, EVENT_ID_SIZE);
        if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
}

static bool _GameState_TryAddSeenEventId(GameState *this, char *newEventIdB64)
{
    bool added = false;
    if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        ESP_LOGI(TAG, "Current seen event map size %d", hashmap_size(&this->seenEventMap));
        char *pIndex = hashmap_get(&this->seenEventMap, newEventIdB64);
        if (pIndex == NULL)
        {
            ESP_LOGI(TAG, "Adding new seen event id %s", newEventIdB64);
            char *pEventIdB64 = calloc(EVENT_ID_B64_SIZE, 1);
            if (pEventIdB64)
            {
                strncpy(pEventIdB64, newEventIdB64, EVENT_ID_B64_SIZE - 1);
                int result = hashmap_put(&this->seenEventMap, pEventIdB64, pEventIdB64);
                added = (result == 0);
                if (!added)
                {
                    ESP_LOGE(TAG, "Failed to add event id [B64] %s to seen event id map, errno %d", newEventIdB64, result);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to allocate memory for event id");
            }
        }
        else
        {
            ESP_LOGI(TAG, "Found seen event id %s", newEventIdB64);
        }

        if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }

    return added;
}

static bool _GameState_IsCurrentEvent(GameState *this)
{
    bool isCurrentEvent = false;
    uint8_t eventId[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    size_t outlen;
    if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        mbedtls_base64_decode(eventId, EVENT_ID_SIZE, &outlen, (uint8_t *)this->gameStateData.status.eventData.currentEventIdB64, EVENT_ID_B64_SIZE - 1);
        if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }

    for (int i = 0; i < EVENT_ID_SIZE; ++i)
    {
        if (eventId[i] != 0)
        {
            isCurrentEvent = true;
            break;
        }
    }

    return isCurrentEvent;
}

static bool _GameState_IsBlankEvent(char *eventIdB64)
{
    bool isBlankEvent = true;
    uint8_t eventId[EVENT_ID_SIZE];
    size_t outlen;
    ESP_LOGI(TAG, "Checking if event id %s is blank", eventIdB64);
    mbedtls_base64_decode(eventId, EVENT_ID_SIZE, &outlen, (uint8_t *)eventIdB64, EVENT_ID_B64_SIZE - 1);
    for (int i = 0; i < EVENT_ID_SIZE; ++i)
    {
        if (eventId[i] != 0)
        {
            ESP_LOGD(TAG, "Event id is not blank");
            isBlankEvent = false;
            break;
        }
    }

    return isBlankEvent;
}

esp_err_t GameState_AddPeerReport(GameState *this, PeerReport *peerReport)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        int *pIndex = hashmap_get(&this->peerMap, peerReport->badgeIdB64);
        if (pIndex != NULL)
        {
            int index = *pIndex;
            // Check if event id changed, if so update
            if (strncmp(this->peerReports[index].eventIdB64, peerReport->eventIdB64, EVENT_ID_B64_SIZE) != 0)
            {
                ESP_LOGI(TAG, "Updating event id for badge id [B64] %s", peerReport->badgeIdB64);
                memcpy(this->peerReports[index].eventIdB64, peerReport->eventIdB64, EVENT_ID_B64_SIZE);
            }

            // Check if peak signal strength is greater (less negative)
            if (peerReport->peakRssi > this->peerReports[index].peakRssi)
            {
                ESP_LOGI(TAG, "Updating peak rssi for badge id [B64] %s", peerReport->badgeIdB64);
                this->peerReports[index].peakRssi = peerReport->peakRssi;
            }

            ret = ESP_OK;
        }
        else
        {
            int hashmapSize = hashmap_size(&this->peerMap);
            if (hashmapSize < MAX_PEER_MAP_DEPTH)
            {
                ESP_LOGI(TAG, "Adding new badge id [B64] %s with event id [B64] %s to peer map", peerReport->badgeIdB64, peerReport->eventIdB64);
                this->peerReports[hashmapSize] = *peerReport;
                int result = hashmap_put(&this->peerMap, this->peerReports[hashmapSize].badgeIdB64, &mapIndices[hashmapSize]);
                if (result != 0)
                {
                    ESP_LOGE(TAG, "Failed to add badge id [B64] %s to peer map", peerReport->badgeIdB64);
                }
                else
                {
                    ret = ESP_OK;
                }
                this->numPeerReports = hashmap_size(&this->peerMap);
            }
            else
            {
                ESP_LOGI(TAG, "Skipping add of new badge id [B64] %s to peer map, map is full", peerReport->badgeIdB64);
            }
        }

        if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
    return ret;
}

static void _GameState_ProcessHeartBeatResponse(GameState *this, HeartBeatResponse response)
{
    assert(this);
    
    ESP_LOGD(TAG, "Processing heartbeat response");
    if (memcmp(&this->gameStateData.status, &response.status, sizeof(this->gameStateData.status)) != 0)
    {
        char oldEventIdB64[EVENT_ID_B64_SIZE];
        if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
        {
            strncpy(oldEventIdB64, this->gameStateData.status.eventData.currentEventIdB64, EVENT_ID_B64_SIZE - 1);
            this->gameStateData.status = response.status;
            ESP_LOGI(TAG, "Old event id: %s", oldEventIdB64);
            ESP_LOGI(TAG, "New status received from cloud. Updating local record");
            if (strncmp(oldEventIdB64, response.status.eventData.currentEventIdB64, EVENT_ID_B64_SIZE) != 0)
            {
                if (!_GameState_IsBlankEvent(response.status.eventData.currentEventIdB64))
                {
                    this->eventEndTime = TimeUtils_GetFutureTimeTicks(this->gameStateData.status.eventData.mSecRemaining);
                    ESP_LOGI(TAG, "New event id: %s  endtime: %lu", this->gameStateData.status.eventData.currentEventIdB64, (uint32_t)this->eventEndTime);
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_GAME_EVENT_JOINED, (void*)this->gameStateData.status.eventData.currentEventIdB64, EVENT_ID_B64_SIZE, DEFAULT_NOTIFY_WAIT_DURATION);                    
                }
                else
                {
                    ESP_LOGI(TAG, "Event ended from cloud response");
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_GAME_EVENT_ENDED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                }
            }
            else
            {
                ESP_LOGD(TAG, "Event id did not change");
            }

            if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
        }
    }
}

bool _GameState_CheckEventIdChanged(GameState *this, char *eventIdB64)
{
    assert(this);
    assert(eventIdB64);
    ESP_LOGI(TAG, "Current event id %s, observed event id %s", this->gameStateData.status.eventData.currentEventIdB64, eventIdB64);
    if (strncmp(this->gameStateData.status.eventData.currentEventIdB64, eventIdB64, EVENT_ID_SIZE) != 0)
    {
        ESP_LOGI(TAG, "Event id changed to %s from %s, sending heartbeat immediately", eventIdB64, this->gameStateData.status.eventData.currentEventIdB64);
        this->sendHeartbeatImmediately = true;
        return true;
    }

    return false;
}

static void _GameState_SendHeartbeatHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    GameState *this = (GameState *)pObj;
    assert(this);
    switch (notificationEvent)
    {
        case NOTIFICATION_EVENTS_SEND_HEARTBEAT:
            ESP_LOGI(TAG, "NOTIFICATION_EVENTS_SEND_HEARTBEAT event");
            this->sendHeartbeatImmediately = true;
            break;
        default:
            break;
    }
}

static void _GameState_NotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    GameState *this = (GameState *)pObj;
    assert(this);
    switch (notificationEvent)
    {
        case NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED:
            if (notificationData != NULL)
            {
                PeerReport peerReport = *((PeerReport *)notificationData);
                ESP_LOGI(TAG, "NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED event with badge id [B64] %s", peerReport.badgeIdB64);
                GameState_AddPeerReport(this, &peerReport);
                if (!_GameState_IsBlankEvent(peerReport.eventIdB64))
                {
                    bool newSeenEventId = _GameState_TryAddSeenEventId(this, peerReport.eventIdB64);
                    if (!_GameState_IsCurrentEvent(this))
                    {
                        if (newSeenEventId)
                        {
                            _GameState_CheckEventIdChanged(this, peerReport.eventIdB64);
                        }
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Currently in event, skipping");
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "Blank event id observed, skipping");
                }
            }
            else
            {
                ESP_LOGE(TAG, "NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED event with NULL notification data");
            }
            break;
        case NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV:
            ESP_LOGI(TAG, "WIFI Response Recv");
            if (notificationData != NULL)
            {
                HeartBeatResponse response = *((HeartBeatResponse *)notificationData);
                _GameState_ProcessHeartBeatResponse(this, response);
            }
            else
            {
                ESP_LOGE(TAG, "NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV event with NULL notification data");
            }
            break;
        case NOTIFICATION_EVENTS_OCARINA_SONG_MATCHED:
            ESP_LOGI(TAG, "Ocarina song match notification received");
            if (notificationData != NULL)
            {
                int songIndex = *((int *)notificationData);
                ESP_LOGI(TAG, "Song index %d matched, checking unlock status", songIndex);
                if (!(this->gameStateData.status.statusData.songUnlockedBits & (1 << songIndex)))
                {
                    ESP_LOGI(TAG, "Unlocked song with index %d", songIndex);
                    this->gameStateData.status.statusData.songUnlockedBits |= (1 << songIndex); 
                    PlaySongEventNotificationData unlockSongNotificationData;
                    unlockSongNotificationData.song = SONG_SECRET_SOUND;
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &unlockSongNotificationData, sizeof(unlockSongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_SEND_HEARTBEAT, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                }
            }
            else
            {
                ESP_LOGE(TAG, "NOTIFICATION_EVENTS_OCARINA_SONG_MATCHED event with NULL notification data");
            }
            break;
        default:
            ESP_LOGE(TAG, "Unexpected notification event: %lu", notificationEvent);
            break;
    }
}


static esp_err_t _GameState_ReadGameStatusDataFileFromDisk(GameState *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);

    GameStatusData gameStatusData;
    if (ReadFileFromDisk(GAME_STATUS_FILE_NAME, (char *)&gameStatusData, sizeof(gameStatusData), NULL, sizeof(gameStatusData)) == ESP_OK)
    {
        if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
        {
            this->gameStateData.status.statusData = gameStatusData;
            ret = ESP_OK;
            if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read game status file");
    }
    return ret;
}
static esp_err_t _GameState_WriteGameStatusDataFileToDisk(GameState *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    if (xSemaphoreTake(this->gameStateDataMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        GameStatusData gameStatusData = this->gameStateData.status.statusData;
        if (xSemaphoreGive(this->gameStateDataMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give badge mutex in %s", __FUNCTION__);
        }
        ret = WriteFileToDisk(this->pBatterySensor, GAME_STATUS_FILE_NAME, (char *)&gameStatusData, sizeof(gameStatusData));
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write game status file");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take badge mutex in %s", __FUNCTION__);
    }
    return ret;
}