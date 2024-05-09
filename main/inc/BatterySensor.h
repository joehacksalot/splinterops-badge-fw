
#ifndef BATT_SENSE_TASK_H_
#define BATT_SENSE_TASK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_adc_cal.h"

#include "NotificationDispatcher.h"

#define BATTERY_NO_FLASH_WRITE_THRESHOLD (10)

typedef struct BatterySensorHwData_t
{
    esp_adc_cal_characteristics_t *adc_chars;
} BatterySensorHwData;

typedef struct BatterySensor_t
{
    bool initialized;
    float batteryPercent;
    float batteryVoltage;
    SemaphoreHandle_t procSyncMutex;
    BatterySensorHwData hwData;
    NotificationDispatcher *pNotificationDispatcher;
} BatterySensor;

esp_err_t BatterySensor_Init(BatterySensor *this, NotificationDispatcher *pNotificationDispatcher);
int BatterySensor_GetBatteryPercent(BatterySensor *this);

#endif // BATT_SENSE_TASK_H_
