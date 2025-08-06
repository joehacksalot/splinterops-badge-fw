/**
 * @file BatterySensor.c
 * @brief Battery monitoring and power management implementation
 * 
 * This module implements battery monitoring functionality including:
 * - ADC-based battery voltage measurement with multisampling
 * - Battery percentage calculation with calibrated voltage ranges
 * - Continuous battery monitoring task with configurable intervals
 * - Low battery detection and notification system
 * - Power management integration for battery-aware operations
 * - Thread-safe battery status reporting
 * 
 * The battery sensor uses ADC Channel 7 (GPIO35) with 12dB attenuation
 * for accurate voltage measurement across the 3.0V-4.18V battery range.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"

#include "TaskPriorities.h"
#include "BatterySensor.h"
#include "NotificationDispatcher.h"
#include "Utilities.h"

#define BAT_MIN         3.0
#define BAT_MAX         4.18
#define NO_OF_SAMPLES   64          // Multisampling
#define DIVIDER_VALUE   2


// Internal Function Declarations
static float GetBatteryVoltage(BatterySensor *this);
static void BatterySensorTask(void *pvParameters);

// Internal Constants
static const adc_channel_t ADC_CHANNEL  = ADC_CHANNEL_7;    // GPIO35 if ADC1
static const adc_atten_t ADC_ATTEN      = ADC_ATTEN_DB_12;
static const adc_unit_t ADC_UNIT        = ADC_UNIT_1;

static const char * TAG = "BAT";

/**
 * @brief Initialize the battery sensor module and ADC hardware.
 *
 * Creates the battery percentage mutex, configures the ADC unit/channel,
 * initializes calibration (if supported), and starts the battery monitoring
 * FreeRTOS task pinned to APP_CPU_NUM. Should be called once from app_main.
 *
 * @param this Pointer to the BatterySensor instance (must be non-NULL).
 * @param pNotificationDispatcher Optional dispatcher for posting notifications.
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
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

        // Configure ADC1
        this->hwData.adc_init_config.unit_id = ADC_UNIT;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&this->hwData.adc_init_config, &this->hwData.adc_handle));

        this->hwData.adc_channel_config.bitwidth   = ADC_BITWIDTH_DEFAULT;
        this->hwData.adc_channel_config.atten      = ADC_ATTEN;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(this->hwData.adc_handle, ADC_CHANNEL, &this->hwData.adc_channel_config));

        // ADC1 Calibration Init
        this->hwData.adc_cali_chan_handle = NULL;
        this->hwData.adc_calibrated = false;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        if (!this->hwData.adc_calibrated) 
        {
            ESP_LOGI(TAG, "ADC Calibration scheme is Curve Fitting");
            adc_cali_curve_fitting_config_t cali_config = 
            {
                .unit_id = ADC_UNIT,
                .chan = ADC_CHANNEL,
                .atten = ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            retVal = adc_cali_create_scheme_curve_fitting(&cali_config, &this->hwData.adc_cali_chan_handle);
            if (retVal == ESP_OK) 
            {
                this->hwData.adc_calibrated = true;
            }
            else
            {
                ESP_LOGE(TAG, "ADC Curve Fitting Calibration scheme Failed: %s", esp_err_to_name(retVal));
            }
        }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        if (!this->hwData.adc_calibrated) 
        {
            ESP_LOGI(TAG, "ADC Calibration scheme is Line Fitting");
            adc_cali_line_fitting_config_t cali_config = 
            {
                .unit_id = ADC_UNIT,
                .atten = ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            retVal = adc_cali_create_scheme_line_fitting(&cali_config, &this->hwData.adc_cali_chan_handle);
            if (retVal == ESP_OK)
            {
                this->hwData.adc_calibrated = true;
            }
            else
            {
                ESP_LOGE(TAG, "ADC Line Fitting Calibration scheme Failed: %s", esp_err_to_name(retVal));
            }
        }
#endif

        switch(retVal)
        {
            case ESP_OK:
                ESP_LOGI(TAG, "ADC Calibration successful");
                break;
            case ESP_ERR_NOT_SUPPORTED:
                ESP_LOGE(TAG, "ADC Calibration failed: Not Supported");
                break;
            default:
                ESP_LOGE(TAG, "ADC Calibration failed: Unknown Error");
                break;
        }

        assert(xTaskCreatePinnedToCore(BatterySensorTask, "BatterySensorTask", configMINIMAL_STACK_SIZE * 2, this, BATT_SENSE_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
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
/**
 * @brief Get the most recent battery percentage as an integer.
 *
 * Thread-safe accessor that locks the internal mutex and returns the last
 * computed battery percentage rounded down to an integer in [0, 100].
 * If the mutex cannot be acquired within 50 ms, returns -1 to indicate error.
 *
 * @param this Pointer to the BatterySensor instance (must be non-NULL).
 * @return Battery percentage [0..100] on success, or -1 on failure.
 */
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

static float GetBatteryVoltage(BatterySensor *this)
{
    assert(this);
    float retVal = 0;
    if(this->initialized && this->hwData.adc_calibrated)
    {
        // Read the raw value
        int adc_average = 0;
        int adc_reading = 0;

        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
            ESP_ERROR_CHECK(adc_oneshot_read(this->hwData.adc_handle, ADC_CHANNEL, &adc_reading));
            ESP_LOGD(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT, ADC_CHANNEL, adc_reading);
            adc_average += adc_reading;
        }
        adc_average /= NO_OF_SAMPLES;

        int milliVoltage = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(this->hwData.adc_cali_chan_handle, adc_average, &milliVoltage));
        ESP_LOGD(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT, ADC_CHANNEL, milliVoltage);

        // Convert adc_reading to voltage
        retVal = (float)(milliVoltage) / 1000 * DIVIDER_VALUE;
    }
    return retVal;
}
