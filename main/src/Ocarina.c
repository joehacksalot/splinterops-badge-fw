#include <string.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "NotificationDispatcher.h"
#include "Ocarina.h"
#include "SynthModeNotifications.h"
#include "TouchSensor.h"

static const char *TAG = "OCAR";

const OcarinaKeySet OcarinaSongKeySets[OCARINA_NUM_SONGS] =
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

//   OCARINA_KEY_A       D4   TOUCH_SENSOR_LEFT_WING_FEATHER_2    T7
//   OCARINA_KEY_X       B3   TOUCH_SENSOR_LEFT_WING_FEATHER_4    T5
//   OCARINA_KEY_Y       A3   TOUCH_SENSOR_TAIL_FEATHER           T4
//   OCARINA_KEY_R       F3   TOUCH_SENSOR_RIGHT_WING_FEATHER_3   T2
//   OCARINA_KEY_L       D3   TOUCH_SENSOR_RIGHT_WING_FEATHER_1   T0


static void Ocarina_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);

esp_err_t Ocarina_Init(Ocarina *this, NotificationDispatcher *pNotificationDispatcher, UserSettings* pUserSettings)
{
    assert(this);
    memset(this, 0, sizeof(Ocarina));
    if (this->initialized == false)
    {
        this->initialized = true;
        assert(CircularBuffer_Init(&this->ocarinaKeys, OCARINA_MAX_SONG_KEYS, sizeof(OcarinaKey)) == ESP_OK);
        this->enabled = false;
        this->pNotificationDispatcher = pNotificationDispatcher;
        this->pUserSettings = pUserSettings;
        ESP_LOGI(TAG, "Ocarina successfully handcrafted");
        NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &Ocarina_TouchSensorNotificationHandler, this);
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t Ocarina_SetModeEnabled(Ocarina *this, bool enabled)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;
    if (this->initialized)
    {
        ESP_LOGI(TAG, "Setting Ocarina enabled to %s", enabled ? "true" : "false");
        this->enabled = enabled;
        ret = ESP_OK;
    }

    return ret;
}

static void Ocarina_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");
    Ocarina *this = (Ocarina *)pObj;
    assert(this);
    assert(notificationData);
    TouchSensorEventNotificationData *touchNotificationData = (TouchSensorEventNotificationData *)notificationData;
    if (!this->enabled)
    {
        return;
    }

    if (touchNotificationData->touchSensorEvent == TOUCH_SENSOR_EVENT_TOUCHED)
    {
        OcarinaKey key = touchNotificationData->touchSensorIdx;
        if (CircularBuffer_Count(&this->ocarinaKeys) == OCARINA_MAX_SONG_KEYS)
        {
            OcarinaKey discard;
            CircularBuffer_PopFront(&this->ocarinaKeys, &discard);
        }

        CircularBuffer_PushBack(&this->ocarinaKeys, &key);
        
        // Check the ocarina keys in the buffer against the song key sets
        for (int i = 0; i < OCARINA_NUM_SONGS; i++)
        {
            OcarinaKeySet songKeySet = OcarinaSongKeySets[i];
            if (CircularBuffer_Count(&this->ocarinaKeys) >= songKeySet.NumKeys)
            {
                if (CircularBuffer_MatchSequence(&this->ocarinaKeys, songKeySet.Keys, songKeySet.NumKeys) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Song Matched: %s", songKeySet.Name);
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_OCARINA_SONG_MATCHED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                    PlaySongEventNotificationData successPlaySongNotificationData;
                    successPlaySongNotificationData.song = SONG_SUCCESS_SOUND;
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &successPlaySongNotificationData, sizeof(successPlaySongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    PlaySongEventNotificationData ocarinaPlaySongNotificationData;
                    ocarinaPlaySongNotificationData.song = songKeySet.Song;
                    NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &ocarinaPlaySongNotificationData, sizeof(ocarinaPlaySongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    if (!this->songStatus[i].unlocked)
                    {
                        ESP_LOGI(TAG, "Unlocked song: %s", songKeySet.Name);
                        this->songStatus[i].unlocked = true;
                        PlaySongEventNotificationData unlockSongNotificationData;
                        unlockSongNotificationData.song = SONG_SECRET_SOUND;
                        NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &unlockSongNotificationData, sizeof(unlockSongNotificationData), DEFAULT_NOTIFY_WAIT_DURATION);
                    }

                    CircularBuffer_Clear(&this->ocarinaKeys);
                    break;
                }
            }
        }
    }
}