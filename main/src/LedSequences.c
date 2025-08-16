#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "led_sequences_json.hpp"
#include "BatterySensor.h"
#include "DiskUtilities.h"
#include "LedControl.h"
#include "LedSequences.h"

#ifdef FMAN25_BADGE
#define LED_SEQ_NUM_BUILT_IN_SEQUENCES 4
#else
#define LED_SEQ_NUM_BUILT_IN_SEQUENCES 2
#endif
#define LED_SEQ_NUM_CUSTOM_SEQUENCES 1
#define NUM_LED_SEQUENCES (LED_SEQ_NUM_BUILT_IN_SEQUENCES + LED_SEQ_NUM_CUSTOM_SEQUENCES)

static const char * TAG = "LEDS";

char * custom_led_sequences[LED_SEQ_NUM_CUSTOM_SEQUENCES] = {0};
char custom_led_sequences_sharecodes[LED_SEQ_NUM_CUSTOM_SEQUENCES][NUM_SHARECODE_BYTES] = {0};

BatterySensor *pBatterySensor = NULL;

static char * user_led_sequences[NUM_LED_SEQUENCES] = {
  (char * )led_seq_default1,
  (char * )led_seq_default2,
#ifdef FMAN25_BADGE
  (char * )led_seq_default3,
  (char * )led_seq_default4,
#endif
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
        ESP_LOGE(TAG, "Invalid index");
        return ESP_FAIL;
    }
    if (sequence_size > MAX_CUSTOM_LED_SEQUENCE_SIZE)
    {
        ESP_LOGE(TAG, "Sequence size too large. %d", sequence_size);
        return ESP_FAIL;
    }

    memset((void *)custom_led_sequences[index], 0, MAX_CUSTOM_LED_SEQUENCE_SIZE);
    memcpy((void *)custom_led_sequences[index], sequence, sequence_size);
    char filename[30] = "";
    snprintf(filename, sizeof(filename), "%s/custom%d.txt", CONFIG_MOUNT_PATH, index);
    // Open and overwrite file

    esp_err_t ret = WriteFileToDisk(pBatterySensor, filename, (void *)custom_led_sequences[index], MAX_CUSTOM_LED_SEQUENCE_SIZE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write badge stats file");
    }
    
    return ESP_OK;
}

esp_err_t LedSequences_Init(BatterySensor *pBatterySensor)
{
  assert(pBatterySensor);

  memset(custom_led_sequences_sharecodes, 0, sizeof(custom_led_sequences_sharecodes));
  for (int i = 0; i < LED_SEQ_NUM_CUSTOM_SEQUENCES; i++)
  {
    custom_led_sequences[i] = (char *)malloc(MAX_CUSTOM_LED_SEQUENCE_SIZE);
    memset((void *)custom_led_sequences[i], 0, MAX_CUSTOM_LED_SEQUENCE_SIZE);

    // Append custom_led_sequences to end of all user_led_sequences
    user_led_sequences[LED_SEQ_NUM_BUILT_IN_SEQUENCES + i] = custom_led_sequences[i];
  }

  // json file loading / creation
  ESP_LOGI(TAG, "JSON file management");
  for (int i = 0; i < LED_SEQ_NUM_CUSTOM_SEQUENCES; i++)
  {
    char filename[30] = "";
    snprintf(filename, sizeof(filename), "%s/custom%d.txt", CONFIG_MOUNT_PATH, i);

    if (ReadFileFromDisk(filename, custom_led_sequences[i], MAX_CUSTOM_LED_SEQUENCE_SIZE, NULL, MAX_CUSTOM_LED_SEQUENCE_SIZE) != ESP_OK)
    {
      WriteFileToDisk(pBatterySensor, filename, custom_led_sequences[i], MAX_CUSTOM_LED_SEQUENCE_SIZE);
    }
  }

  return ESP_OK;
}
