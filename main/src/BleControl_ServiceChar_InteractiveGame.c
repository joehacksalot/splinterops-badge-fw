
#include <string.h>

#include "esp_log.h"

#include "host/ble_gatt.h"

#include "BleControl.h"
#include "NotificationEvents.h"

#define TAG "BleGame"
#define BITS_IN_A_BYTE (8)

extern uint16_t interactive_game_app_gatt_svr_chr_val_handle;

void _BleControl_BleServiceInteractiveGameReset(BleControl *this)
{
    assert(this);
    this->feathersToLightBits.u = 0;
}

esp_err_t _BleControl_BleReceiveInteractiveGameDataAction(BleControl *this, uint8_t * data, int size, bool final)
{
    assert(this);
    esp_err_t ret = ESP_OK;
    if (size == sizeof(uint16_t))
    {
        // uint16_t newBits = *(uint16_t*)data;
        // if (newBits != this->feathersToLightBits.u)
        {
            this->feathersToLightBits.u = *(uint16_t*)data;
            ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_INTERACTIVE_GAME_ACTION, (void*)data, size, DEFAULT_NOTIFY_WAIT_DURATION);
        }
    }
    else
    {
        ESP_LOGE(TAG, "InteractiveGameDataAction Invalid size %d, expected %d", size, sizeof(uint16_t));
        ret = ESP_FAIL;
    }
    return ret;
}

esp_err_t _BleControl_GetInteractiveGameReadResponse(BleControl *this, uint8_t * buffer, uint32_t size, uint16_t * pLength)
{
    assert(this);
    assert(buffer);
    assert(pLength);
    InteractiveGameData response;
    response.u = this->touchSensorsActiveBits.u;
    response.s.active = this->feathersToLightBits.s.active;
    response.s.lastFailed = this->feathersToLightBits.s.lastFailed;
    memcpy(buffer, &response, sizeof(response));
    *pLength = sizeof(this->touchSensorsActiveBits);
    return ESP_OK;
}

void BleControl_SetTouchSensorActive(BleControl *this, uint32_t touchSensorIndex, bool active)
{
    if (touchSensorIndex < sizeof(this->touchSensorsActiveBits) * BITS_IN_A_BYTE)
    {
        uint16_t prevBits = this->touchSensorsActiveBits.u;
        uint16_t mask = (1uL << touchSensorIndex);
        if (active)
        {
            this->touchSensorsActiveBits.u |= mask;
        }
        else
        {
            this->touchSensorsActiveBits.u &= ~mask;
        }

        if (this->touchSensorsActiveBits.u != prevBits)
        {
            /**
             * Send notification (or indication) to any connected devices that have
             * subscribed for notification (or indication) for specified characteristic.
             *
             * @param chr_val_handle        Characteristic value handle
             */
            ble_gatts_chr_updated(interactive_game_app_gatt_svr_chr_val_handle);
            ESP_LOGD(TAG, "Touch Sensor Updated, Pushing Ble Notification");
        }
    }
}