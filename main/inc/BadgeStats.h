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

typedef struct BadgeStats_t
{
    BadgeStatsFile badgeStats;
    bool updateNeeded;
    SemaphoreHandle_t mutex;
    BatterySensor *pBatterySensor;
} BadgeStats;

esp_err_t BadgeStats_Init(BadgeStats *this);
esp_err_t BadgeStats_RegisterBatterySensor(BadgeStats *this, BatterySensor *pBatterySensor);

void BadgeStats_IncrementNumPowerOns(BadgeStats *this);
void BadgeStats_IncrementNumTouches(BadgeStats *this);
void BadgeStats_IncrementNumTouchCmds(BadgeStats *this);
void BadgeStats_IncrementNumLedCycles(BadgeStats *this);
void BadgeStats_IncrementNumBatteryChecks(BadgeStats *this);
void BadgeStats_IncrementNumBleEnables(BadgeStats *this);
void BadgeStats_IncrementNumBleDisables(BadgeStats *this);
void BadgeStats_IncrementNumBleSeqXfers(BadgeStats *this);
void BadgeStats_IncrementNumBleSetXfers(BadgeStats *this);
void BadgeStats_IncrementNumNetworkTests(BadgeStats *this);

#endif // BADGE_STATS_H_
