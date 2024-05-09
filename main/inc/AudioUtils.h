#include <stdint.h>

#include "esp_err.h"

#include "TouchSensor.h"

typedef struct wav_header_t {
    // The "RIFF" chunk descriptor
    uint8_t ChunkID[4];
    int32_t ChunkSize;
    uint8_t Format[4];
    // The "fmt" sub-chunk
    uint8_t Subchunk1ID[4];
    int32_t Subchunk1Size;
    int16_t AudioFormat;
    int16_t NumChannels;
    int32_t SampleRate;
    int32_t ByteRate;
    int16_t BlockAlign;
    int16_t BitsPerSample;
    // The "data" sub-chunk
    uint8_t Subchunk2ID[4];
    int32_t Subchunk2Size;
} wav_header_t;

typedef struct ToneData_t
{
    int freq[TOUCH_SENSOR_NUM_BUTTONS];
} ToneData;

// esp_err_t ToneGenerator_init(ToneGenerator this*);