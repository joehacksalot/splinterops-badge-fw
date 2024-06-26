#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "AudioUtils.h"
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
#define DEFAULT_LEDC_DURATION 400

static const char * TAG = "SYN";

// middle C - major scale
static int touchFrequencyMapping[TOUCH_SENSOR_NUM_BUTTONS] = 
#if defined (REACTOR_BADGE) || defined (TRON_BADGE)
{
    NOTE_D3, // TOUCH_SENSOR_12_OCLOCK
    NOTE_E3, // TOUCH_SENSOR_1_OCLOCK
    NOTE_F3, // TOUCH_SENSOR_2_OCLOCK
    NOTE_G3, // TOUCH_SENSOR_4_OCLOCK
    NOTE_A4, // TOUCH_SENSOR_5_OCLOCK
    NOTE_B4, // TOUCH_SENSOR_7_OCLOCK
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
    NOTE_A4, // TOUCH_SENSOR_TAIL_FEATHER,             // 4
    NOTE_B4, // TOUCH_SENSOR_LEFT_WING_FEATHER_4,      // 5
    NOTE_C4, // TOUCH_SENSOR_LEFT_WING_FEATHER_3,      // 6
    NOTE_D4, // TOUCH_SENSOR_LEFT_WING_FEATHER_2,      // 7
    NOTE_E4  // TOUCH_SENSOR_LEFT_WING_FEATHER_1,      // 8
};

//   OCARINA_KEY_A = 0,  // D4
//   OCARINA_KEY_X,      // B4
//   OCARINA_KEY_Y,      // A4
//   OCARINA_KEY_R,      // F3
//   OCARINA_KEY_L,      // D3

#endif

static void SynthModeTask(void *pvParameters);
static void SynthMode_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);
static void SynthMode_PlaySongNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData);
static esp_err_t SynthMode_ConfigurePWM(SynthMode *this);
static esp_err_t SynthMode_PlayTone(SynthMode* this, int frequency);
static esp_err_t SynthMode_StopTone(SynthMode* this);


esp_err_t SynthMode_Init(SynthMode *this, NotificationDispatcher *pNotificationDispatcher, UserSettings* pUserSettings)
{
    assert(this);
    memset(this, 0, sizeof(SynthMode));

    if (this->initialized == false)
    {
        this->initialized = true;
        this->audioEnabled = false;
        this->selectedSong = SONG_NONE;
        this->currentNoteIdx = 0;
        this->nextNotePlayTime = 0;
        this->pNotificationDispatcher = pNotificationDispatcher;
        this->pUserSettings = pUserSettings;
        esp_err_t ret = SynthMode_ConfigurePWM(this);
        if (ret == ESP_OK)
        {
            this->initialized = true;
            ESP_LOGI(TAG, "Synth Mode succesfully initialized");
            NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, &SynthMode_TouchSensorNotificationHandler, this);
            NotificationDispatcher_RegisterNotificationEventHandler(this->pNotificationDispatcher, NOTIFICATION_EVENTS_PLAY_SONG, &SynthMode_PlaySongNotificationHandler, this);

            xTaskCreate(SynthModeTask, "SynthModeTask", configMINIMAL_STACK_SIZE * 10, this, SYNTH_MODE_TASK_PRIORITY, NULL);
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
            ESP_LOGI(TAG, "Playing song %s (%d) note %d of %d", pSong->songName, this->selectedSong, this->currentNoteIdx, pSong->numNotes);
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
                ESP_LOGI(TAG, "Note Idx %d - Note: %d  Type: %f  HoldTime: %d  Freq: %d", 
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
                    SynthMode_PlayTone(this, frequency);
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
            }
        }
        else
        {
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
            ESP_LOGD(TAG, "Interrupting Song %d", this->selectedSong);
        }
        ESP_LOGD(TAG, "Settings song to Song %d", song);
        this->selectedSong = song;
        this->currentNoteIdx = 0;
        this->nextNotePlayTime = TimeUtils_GetFutureTimeTicks(0);
        ret = ESP_OK;
    }
    return ret;
}


static esp_err_t SynthMode_PlayTone(SynthMode* this, int frequency)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    // if (this->initialized)
    // {
    //     if (this->audioEnabled)
    //     {
            ESP_LOGI(TAG, "Starting tone at %d", frequency);
            ledc_set_freq(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_TIMER, frequency);
            ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_ON);
            ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);
        // }
        ret = ESP_OK;
    // }

    return ret;
}


static esp_err_t SynthMode_StopTone(SynthMode* this)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;
    if (this->initialized)
    {
        ESP_LOGD(TAG, "Stopping tone");
        ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_OFF);
        ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);
        ret = ESP_OK;
    }

    return ret;
}


esp_err_t SynthMode_SetEnabled(SynthMode *this, bool enabled)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    if (this->initialized)
    {
        ESP_LOGD(TAG, "Setting audio enabled to %s", enabled ? "true" : "false");
        this->audioEnabled = enabled;
        ret = ESP_OK;
    }

    return ret;
}


static void SynthMode_TouchSensorNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGD(TAG, "Handling Touch Sensor Notification");
    SynthMode *this = (SynthMode *)pObj;
    assert(this);
    assert(notificationData);
    TouchSensorEventNotificationData touchNotificationData = *(TouchSensorEventNotificationData *)notificationData;

    
    // if (this->selectedSong == SONG_NONE)
    // {
    //     if (touchNotificationData.touchSensorEvent == TOUCH_SENSOR_EVENT_RELEASED)
    //     {
    //         SynthMode_StopTone(this);
    //     }
    //     else
    //     {
    //         SynthMode_PlayTone(this, touchFrequencyMapping[touchNotificationData.touchSensorIdx]);
    //     }
    // }
}


static void SynthMode_PlaySongNotificationHandler(void *pObj, esp_event_base_t eventBase, int notificationEvent, void *notificationData)
{
    ESP_LOGI(TAG, "Handling Play Song Notification");
    SynthMode *this = (SynthMode *)pObj;
    assert(this);
    assert(notificationData);
    PlaySongEventNotificationData playSongNotificationData = *(PlaySongEventNotificationData *)notificationData;
    
    // global sound disabled
    if (this->pUserSettings->settings.soundEnabled == false)
    {
        ESP_LOGD(TAG, "Sound disabled, not playing audio");
        return;
    }

    SynthMode_PlaySong(this, playSongNotificationData.song);
}
