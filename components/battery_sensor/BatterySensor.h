
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
    adc_channel_t adcChannel;
} BatterySensor;

esp_err_t BatterySensor_Init(BatterySensor *this, NotificationDispatcher *pNotificationDispatcher, int adcChannel, int priority, int cpuNumber);
int BatterySensor_GetBatteryPercent(BatterySensor *this);

#endif // BATT_SENSE_TASK_H_
