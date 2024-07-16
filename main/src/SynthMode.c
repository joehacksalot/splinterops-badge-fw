#include <string.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "NotificationDispatcher.h"
#include "TaskPriorities.h"
#include "TouchSensor.h"
#include "Song.h"
#include "SynthMode.h"
#include "TouchSensor.h"
#include "TimeUtils.h"
#include "UserSettings.h"

#define SPEAKER_GPIO_NUM GPIO_NUM_18

#define DEFAULT_LEDC_TIMER LEDC_TIMER_0
#define DEFAULT_LEDC_CHANNEL LEDC_CHANNEL_0
#define DEFAULT_LEDC_DUTY_RESOLUTION LEDC_TIMER_3_BIT
#define DEFAULT_LEDC_SPEED_MODE LEDC_HIGH_SPEED_MODE
#define DEFAULT_LEDC_DUTY_OFF 0
#define DEFAULT_LEDC_DUTY_ON 3
#define DEFAULT_LEDC_FREQ 440

#define MUTEX_MAX_WAIT_MS     (50)

#define DISABLE_SOUND (1)

static const char * TAG = "SYN";

// middle C - major scale
const int touchFrequencyMapping[TOUCH_SENSOR_NUM_BUTTONS] = 
#if defined (REACTOR_BADGE) || defined (TRON_BADGE)
{
    NOTE_D3, // TOUCH_SENSOR_12_OCLOCK
    NOTE_E3, // TOUCH_SENSOR_1_OCLOCK
    NOTE_F3, // TOUCH_SENSOR_2_OCLOCK
    NOTE_G3, // TOUCH_SENSOR_4_OCLOCK
    NOTE_A3, // TOUCH_SENSOR_5_OCLOCK
    NOTE_B3, // TOUCH_SENSOR_7_OCLOCK
    NOTE_C4, // TOUCH_SENSOR_8_OCLOCK
    NOTE_D4, // TOUCH_SENSOR_10_OCLOCK
    NOTE_E4  // TOUCH_SENSOR_11_OCLOCK
};
#elif defined (CREST_BADGE)
{
    NOTE_D3, // TOUCH_SENSOR_RIGHT_WING_FEATHER_1 = 0, // 0
    NOTE_E3, // TOUCH_SENSOR_RIGHT_WING_FEATHER_2,     // 1
    NOTE_F3, // TOUCH_SENSOR_RIGHT_WING_FEATHER_3,     // 2
    NOTE_G3, // TOUCH_SENSOR_RIGHT_WING_FEATHER_4,     // 3
    NOTE_A3, // TOUCH_SENSOR_TAIL_FEATHER,             // 4
    NOTE_B3, // TOUCH_SENSOR_LEFT_WING_FEATHER_4,      // 5
    NOTE_C4, // TOUCH_SENSOR_LEFT_WING_FEATHER_3,      // 6
    NOTE_D4, // TOUCH_SENSOR_LEFT_WING_FEATHER_2,      // 7
    NOTE_E4  // TOUCH_SENSOR_LEFT_WING_FEATHER_1,      // 8
};

//   OCARINA_KEY_A = 0,  // D4 TOUCH_SENSOR_LEFT_WING_FEATHER_2
//   OCARINA_KEY_X,      // B3 TOUCH_SENSOR_LEFT_WING_FEATHER_4
//   OCARINA_KEY_Y,      // A3 TOUCH_SENSOR_TAIL_FEATHER
//   OCARINA_KEY_R,      // F3 TOUCH_SENSOR_RIGHT_WING_FEATHER_3
//   OCARINA_KEY_L,      // D3 TOUCH_SENSOR_RIGHT_WING_FEATHER_1

#endif

static void SynthModeTask(void *pvParameters);
static void SynthMode_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static void SynthMode_PlaySongNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData);
static esp_err_t SynthMode_ConfigurePWM(SynthMode *this);
static esp_err_t SynthMode_StopTone(SynthMode* this);
static esp_err_t SynthMode_PlaySong(SynthMode* this, Song song);
static esp_err_t SynthMode_PlayTone(SynthMode* this, NoteName note);


esp_err_t SynthMode_Init(SynthMode *this, NotificationDispatcher *pNotificationDispatcher, UserSettings* pUserSettings)
{
    assert(this);
    memset(this, 0, sizeof(SynthMode));

    if (this->initialized == false)
    {
        this->initialized = true;
        this->touchSoundEnabled = false;
        this->selectedSong = SONG_NONE;
        this->currentNoteIdx = 0;
        this->nextNotePlayTime = 0;
        this->pNotificationDispatcher = pNotificationDispatcher;
        this->pUserSettings = pUserSettings;
        this->queueMutex = xSemaphoreCreateMutex();
        assert(this->queueMutex);
        this->toneMutex = xSemaphoreCreateMutex();
        assert(this->toneMutex);
        assert(CircularBuffer_Init(&this->songQueue, 10, sizeof(PlaySongEventNotificationData)) == ESP_OK);
        esp_err_t ret = SynthMode_ConfigurePWM(this);
        if (ret == ESP_OK)
        {
            this->initialized = true;
            ESP_LOGI(TAG, "Synth Mode succesfully initialized");
            NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &SynthMode_TouchSensorNotificationHandler, this);
            NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &SynthMode_PlaySongNotificationHandler, this);

            assert(xTaskCreatePinnedToCore(SynthModeTask, "SynthModeTask", configMINIMAL_STACK_SIZE * 2, this, SYNTH_MODE_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
            return ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "Synth Mode Init Failed");
            return ret;
        }
    }

    return ESP_FAIL;
}

static void SynthModeTask(void *pvParameters)
{
    SynthMode* this = (SynthMode*)pvParameters;
    assert(this);
    while (true)
    {
        if (this->selectedSong != SONG_NONE)
        {
            const SongNotes *pSong = GetSong(this->selectedSong);
            ESP_LOGD(TAG, "Playing song %s (%d) note %d of %d", pSong->songName, this->selectedSong, this->currentNoteIdx, pSong->numNotes);
            if (this->currentNoteIdx < pSong->numNotes)
            {
                this->nextNotePlayTime = TimeUtils_GetFutureTimeTicks(0);
                int pauseTime = 50; //msec
                int frequency = (int)GetNoteFrequency(pSong->notes[this->currentNoteIdx].note);
                int holdTimeMs = GetNoteTypeInMilliseconds(pSong->tempo, pSong->notes[this->currentNoteIdx].noteType);
                if (pSong->notes[this->currentNoteIdx].slur == 0)
                {
                    holdTimeMs -= pauseTime;
                }
                ESP_LOGD(TAG, "Note Idx %d - Note: %d  Type: %f  HoldTime: %d  Freq: %d", 
                    this->currentNoteIdx,
                    pSong->notes[this->currentNoteIdx].note,
                    pSong->notes[this->currentNoteIdx].noteType,
                    holdTimeMs,
                    frequency);
                if (pSong->notes[this->currentNoteIdx].note == NOTE_REST)
                {
                    SynthMode_StopTone(this);
                }
                else
                {
                    SynthMode_PlayTone(this, pSong->notes[this->currentNoteIdx].note);
                }
                vTaskDelay(pdMS_TO_TICKS(holdTimeMs));
                if (pSong->notes[this->currentNoteIdx].slur == 0)
                {
                    SynthMode_StopTone(this);
                    vTaskDelay(pdMS_TO_TICKS(pauseTime));
                }
                this->currentNoteIdx++;
            }
            if (this->currentNoteIdx >= pSong->numNotes)
            {
                this->selectedSong = SONG_NONE;
                this->currentNoteIdx = 0;
                SynthMode_StopTone(this);
                ESP_LOGI(TAG, "Finished playing song");
                SongNoteChangeEventNotificationData data;
                data.action = SONG_NOTE_CHANGE_TYPE_SONG_STOP;
                data.note = SONG_NONE;
                NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_SONG_NOTE_ACTION, &data, sizeof(data), DEFAULT_NOTIFY_WAIT_DURATION);
            }
        }
        else
        {
            // if (xSemaphoreTake(this->queueMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
            {
                if (this->songQueue.count > 0)
                {
                    PlaySongEventNotificationData playSongNotificationData;
                    CircularBuffer_PopFront(&this->songQueue, &playSongNotificationData);
                    ESP_LOGI(TAG, "Popped song %d off song queue. %d songs left in queue", playSongNotificationData.song, this->songQueue.count);
                    SynthMode_PlaySong(this, playSongNotificationData.song);
                }
                // if (xSemaphoreGive(this->queueMutex) != pdTRUE)
                // {
                //     ESP_LOGE(TAG, "Failed to give badge queueMutex in %s", __FUNCTION__);
                // }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}


static esp_err_t SynthMode_ConfigurePWM(SynthMode *this)
{
    esp_err_t ret = ESP_FAIL;

    ledc_timer_config_t ledc_timer =
    {
        .duty_resolution = DEFAULT_LEDC_DUTY_RESOLUTION, // resolution of PWM duty
        .freq_hz = DEFAULT_LEDC_FREQ,                    // frequency of PWM signal
        .speed_mode = DEFAULT_LEDC_SPEED_MODE,           // timer mode
        .timer_num = DEFAULT_LEDC_TIMER,                 // timer number
        .clk_cfg = LEDC_AUTO_CLK                         // timer index
    };
    ret = ledc_timer_config(&ledc_timer);
    if (ret == ESP_FAIL || ret == ESP_ERR_INVALID_ARG)
    {
        ESP_LOGE(TAG, "PWM LEDC timer failed to init");
        return ret;
    }

    ledc_channel_config_t ledc_channel =
    {
        .channel    = DEFAULT_LEDC_CHANNEL,
        .duty       = DEFAULT_LEDC_DUTY_OFF,
        .gpio_num   = SPEAKER_GPIO_NUM,
        .speed_mode = DEFAULT_LEDC_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = DEFAULT_LEDC_TIMER
    };
    ret = ledc_channel_config(&ledc_channel);
    if (ret == ESP_FAIL)
    {
        ESP_LOGE(TAG, "PWM LEDC channel failed to init");
        return ret;
    }

    return ret;
}


static esp_err_t SynthMode_PlaySong(SynthMode* this, Song song)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    if (this->initialized)
    {
        if (this->selectedSong != SONG_NONE)
        {
            ESP_LOGI(TAG, "Interrupting Song %d", this->selectedSong);
        }
        SongNoteChangeEventNotificationData data;
        data.action = SONG_NOTE_CHANGE_TYPE_SONG_START;
        data.note = SONG_NONE;
        NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_SONG_NOTE_ACTION, &data, sizeof(data), DEFAULT_NOTIFY_WAIT_DURATION);

        ESP_LOGD(TAG, "Settings song to Song %d", song);
        this->selectedSong = song;
        this->currentNoteIdx = 0;
        this->nextNotePlayTime = TimeUtils_GetFutureTimeTicks(0);
        ret = ESP_OK;
    }
    return ret;
}


static esp_err_t SynthMode_PlayTone(SynthMode* this, NoteName note)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    if (this->initialized)
    {
        if (this->pUserSettings->settings.soundEnabled)
        {
            float frequency = GetNoteFrequency(note);
            SongNoteChangeEventNotificationData data;
            data.action = SONG_NOTE_CHANGE_TYPE_TONE_START;
            data.note = note;
            NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_SONG_NOTE_ACTION, &data, sizeof(data), DEFAULT_NOTIFY_WAIT_DURATION);

            // if (xSemaphoreTake(this->toneMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
            {
                ESP_LOGD(TAG, "Starting tone at %f", frequency);
#if DISABLE_SOUND
                ledc_set_freq(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_TIMER, frequency);
                ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_ON);
                ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);
#endif

                // if (xSemaphoreGive(this->toneMutex) != pdTRUE)
                // {
                //     ESP_LOGE(TAG, "Failed to give badge toneMutex in %s", __FUNCTION__);
                // }
            }
        }
        else
        {
            ESP_LOGI(TAG, "Sound disabled in settings, not playing tone");
        }
        
        ret = ESP_OK;
    }

    return ret;
}


static esp_err_t SynthMode_StopTone(SynthMode* this)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;
    if (this->initialized)
    {
        // if (xSemaphoreTake(this->toneMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
        {
            ESP_LOGD(TAG, "Stopping tone");
            SongNoteChangeEventNotificationData data;
            data.note = SONG_NONE;
            data.action = SONG_NOTE_CHANGE_TYPE_TONE_STOP;
            NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_SONG_NOTE_ACTION, &data, sizeof(data), DEFAULT_NOTIFY_WAIT_DURATION);

            ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_OFF);
            ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);

            // if (xSemaphoreGive(this->toneMutex) != pdTRUE)
            // {
            //     ESP_LOGE(TAG, "Failed to give badge toneMutex in %s", __FUNCTION__);
            // }
        }
        ret = ESP_OK;
    }

    return ret;
}


esp_err_t SynthMode_SetTouchSoundEnabled(SynthMode *this, bool enabled)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    if (this->initialized)
    {
        ESP_LOGI(TAG, "Setting touch sound enabled to %s", enabled ? "true" : "false");
        this->touchSoundEnabled = enabled;
        ret = ESP_OK;
    }

    return ret;
}

bool SynthMode_GetTouchSoundEnabled(SynthMode *this)
{
    assert(this);
    return this->touchSoundEnabled;
}


static void SynthMode_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");
    SynthMode *this = (SynthMode *)pObj;
    assert(this);
    assert(notificationData);
    TouchSensorEventNotificationData touchNotificationData = *(TouchSensorEventNotificationData *)notificationData;

    if (this->selectedSong == SONG_NONE)
    {
        if (touchNotificationData.touchSensorEvent == TOUCH_SENSOR_EVENT_RELEASED)
        {
            SynthMode_StopTone(this);
        }
        else
        {
            if (this->touchSoundEnabled)
            {
                SynthMode_PlayTone(this, touchFrequencyMapping[touchNotificationData.touchSensorIdx]);
            }
        }
    }
}


static void SynthMode_PlaySongNotificationHandler(void *pObj, esp_event_base_t eventBase, int32_t notificationEvent, void *notificationData)
{
    ESP_LOGI(TAG, "Handling Play Song Notification");
    SynthMode *this = (SynthMode *)pObj;
    assert(this);
    assert(notificationData);
    PlaySongEventNotificationData playSongNotificationData = *(PlaySongEventNotificationData *)notificationData;
    
    // global sound disabled
    if (this->pUserSettings->settings.soundEnabled == false)
    {
        ESP_LOGD(TAG, "Sound disabled in settings, not playing song");
        return;
    }

    // if (xSemaphoreTake(this->queueMutex, pdMS_TO_TICKS(MUTEX_MAX_WAIT_MS)) == pdTRUE)
    {
        if (CircularBuffer_PushBack(&this->songQueue, &playSongNotificationData) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to push song to queue");
            return;
        }
        // if (xSemaphoreGive(this->queueMutex) != pdTRUE)
        // {
        //     ESP_LOGE(TAG, "Failed to give badge queueMutex in %s", __FUNCTION__);
        // }
    }
}
