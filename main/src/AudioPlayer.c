// #include <stdio.h>
// #include <string.h>

// #include "esp_log.h"
// #include "ff.h"
// #include "diskio.h"
// // #include "driver/ledc.h"
// #include "pwm_audio.h"
// // #include "driver/gpio.h"

// #include "TaskPriorities.h"
// #include "AudioPlayer.h"
// #include "AudioUtils.h"
// #include "DiscUtils.h"
// #include "NotificationDispatcher.h"

// static const int PWM_GPIO = 18;

// static const char * TAG = "AUD";

// // funtion to list get list of wav files from /data
// // wav_file_node* get_file_list(void) {
// //     FRESULT res;
// //     DIR dir;
// //     static FILINFO fno;

// //     fr = f_opendir(&dir, MOUNT_PATH);


// // }


// // esp_err_t AudioPlayer_Init(AudioPlayer *this, NotificationDispatcher *pNotificationDispatcher)
// // {
// //     esp_err_t ret = ESP_FAIL;
// //     assert(this);

// //     if(this->initialized == false) {
// //         this->initialized = true;
// //         this->pNotificationDispatcher = pNotificationDispatcher;

// //         this->procSyncMutex = xSemaphoreCreateMutex();
// //         assert(this->procSyncMutex);

// //         // TODO: create notification dispatcher event for audio
// //         // NotificationDispatcher_NotifyEvent(TODO AUDIO EVENT)

// //         ret = AudioPlayer_InitStorage();

// //         // init pwm audio player
// //         pwm_audio_config_t audioConfig = {
// //             .duty_resolution    = LEDC_TIMER_10_BIT,
// //             .gpio_num_left      = PWM_GPIO,
// //             .ledc_channel_left  = LEDC_CHANNEL_0,
// //             .ledc_timer_sel     = LEDC_TIMER_0,
// //             .ringbuf_len        = 1024 * 8
// //         };

// //         pwm_audio_init(&audioConfig);

// //         ret = ESP_OK;

// //     }

// //     return ret;
// // }

// // esp_err_t play_wav(const char* filepath) {
// //     FILE *fd = NULL;
// //     struct stat file_stat;

// //     if (stat(filepath, &file_stat) == -1) {
// //         ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
// //         return ESP_FAIL;
// //     }

// //     ESP_LOGI(TAG, "Wav file info: %s (%ld bytes)...", filepath, file_stat.st_size);

// //     fd = open(filepath, "r");

// //     if (fd == NULL) {
// //         ESP_LOGE(TAG, "Failed to open file : %s", filepath);
// //         return ESP_FAIL;
// //     }

// //     const size_t chunk_size = 4096;
// //     uint8_t* buffer = malloc(chunk_size);

// //     if (buffer == NULL) {
// //         ESP_LOGE(TAG, "Failed to allocate audio buffer");
// //         fclose(fd);
// //         return ESP_FAIL;
// //     }
// //     // Read wav file's wav header
// //     wav_header_t wav_header;
// //     int len = fread(&wav_header, 1, sizeof(wav_header_t), fd);
// //     if (len <= 0) {
// //         ESP_LOGE(TAG, "Read wav header failed");
// //         fclose(fd);
// //         return ESP_FAIL;
// //     }
// //     if (NULL == strstr((char *)wav_header.Subchunk1ID, "fmt") &&
// //             NULL == strstr((char *)wav_header.Subchunk2ID, "data")) {
// //         ESP_LOGE(TAG, "Header of wav format error");
// //         fclose(fd);
// //         return ESP_FAIL;
// //     }

// //     ESP_LOGI(TAG, "sample_rate=%"PRIi32", channels=%d, sample_width=%d", wav_head.SampleRate, wav_head.NumChannels, wav_head.BitsPerSample);

// //     pwm_audio_set_param(wav_header.SampleRate, wav_header.BitsPerSample, wav_header.NumChannels);
// //     pwm_audio_start();

// //     // read wave data of WAV file
// //     size_t write_num = 0;
// //     size_t count;

// //     do {
// //         /* Read file in chunks into the scratch buffer */
// //         len = fread(buffer, 1, chunk_size, fd);
// //         if (len <= 0) {
// //             break;
// //         }
// //         pwm_audio_write(buffer, len, &count, 1000 / portTICK_PERIOD_MS);

// //         write_num += len;

// //     } while (1);

// //     pwm_audio_stop();
// //     fclose(fd);
// //     ESP_LOGI(TAG, "File reading complete, total: %d bytes", write_num);
    
// //     return ESP_OK;

// // }