/**
 * @file BadgeStats.h
 * @brief Badge usage statistics tracking and analytics system
 * 
 * This module provides comprehensive usage tracking for badge activities including:
 * - Power-on cycle counting
 * - Touch sensor interaction statistics
 * - LED pattern usage tracking
 * - BLE connection and transfer statistics
 * - Battery check frequency monitoring
 * - Network test and connectivity metrics
 * - UART input activity tracking
 * - Persistent storage of statistics data
 * - Thread-safe statistics updates
 * 
 * The statistics system enables usage analysis and helps optimize
 * badge performance and user experience through data-driven insights.
 * 
 * @author Badge Development Team
 * @date 2024
 */

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

/**
 * @brief Initialize the badge statistics tracking system
 * 
 * Initializes the statistics collection system, loads existing stats
 * from flash storage, and sets up thread-safe tracking mechanisms.
 * 
 * @param this Pointer to BadgeStats instance to initialize
 * @return ESP_OK on success, error code on failure
 */
esp_err_t BadgeStats_Init(BadgeStats *this);

/**
 * @brief Register battery sensor for power-aware statistics
 * 
 * Integrates battery monitoring with statistics tracking to enable
 * power-aware data collection and battery usage analytics.
 * 
 * @param this Pointer to BadgeStats instance
 * @param pBatterySensor Battery sensor for power monitoring
 * @return ESP_OK on success, error code on failure
 */
esp_err_t BadgeStats_RegisterBatterySensor(BadgeStats *this, BatterySensor *pBatterySensor);

/**
 * @brief Increment power-on event counter
 * 
 * Thread-safe increment of the power-on event counter for tracking
 * badge boot cycles and usage patterns.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumPowerOns(BadgeStats *this);

/**
 * @brief Increment touch sensor event counter
 * 
 * Thread-safe increment of touch sensor interaction counter for
 * tracking user engagement and touch sensor usage patterns.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumTouches(BadgeStats *this);

/**
 * @brief Increment touch command counter
 * 
 * Thread-safe increment of touch-based command execution counter
 * for tracking command usage patterns.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumTouchCmds(BadgeStats *this);

/**
 * @brief Increment LED cycle counter
 * 
 * Thread-safe increment of LED animation cycle counter for tracking
 * LED system usage and visual feedback patterns.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumLedCycles(BadgeStats *this);

/**
 * @brief Increment battery check counter
 * 
 * Thread-safe increment of battery monitoring event counter for
 * tracking power management system usage.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBatteryChecks(BadgeStats *this);

/**
 * @brief Increment BLE enable counter
 * 
 * Thread-safe increment of BLE activation event counter for tracking
 * Bluetooth usage patterns and connectivity statistics.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBleEnables(BadgeStats *this);

/**
 * @brief Increment BLE disable counter
 * 
 * Thread-safe increment of BLE deactivation event counter for tracking
 * Bluetooth power management and usage patterns.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBleDisables(BadgeStats *this);

/**
 * @brief Increment BLE sequence transfer counter
 * 
 * Thread-safe increment of BLE sequence data transfer counter for
 * tracking badge-to-badge sequence sharing activities.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBleSeqXfers(BadgeStats *this);

/**
 * @brief Increment BLE settings transfer counter
 * 
 * Thread-safe increment of BLE settings data transfer counter for
 * tracking configuration sharing between badges.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumBleSetXfers(BadgeStats *this);

/**
 * @brief Increment network test counter
 * 
 * Thread-safe increment of network connectivity test counter for
 * tracking WiFi and network diagnostics usage.
 * 
 * @param this Pointer to BadgeStats instance
 */
void BadgeStats_IncrementNumNetworkTests(BadgeStats *this);

#endif // BADGE_STATS_H_
