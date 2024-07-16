#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "led_sequences_json.hpp"
#include "BatterySensor.h"
#include "DiskDefines.h"
#include "DiskUtilities.h"
#include "LedControl.h"
#include "LedSequences.h"

#define LED_SEQ_NUM_BUILT_IN_SEQUENCES 3
#define LED_SEQ_NUM_CUSTOM_SEQUENCES 1
#define NUM_LED_SEQUENCES (LED_SEQ_NUM_BUILT_IN_SEQUENCES + LED_SEQ_NUM_CUSTOM_SEQUENCES)

static const char * TAG = "LEDS";

char * custom_led_sequences[LED_SEQ_NUM_CUSTOM_SEQUENCES] = {0};
char custom_led_sequences_sharecodes[LED_SEQ_NUM_CUSTOM_SEQUENCES][NUM_SHARECODE_BYTES] = {0};

BatterySensor *pBatterySensor = NULL;

static char * user_led_sequences[NUM_LED_SEQUENCES] = {
  (char * )led_seq_proto_test_1,
  (char * )led_seq_heat_pulse,
  (char * )led_seq_jose,
  0 // custom led seq 0
};

int LedSequences_GetNumLedSequences(void)
{
  return NUM_LED_SEQUENCES;
}

int LedSequences_GetCustomLedSequencesOffset(void)
{
  return LED_SEQ_NUM_BUILT_IN_SEQUENCES;
}

int LedSequences_GetNumCustomLedSequences(void)
{
  return LED_SEQ_NUM_CUSTOM_SEQUENCES;
}
 
int LedSequences_GetNumStatusSequences(void)
{
  return NUM_LED_STATUS_SEQUENCES;
}

const char * LedSequences_GetLedSequenceJson(int index)
{
  if (index < NUM_LED_SEQUENCES)
  {
    return user_led_sequences[index];
  }
  return 0;
}

char * LedSequences_GetCustomLedSequenceSharecode(int index)
{
  if (index < LED_SEQ_NUM_CUSTOM_SEQUENCES)
  {
    return custom_led_sequences_sharecodes[index];
  }
  return 0;
}

esp_err_t LedSequences_UpdateCustomLedSequence(int index, const char * const sequence, int sequence_size)
{
    if (index >= LED_SEQ_NUM_CUSTOM_SEQUENCES)
    {
        ESP_LOGE(TAG, "Invalid index\n");
        return ESP_FAIL;
    }
    if (sequence_size > MAX_CUSTOM_LED_SEQUENCE_SIZE)
    {
        ESP_LOGE(TAG, "Sequence size too large. %d\n", sequence_size);
        return ESP_FAIL;
    }

    memset((void *)custom_led_sequences[index], 0, MAX_CUSTOM_LED_SEQUENCE_SIZE);
    memcpy((void *)custom_led_sequences[index], sequence, sequence_size);
    char filename[30] = "";
    snprintf(filename, sizeof(filename), "%s/custom%d.txt", MOUNT_PATH, index);
    // Open and overwrite file

    int status = remove(filename); // TODO: not protected from being written at low battery, risk corrupting flash if brownout occurs
    if (status != 0)
    {
        ESP_LOGE(TAG, "Error: unable to remove the file. %s", filename);
    }
    esp_err_t ret = WriteFileToDisk(pBatterySensor, filename, (void *)custom_led_sequences[index], MAX_CUSTOM_LED_SEQUENCE_SIZE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write badge stats file");
    }
    
    return ESP_OK;
}

esp_err_t LedSequences_Init(BatterySensor *pBatterySensorRef)
{
  assert(pBatterySensorRef);

  pBatterySensor = pBatterySensorRef;
  memset(custom_led_sequences_sharecodes, 0, sizeof(custom_led_sequences_sharecodes));
  for (int i = 0; i < LED_SEQ_NUM_CUSTOM_SEQUENCES; i++)
  {
    custom_led_sequences[i] = (char *)malloc(MAX_CUSTOM_LED_SEQUENCE_SIZE);
    memset((void *)custom_led_sequences[i], 0, MAX_CUSTOM_LED_SEQUENCE_SIZE);

    // Append custom_led_sequences to end of all user_led_sequences
    user_led_sequences[LED_SEQ_NUM_BUILT_IN_SEQUENCES + i] = custom_led_sequences[i];
  }

  esp_err_t ret = ESP_OK;
  // json file loading / creation
  ESP_LOGI(TAG, "JSON file management");
  for (int i = 0; i < LED_SEQ_NUM_CUSTOM_SEQUENCES; i++)
  {
    char filename[30] = "";
    snprintf(filename, sizeof(filename), "%s/custom%d.txt", MOUNT_PATH, i);

    FILE * fp = fopen(filename, "rb");
    if (fp != 0)
    {
        // Check file size matches
        fseek(fp, 0L, SEEK_END);
        ssize_t file_size = ftell(fp);
        if (file_size != MAX_CUSTOM_LED_SEQUENCE_SIZE)
        {
            ESP_LOGI(TAG, "Actual: %d, Expected: %d, Overwriting", file_size, MAX_CUSTOM_LED_SEQUENCE_SIZE);
            
            // Overwrite file
            fclose(fp);

            if (pBatterySensor != NULL && BatterySensor_GetBatteryPercent(pBatterySensor) > BATTERY_NO_FLASH_WRITE_THRESHOLD)
            {
                int status = remove(filename); // TODO: not protected from being written at low battery, risk corrupting flash if brownout occurs
                if(status != 0)
                {
                    ESP_LOGE(TAG, "Error: unable to remove the file. %s", filename);
                }
                FILE * fp = fopen(filename, "wb");
                if(fp != NULL)
                {
                  ssize_t bytes_written = fwrite((void *)custom_led_sequences[i], 1, MAX_CUSTOM_LED_SEQUENCE_SIZE, fp);
                  if(bytes_written != MAX_CUSTOM_LED_SEQUENCE_SIZE)
                  {
                    ESP_LOGE(TAG, "Overwrite failed. %d\n", bytes_written);
                    // ret = ESP_FAIL;
                  }
                  fclose(fp);
                }
                else
                {
                  ESP_LOGE(TAG, "Failed to overwrite");
                }
            }
            else
            {
                ESP_LOGE(TAG, "Battery too low to write to flash");
            }
      }
      else
      {
        // File size matches expected, read data into offset
        fseek(fp, 0, SEEK_SET);
        if(fread(custom_led_sequences[i], 1, MAX_CUSTOM_LED_SEQUENCE_SIZE, fp) != MAX_CUSTOM_LED_SEQUENCE_SIZE)
        {
          ESP_LOGW(TAG, "Partial read completed into custom offset %d", i);
        }
        fclose(fp);
      }
    }
    else
    {
      // File doesn't exist, create
      FILE * fp = fopen(filename, "wb");
      if (fp != 0)
      {
        ESP_LOGI(TAG, "Creating file %s with size %d", filename, MAX_CUSTOM_LED_SEQUENCE_SIZE);
        ssize_t bytes_written = fwrite((void *)custom_led_sequences[i], 1, MAX_CUSTOM_LED_SEQUENCE_SIZE, fp);
        fclose(fp);
        if (bytes_written != MAX_CUSTOM_LED_SEQUENCE_SIZE)
        {
          ESP_LOGE(TAG, "Write failed. %d\n", bytes_written);
          // ret = ESP_FAIL;
        }
      }
      else
      {
        ESP_LOGE(TAG, "Creation of %s failed", filename);
        // ret = ESP_FAIL;
      }
    }
  }

  return ret;
}
