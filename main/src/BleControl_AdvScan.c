
#include "esp_log.h"

// BLE 
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "host/ble_gap.h"

// mbedtls
#include "mbedtls/base64.h"

#include "BleControl.h"
#include "BleControl_Service.h"

/**
 * @file BleControl_AdvScan.c
 * @brief BLE advertisement scanning and parsing for peer detection and service enable triggers.
 *
 * This module configures and runs a passive NimBLE scan, parses incoming
 * advertisement payloads to detect:
 * - Peer badge heartbeats (custom manufacturer data), which are translated
 *   into PeerReport notifications for the NotificationDispatcher.
 * - A special 128-bit UUID pattern used to request enabling the badge's BLE
 *   GATT service (pairing flow).
 *
 * Responsibilities:
 * - Start and manage GAP discovery scanning via NimBLE.
 * - Parse manufacturer data into IwcAdvertisingPayload and build PeerReport.
 * - Notify interested subsystems when peers are detected or when a service
 *   enable request is observed.
 *
 * Notes:
 * - Scanning is configured as passive to minimize traffic and conserve power.
 * - Uses MbedTLS base64 for compact ID encoding in PeerReport.
 * - Thread-safety: invoked from NimBLE host context; interacts with
 *   NotificationDispatcher which internally serializes notifications.
 */
#define TAG "BLE"

static void _BleControl_ProcessAdvertisement(BleControl * this, const uint8_t *advertisementData, uint8_t advertisementDataLen, uint16_t rssi);
static bool _BleControl_ParseEventAdvertisingPacket(BleControl *this, const uint8_t *advertisementData, uint8_t advertisementDataLen, IwcAdvertisingPayload *pEventAdvertisementPayload);
static bool _BleControl_ParseEnableBleServiceAdvertisingPacket(BleControl *this, const uint8_t *advertisementData, uint8_t advertisementDataLen);
static int _BleControl_ScanEventHandler(struct ble_gap_event *event, void *arg);
static PeerReport _BleControl_CreatePeerReport(BleControl * this, IwcAdvertisingPayload eventAdvPacket, uint16_t rssi);

/**
 * @brief Start a passive BLE advertisement scan using NimBLE.
 *
 * Configures GAP discovery to run indefinitely and forwards each discovered
 * advertisement to the internal event handler for parsing and processing.
 *
 * @param this Pointer to BleControl instance.
 */
void BleControl_StartAdvertisementScan(BleControl * this)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    ESP_LOGI(TAG, "Starting advertisement scan");

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 0;

    
    // disc_params.itvl = BLE_GAP_SCAN_SLOW_INTERVAL1;
    // disc_params.window = BLE_GAP_SCAN_SLOW_WINDOW1;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.passive = 1; // Perform a passive scan.  I.e., don't send follow-up scan requests to each advertiser.
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      _BleControl_ScanEventHandler, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int _BleControl_ScanEventHandler(struct ble_gap_event *event, void *arg)
{
    BleControl *this = BleControl_GetInstance();
    assert(this);

    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        /* An advertisment report was received during GAP discovery. */
        struct ble_gap_disc_desc * disc = &event->disc;
        _BleControl_ProcessAdvertisement(this, disc->data, disc->length_data, disc->rssi);
        return 0;
    default:
        return 0;
    }
}

/**
 * @brief Process a single advertisement report.
 *
 * Parses the payload to detect either a badge heartbeat or a BLE service
 * enable request and takes appropriate action (notify peer heartbeat,
 * enable service, or ignore).
 *
 * @param this Pointer to BleControl instance.
 * @param advertisementData Raw advertisement data buffer.
 * @param advertisementDataLen Length of the advertisement data buffer.
 * @param rssi RSSI associated with the advertisement.
 */
static void _BleControl_ProcessAdvertisement(BleControl * this, const uint8_t *advertisementData, uint8_t advertisementDataLen, uint16_t rssi)
{
    IwcAdvertisingPayload eventAdvPacket;
    if (_BleControl_ParseEventAdvertisingPacket(this, advertisementData, advertisementDataLen, &eventAdvPacket))
    {
        ESP_LOGD(TAG, "Badge advertising packet found");
        PeerReport peerReport = _BleControl_CreatePeerReport(this, eventAdvPacket, rssi);
        esp_err_t ret;
        if ((ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED, (void*)&peerReport, sizeof(peerReport), DEFAULT_NOTIFY_WAIT_DURATION)) != ESP_OK)
        {
            ESP_LOGE(TAG, "NotificationDispatcher_NotifyEvent NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED failed: %s", esp_err_to_name(ret));
        }
    }
    else if (_BleControl_ParseEnableBleServiceAdvertisingPacket(this, advertisementData, advertisementDataLen))
    {
        if (this->bleServiceEnabled == false)
        {
            ESP_LOGI(TAG, "BLE Service Enable GAP advertisement uuid found, enabling BLE Service");
            esp_err_t ret;
            if ((ret = BleControl_EnableBleService(this, false, 0)) != ESP_OK)
            {
                ESP_LOGE(TAG, "BleControl_EnableBleService failed: %s", esp_err_to_name(ret));
            }
        }
        else
        {
            ESP_LOGD(TAG, "BLE Service Enable GAP advertisement uuid found but BLE Service already enabed. Ignoring request");
        }
        
    }
}

/**
 * @brief Parse manufacturer data for a valid event advertising payload.
 *
 * Validates the custom manufacturer data length and magic number, and if
 * valid, copies the payload out to the provided structure.
 *
 * @param this Pointer to BleControl instance.
 * @param advertisementData Raw advertisement data buffer.
 * @param advertisementDataLen Length of the advertisement data.
 * @param pEventAdvertisementPayload Out pointer to filled payload on success.
 * @return true if a valid payload was found; false otherwise.
 */
static bool _BleControl_ParseEventAdvertisingPacket(BleControl *this, const uint8_t *advertisementData, uint8_t advertisementDataLen, IwcAdvertisingPayload *pEventAdvertisementPayload)
{
    assert(this);
    assert(advertisementData);
    assert(pEventAdvertisementPayload);
    bool result = false;

    struct ble_hs_adv_fields fields;
    int rc = ble_hs_adv_parse_fields(&fields, advertisementData, advertisementDataLen);
    if (rc == 0)
    {
        if (fields.mfg_data_len == sizeof(IwcAdvertisingPayload))
        {
            IwcAdvertisingPayload eventAdvertisingPayload = *(IwcAdvertisingPayload*)fields.mfg_data;
            if (eventAdvertisingPayload.magicNum == EVENT_ADV_MAGIC_NUMBER)
            {
                *pEventAdvertisementPayload = eventAdvertisingPayload;
                result = true;
            }
        }
    }
    return result;
}

/**
 * @brief Format a MAC address string from a 6-byte address.
 *
 * @param addr Pointer to a 6-byte address.
 * @return Pointer to a static buffer containing the formatted string.
 */
char *
addr_str(const void *addr)
{
    static char buf[6 * 2 + 5 + 1];
    const uint8_t *u8p;

    u8p = addr;
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
            u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);

    return buf;
}

/**
 * @brief Print a byte buffer as colon-separated hex values.
 *
 * @param bytes Buffer to print.
 * @param len Length of the buffer.
 */
void print_bytes(const uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        printf("%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

/**
 * @brief Print a BLE UUID to stdout using NimBLE formatting.
 *
 * @param uuid UUID to print.
 */
void
print_uuid(const ble_uuid_t *uuid)
{
    char buf[BLE_UUID_STR_LEN];

    printf("%s", ble_uuid_to_str(uuid, buf));
}

/**
 * @brief Debug print of parsed advertisement fields.
 *
 * Prints common fields that may be present in a parsed advertisement for
 * development and debugging visibility.
 *
 * @param fields Parsed advertisement fields from NimBLE helper.
 */
void
print_adv_fields(const struct ble_hs_adv_fields *fields)
{
    char s[BLE_HS_ADV_MAX_SZ];
    const uint8_t *u8p;
    int i;

    if (fields->flags != 0) {
        printf("    flags=0x%02x\n", fields->flags);
    }

    if (fields->uuids16 != NULL) {
        printf("    uuids16(%scomplete)=",
                    fields->uuids16_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids16; i++) {
            print_uuid(&fields->uuids16[i].u);
            printf(" ");
        }
        printf("\n");
    }

    if (fields->uuids32 != NULL) {
        printf("    uuids32(%scomplete)=",
                    fields->uuids32_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids32; i++) {
            print_uuid(&fields->uuids32[i].u);
            printf(" ");
        }
        printf("\n");
    }

    if (fields->uuids128 != NULL) {
        printf("    uuids128(%scomplete)=",
                    fields->uuids128_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids128; i++) {
            print_uuid(&fields->uuids128[i].u);
            printf(" ");
        }
        printf("\n");
    }

    if (fields->name != NULL) {
        assert(fields->name_len < sizeof s - 1);
        memcpy(s, fields->name, fields->name_len);
        s[fields->name_len] = '\0';
        printf("    name(%scomplete)=%s\n",
                    fields->name_is_complete ? "" : "in", s);
    }

    if (fields->tx_pwr_lvl_is_present) {
        printf("    tx_pwr_lvl=%d\n", fields->tx_pwr_lvl);
    }

    if (fields->slave_itvl_range != NULL) {
        printf("    slave_itvl_range=");
        print_bytes(fields->slave_itvl_range, BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
        printf("\n");
    }

    if (fields->svc_data_uuid16 != NULL) {
        printf("    svc_data_uuid16=");
        print_bytes(fields->svc_data_uuid16, fields->svc_data_uuid16_len);
        printf("\n");
    }

    if (fields->public_tgt_addr != NULL) {
        printf("    public_tgt_addr=");
        u8p = fields->public_tgt_addr;
        for (i = 0; i < fields->num_public_tgt_addrs; i++) {
            printf("public_tgt_addr=%s ", addr_str(u8p));
            u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        }
        printf("\n");
    }

    if (fields->appearance_is_present) {
        printf("    appearance=0x%04x\n", fields->appearance);
    }

    if (fields->adv_itvl_is_present) {
        printf("    adv_itvl=0x%04x\n", fields->adv_itvl);
    }

    if (fields->svc_data_uuid32 != NULL) {
        printf("    svc_data_uuid32=");
        print_bytes(fields->svc_data_uuid32, fields->svc_data_uuid32_len);
        printf("\n");
    }

    if (fields->svc_data_uuid128 != NULL) {
        printf("    svc_data_uuid128=");
        print_bytes(fields->svc_data_uuid128, fields->svc_data_uuid128_len);
        printf("\n");
    }

    if (fields->uri != NULL) {
        printf("    uri=");
        print_bytes(fields->uri, fields->uri_len);
        printf("\n");
    }

    if (fields->mfg_data != NULL) {
        printf("    mfg_data=");
        print_bytes(fields->mfg_data, fields->mfg_data_len);
        printf("\n");
    }
}

/**
 * @brief Parse advertisements for a BLE service enable request.
 *
 * Matches a 128-bit UUID pattern derived from the current pairId and a
 * trailing magic number to determine if the badge should enable its GATT
 * service.
 *
 * @param this Pointer to BleControl instance.
 * @param advertisementData Raw advertisement data buffer.
 * @param advertisementDataLen Length of the advertisement data.
 * @return true if the enable pattern is detected; false otherwise.
 */
static bool _BleControl_ParseEnableBleServiceAdvertisingPacket(BleControl *this, const uint8_t *advertisementData, uint8_t advertisementDataLen)
{
    assert(this);
    assert(advertisementData);
    bool ret = false;

    struct ble_hs_adv_fields fields;
    int rc = ble_hs_adv_parse_fields(&fields, advertisementData, advertisementDataLen);
    if (rc == 0)
    {
        if ((fields.num_uuids128 > 0) && (fields.uuids128 != NULL))
        {
            uint8_t magicNum[2] = { 0x38, 0x13 };
            uint8_t toCheck[BLE_HS_ADV_SVC_DATA_UUID128_MIN_LEN]; // 16 bytes
            memset(toCheck, 0, BLE_HS_ADV_SVC_DATA_UUID128_MIN_LEN);

            // Reverse pair id bytes for comparison
            for (int i = 0; i < PAIR_ID_SIZE; ++i)
            {
                memcpy(toCheck + (BLE_HS_ADV_SVC_DATA_UUID128_MIN_LEN - (PAIR_ID_SIZE + 2)) + i, this->pUserSettings->settings.pairId + (PAIR_ID_SIZE - 1) - i, 1);
            }

            // Add magic numbers at the end of toCheck
            memcpy(toCheck + (BLE_HS_ADV_SVC_DATA_UUID128_MIN_LEN - 2), magicNum, 2);

            // Compare the first UUID in fields.uuids128 with toCheck
            if (!memcmp(fields.uuids128[0].value, toCheck, BLE_HS_ADV_SVC_DATA_UUID128_MIN_LEN))
            {
                ret = true;
            }
        }
    }
    return ret;
}

/**
 * @brief Build a PeerReport from a valid event advertising payload.
 *
 * Base64 encodes badge and event IDs for compact transport and attaches RSSI
 * and parsed badge type.
 *
 * @param this Pointer to BleControl instance.
 * @param eventAdvPacket Parsed event advertising payload.
 * @param rssi RSSI observed for the advertisement.
 * @return A filled PeerReport structure.
 */
static PeerReport _BleControl_CreatePeerReport(BleControl * this, IwcAdvertisingPayload eventAdvPacket, uint16_t rssi)
{
    assert(this);

    size_t b64Outlen;
    uint8_t badgeIdB64[BADGE_ID_B64_SIZE];
    mbedtls_base64_encode((unsigned char*)badgeIdB64, BADGE_ID_B64_SIZE, &b64Outlen, eventAdvPacket.badgeId, BADGE_ID_SIZE);
    ESP_LOGD(TAG, "_BleControl_CreatePeerReport:  BadgeId [B64]: %s", badgeIdB64);
    uint8_t eventIdB64[EVENT_ID_B64_SIZE];
    mbedtls_base64_encode((unsigned char*)eventIdB64, EVENT_ID_B64_SIZE, &b64Outlen, eventAdvPacket.eventId, EVENT_ID_SIZE);
    ESP_LOGD(TAG, "_BleControl_CreatePeerReport:  Peer Report: %s %s %d", badgeIdB64, eventIdB64, rssi);

    PeerReport peerReport;
    memcpy(peerReport.badgeIdB64, badgeIdB64, BADGE_ID_B64_SIZE);
    memcpy(peerReport.eventIdB64, eventIdB64, EVENT_ID_B64_SIZE);
    peerReport.peakRssi = rssi;
    peerReport.badgeType = ParseBadgeType(eventAdvPacket.badgeType);
    return peerReport;
}