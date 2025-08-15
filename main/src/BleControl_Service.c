

#include "esp_log.h"

// mBedTls
#include "mbedtls/base64.h"

// NimBLE
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"

// Application
#include "BleControl.h"
#include "BleControl_Service.h"
#include "BleControl_ServiceChar_FileTransfer.h"
#include "BleControl_ServiceChar_InteractiveGame.h"
#include "NotificationEvents.h"

#define TAG "BLE"
#define BLE_DISABLE_TIMER_TIMEOUT_USEC      60 * 1000 * 1000    // 1 minute of service inactivity
#define BLE_PREFERRED_MAX_TX_TIMEOUT_USEC   1500                // 1.5ms
#define BLE_ATT_PREFERRED_MTU               CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU

static esp_err_t _BleControl_BleReceiveDataAction(BleControl *this, BleServiceProfile profileId, uint8_t * data, int size, bool final);
esp_err_t _BleControl_BleReceiveFileDataAction(BleControl *this, uint8_t * data, int size, bool final);
esp_err_t _BleControl_BleReceiveInteractiveGameDataAction(BleControl *this, uint8_t * data, int size, bool final);

static esp_err_t _BleControl_GetBleReadResponse(BleControl *this, BleServiceProfile profileId, uint8_t * buffer, uint32_t size, uint16_t * pLength);
esp_err_t BleControl_BleOnConnectInteractiveGameDataAction(BleControl *this);
esp_err_t BleControl_BleOnDisconnectInteractiveGameDataAction(BleControl *this);
void _BleControl_BleServiceInteractiveGameReset(BleControl *this);

static int _CopyDataFromMBufToBuffer(struct os_mbuf *om, uint16_t min_len, uint16_t max_len, void *dst, uint16_t *len);
static int _BleControl_GattServiceEventHandler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int _BleControl_GapEventHandler(struct ble_gap_event *event, void *arg);

static esp_err_t _BleControl_StartBleServiceDisableTimer(BleControl *this, uint32_t timeoutUsec);
static esp_err_t _BleControl_ResetBleServiceDisableTimer(BleControl *this, uint32_t timeoutUsec);
static esp_err_t _BleControl_StopBleServiceDisableTimer(BleControl *this);

static void dynamic_service_print_conn_desc(struct ble_gap_conn_desc *desc);
static esp_err_t _BleControl_BleServiceNotifyDisconnect(BleControl *this);
static esp_err_t _BleControl_BleServiceNotifyConnect(BleControl *this);
void _BleControl_StartAdvertisement(BleControl *this, bool advertiseService);
void _BleControl_StopAdvertisement(BleControl *this);


/* service uuid */
static ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x8b, 0xff, 0x00, 0x00);

static const ble_uuid128_t serviceUuidBase = 
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 
                     0x00, 0x10, 0x00, 0x00, 0x8b, 0xff, 0x00, 0x00);

/* file app characteristic */
uint16_t file_transfer_app_gatt_svr_chr_val_handle;
static const ble_uuid128_t file_transfer_app_gatt_svr_chr_uuid = 
    BLE_UUID128_INIT(0x77, 0x4e, 0x8a, 0x86, 0xd1, 0xc7, 0x4d, 0xf8,
                     0x8c, 0xa2, 0xda, 0x2b, 0x64, 0x53, 0x3d, 0x4c);
/* file app characteristic descriptor */
static uint8_t file_transfer_app_gatt_svr_dsc_val[] = {0xde, 0xc0, 0xdd, 0xba};
static const ble_uuid128_t file_transfer_app_gatt_svr_dsc_uuid =
    BLE_UUID128_INIT(0x78, 0x4e, 0x8a, 0x86, 0xd1, 0xc7, 0x4d, 0xf8, 
                     0x8c, 0xa2, 0xda, 0x2b, 0x64, 0x53, 0x3d, 0x4c);

/* interactive game app characteristic */
uint16_t interactive_game_app_gatt_svr_chr_val_handle;
static const ble_uuid128_t interactive_game_app_gatt_svr_chr_uuid = 
    BLE_UUID128_INIT(0x77, 0x4f, 0x8a, 0x86, 0xd1, 0xc7, 0x4d, 0xf8, 
                     0x8c, 0xa2, 0xda, 0x2b, 0x64, 0x53, 0x3d, 0x4c);
/* interactive game app characteristic descriptor */
static uint8_t interactive_game_app_gatt_svr_dsc_val[] = {0xfe, 0xca, 0xde, 0xc0};
static const ble_uuid128_t interactive_game_app_gatt_svr_dsc_uuid =
    BLE_UUID128_INIT(0x78, 0x4f, 0x8a, 0x86, 0xd1, 0xc7, 0x4d, 0xf8, 
                     0x8c, 0xa2, 0xda, 0x2b, 0x64, 0x53, 0x3d, 0x4c);


extern struct ble_gatt_chr_def file_transfer_app_gatt_chr_def;
extern struct ble_gatt_chr_def interactive_game_app_gatt_chr_def;
const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service ***/
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        {
            {
                /*** This characteristic can be subscribed to by writing 0x00 and 0x01 to the CCCD ***/
                .uuid = &file_transfer_app_gatt_svr_chr_uuid.u,
                .access_cb = _BleControl_GattServiceEventHandler,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
                .val_handle = &file_transfer_app_gatt_svr_chr_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[])
                {
                    {
                        .uuid = &file_transfer_app_gatt_svr_dsc_uuid.u,
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = _BleControl_GattServiceEventHandler,
                    },
                    {
                        0, /* No more descriptors in this characteristic */
                    }
                },
            },
            {
                /*** This characteristic can be subscribed to by writing 0x00 and 0x01 to the CCCD ***/
                .uuid = &interactive_game_app_gatt_svr_chr_uuid.u,
                .access_cb = _BleControl_GattServiceEventHandler,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
                .val_handle = &interactive_game_app_gatt_svr_chr_val_handle,
                .descriptors = (struct ble_gatt_dsc_def[])
                {
                    {
                        .uuid = &interactive_game_app_gatt_svr_dsc_uuid.u,
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = _BleControl_GattServiceEventHandler,
                    },
                    {
                        0, /* No more descriptors in this characteristic */
                    }
                },
            },
            {
                0, /* No more characteristics in this service. */
            }
        },
    },
    {
        0, /* No more services. */
    },
};


void _BleControl_StopAdvertisement(BleControl *this)
{
    assert(this);

    ESP_LOGI(TAG, "Stopping advertising");

    int rc = ble_gap_adv_stop();
    if (rc != 0)
    {
        ESP_LOGW(TAG, "error disabling advertisement; rc=%d", rc);
        return;
    }
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
void _BleControl_StartAdvertisement(BleControl *this, bool advertiseService)
{
    int rc;
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields adv_fields;
    const char *name;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&adv_fields, 0, sizeof adv_fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    // adv_fields.tx_pwr_lvl_is_present = 1;
    // adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    if (advertiseService)
    {
        adv_fields.uuids128 = &gatt_svr_svc_uuid;
        adv_fields.num_uuids128 = 1;
        adv_fields.uuids128_is_complete = true;
    }
    else
    {
        adv_fields.mfg_data = (uint8_t*)&this->iwcAdvPayload;
        adv_fields.mfg_data_len = sizeof(this->iwcAdvPayload);
    }

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0)
    {
        if (rc == BLE_HS_EMSGSIZE)
        {
            ESP_LOGE(TAG, "error setting advertisement data; exceeded maximum advertisement size");
        }
        else
        {
           ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc); 
        }
        
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = this->bleServiceEnabled? BLE_GAP_CONN_MODE_UND : BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    // adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(120);
    // adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(240);
    ESP_LOGI(TAG, "Starting advertising %s", advertiseService ? "with service" : "with game data");

    /** @brief Start advertising
     *
     * This function configures and start advertising procedure.
     *
     * @param own_addr_type The type of address the stack should use for itself.
     *                      Valid values are:
     *                         - BLE_OWN_ADDR_PUBLIC
     *                         - BLE_OWN_ADDR_RANDOM
     *                         - BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT
     *                         - BLE_OWN_ADDR_RPA_RANDOM_DEFAULT
     * @param direct_addr   The peer's address for directed advertising. This
     *                      parameter shall be non-NULL if directed advertising is
     *                      being used.
     * @param duration_ms   The duration of the advertisement procedure. On
     *                      expiration, the procedure ends and a
     *                      BLE_GAP_EVENT_ADV_COMPLETE event is reported. Units are
     *                      milliseconds. Specify BLE_HS_FOREVER for no expiration.
     * @param adv_params    Additional arguments specifying the particulars of the
     *                      advertising procedure.
     * @param cb            The callback to associate with this advertising
     *                      procedure.  If advertising ends, the event is reported
     *                      through this callback.  If advertising results in a
     *                      connection, the connection inherits this callback as its
     *                      event-reporting mechanism.
     * @param cb_arg        The optional argument to pass to the callback function.
     *
     * @return              0 on success, error code on failure.
     */
    rc = ble_gap_adv_start(this->ownAddrType, NULL, BLE_HS_FOREVER,
                           &adv_params, _BleControl_GapEventHandler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
}

esp_err_t BleControl_UpdateEventId(BleControl *this, char *newEventIdB64)
{
    ESP_LOGI(TAG, "Update event id");
    uint8_t eventId[EVENT_ID_SIZE];
    size_t outlen;
    mbedtls_base64_decode(eventId, sizeof(eventId), &outlen, (uint8_t *)newEventIdB64, EVENT_ID_B64_SIZE - 1);
    if (memcmp(this->iwcAdvPayload.eventId, eventId, sizeof(this->iwcAdvPayload.eventId)) != 0)
    {
        ESP_LOGI(TAG, "Updating event id");
        memcpy(this->iwcAdvPayload.eventId, eventId, sizeof(this->iwcAdvPayload.eventId));
        _BleControl_StopAdvertisement(this);
        _BleControl_StartAdvertisement(this, false);
        return ESP_OK;
    }

    return ESP_OK;
}


esp_err_t BleControl_EnableBleService(BleControl *this, bool pairingMode, uint32_t timeoutBleServiceDisableUSec)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);

    if (xSemaphoreTake(this->bleMutex, pdMS_TO_TICKS(BLE_MUTEX_WAIT_TIME_MS)) == pdTRUE)
    {
        if (this->bleServiceEnabled == false)
        {
            ESP_LOGI(TAG, "Enabling BLE Service. pairing mode = %d", pairingMode);

            this->bleServiceEnabled = true;
            if (pairingMode)
            {
                // Wipe pairing data
                UserSettings_SetPairId(this->pUserSettings, NULL);
                _BleControl_RefreshServiceUuid(this);
            }
            memcpy(gatt_svr_svc_uuid.value, this->serviceUuid.value, sizeof(gatt_svr_svc_uuid.value));
            _BleControl_StopAdvertisement(this);
            _BleControl_StartAdvertisement(this, true);

            /**
             * Adds a set of services for registration.  All services added
             * in this manner get registered immidietely.
             *
             * @param svcs                  An array of service definitions to queue for
             *                                  registration.  This array must be
             *                                  terminated with an entry whose 'type'
             *                                  equals 0.
             *
             * @return                      0 on success;
             *                              BLE_HS_ENOMEM on heap exhaustion.
             */
            int rc = ble_gatts_add_dynamic_svcs(gatt_svr_svcs);
            if(rc != 0)
            {
                /* not able to add service return immidietely */
                ESP_LOGE(TAG, "Failed to add service");
            }
            _BleControl_StartBleServiceDisableTimer(this, timeoutBleServiceDisableUSec);

            ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_SERVICE_ENABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
            if(ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to notify event");
            }
        }
        else
        {
            ESP_LOGW(TAG, "BLE Service already enabled");
        }

        // Give back semaphore
        if (xSemaphoreGive(this->bleMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give back mutex: %s", __func__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take mutex: %s", __func__);
    }

    return ret;
}

esp_err_t BleControl_DisableBleService(BleControl *this, bool notify)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);

    if (xSemaphoreTake(this->bleMutex, pdMS_TO_TICKS(BLE_MUTEX_WAIT_TIME_MS)) == pdTRUE)
    {
        if (this->bleServiceEnabled)
        {
            ESP_LOGI(TAG, "Disabling BLE Service");
            this->bleServiceEnabled = false;
            _BleControl_StopBleServiceDisableTimer(this);
            _BleControl_StopAdvertisement(this);
            _BleControl_StartAdvertisement(this, false);

            /**
             * Deletes a service with corresponding uuid.  All services deleted
             * in this manner will be deleted immidietely.
             *
             * @param uuid                  uuid of the service to be deleted.
             *
             * @return                      0 on success;
             *                              BLE_HS_ENOENT on invalid uuid.
             */
            int rc = ble_gatts_delete_svc(gatt_svr_svcs[0].uuid); //TODO remove hardcoded index

            if (notify)
            {
                if(rc != 0)
                {
                    /* not able to delete service return immidietely */
                    ESP_LOGE(TAG, "Failed to delete service");
                }
                ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_SERVICE_DISABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                if(ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to notify ble service disabled event");
                }
                
                uint16_t data = 0;
                ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_INTERACTIVE_GAME_ACTION, (void*)&data, sizeof(data), DEFAULT_NOTIFY_WAIT_DURATION);
                if(ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to notify reset interactive game action event");
                }
            }
        }
        else
        {
            ESP_LOGW(TAG, "BLE Service already disabled");
        }

        // Give back semaphore
        if (xSemaphoreGive(this->bleMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give back mutex: %s", __func__);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take mutex: %s", __func__);
    }

    return ret;
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int _BleControl_GapEventHandler(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    BleControl *this = BleControl_GetInstance();
    assert(this);

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        /**
         * The status of the connection attempt;
         *     o 0: the connection was successfully established.
         *     o BLE host error code: the connection attempt failed for
         *       the specified reason.
         */
        if (event->connect.status == 0)
        {
            ESP_LOGI(TAG, "Device %u Connected. status=%d ", event->connect.conn_handle, event->connect.status);
            /**
             * Searches for a connection with the specified handle.  If a matching
             * connection is found, the supplied connection descriptor is filled
             * correspondingly.
             *
             * @param handle    The connection handle to search for.
             * @param out_desc  On success, this is populated with information relating to
             *                  the matching connection.  Pass NULL if you don't need this
             *                  information.
             *
             * @return          0 on success, BLE_HS_ENOTCONN if no matching connection was
             *                  found.
             */
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            dynamic_service_print_conn_desc(&desc);
            ESP_LOGI(TAG, "\n");

            // Update connection parameters for speed
            // https://github.com/espressif/esp-idf/issues/12789#issuecomment-1862332433
            struct ble_gap_upd_params conn_parameters = { 0 };
            conn_parameters.itvl_min = 6;   // 7.5ms
            conn_parameters.itvl_max = 24;  // 30ms
            conn_parameters.latency = 0;
            conn_parameters.supervision_timeout = 20; 
            // https://github.com/apache/mynewt-nimble/issues/793#issuecomment-616022898
            conn_parameters.min_ce_len = 0x00;
            conn_parameters.max_ce_len = 0x00;
            rc = ble_gap_update_params(event->connect.conn_handle, &conn_parameters);
            if(rc != 0)
            {
                ESP_LOGE(TAG, "Device %u failed to update connection parameters: 0x%0x", event->connect.conn_handle, rc);
            }

            // Update connection MTU
            rc = ble_gap_set_data_len(event->connect.conn_handle, BLE_ATT_PREFERRED_MTU, BLE_PREFERRED_MAX_TX_TIMEOUT_USEC);
            if(rc != 0)
            {
                ESP_LOGE(TAG, "Device %u failed to set data length(%d,%d): 0x%0x", event->connect.conn_handle, BLE_ATT_PREFERRED_MTU, BLE_PREFERRED_MAX_TX_TIMEOUT_USEC, rc);
            }

            _BleControl_BleServiceNotifyConnect(this);
        }
        else
        {
            ESP_LOGI(TAG, "Device %u Failed Attempting to Connected. status=%d", event->connect.conn_handle, event->connect.status);
        }

        /* restart advertising without the service */
        _BleControl_StopAdvertisement(this);
        _BleControl_StartAdvertisement(this, false);
        _BleControl_ResetBleServiceDisableTimer(this, 0);
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Device %u Disconnected. reason=%d", event->disconnect.conn.conn_handle, event->disconnect.reason);
        dynamic_service_print_conn_desc(&event->disconnect.conn);
        ESP_LOGI(TAG, "\n");

        /* Connection terminated; Disable ble service and notify disconnect */
        if (NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_DROPPED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to notify BLE dropped event");
        }
        BleControl_DisableBleService(this, false);
        _BleControl_BleServiceInteractiveGameReset(this);
        BleControl_EnableBleService(this, false, 10*1000*1000); // 10 seconds
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        ESP_LOGI(TAG, "Device Connection Updated; status=%d", event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        ESP_LOGI(TAG, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertise complete; reason=%d",
                    event->adv_complete.reason);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_TX:
        ESP_LOGD(TAG, "notify_tx event; conn_handle=%d attr_handle=%d status=%d is_indication=%d",
                    event->notify_tx.conn_handle, event->notify_tx.attr_handle, event->notify_tx.status, event->notify_tx.indication);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        /**
         * Represents a state change in a peer's subscription status.  In this
         * comment, the term "update" is used to refer to either a notification
         * or an indication.  This event is triggered by any of the following
         * occurrences:
         *     o Peer enables or disables updates via a CCCD write.
         *     o Connection is about to be terminated and the peer is
         *       subscribed to updates.
         *     o Peer is now subscribed to updates after its state was restored
         *       from persistence.  This happens when bonding is restored.
         *
         * Valid for the following event types:
         *     o BLE_GAP_EVENT_SUBSCRIBE
         */
        // struct {
        //     /** The handle of the relevant connection. */
        //     uint16_t conn_handle;
        //
        //     /** The value handle of the relevant characteristic. */
        //     uint16_t attr_handle;
        //
        //     /** One of the BLE_GAP_SUBSCRIBE_REASON codes. */
        //     uint8_t reason;
        //
        //     /** Whether the peer was previously subscribed to notifications. */
        //     uint8_t prev_notify:1;
        //
        //     /** Whether the peer is currently subscribed to notifications. */
        //     uint8_t cur_notify:1;
        //
        //     /** Whether the peer was previously subscribed to indications. */
        //     uint8_t prev_indicate:1;
        //
        //     /** Whether the peer is currently subscribed to indications. */
        //     uint8_t cur_indicate:1;
        // } subscribe;

        const char * reason = "Unknown";
        switch (event->subscribe.reason)
        {
        case BLE_GAP_SUBSCRIBE_REASON_WRITE:
            reason = "Write";
            break;
        case BLE_GAP_SUBSCRIBE_REASON_TERM:
            reason = "Terminate";
            break;
        case BLE_GAP_SUBSCRIBE_REASON_RESTORE:
            reason = "Restore";
            break;
        }

        if (event->subscribe.cur_notify == 1 && event->subscribe.prev_notify == 0)
        {
            ESP_LOGI(TAG, "Device %u Subscribed to Notifications for Characteristic %u. reason %s", 
                    event->subscribe.conn_handle, event->subscribe.attr_handle, reason);
        }
        else if (event->subscribe.cur_notify == 0 && event->subscribe.prev_notify == 1)
        {
            ESP_LOGI(TAG, "Device %u Unsubscribed to Notifications for Characteristic %u. reason %s", 
                    event->subscribe.conn_handle, event->subscribe.attr_handle, reason);
        }
        if (event->subscribe.cur_indicate == 1 && event->subscribe.prev_indicate == 0)
        {
            ESP_LOGI(TAG, "Device %u Subscribed to Indications for Characteristic %u. reason %s", 
                    event->subscribe.conn_handle, event->subscribe.attr_handle, reason);
        }
        else if (event->subscribe.cur_indicate == 0 && event->subscribe.prev_indicate == 1)
        {
            ESP_LOGI(TAG, "Device %u Unsubscribed to Indications for Characteristic %u. reason %s", 
                    event->subscribe.conn_handle, event->subscribe.attr_handle, reason);
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU Update for Connection conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
        return 0;
    }

    return 0;
}

/**
 * Access callback whenever a characteristic/descriptor is read or written to.
 * Here reads and writes need to be handled.
 * ctxt->op tells weather the operation is read or write and
 * weather it is on a characteristic or descriptor,
 * ctxt->dsc->uuid tells which characteristic/descriptor is accessed.
 * attr_handle give the value handle of the attribute being accessed.
 * Accordingly do:
 *     Append the value to ctxt->om if the operation is READ
 *     Write ctxt->om to the value if the operation is WRITE
 **/

/**
 * Context for an access to a GATT characteristic or descriptor.  When a client
 * reads or writes a locally registered characteristic or descriptor, an
 * instance of this struct gets passed to the application callback.
 */
// struct ble_gatt_access_ctxt {
//     /**
//      * Indicates the gatt operation being performed.  This is equal to one of
//      * the following values:
//      *     o  BLE_GATT_ACCESS_OP_READ_CHR
//      *     o  BLE_GATT_ACCESS_OP_WRITE_CHR
//      *     o  BLE_GATT_ACCESS_OP_READ_DSC
//      *     o  BLE_GATT_ACCESS_OP_WRITE_DSC
//      */
//     uint8_t op;

//     /**
//      * A container for the GATT access data.
//      *     o For reads: The application populates this with the value of the
//      *       characteristic or descriptor being read.
//      *     o For writes: This is already populated with the value being written
//      *       by the peer.  If the application wishes to retain this mbuf for
//      *       later use, the access callback must set this pointer to NULL to
//      *       prevent the stack from freeing it.
//      */
//     struct os_mbuf *om;

//     /**
//      * The GATT operation being performed dictates which field in this union is
//      * valid.  If a characteristic is being accessed, the chr field is valid.
//      * Otherwise a descriptor is being accessed, in which case the dsc field
//      * is valid.
//      */
//     union {
//         /**
//          * The characteristic definition corresponding to the characteristic
//          * being accessed.  This is what the app registered at startup.
//          */
//         const struct ble_gatt_chr_def *chr;

//         /**
//          * The descriptor definition corresponding to the descriptor being
//          * accessed.  This is what the app registered at startup.
//          */
//         const struct ble_gatt_dsc_def *dsc;
//     };
// };
static int _BleControl_GattServiceEventHandler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid;
    int rc;

    BleControl *this = BleControl_GetInstance();
    assert(this);
    _BleControl_ResetBleServiceDisableTimer(this, 0);

    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            ESP_LOGD(TAG, "Characteristic read; conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
        }
        else
        {
            ESP_LOGD(TAG, "Characteristic read by NimBLE stack; attr_handle=%d", attr_handle);
        }
        uuid = ctxt->chr->uuid;
        if (attr_handle == file_transfer_app_gatt_svr_chr_val_handle)
        {
            ESP_LOGD(TAG, "Read characteristic value for Service 0");
            uint8_t bleReadBuffer[sizeof(BleFileTransferResponseData)] = {0};
            uint16_t bytesToSend;
            esp_err_t res = _BleControl_GetBleReadResponse(this, BLE_PROFILE_FILE_TRANSFER_APP_ID, 
                                                           bleReadBuffer, 
                                                           sizeof(bleReadBuffer),
                                                           &bytesToSend);
            if (bytesToSend != sizeof(bleReadBuffer))
            {
                ESP_LOGE(TAG, "Failed to get file transfer app characteristic value. Invalid read size");
            }
            if (res != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to get file transfer app characteristic value");
            }
            /**
             * Append data onto a mbuf (Chained memory buffer)
             *
             * @param om   The mbuf to append the data onto
             * @param data The data to append onto the mbuf
             * @param len  The length of the data to append
             *
             * @return 0 on success, and an error code on failure
             */
            rc = os_mbuf_append(ctxt->om,
                                bleReadBuffer,
                                bytesToSend);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        else if (attr_handle == interactive_game_app_gatt_svr_chr_val_handle)
        {
            ESP_LOGD(TAG, "Read characteristic value for Service 1");
            uint16_t bytesToSend;
            uint8_t bleReadBuffer[sizeof(this->touchSensorsActiveBits)] = {0};
            esp_err_t res = _BleControl_GetBleReadResponse(this, BLE_PROFILE_INTERACTIVE_GAME_APP_ID, 
                                                           bleReadBuffer, 
                                                           sizeof(bleReadBuffer),
                                                           &bytesToSend);
            if (bytesToSend != sizeof(bleReadBuffer))
            {
                ESP_LOGE(TAG, "Failed to get interactive game app characteristic value. Invalid read size");
            }
            if (res != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to get interactive game app characteristic value");
            }
            /**
             * Append data onto a mbuf (Chained memory buffer)
             *
             * @param om   The mbuf to append the data onto
             * @param data The data to append onto the mbuf
             * @param len  The length of the data to append
             *
             * @return 0 on success, and an error code on failure
             */
            rc = os_mbuf_append(ctxt->om,
                                bleReadBuffer,
                                bytesToSend);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto unknown;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            ESP_LOGD(TAG, "Characteristic write; conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
        }
        else 
        {
            ESP_LOGI(TAG, "Characteristic write by NimBLE stack; attr_handle=%d", attr_handle);
        }
        uuid = ctxt->chr->uuid;
        if (attr_handle == file_transfer_app_gatt_svr_chr_val_handle)
        {
            uint8_t bytesBeingReceived[DATA_FRAME_MAX_SIZE] = {0}; // JER: Talk to Shaun about this
            uint16_t bytesReceived = 0;
            // Copies the written data into application level data structure, in this example its the uint8_t variable gatt_svr_chr_val with a size of 1
            rc = _CopyDataFromMBufToBuffer(ctxt->om,
                                0,                          // min length to copy
                                sizeof(bytesBeingReceived), // max length to copy
                                &bytesBeingReceived,        // location where to copy the data to
                                &bytesReceived);            // location to store bytes copied, use NULL when you want that information
            esp_err_t res = _BleControl_BleReceiveDataAction(this, BLE_PROFILE_FILE_TRANSFER_APP_ID, bytesBeingReceived, bytesReceived, false);
            if (res != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to execute ble write for file transfer app characteristic");
            }
            return rc;
        }
        else if (attr_handle == interactive_game_app_gatt_svr_chr_val_handle)
        {
            uint8_t bytesBeingReceived[sizeof(uint16_t)] = {0};
            uint16_t bytesReceived = 0;

            // Copies the written data into application level data structure, in this example its the uint8_t variable gatt_svr_chr_val with a size of 1
            rc = _CopyDataFromMBufToBuffer(ctxt->om,
                                sizeof(bytesBeingReceived), // min length to copy
                                sizeof(bytesBeingReceived), // max length to copy
                                &bytesBeingReceived,        // location where to copy the data to
                                &bytesReceived);            // location to store bytes copied, use NULL when you want that information
            esp_err_t res = _BleControl_BleReceiveDataAction(this, BLE_PROFILE_INTERACTIVE_GAME_APP_ID, bytesBeingReceived, bytesReceived, false);
            if (res != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to execute ble write for interactive game app characteristic");
            }
            return rc;
        }
        goto unknown;

    case BLE_GATT_ACCESS_OP_READ_DSC:
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            ESP_LOGI(TAG, "Descriptor read; conn_handle=%d attr_handle=%d", conn_handle, attr_handle);
        }
        else
        {
            ESP_LOGI(TAG, "Descriptor read by NimBLE stack; attr_handle=%d", attr_handle);
        }
        uuid = ctxt->dsc->uuid;
        /** @brief Compares two Bluetooth UUIDs.
         *
         * @param uuid1  The first UUID to compare.
         * @param uuid2  The second UUID to compare.
         *
         * @return       0 if the two UUIDs are equal, nonzero if the UUIDs differ.
         */
        
        if (ble_uuid_cmp(uuid, &file_transfer_app_gatt_svr_dsc_uuid.u) == 0)
        {
            /**
             * Append data onto a mbuf (Chained memory buffer)
             *
             * @param om   The mbuf to append the data onto
             * @param data The data to append onto the mbuf
             * @param len  The length of the data to append
             *
             * @return 0 on success, and an error code on failure
             */
            rc = os_mbuf_append(ctxt->om,
                                file_transfer_app_gatt_svr_dsc_val,
                                sizeof(file_transfer_app_gatt_svr_dsc_val)); // JER
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        else if (ble_uuid_cmp(uuid, &interactive_game_app_gatt_svr_dsc_uuid.u) == 0)
        {
            /**
             * Append data onto a mbuf (Chained memory buffer)
             *
             * @param om   The mbuf to append the data onto
             * @param data The data to append onto the mbuf
             * @param len  The length of the data to append
             *
             * @return 0 on success, and an error code on failure
             */
            rc = os_mbuf_append(ctxt->om,
                                interactive_game_app_gatt_svr_dsc_val,
                                sizeof(interactive_game_app_gatt_svr_dsc_val)); // JER
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto unknown;

    case BLE_GATT_ACCESS_OP_WRITE_DSC:
        goto unknown;

    default:
        goto unknown;
    }

unknown:
    /* Unknown characteristic/descriptor;
     * The NimBLE host should not have called this function;
     */
    // assert(0);
    ESP_LOGE(TAG, "unknown access type %d", ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

void _BleControl_ServiceEventCallbackHandler(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

static esp_err_t _BleControl_BleReceiveDataAction(BleControl *this, BleServiceProfile profileId, uint8_t * data, int size, bool final)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
#if DEBUG_BLE
    esp_log_buffer_hex(TAG, this->bleServiceParameters.prepareWriteEnv[profileId].prepareBuf, this->bleServiceParameters.prepareWriteEnv[profileId].prepareLen);
#endif
    switch(profileId)
    {
        case BLE_PROFILE_FILE_TRANSFER_APP_ID:
            ret = _BleControl_BleReceiveFileDataAction(this, data, size, final);
            break;
        case BLE_PROFILE_INTERACTIVE_GAME_APP_ID:
            ret = _BleControl_BleReceiveInteractiveGameDataAction(this, data, size, final);
            break;
        default:
            break;
    }
    return ret;
}

static esp_err_t _BleControl_GetBleReadResponse(BleControl *this, BleServiceProfile profileId, uint8_t * buffer, uint32_t size, uint16_t * pLength)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;
    switch (profileId)
    {
        case BLE_PROFILE_FILE_TRANSFER_APP_ID:
            ret = _BleControl_GetFileTransferReadResponse(this, buffer, size, pLength);
            break;
        case BLE_PROFILE_INTERACTIVE_GAME_APP_ID:
            ret = _BleControl_GetInteractiveGameReadResponse(this, buffer, size, pLength);
            break;
        default:
            break;
    }
    return ret;
}


// Copies the written data into application level data structure, in this example its the uint8_t variable gatt_svr_chr_val with a size of 1
static int _CopyDataFromMBufToBuffer(struct os_mbuf *om, uint16_t min_len, uint16_t max_len, void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len)
    {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

static void _BleControl_UpdateUuidWithPairId(uint8_t *uuid, uint8_t *pairId) 
{
    // Check if pairId is NULL before proceeding
    if (pairId == NULL || uuid == NULL) 
    {
        return;
    }

    // Check if pairId is all zeros (NULL'd settings)
    static uint8_t null_pair_id[PAIR_ID_SIZE] = {0};
    if(memcmp(pairId, null_pair_id, PAIR_ID_SIZE) == 0)
    {
        // Do nothing if pairID is set of null bytes
        return;
    }

    // Assuming uuid is a 16-byte array and pairId is 8 characters long
    static const uint8_t offset = 8;
    static const uint8_t copy_size = 8;
    memcpy(uuid+offset, pairId, copy_size);
}

void _BleControl_RefreshServiceUuid(BleControl *this)
{
    assert(this);
    assert(this->pUserSettings);
    // Overwrite with base service UUID
    memcpy(this->serviceUuid.value, serviceUuidBase.value, sizeof(this->serviceUuid.value));
    
    // If pairID is set, update the UUID with the pairID
    // ASSUMPTION that pairID byte is 0x7b when set
    if (this->pUserSettings->settings.pairId[0] != '\0')
    {
        ESP_LOGI(TAG, "BLE Pair ID: %s", this->pUserSettings->settings.pairId);
        _BleControl_UpdateUuidWithPairId(this->serviceUuid.value, this->pUserSettings->settings.pairId);
    }
}

static esp_err_t _BleControl_StartBleServiceDisableTimer(BleControl *this, uint32_t timeout_usec)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "Starting BLE service disable timer");
    uint32_t timeout = timeout_usec != 0? timeout_usec : BLE_DISABLE_TIMER_TIMEOUT_USEC;
    esp_timer_start_once(this->bleServiceDisableTimerHandle, timeout);

    return ret;
}

static esp_err_t _BleControl_StopBleServiceDisableTimer(BleControl *this)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "Stopping BLE service disable timer");
    esp_timer_stop(this->bleServiceDisableTimerHandle);

    return ret;
}

static esp_err_t _BleControl_ResetBleServiceDisableTimer(BleControl *this, uint32_t timeoutUsec)
{
    assert(this);
    esp_err_t ret = ESP_FAIL;

    ESP_LOGD(TAG, "Resetting BLE service disable timer");
    uint32_t timeout = timeoutUsec != 0? timeoutUsec : BLE_DISABLE_TIMER_TIMEOUT_USEC;
    esp_timer_stop(this->bleServiceDisableTimerHandle);
    esp_timer_start_once(this->bleServiceDisableTimerHandle, timeout);

    return ret;
}

void _BleControl_BleServiceDisableTimeoutEventHandler(void* arg)
{
    BleControl *this = (BleControl*) arg;
    assert(this);
    ESP_LOGI(TAG, "BLE service disable timer triggered");

    // Release disable xfer BLE task
    BleControl_DisableBleService(this, true);
    _BleControl_BleServiceNotifyDisconnect(this);
}

/**
 * Logs information about a connection to the console.
 */
static void dynamic_service_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(TAG, "Connection Description:");
    ESP_LOGI(TAG, "    handle:              %u", desc->conn_handle);
    ESP_LOGI(TAG, "    our_ota_addr_type:   %u",desc->our_ota_addr.type);
    ESP_LOGI(TAG, "    our_ota_addr:        %02x:%02x:%02x:%02x:%02x:%02x", 
                                            desc->our_ota_addr.val[5], desc->our_ota_addr.val[4], desc->our_ota_addr.val[3],
                                            desc->our_ota_addr.val[2], desc->our_ota_addr.val[1], desc->our_ota_addr.val[0]);
    ESP_LOGI(TAG, "    our_id_addr_type:    %u", desc->our_id_addr.type);
    ESP_LOGI(TAG, "    our_id_addr:         %02x:%02x:%02x:%02x:%02x:%02x", 
                                            desc->our_id_addr.val[5], desc->our_id_addr.val[4], desc->our_id_addr.val[3],
                                            desc->our_id_addr.val[2], desc->our_id_addr.val[1], desc->our_id_addr.val[0]);
    ESP_LOGI(TAG, "    peer_ota_addr_type:  %u", desc->peer_ota_addr.type);
    ESP_LOGI(TAG, "    peer_ota_addr:       %02x:%02x:%02x:%02x:%02x:%02x", 
                                            desc->peer_ota_addr.val[5], desc->peer_ota_addr.val[4], desc->peer_ota_addr.val[3],
                                            desc->peer_ota_addr.val[2], desc->peer_ota_addr.val[1], desc->peer_ota_addr.val[0]);
    ESP_LOGI(TAG, "    peer_id_addr_type:   %u", desc->peer_id_addr.type);
    ESP_LOGI(TAG, "    peer_id_addr:        %02x:%02x:%02x:%02x:%02x:%02x", 
                                            desc->peer_id_addr.val[5], desc->peer_id_addr.val[4], desc->peer_id_addr.val[3],
                                            desc->peer_id_addr.val[2], desc->peer_id_addr.val[1], desc->peer_id_addr.val[0]);
    ESP_LOGI(TAG, "    conn_itvl:           %u", desc->conn_itvl);
    ESP_LOGI(TAG, "    conn_latency:        %u", desc->conn_latency);
    ESP_LOGI(TAG, "    supervision_timeout: %u", desc->supervision_timeout);
    ESP_LOGI(TAG, "    encrypted:           %s", desc->sec_state.encrypted? "true" : "false");
    ESP_LOGI(TAG, "    authenticated:       %s", desc->sec_state.authenticated? "true" : "false");
    ESP_LOGI(TAG, "    bonded:              %s", desc->sec_state.bonded? "true" : "false");
}


static esp_err_t _BleControl_BleServiceNotifyConnect(BleControl *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    ESP_LOGI(TAG, "_BleControl_BleServiceNotifyConnect");

    _BleControl_ResetFrameContext(this);
    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_SERVICE_CONNECTED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
    return ret;
}

static esp_err_t _BleControl_BleServiceNotifyDisconnect(BleControl *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    ESP_LOGI(TAG, "On Disconnect. frameBytesReceived = %d", this->fileTransferFrameContext.frameBytesReceived);

    _BleControl_ResetFrameContext(this);
    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_SERVICE_DISCONNECTED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to notify event for NOTIFICATION_EVENTS_BLE_DISCONNECTED event. ret=%s", esp_err_to_name(ret));
    }
    return ret;
}