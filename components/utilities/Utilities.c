#include "esp_system.h"
#include "esp_random.h"
#include "Utilities.h"

uint32_t GetRandomNumber(uint32_t min, uint32_t max)
{
    return (esp_random() % (max - min + 1)) + min;
}
