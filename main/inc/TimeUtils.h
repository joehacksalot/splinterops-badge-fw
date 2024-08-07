#ifndef TIMEUTILS_H_
#define TIMEUTILS_H_

#include "sdkconfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

uint32_t TimeUtils_GetElapsedTimeMSec(TickType_t startTime);

bool TimeUtils_IsTimeExpired(TickType_t endTime);

TickType_t TimeUtils_GetFutureTimeTicks(uint32_t mSec);

TickType_t TimeUtils_GetCurTimeTicks(void);

uint32_t TimeUtils_GetMSecFromTicks(uint32_t ticks);

#endif // TIMEUTILS_H_