/**
 * @file TimeUtils.c
 * @brief FreeRTOS tick/time helper utilities.
 *
 * Small collection of helpers to work with FreeRTOS ticks and milliseconds:
 * - Convert ticks to milliseconds and vice-versa.
 * - Compute elapsed time since a start tick.
 * - Check if a future tick time has expired.
 * - Get current tick count or a future tick at +N ms.
 */
 
#include <stdio.h>
#include "TimeUtils.h"

// Reference: 
//    pdMS_TO_TICKS(ms)
//    pdTICKS_TO_MS(ticks)

/**
 * @brief Get elapsed time in milliseconds since a given start tick.
 *
 * @param startTime The starting tick count.
 * @return Elapsed time in milliseconds.
 */
uint32_t TimeUtils_GetElapsedTimeMSec(TickType_t startTime)
{
    TickType_t curTime = xTaskGetTickCount();
    return ((uint32_t)pdTICKS_TO_MS(curTime - startTime));
}

// This is an inclusive expire
/**
 * @brief Check whether the specified end tick time has been reached or passed.
 *
 * Inclusive expire: returns true when current time >= endTime.
 *
 * @param endTime The target tick count to compare against.
 * @return true if expired, false otherwise.
 */
bool TimeUtils_IsTimeExpired(TickType_t endTime)
{
    TickType_t curTime = xTaskGetTickCount();// + pdMS_TO_TICKS(holdTime)
    return ((int)(endTime - curTime) <= 0);
}

/**
 * @brief Get a future tick count mSec milliseconds from now.
 *
 * @param mSec Milliseconds in the future.
 * @return TickType_t representing the future time.
 */
TickType_t TimeUtils_GetFutureTimeTicks(uint32_t mSec)
{
    TickType_t curTime = xTaskGetTickCount();
    return curTime + pdMS_TO_TICKS(mSec);
}

/**
 * @brief Get the current FreeRTOS tick count.
 *
 * @return Current tick count.
 */
TickType_t TimeUtils_GetCurTimeTicks()
{
    return xTaskGetTickCount();
}

/**
 * @brief Convert FreeRTOS tick count to milliseconds.
 *
 * @param ticks Number of ticks.
 * @return Milliseconds represented by the tick count.
 */
uint32_t TimeUtils_GetMSecFromTicks(uint32_t ticks)
{
    return pdTICKS_TO_MS(ticks);
}
