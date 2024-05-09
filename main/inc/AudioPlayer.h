#ifndef AUDIO_TASK_H_
#define AUDIO_TASK_H_

#include "esp_err.h"

#include "NotificationDispatcher.h"
#include "TouchSensor.h"

typedef struct wav_file_node_t {
    char* filename;
    struct wav_file_node_t* next;
} wav_file_node;

typedef struct AudioPlayer_t
{
    bool initialized;
    wav_file_node* file_list;
    int lastPlayed;
    SemaphoreHandle_t procSyncMutex;
    NotificationDispatcher *pNotificationDispatcher;

} AudioPlayer;

char** get_file_list(void);
esp_err_t AudioPlayer_Init(AudioPlayer* this, NotificationDispatcher *pNotificationDispatcher);
esp_err_t play_wav(const char* filepath);

#endif