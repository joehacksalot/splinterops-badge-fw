
#ifndef LED_SEQUENCES_H_
#define LED_SEQUENCES_H_

#include "esp_check.h"
#include "BatterySensor.h"

typedef enum LedStatusSequence_e
{
  LED_STATUS_SEQUENCE_BLE_ENABLE = 0,
  LED_STATUS_SEQUENCE_BLE_XFER,
  LED_STATUS_SEQUENCE_ERROR,
  LED_STATUS_SEQUENCE_SUCCESS,

  NUM_LED_STATUS_SEQUENCES,
} LedStatusSequence;

#define NUM_SHARECODE_BYTES 7

esp_err_t LedSequences_Init(BatterySensor *pBatterySensor);
int LedSequences_GetNumLedSequences(void);
int LedSequences_GetCustomLedSequencesOffset(void);
int LedSequences_GetNumCustomLedSequences(void);
int LedSequences_GetNumStatusSequences(void);
const char * LedSequences_GetLedSequenceJson(int index);
esp_err_t LedSequences_UpdateCustomLedSequence(int index, const char * const sequence, int sequence_size);
int GetLedSeqIndexByCustomIndex(int custom_index);
char * LedSequences_GetCustomLedSequenceSharecode(int index);

#define MAX_CUSTOM_LED_SEQUENCE_SIZE (128*1024)

#endif // LED_SEQUENCES_H_