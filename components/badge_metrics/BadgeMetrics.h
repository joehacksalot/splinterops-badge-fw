#ifndef BADGE_STATS_H_
#define BADGE_STATS_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "BatterySensor.h"

typedef struct BadgeStatsFile_t
{
    uint32_t numPowerOns;
    uint32_t numTouches;
    uint32_t numTouchCmds;
    uint32_t numLedCycles;
    uint32_t numBattChecks;
    uint32_t numBleEnables;
    uint32_t numBleDisables;
    uint32_t numBleSeqXfers;
    uint32_t numBleSetXfers;
    uint32_t numUartInputs;
    uint32_t numNetworkTests;
} BadgeStatsFile;

typedef struct BadgeMetrics_t
{
    BadgeStatsFile badgeStats;
    bool updateNeeded;
    SemaphoreHandle_t mutex;
    BatterySensor *pBatterySensor;
} BadgeMetrics;

esp_err_t BadgeMetrics_Init(BadgeMetrics *this);
// esp_err_t BadgeMetrics_RegisterBatterySensor(BadgeMetrics *this, BatterySensor *pBatterySensor);

void BadgeMetrics_IncrementNumPowerOns(BadgeMetrics *this);
void BadgeMetrics_IncrementNumTouches(BadgeMetrics *this);
void BadgeMetrics_IncrementNumTouchCmds(BadgeMetrics *this);
void BadgeMetrics_IncrementNumLedCycles(BadgeMetrics *this);
void BadgeMetrics_IncrementNumBatteryChecks(BadgeMetrics *this);
void BadgeMetrics_IncrementNumBleEnables(BadgeMetrics *this);
void BadgeMetrics_IncrementNumBleDisables(BadgeMetrics *this);
void BadgeMetrics_IncrementNumBleSeqXfers(BadgeMetrics *this);
void BadgeMetrics_IncrementNumBleSetXfers(BadgeMetrics *this);
void BadgeMetrics_IncrementNumNetworkTests(BadgeMetrics *this);

#endif // BADGE_STATS_H_
