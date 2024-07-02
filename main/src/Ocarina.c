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

static void Ocarina_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);

esp_err_t Ocarina_Init(Ocarina *this, NotificationDispatcher *pNotificationDispatcher, UserSettings* pUserSettings)
{
    assert(this);
    memset(this, 0, sizeof(Ocarina));
    if (this->initialized == false)
    {
        assert(CircularBuffer_Init(&this->ocarinaKeys, OCARINA_MAX_SONG_KEYS, sizeof(OcarinaKey)) == ESP_OK);
        this->pNotificationDispatcher = pNotificationDispatcher;
        this->pUserSettings = pUserSettings;
        ESP_LOGI(TAG, "Ocarina successfully handcrafted");
        NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &Ocarina_TouchSensorNotificationHandler, this);
        return ESP_OK;
    }

    return ESP_FAIL;
}

static void Ocarina_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");
    Ocarina *this = (Ocarina *)pObj;
    assert(this);
    assert(notificationData);
    TouchSensorEventNotificationData touchNotificationData = *(TouchSensorEventNotificationData *)notificationData;
    if (touchNotificationData.touchSensorEvent == TOUCH_SENSOR_EVENT_TOUCHED)
    {
        OcarinaKey key = touchNotificationData.touchSensorIdx;
        CircularBuffer_PushBack(&this->ocarinaKeys, &key);
        
        // Check the ocarina keys in the buffer against the song key sets
        for (int i = 0; i < sizeof(OcarinaSongKeySets) / sizeof(OcarinaKeySet); i++)
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
                    CircularBuffer_Clear(&this->ocarinaKeys);
                    break;
                }
            }
        }
    }
}