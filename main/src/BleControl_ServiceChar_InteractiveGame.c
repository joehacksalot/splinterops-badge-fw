
#include <string.h>

#include "esp_log.h"

#include "host/ble_gatt.h"

#include "BleControl.h"

/**
 * @file BleControl_ServiceChar_InteractiveGame.c
 * @brief BLE Interactive Game characteristic implementation.
 *
 * Provides a bidirectional characteristic for simple interactive gameplay:
 * - Write: host sends which feathers to light or last-failed state.
 * - Read: client reads current touch sensor active bits and state flags.
 * - Notify: touch activity changes push updates to subscribers.
 *
 * This module translates ATT writes into internal events and packages state
 * for reads and notifications. It is lightweight to fit into NimBLE host
 * callbacks and defers heavy work to NotificationDispatcher.
 */
#define TAG "BleGame"
#define BITS_IN_A_BYTE (8)

extern uint16_t interactive_game_app_gatt_svr_chr_val_handle;

/**
 * @brief Reset runtime state for interactive game characteristic.
 *
 * Clears the feathers-to-light bitfield so no LEDs are requested.
 *
 * @param this Pointer to BleControl instance.
 */
void _BleControl_BleServiceInteractiveGameReset(BleControl *this)
{
    assert(this);
    this->feathersToLightBits.u = 0;
}

/**
 * @brief Handle a write to the interactive game characteristic.
 *
 * Expects a 16-bit value encoding which feathers to light and failure status.
 * On valid size, updates internal state and notifies the system via
 * NotificationDispatcher.
 *
 * @param this Pointer to BleControl instance.
 * @param data Pointer to received payload (expected sizeof(uint16_t)).
 * @param size Length of payload.
 * @param final Unused; included for API symmetry.
 * @return ESP_OK on success; ESP_FAIL on invalid size.
 */
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

/**
 * @brief Build a read response for the interactive game characteristic.
 *
 * Packages the current touch activity bitmap and feathers-to-light flags into
 * an `InteractiveGameData` response.
 *
 * @param this Pointer to BleControl instance.
 * @param buffer Output buffer to write response into.
 * @param size Size of output buffer.
 * @param pLength Out: number of bytes written.
 * @return ESP_OK on success.
 */
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

/**
 * @brief Set or clear a touch sensor active bit and notify subscribers.
 *
 * If the bitfield changes, triggers a GATT notification for the interactive
 * game characteristic to inform connected clients.
 *
 * @param this Pointer to BleControl instance.
 * @param touchSensorIndex Index of the sensor bit to modify.
 * @param active True to set (active), false to clear.
 */
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
            /*
             * Send a GATT notification (or indication) to any connected devices
             * that have subscribed for updates on this characteristic.
             */
            ble_gatts_chr_updated(interactive_game_app_gatt_svr_chr_val_handle);
            ESP_LOGD(TAG, "Touch Sensor Updated, Pushing Ble Notification");
        }
    }
}