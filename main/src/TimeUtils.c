
#include <stdio.h>
#include "TimeUtils.h"

// Reference: 
//    pdMS_TO_TICKS(ms)
//    pdTICKS_TO_MS(ticks)

uint32_t TimeUtils_GetElapsedTimeMSec(TickType_t startTime)
{
    TickType_t curTime = xTaskGetTickCount();
    return ((uint32_t)pdTICKS_TO_MS(curTime - startTime));
}

// This is an inclusive expire
bool TimeUtils_IsTimeExpired(TickType_t endTime)
{
    TickType_t curTime = xTaskGetTickCount();// + pdMS_TO_TICKS(holdTime)
    return ((int)(endTime - curTime) <= 0);
}

TickType_t TimeUtils_GetFutureTimeTicks(uint32_t mSec)
{
    TickType_t curTime = xTaskGetTickCount();
    return curTime + pdMS_TO_TICKS(mSec);
}

TickType_t TimeUtils_GetCurTimeTicks()
{
    return xTaskGetTickCount();
}
