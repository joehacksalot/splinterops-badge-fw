/**
 * @file TimeUtils.h
 * @brief Time management and timing utility functions
 * 
 * This module provides essential time-related utilities for the badge system including:
 * - Elapsed time calculation in milliseconds
 * - Timer expiration checking
 * - Future time calculation for scheduling
 * - FreeRTOS tick conversion utilities
 * - Current time retrieval functions
 * - Cross-platform time abstraction
 * 
 * These utilities are used throughout the badge system for timing operations,
 * timeouts, scheduling, and performance measurement.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef TIMEUTILS_H_
#define TIMEUTILS_H_

#include "sdkconfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

/**
 * @brief Calculate elapsed time in milliseconds
 * 
 * Calculates the elapsed time in milliseconds since the specified start time.
 * Handles FreeRTOS tick wraparound correctly for accurate timing measurements.
 * 
 * @param startTime Starting time in FreeRTOS ticks
 * @return Elapsed time in milliseconds
 */
uint32_t TimeUtils_GetElapsedTimeMSec(TickType_t startTime);

/**
 * @brief Check if a timer has expired
 * 
 * Determines if the current time has passed the specified end time.
 * Used for timeout checking and timer expiration detection.
 * 
 * @param endTime End time in FreeRTOS ticks to check against
 * @return true if current time >= end time, false otherwise
 */
bool TimeUtils_IsTimeExpired(TickType_t endTime);

/**
 * @brief Calculate future time in ticks
 * 
 * Calculates a future time point by adding the specified milliseconds
 * to the current time. Used for setting timeouts and scheduling events.
 * 
 * @param mSec Milliseconds to add to current time
 * @return Future time in FreeRTOS ticks
 */
TickType_t TimeUtils_GetFutureTimeTicks(uint32_t mSec);

/**
 * @brief Get current time in ticks
 * 
 * Returns the current system time in FreeRTOS ticks. Provides a
 * consistent time reference for timing operations.
 * 
 * @return Current time in FreeRTOS ticks
 */
TickType_t TimeUtils_GetCurTimeTicks(void);

/**
 * @brief Convert ticks to milliseconds
 * 
 * Converts FreeRTOS ticks to milliseconds using the system tick rate.
 * Provides cross-platform time conversion for consistent timing.
 * 
 * @param ticks Time value in FreeRTOS ticks
 * @return Equivalent time in milliseconds
 */
uint32_t TimeUtils_GetMSecFromTicks(uint32_t ticks);

#endif // TIMEUTILS_H_