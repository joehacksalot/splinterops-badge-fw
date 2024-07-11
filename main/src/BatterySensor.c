
#include <stdio.h>
#include <stdlib.h>
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#include "TaskPriorities.h"
#include "BatterySensor.h"
#include "NotificationDispatcher.h"
#include "Utilities.h"

#define BAT_MIN         3.0
#define BAT_MAX         4.18
#define DEFAULT_VREF    1100        // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          // Multisampling
#define DIVIDER_VALUE   2


// Internal Function Declarations
static void CheckEFuse(void);
static void PrintCharValueType(esp_adc_cal_value_t val_type);
static float GetBatteryVoltage(BatterySensor *this);
static void BatterySensorTask(void *pvParameters);

// Internal Constants
static const adc_channel_t channel = ADC_CHANNEL_7;     // GPIO35 if ADC1
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;       // 150 mV ~ 2450 mV
static const adc_unit_t unit = ADC_UNIT_1;

static const char * TAG = "BAT";

// Should be called from app_main to initialize hardware once
esp_err_t BatterySensor_Init(BatterySensor *this, NotificationDispatcher *pNotificationDispatcher)
{
    esp_err_t retVal = ESP_FAIL;
    assert(this);

    if(!this->initialized)
    {
        this->initialized = true;
        this->pNotificationDispatcher = pNotificationDispatcher;

        this->batteryPercentMutex = xSemaphoreCreateMutex();
        assert(this->batteryPercentMutex);

        // Check if Two Point or Vref are burned into eFuse
        CheckEFuse();

        // Configure ADC1
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);

        // Characterize ADC
        this->hwData.adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, this->hwData.adc_chars);
        PrintCharValueType(val_type);
     
        xTaskCreate(BatterySensorTask, "BatterySensorTask", configMINIMAL_STACK_SIZE * 5, this, BATT_SENSE_TASK_PRIORITY, NULL);

        retVal = ESP_OK;
    }

    return retVal;
}

// float BatterySensor_GetBatteryPercent(BatterySensor *this)
// {
//     assert(this);
//     float ret = 0; // TODO: return negative instead of 0 when fail
//     if (xSemaphoreTake(this->batteryPercentMutex, 50 / portTICK_PERIOD_MS) == pdTRUE)
//     {
//         ret = this->batteryPercent;
//         if (xSemaphoreGive(this->batteryPercentMutex) != pdTRUE)
//         {
//             ESP_LOGE(TAG, "Failed to give bat mutex at GetBatteryPercent");
//         }
//     }
//     else
//     {
//         ESP_LOGE(TAG, "Failed to take mutex");
//     }
//     return ret;
// }

int BatterySensor_GetBatteryPercent(BatterySensor *this)
{
    assert(this);
    int ret = 0;
    if (xSemaphoreTake(this->batteryPercentMutex, 50 / portTICK_PERIOD_MS) == pdTRUE)
    {
        ret = (int)this->batteryPercent;
        if (xSemaphoreGive(this->batteryPercentMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give bat mutex at GetBatteryPercent");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        ret = -1;
    }
    return ret;
}

static void BatterySensorTask(void *pvParameters)
{
    BatterySensor *this = (BatterySensor *)pvParameters;
    assert(this);
    registerCurrentTaskInfo();
    
    //Continuously sample ADC1
    while (true)
    {
        this->batteryVoltage = GetBatteryVoltage(this);
        if (xSemaphoreTake(this->batteryPercentMutex, 50 / portTICK_PERIOD_MS) == pdTRUE)
        {
            this->batteryPercent = ((MIN(BAT_MAX, this->batteryVoltage) - BAT_MIN) * 100.0) / (BAT_MAX - BAT_MIN); // keep math bounded in case batter reads above max, percentage will never be over 100%
            if (xSemaphoreGive(this->batteryPercentMutex) != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to give bat mutex in bat task");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void CheckEFuse(void)
{
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
    {
        ESP_LOGD(TAG, "eFuse Two Point: Supported");
    }
    else
    {
        ESP_LOGW(TAG, "eFuse Two Point: NOT supported");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
    {
        ESP_LOGD(TAG, "eFuse Vref: Supported");
    }
    else
    {
        ESP_LOGW(TAG, "eFuse Vref: NOT supported");
    }
}

static void PrintCharValueType(esp_adc_cal_value_t valType)
{
    if (valType == ESP_ADC_CAL_VAL_EFUSE_TP)
    {
        ESP_LOGD(TAG, "Characterized using Two Point Value");
    }
    else if (valType == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        ESP_LOGD(TAG, "Characterized using eFuse Vref");
    }
    else
    {
        ESP_LOGD(TAG, "Characterized using Default Vref");
    }
}

static float GetBatteryVoltage(BatterySensor *this)
{
    assert(this);
    float retVal = 0;
    if(this->initialized)
    {
        uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
            if (unit == ADC_UNIT_1)
            {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            }
            else
            {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, width, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        // Convert adc_reading to voltage
        // Multiply by divider to get full batt voltage
        retVal = (float)esp_adc_cal_raw_to_voltage(adc_reading, this->hwData.adc_chars) / 1000 * DIVIDER_VALUE;
    }
    return retVal;
}
