/**
 * @file BatterySensor.h
 * @brief Battery monitoring and power management system
 * 
 * This module provides comprehensive battery monitoring functionality including:
 * - ADC-based battery voltage measurement
 * - Battery percentage calculation and calibration
 * - Low battery detection and warnings
 * - Power management integration
 * - Thread-safe battery status access
 * - Hardware abstraction for different ADC configurations
 * - Integration with notification system for battery events
 * 
 * The system continuously monitors battery levels and provides accurate
 * percentage readings to help users manage power consumption effectively.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef BATT_SENSE_TASK_H_
#define BATT_SENSE_TASK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "NotificationDispatcher.h"

#define BATTERY_NO_FLASH_WRITE_THRESHOLD (10)

typedef struct BatterySensorHwData_t
{
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t adc_init_config;
    adc_oneshot_chan_cfg_t adc_channel_config;
    adc_cali_handle_t adc_cali_chan_handle;
    bool adc_calibrated;
} BatterySensorHwData;

typedef struct BatterySensor_t
{
    bool initialized;
    float batteryPercent;
    float batteryVoltage;
    SemaphoreHandle_t batteryPercentMutex;
    BatterySensorHwData hwData;
    NotificationDispatcher *pNotificationDispatcher;
} BatterySensor;

/**
 * @brief Initialize the battery sensor system
 * 
 * Initializes the ADC for battery voltage monitoring, configures voltage
 * divider calculations, sets up battery level thresholds, and integrates
 * with the notification dispatcher for low battery warnings.
 * 
 * @param this Pointer to BatterySensor instance to initialize
 * @param pNotificationDispatcher Notification system for battery events
 * @return ESP_OK on success, error code on failure
 */
esp_err_t BatterySensor_Init(BatterySensor *this, NotificationDispatcher *pNotificationDispatcher);

/**
 * @brief Get current battery charge percentage
 * 
 * Reads the battery voltage via ADC and converts it to a percentage
 * based on the battery discharge curve. Provides real-time battery
 * status for power management and user feedback.
 * 
 * @param this Pointer to BatterySensor instance
 * @return Battery charge percentage (0-100), or -1 on error
 */
int BatterySensor_GetBatteryPercent(BatterySensor *this);

#endif // BATT_SENSE_TASK_H_
