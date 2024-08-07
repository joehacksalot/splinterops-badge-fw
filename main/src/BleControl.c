/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/****************************************************************************
*
* This demo showcases BLE GATT server. It can send adv data, be connected by client.
* Run the gatt_client demo, the client demo will automatically connect to the gatt_server demo.
* Client demo will enable gatt_server's notify after connection. The two devices will then exchange
* data.
*
****************************************************************************/

#define DEBUG_BLE 1


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

// ESP-IDF
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"

// NimBLE
#include "esp_bt.h"
#include "console/console.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/ans/ble_svc_ans.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"


// Application
#include "BleControl.h"
#include "BleControl_Service.h"
#include "BleControl_AdvScan.h"
#include "InteractiveGame.h"
#include "JsonUtils.h"
#include "LedSequences.h"
#include "GameState.h"
#include "NotificationDispatcher.h"
#include "TaskPriorities.h"
#include "UserSettings.h"
#include "Utilities.h"

#define TAG "BLECtrl"

#define BLE_DYNAMIC_SERVICE_ADD 1
#define BLE_DYNAMIC_SERVICE_DELETE 0
extern const struct ble_gatt_svc_def gatt_svr_svcs[];

static BleControl * pBleControl = NULL;

// // Internal Function Declarations
static void _BleControlTask(void *pvParameters);
void _BleControl_RefreshServiceUuid(BleControl *this);
void _BleControl_ServiceEventCallbackHandler(struct ble_gatt_register_ctxt *ctxt, void *arg);
static void _BleControl_ResetCallbackHandler(int reason);
static void _BleControl_SyncCallbackHandler(void);

/*** Maximum number of characteristics with the notify flag ***/
#define MAX_NOTIFY 5

esp_err_t BleControl_Init(BleControl *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings, GameState *pGameState)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    assert(pNotificationDispatcher);
    assert(pUserSettings);
    assert(pGameState);
    
    memset(this, 0, sizeof(*this));

    if (pBleControl == NULL)
    {
        pBleControl = this;
    }

    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pUserSettings = pUserSettings;
    this->pGameState = pGameState;
    this->bleMutex = xSemaphoreCreateMutex();
    assert(this->bleMutex);


    GetBadgeBleDeviceName(this->bleName, sizeof(this->bleName));

    memcpy(this->iwcAdvPayload.badgeId, this->pUserSettings->badgeId, sizeof(this->iwcAdvPayload.badgeId));
    this->iwcAdvPayload.badgeType = GetBadgeType();
    this->iwcAdvPayload.magicNum = EVENT_ADV_MAGIC_NUMBER;
    memset(this->iwcAdvPayload.eventId, 0, sizeof(this->iwcAdvPayload.eventId));

    _BleControl_RefreshServiceUuid(this);

    memset(&this->fileTransferFrameContext, 0, sizeof(this->fileTransferFrameContext));
    this->fileTransferFrameContext.rcvBuffer = malloc(MAX_BLE_FILE_TRANSFER_FILE_SIZE);
    memset((uint8_t*)this->fileTransferFrameContext.rcvBuffer, 0, MAX_BLE_FILE_TRANSFER_FILE_SIZE);

    // Create the esp timer for BLE shutdown
    this->bleServiceDisableTimerHandleArgs.callback = &_BleControl_BleServiceDisableTimeoutEventHandler;
    this->bleServiceDisableTimerHandleArgs.arg = (void*)(this); // argument specified here will be passed to timer callback function
    this->bleServiceDisableTimerHandleArgs.name = "ble-xfer-timeout";
    ESP_ERROR_CHECK(esp_timer_create(&this->bleServiceDisableTimerHandleArgs, &this->bleServiceDisableTimerHandle));

    ESP_ERROR_CHECK(nimble_port_init());
    
    // struct ble_hs_cfg {
    // /**
    //  * An optional callback that gets executed upon registration of each GATT
    //  * resource (service, characteristic, or descriptor).
    //  */
    // ble_gatt_register_fn *gatts_register_cb;

    // /**
    //  * An optional argument that gets passed to the GATT registration
    //  * callback.
    //  */
    // void *gatts_register_arg;

    // /** Security Manager Local Input Output Capabilities */
    // uint8_t sm_io_cap;

    // /** @brief Security Manager OOB flag
    //  *
    //  * If set proper flag in Pairing Request/Response will be set.
    //  */
    // unsigned sm_oob_data_flag:1;

    // /** @brief Security Manager Bond flag
    //  *
    //  * If set proper flag in Pairing Request/Response will be set. This results
    //  * in storing keys distributed during bonding.
    //  */
    // unsigned sm_bonding:1;

    // /** @brief Security Manager MITM flag
    //  *
    //  * If set proper flag in Pairing Request/Response will be set. This results
    //  * in requiring Man-In-The-Middle protection when pairing.
    //  */
    // unsigned sm_mitm:1;

    // /** @brief Security Manager Secure Connections flag
    //  *
    //  * If set proper flag in Pairing Request/Response will be set. This results
    //  * in using LE Secure Connections for pairing if also supported by remote
    //  * device. Fallback to legacy pairing if not supported by remote.
    //  */
    // unsigned sm_sc:1;

    // /** @brief Security Manager Key Press Notification flag
    //  *
    //  * Currently unsupported and should not be set.
    //  */
    // unsigned sm_keypress:1;

    // /** @brief Security Manager Local Key Distribution Mask */
    // uint8_t sm_our_key_dist;

    // /** @brief Security Manager Remote Key Distribution Mask */
    // uint8_t sm_their_key_dist;

    // /** @brief Stack reset callback
    //  *
    //  * This callback is executed when the host resets itself and the controller
    //  * due to fatal error.
    //  */
    // ble_hs_reset_fn *reset_cb;

    // /** @brief Stack sync callback
    //  *
    //  * This callback is executed when the host and controller become synced.
    //  * This happens at startup and after a reset.
    //  */
    // ble_hs_sync_fn *sync_cb;

    // /* XXX: These need to go away. Instead, the nimble host package should
    //  * require the host-store API (not yet implemented)..
    //  */
    // /** Storage Read callback handles read of security material */
    // ble_store_read_fn *store_read_cb;

    // /** Storage Write callback handles write of security material */
    // ble_store_write_fn *store_write_cb;

    // /** Storage Delete callback handles deletion of security material */
    // ble_store_delete_fn *store_delete_cb;

    // /** @brief Storage Status callback.
    //  *
    //  * This callback gets executed when a persistence operation cannot be
    //  * performed or a persistence failure is imminent. For example, if is
    //  * insufficient storage capacity for a record to be persisted, this
    //  * function gets called to give the application the opportunity to make
    //  * room.
    //  */
    // ble_store_status_fn *store_status_cb;


        /**
         * Round-robin status callback.  If a there is insufficient storage capacity
         * for a new record, delete the oldest bond and proceed with the persist
         * operation.
         *
         * Note: This is not the best behavior for an actual product because
         * uninteresting peers could cause important bonds to be deleted.  This is
         * useful for demonstrations and sample apps.
         */
        // ble_store_util_status_rr

    // /** An optional argument that gets passed to the storage status callback. */
    // void *store_status_arg;
    // };
    // /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = _BleControl_ResetCallbackHandler;
    ble_hs_cfg.sync_cb = _BleControl_SyncCallbackHandler;
    ble_hs_cfg.gatts_register_cb = _BleControl_ServiceEventCallbackHandler;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P3);      // ESP_PWR_LVL_P9 is max value
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN ,ESP_PWR_LVL_P3);     // ESP_PWR_LVL_P9 is max value
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT ,ESP_PWR_LVL_P3);  // ESP_PWR_LVL_P9 is max value

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    /* Set the default device name. */
    assert(ble_svc_gap_device_name_set(this->bleName)==0);
    assert(xTaskCreatePinnedToCore(_BleControlTask, "NimbleHostTask", NIMBLE_HS_STACK_SIZE, this, BLE_CONTROL_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    BleControl_StartAdvertisementScan(this);

    return ret;
}

BleControl * BleControl_GetInstance()
{
    return pBleControl;
}


// This callback is executed when the host resets itself and the controller
// due to fatal error.
static void _BleControl_ResetCallbackHandler(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d\n", reason);
}


// This callback is executed when the host and controller become synced.
// This happens at startup and after a reset.
static void _BleControl_SyncCallbackHandler(void)
{
    BleControl *this = BleControl_GetInstance();
    int rc;

    /**
     * Tries to configure the device with at least one Bluetooth address.
     * Addresses are restored in a hardware-specific fashion.
     *
     * @param prefer_random         Whether to attempt to restore a random address
     *                                  before checking if a public address has
     *                                  already been configured.
     *
     * @return                      0 on success;
     *                              BLE_HS_ENOADDR if the device does not have any
     *                                  available addresses.
     *                              Other BLE host core code on error.
     */
    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    /**
     * Determines the best address type to use for automatic address type
     * resolution.  Calculation of the best address type is done as follows:
     *
     * if privacy requested:
     *     if we have a random static address:
     *          --> RPA with static random ID
     *     else
     *          --> RPA with public ID
     *     end
     * else
     *     if we have a random static address:
     *          --> random static address
     *     else
     *          --> public address
     *     end
     * end
     *
     * @param privacy               (0/1) Whether to use a private address.
     * @param out_addr_type         On success, the "own addr type" code gets
     *                                  written here.
     *
     * @return                      0 if an address type was successfully inferred.
     *                              BLE_HS_ENOADDR if the device does not have a
     *                                  suitable address.
     *                              Other BLE host core code on error.
     */
    rc = ble_hs_id_infer_auto(0, &this->ownAddrType);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    /**
     * Determines the best address type to use for automatic address type
     * resolution.  Calculation of the best address type is done as follows:
     *
     * if privacy requested:
     *     if we have a random static address:
     *          --> RPA with static random ID
     *     else
     *          --> RPA with public ID
     *     end
     * else
     *     if we have a random static address:
     *          --> random static address
     *     else
     *          --> public address
     *     end
     * end
     *
     * @param privacy               (0/1) Whether to use a private address.
     * @param out_addr_type         On success, the "own addr type" code gets
     *                                  written here.
     *
     * @return                      0 if an address type was successfully inferred.
     *                              BLE_HS_ENOADDR if the device does not have a
     *                                  suitable address.
     *                              Other BLE host core code on error.
    */
    rc = ble_hs_id_copy_addr(this->ownAddrType, addr_val, NULL);

    ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
                  addr_val[5], addr_val[4], addr_val[3], addr_val[2], addr_val[1], addr_val[0]);
    /* Begin advertising. */
    _BleControl_StartAdvertisement(this, false);
}

static void _BleControlTask(void *pvParameters)
{
    BleControl *this = (BleControl *)pvParameters;
    assert(this);

    nimble_port_run();

    ESP_LOGI(TAG, "_BleControlTask exiting");
}
