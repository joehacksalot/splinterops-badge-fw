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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "mbedtls/base64.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "nvs_flash.h"

#include "BleControl.h"
#include "JsonUtils.h"
#include "LedSequences.h"
#include "GameState.h"
#include "NotificationDispatcher.h"
#include "TaskPriorities.h"
#include "UserSettings.h"
#include "Utilities.h"

#define TAG "BLE"

// Internal Function Declarations
static void BleEventGapHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void ResetFrameContext(BleControl *this);
static void BleXferControlTask(void *pvParameters);
static esp_err_t BleXferConnectionEstablishedAction(BleControl *this);
static esp_err_t ProcessTransferedFile(BleControl *this);
static esp_err_t VerifyAllFramesPresent(BleControl *this);
static esp_err_t BleXferDisconnectedAction(BleControl *this);
static esp_err_t BleXferReceiveDataAction(BleControl *this, const char * data, int size, bool final);
static esp_err_t BleXferWriteEventAction(BleControl *this, esp_gatt_if_t gattsIf, PrepareTypeEnv *prepare_write_env, esp_ble_gatts_cb_param_t *param);
static esp_err_t BleXferExecWriteEventAction(BleControl *this, PrepareTypeEnv *prepare_write_env, esp_ble_gatts_cb_param_t *param);
static void BleXferGattsProfileAEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gattsIf, esp_ble_gatts_cb_param_t *param);
static void BleXferGapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void BleXferGapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void BleXferGattsEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gattsIf, esp_ble_gatts_cb_param_t *param);
static void BleXferDisableTimeoutEventHandler(void* arg);

// Internal Type Definitions
typedef enum FileType_e
{
   FILE_TYPE_LED_SEQUENCE = 1,
   FILE_TYPE_SETTINGS_FILE = 2,
   FILE_TYPE_TEST = 3
} FileType;

// Internal Constants
#define MAX_BLE_XFER_PAYLOAD_SIZE MAX_CUSTOM_LED_SEQUENCE_SIZE
uint8_t GATTS_SERVICE_UUID_TEST_A_128[16] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x8b, 0xff, 0x00, 0x00};
uint8_t GATTS_CHAR_UUID_TEST_A_128[16] = {0x77, 0x4e, 0x8a, 0x86, 0xd1, 0xc7, 0x4d, 0xf8, 0x8c, 0xa2, 0xda, 0x2b, 0x64, 0x53, 0x3d, 0x4c};
#define GATTS_DESCR_UUID_TEST_A     0x3333
#define GATTS_NUM_HANDLE_TEST_A     4

uint8_t GATTS_SERVICE_UUID_TEST_B_128[16] = {0xbe, 0x6b, 0xc4, 0x79, 0x74, 0x6a, 0x4f, 0xb3, 0xab, 0x2f, 0x7f, 0x39, 0x3f, 0xec, 0xe4, 0xc8};
uint8_t GATTS_CHAR_UUID_TEST_B_128[16] = {0xbf, 0xe7, 0xfa, 0x46, 0xb7, 0x29, 0x4e, 0xdb, 0xaf, 0xfd, 0x8e, 0xa1, 0x0f, 0x48, 0x8c, 0x9c};
#define GATTS_DESCR_UUID_TEST_B     0x2222
#define GATTS_NUM_HANDLE_TEST_B     4

#if defined(TRON_BADGE)
  #define BLE_DEVICE_NAME            "IWCv1"
#elif defined(REACTOR_BADGE)
  #define BLE_DEVICE_NAME            "IWCv2"
#elif defined(CREST_BADGE)
  #define BLE_DEVICE_NAME            "IWCv3"
#endif

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40
#define PREPARE_BUF_MAX_SIZE 1024
#define TIMER_TIMEOUT_US 120 * 1000 * 1000 // One minute

static const uint8_t char1_str[] = {0x11,0x22,0x33};

#define adv_config_flag      (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

static const uint8_t ADV_BASE_SERVICE_UUID128[] = {
    // LSB <--------------------------------------------------------------------------------> MSB
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x8b, 0xff, 0x00, 0x00,
};

static uint8_t ADV_SERVICE_UUID128[] = {
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x8b, 0xff, 0x00, 0x00,
};

static BleControl bleControl;

/* Constant part of iBeacon data */
AdvertisingHeader eventAdvHeader =
{
    .flags = {0x02, 0x01, 0x06},
    .length = 0x13, // 1 byte for type, 2 bytes for magic number, 16 bytes for payload
    .type = 0xFF,
    .magicNum = EVENT_ADV_MAGIC_NUMBER
};

void update_uuid_with_pairId(uint8_t *uuid, uint8_t *pairId) 
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

bool IsEventAdvertisingPacket(uint8_t *advData, uint8_t advDataLen)
{
    bool result = false;
    if ((advData != NULL) && (advDataLen == sizeof(EventAdvertisingPacket)))
    {
        if (!memcmp(advData, (uint8_t*)&eventAdvHeader, sizeof(AdvertisingHeader)))
        {
            result = true;
        }
    }

    return result;
}

esp_err_t SetEventAdvertisingData(BleControl *this)
{
    esp_err_t status;
    ESP_LOGW(TAG, "Set event advertising data");

    // Create advertising packet
    EventAdvertisingPacket eventAdvPacket;
    memset(&eventAdvPacket, 0, sizeof(EventAdvertisingPacket));
    memcpy(&eventAdvPacket.eventAdvHeader, &eventAdvHeader, sizeof(AdvertisingHeader));
    memcpy(&eventAdvPacket.eventAdvPayload.badgeId, this->pUserSettings->badgeId, BADGE_ID_SIZE);
    size_t outlen;
    mbedtls_base64_decode(eventAdvPacket.eventAdvPayload.eventId, EVENT_ID_SIZE, &outlen, (uint8_t *)this->pGameState->gameStateData.status.currentEventIdB64, EVENT_ID_B64_SIZE - 1);

    // Set advertising data
    if ((status = esp_ble_gap_config_adv_data_raw((uint8_t*)&eventAdvPacket, sizeof(EventAdvertisingPacket))) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_config_adv_data_raw failed: %s", esp_err_to_name(status));
    }

    return status;
}

static esp_ble_adv_params_t eventAdvParams = {
    .adv_int_min = 0x4000, // N * 0.625 msec, 0x4000 = 10.24 sec
    .adv_int_max = 0x4000, // N * 0.625 msec, 0x4000 = 10.24 sec
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_scan_params_t eventScanParams = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x4000, // N * 0.625 msec, 0x4000 = 10.24 sec
    .scan_window            = 0x4000, // N * 0.625 msec, 0x4000 = 10.24 sec
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

BleControl * BleControl_GetInstance()
{
    return &bleControl;
}

bool BleControl_BleEnabled(BleControl *this)
{
    return this->bleEnabled;
}

bool BleControl_BleXferEnabled(BleControl *this)
{
    return this->bleXferEnabled;
}

esp_err_t BleControl_Init(BleControl *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings, GameState *pGameState)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    assert(pNotificationDispatcher);
    assert(pUserSettings);
    assert(pGameState);
    
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pUserSettings = pUserSettings;
    this->pGameState = pGameState;

    refresh_service_uuid(this);

    this->bleXferParameters.aProperty = 0;
    this->bleXferParameters.bProperty = 0;

    this->frameContext.rcvBuffer = heap_caps_malloc(MAX_BLE_XFER_PAYLOAD_SIZE, MALLOC_CAP_SPIRAM);
    ResetFrameContext(this);

    // Release and free classic, we are only using BLE
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Create the esp timer for BLE shutdown
    this->bleXferDisableTimerHandleArgs.callback = &BleXferDisableTimeoutEventHandler;
    this->bleXferDisableTimerHandleArgs.arg = (void*)(this); // argument specified here will be passed to timer callback function
    this->bleXferDisableTimerHandleArgs.name = "ble-xfer-timeout";
    ESP_ERROR_CHECK(esp_timer_create(&this->bleXferDisableTimerHandleArgs, &this->bleXferDisableTimerHandle));

    // Create tasks to disable BLE and BLE Xfer
    this->bleXferDisableSem = xSemaphoreCreateBinary();

    this->bleXferParameters.gattsChar1Val.attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX;
    this->bleXferParameters.gattsChar1Val.attr_len     = sizeof(char1_str);
    this->bleXferParameters.gattsChar1Val.attr_value   = (uint8_t *)char1_str;

    // static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x12, 0x23, 0x45, 0x56};
    
    // The length of adv data must be less than 31 bytes
    this->bleXferParameters.advData.set_scan_rsp = false;
    this->bleXferParameters.advData.include_name = true;
    this->bleXferParameters.advData.include_txpower = false;
    this->bleXferParameters.advData.min_interval = 0x0006; //slave connection min interval, Time = min_interval * 1.25 msec
    this->bleXferParameters.advData.max_interval = 0x0010; //slave connection max interval, Time = max_interval * 1.25 msec
    this->bleXferParameters.advData.appearance = 0x00;
    this->bleXferParameters.advData.manufacturer_len = 0; //TEST_MANUFACTURER_DATA_LEN,
    this->bleXferParameters.advData.p_manufacturer_data =  NULL; //&test_manufacturer[0],
    this->bleXferParameters.advData.service_data_len = 0;
    this->bleXferParameters.advData.p_service_data = NULL;
    this->bleXferParameters.advData.service_uuid_len = sizeof(ADV_SERVICE_UUID128);
    this->bleXferParameters.advData.p_service_uuid = (uint8_t *)ADV_SERVICE_UUID128;
    this->bleXferParameters.advData.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    // scan response data
    this->bleXferParameters.scanRspData.set_scan_rsp = true;
    this->bleXferParameters.scanRspData.include_name = true;
    this->bleXferParameters.scanRspData.include_txpower = true;
    // this->bleXferParameters.scanRspData.min_interval = 0x0006;
    // this->bleXferParameters.scanRspData.max_interval = 0x0010;
    this->bleXferParameters.scanRspData.appearance = 0x00;
    this->bleXferParameters.scanRspData.manufacturer_len = 0; //TEST_MANUFACTURER_DATA_LEN,
    this->bleXferParameters.scanRspData.p_manufacturer_data =  NULL; //&test_manufacturer[0],
    this->bleXferParameters.scanRspData.service_data_len = 0;
    this->bleXferParameters.scanRspData.p_service_data = NULL;
    this->bleXferParameters.scanRspData.service_uuid_len = sizeof(ADV_SERVICE_UUID128);
    this->bleXferParameters.scanRspData.p_service_uuid = (uint8_t *)ADV_SERVICE_UUID128;
    this->bleXferParameters.scanRspData.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    this->bleXferParameters.advParams.adv_int_min        = 0x20;
    this->bleXferParameters.advParams.adv_int_max        = 0x40;
    this->bleXferParameters.advParams.adv_type           = ADV_TYPE_IND;
    this->bleXferParameters.advParams.own_addr_type      = BLE_ADDR_TYPE_PUBLIC;
    // this->bleXferParameters.advParams.peer_addr            =
    // this->bleXferParameters.advParams.peer_addr_type       =
    this->bleXferParameters.advParams.channel_map        = ADV_CHNL_ALL;
    this->bleXferParameters.advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    // One gatt-based profile one appId and one gattsIf, this array will store the gattsIf returned by ESP_GATTS_REG_EVT
    this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].gattsCb = BleXferGattsProfileAEventHandler;
    this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].gattsIf = ESP_GATT_IF_NONE;       // Not get the gatt_if, so initial is ESP_GATT_IF_NONE
    // this->bleXferParameters.glProfileTable[PROFILE_B_APP_ID].gattsCb = gatts_profile_b_event_handler;                   // This demo does not implement, similar as profile A
    // this->bleXferParameters.glProfileTable[PROFILE_B_APP_ID].gattsIf = ESP_GATT_IF_NONE;                                // Not get the gatt_if, so initial is ESP_GATT_IF_NONE

    // Initialze Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));

    xTaskCreate(BleXferControlTask, "BleXferControlTask", configMINIMAL_STACK_SIZE * 5, this, BLE_DISABLE_TASK_PRIORITY, NULL);
    if ((ret = BleControl_EnableBle(this)) != ESP_OK)
    {
        ESP_LOGE(TAG, "BleControl_EnableBle failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ret;
}

esp_err_t BleControl_UpdateEventId(BleControl *this, char *newEventIdB64)
{
    ESP_LOGI(TAG, "Update event id");
    if (strncmp(this->eventIdB64, newEventIdB64, EVENT_ID_SIZE) != 0)
    {
        ESP_LOGI(TAG, "Updating event id");
        strncpy(this->eventIdB64, newEventIdB64, EVENT_ID_SIZE - 1);
        return SetEventAdvertisingData(this);
    }

    return ESP_OK;
}

esp_err_t BleControl_RegisterEventModeCallback(BleControl *this)
{
    esp_err_t status;
    ESP_LOGI(TAG, "Register event mode callback");

    // Register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(BleEventGapHandler)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %s", esp_err_to_name(status));
    }

    return status;
}

esp_err_t BleControl_EnableEventMode(BleControl *this)
{
    esp_err_t status;
    ESP_LOGI(TAG, "Enable event mode");

    // Register callback
    if ((status = BleControl_RegisterEventModeCallback(this)) != ESP_OK) {
        ESP_LOGE(TAG, "BleControl_RegisterEventModeCallback failed: %s", esp_err_to_name(status));
        return status;
    }

    // Set scan parameters
    if (!this->bleScanActive) {
        if ((status = esp_ble_gap_set_scan_params(&eventScanParams)) != ESP_OK) {
            ESP_LOGE(TAG, "esp_ble_gap_set_scan_params failed: %s", esp_err_to_name(status));
            return status;
        }
    }

    return SetEventAdvertisingData(this);
}

static void BleEventGapHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:{
        ESP_LOGI(TAG, "Start event advertising");
        esp_ble_gap_start_advertising(&eventAdvParams);
        break;
    }
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        // The unit of the duration is second, 0 means scan permanently
        ESP_LOGI(TAG, "Start event scanning");
        uint32_t duration = 0;
        BleControl* bleControl = BleControl_GetInstance();
        if (!bleControl->bleScanActive) {
            esp_ble_gap_start_scanning(duration);
        }

        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        // Scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Event scan start failed: %s", esp_err_to_name(err));
        }
        BleControl* bleControl = BleControl_GetInstance();
        bleControl->bleScanActive = true;
        ESP_LOGI(TAG, "Event scan started");
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // Adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Event adv start failed: %s", esp_err_to_name(err));
        }
        ESP_LOGI(TAG, "Event advertising started");
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
        {
            // Search for BLE Event Packet
            BleControl* bleControl = BleControl_GetInstance();
            if (IsEventAdvertisingPacket(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len))
            {
                ESP_LOGI(TAG, "Badge advertising packet found");
                EventAdvertisingPacket *eventAdvPacket = (EventAdvertisingPacket*)(scan_result->scan_rst.ble_adv);
                size_t b64Outlen;
                char badgeIdB64[BADGE_ID_B64_SIZE];
                mbedtls_base64_encode((unsigned char*)badgeIdB64, BADGE_ID_B64_SIZE, &b64Outlen, eventAdvPacket->eventAdvPayload.badgeId, BADGE_ID_SIZE);
                ESP_LOGI(TAG, "BadgeId [B64]: %s", badgeIdB64);
                char eventIdB64[EVENT_ID_B64_SIZE];
                mbedtls_base64_encode((unsigned char*)eventIdB64, EVENT_ID_B64_SIZE, &b64Outlen, eventAdvPacket->eventAdvPayload.eventId, EVENT_ID_SIZE);
                ESP_LOGI(TAG, "Peer Report: %s %s %d", badgeIdB64, eventIdB64, scan_result->scan_rst.rssi);
                PeerReport peerReport;
                memcpy(peerReport.badgeIdB64, badgeIdB64, BADGE_ID_B64_SIZE);
                memcpy(peerReport.eventIdB64, eventIdB64, EVENT_ID_B64_SIZE);
                peerReport.peakRssi = scan_result->scan_rst.rssi;
                if ((err = NotificationDispatcher_NotifyEvent(bleControl->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED, (void*)&peerReport, sizeof(peerReport), DEFAULT_NOTIFY_WAIT_DURATION)) != ESP_OK) {
                    ESP_LOGE(TAG, "NotificationDispatcher_NotifyEvent NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED failed: %s", esp_err_to_name(err));
                }
            }
            else
            {
                uint8_t service128UuidLen;
                uint8_t *service128Uuid = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_128SRV_CMPL, &service128UuidLen);
                if (service128Uuid != NULL)
                {
                    if (service128UuidLen < PAIR_ID_SIZE + 2)
                    {
                        break;
                    }

                    uint8_t magicNum[2] = { 0x38, 0x13 };
                    uint8_t toCheck[service128UuidLen];
                    memset(toCheck, 0, service128UuidLen);

                    // Reverse pair id bytes for comparison
                    for (int i = 0; i < PAIR_ID_SIZE; ++i)
                    {
                        memcpy(toCheck + (service128UuidLen - (PAIR_ID_SIZE + 2)) + i, bleControl->pUserSettings->settings.pairId + (PAIR_ID_SIZE - 1) - i, 1);
                    }

                    memcpy(toCheck + (service128UuidLen - 2), magicNum, 2);
                    if (!memcmp(service128Uuid, toCheck, service128UuidLen))
                    {
                        ESP_LOGI(TAG, "BLE Xfer service uuid found, enabling BLE Xfer");
                        if ((err = NotificationDispatcher_NotifyEvent(bleControl->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_REQ_RECV, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION)) != ESP_OK)
                        {
                            ESP_LOGE(TAG, "NotificationDispatcher_NotifyEvent NOTIFICATION_EVENTS_BLE_XFER_REQ_RECV failed: %s", esp_err_to_name(err));
                        }
                    }
                }
            }
            break;
        }
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Event scan stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "Event stop scan successfully");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Event adv stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "Event stop adv successfully");
        }
        break;

    default:
        break;
    }
}

esp_err_t BleControl_EnableBle(BleControl *this)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    ESP_LOGI(TAG, "Enabling BLE");
    if (!this->bleEnabled)
    {
        // Enable BT Controller
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret)
        {
            ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
            goto CONTROLLER_ENABLE_FAIL;
        }

        // Initialize and allocate resources for bluetooth
        ret = esp_bluedroid_init();
        if (ret)
        {
            ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
            goto BLUEDROID_INIT_FAIL;
        }

        // Enable Bluetooth
        ret = esp_bluedroid_enable();
        if (ret)
        {
            ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
            goto BLUEDROID_ENABLE_FAIL;
        }

        // Enable Event Mode
        ret = BleControl_EnableEventMode(this);
        if (ret)
        {
            ESP_LOGE(TAG, "%s enable event mode failed: %s", __func__, esp_err_to_name(ret));
            goto EVENT_MODE_ENABLE_FAIL;
        }

        this->bleEnabled = true;
        NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_ENABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
        ESP_LOGD(TAG, "BLE Enabled");
        return ret;

EVENT_MODE_ENABLE_FAIL:
BLUEDROID_ENABLE_FAIL:
        esp_bluedroid_deinit();
BLUEDROID_INIT_FAIL:
        esp_bt_controller_disable();
CONTROLLER_ENABLE_FAIL:
        esp_bt_controller_deinit();
        ESP_LOGE(TAG, "BLE Enable Failed");
    }

    return ret;
}

esp_err_t BleControl_DisableBle(BleControl *this)
{
    esp_err_t ret = ESP_OK;
    assert(this);

    if (this->bleEnabled)
    {
        ESP_LOGI(TAG, "Disabling BLE");
        ret = esp_ble_gap_stop_scanning();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ble_gap_stop_scanning failed. error code = %s", esp_err_to_name(ret));
        }

        // Disable bluetooth
        ret = esp_bluedroid_disable();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_bluedroid_disable failed. error code = %s", esp_err_to_name(ret));
        }

        // Frees resources from bluetooth
        ret = esp_bluedroid_deinit();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_bluedroid_deinit failed. error code = %s", esp_err_to_name(ret));
        }

        // Disable bluetooth controller
        ret = esp_bt_controller_disable();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_bt_controller_disable failed. error code = %s", esp_err_to_name(ret));
        }

        NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_DISABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
        this->bleScanActive = false;
        this->bleEnabled = false;
    }
    
    ESP_LOGI(TAG, "BLE Disabled");
    return ret;
}

esp_err_t BleControl_EnableBleXfer(BleControl *this, bool genericMode)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    if (genericMode)
    {
        // Wipe pairing data
        UserSettings_SetPairId(this->pUserSettings, NULL);
        refresh_service_uuid(this);
    }
    strncpy(this->bleName, BLE_DEVICE_NAME, sizeof(this->bleName));

    ESP_LOGI(TAG, "Enable BLE Xfer, generic mode %d", genericMode);
    ResetFrameContext(this);
    if (this->bleXferEnabled)
    {
        // Do not care about status of this
        esp_timer_stop(this->bleXferDisableTimerHandle);

        // Start the timer for timeout
        ESP_ERROR_CHECK(esp_timer_start_once(this->bleXferDisableTimerHandle, TIMER_TIMEOUT_US));
    }
    else
    {
        // Start BLE Xfer Enable Timer
        ESP_ERROR_CHECK(esp_timer_start_once(this->bleXferDisableTimerHandle, TIMER_TIMEOUT_US)); // 60 seconds

        ret = esp_ble_gap_stop_scanning();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ble_gap_stop_scanning failed. error code = %s", esp_err_to_name(ret));
        }

        // Register application callbacks with BTA GATTS module
        ret = esp_ble_gatts_register_callback(BleXferGattsEventHandler);
        if (ret)
        {
            ESP_LOGE(TAG, "gatts register error, error code = %s", esp_err_to_name(ret));
            goto GATTS_REGISTER_CALLBACK_FAIL;
        }

        // Function called when a gap event occurs, such as scan result
        ret = esp_ble_gap_register_callback(BleXferGapEventHandler);
        if (ret)
        {
            ESP_LOGE(TAG, "gap register error, error code = %s", esp_err_to_name(ret));
            goto GAP_REGISTER_CALLBACK_FAIL;
        }

        // Register application identifier
        if (!this->bleXferAppRegistered)
        {
            ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
            if (ret)
            {
                ESP_LOGE(TAG, "gatts app register error, error code = %s", esp_err_to_name(ret));
                goto GATTS_APP_REGISTER_FAIL;
            }
            this->bleXferAppRegistered = true;
        }

        // NOTE: NOT USED RIGHT NOW
        // ret = esp_ble_gatts_app_register(PROFILE_B_APP_ID);
        // if (ret)
        // {
        //     ESP_LOGE(TAG, "gatts app register error, error code = %s", esp_err_to_name(ret));
        //     return ret;
        // }

        // Change BLE MTU
        ret = esp_ble_gatt_set_local_mtu(500);
        if (ret)
        {
            ESP_LOGE(TAG, "set local MTU failed, error code = %s", esp_err_to_name(ret));
            goto GATT_SET_LOCAL_MTU_FAIL;
        }

        this->bleXferEnabled = true;
        ESP_LOGD(TAG, "BLE Xfer Enabled");
        NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_ENABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
        return ret;

GATT_SET_LOCAL_MTU_FAIL:
//     esp_ble_gatts_app_unregister();
GATTS_APP_REGISTER_FAIL:
GAP_REGISTER_CALLBACK_FAIL:
GATTS_REGISTER_CALLBACK_FAIL:
        this->bleXferEnabled = false;
        ESP_LOGE(TAG, "BLE Xfer Enable Failed");
    }
    return ret;
}

esp_err_t BleControl_DisableBleXfer(BleControl *this)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    ESP_LOGI(TAG, "Releasing Disable BLE Xfer Sem");

    // Release disable xfer BLE task
    if(this->bleXferDisableSem)
    {
        if(xSemaphoreGive(this->bleXferDisableSem) != pdTRUE)
        {
            ESP_LOGE(TAG, "BleControl_DisableBleXfer: Failed to release timeout task for BLE xfer");
            ret = ESP_FAIL;
        }
    }

    if (!this->bleXferRefreshInProgress)
    {
        this->bleXferPairMode = false;
    }
    
    return ret;
}

static esp_err_t BleControl_DisableBleXferAction(BleControl *this)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    ESP_LOGI(TAG, "Disabling BLE Xfer");

    if (this->bleXferEnabled)
    {
        // Stop the timer just in case, don't care about return value
        esp_timer_stop(this->bleXferDisableTimerHandle);
        this->bleXferEnabled = false;
        this->bleXferAppRegistered = false;

        // Re-enable event mode
        ret = BleControl_EnableEventMode(this);
        ESP_LOGI(TAG, "BleControl_EnableEventMode ret=%s", esp_err_to_name(ret));
        uint32_t percentComplete = 0;
        ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_PERCENT_CHANGED, &percentComplete, sizeof(percentComplete), DEFAULT_NOTIFY_WAIT_DURATION);
        ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_XFER_PERCENT_CHANGED event ret=%s", esp_err_to_name(ret));
        ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_DISABLED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
        ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_XFER_DISABLED event ret=%s", esp_err_to_name(ret));
    }
    
    ESP_LOGD(TAG, "BLE Xfer Disabled");
    return ret;
}

static void ResetFrameContext(BleControl *this)
{
    assert(this);
    memset((void*)this->frameContext.frameReceived, 0, MAX_BLE_FRAMES);
    this->frameContext.curNumFrames = 0;
    this->frameContext.frameLen = 0;
    this->frameContext.curCustomSeqSlot = 0;
    this->frameContext.fileType = 0;
    this->frameContext.configFrameProcessed = false;
    this->frameContext.fileProcessed = false;
    this->frameContext.frameBytesReceived = 0;
    this->frameContext.frameInProgress = false;
    if(this->frameContext.rcvBuffer)
    {
        memset((void*)this->frameContext.rcvBuffer, 0, MAX_BLE_XFER_PAYLOAD_SIZE);
    }
}

// These are used to disable ble outside the callback context
// Tryign to disable BLE inside the callback context will result in a deadlock
static void BleXferControlTask(void *pvParameters)
{
    BleControl *this = (BleControl *)pvParameters;
    assert(this);
    registerCurrentTaskInfo();
    while (true)
    {
        if(this->bleXferDisableSem)
        {
            if(xSemaphoreTake(this->bleXferDisableSem, portMAX_DELAY) == pdTRUE)
            {
                esp_err_t ret = ESP_FAIL;
                ESP_LOGI(TAG, "BleXferControlTask received bleXferDisableSem");
                ret = BleControl_DisableBleXferAction(this);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Error stopping BleControl_DisableBleXferAction. ret=%s", esp_err_to_name(ret));
                }
                ret = BleControl_DisableBle(this);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Error stopping BleControl_DisableBle. ret=%s", esp_err_to_name(ret));
                }
                ret = BleControl_EnableBle(this);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Error stopping BleControl_EnableBle. ret=%s", esp_err_to_name(ret));
                }
            }
            else
            {
                ESP_LOGE(TAG, "Error taking bleXferDisableSem");
            }
        }
        else
        {
            ESP_LOGE(TAG, "bleXferDisableSem is NULL");
            vTaskDelay(pdMS_TO_TICKS(60*1000));
        }
    }

    ESP_LOGI(TAG, "BleControlTask exiting");
}

static esp_err_t BleXferConnectionEstablishedAction(BleControl *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    ESP_LOGI(TAG, "BleConnectionEstablishedAction");

    // Disable timeout timer
    ret = esp_timer_stop(this->bleXferDisableTimerHandle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error stopping bleDisableTimerHandle. ret=%s", esp_err_to_name(ret));
    }

    ResetFrameContext(this);
    ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_CONNECTED event ret=%s", esp_err_to_name(ret));
    return NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_CONNECTED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
}

static esp_err_t ProcessTransferedFile(BleControl *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    if (this->frameContext.fileProcessed == false)
    {
        this->frameContext.fileProcessed = true;
        if (JsonUtils_ValidateJson(this->frameContext.rcvBuffer))
        {
            ESP_LOGI(TAG, "Valid JSON");
            if (this->frameContext.fileType == FILE_TYPE_LED_SEQUENCE)
            {
                ESP_LOGI(TAG, "Updating custom led sequence");
                if (LedSequences_UpdateCustomLedSequence(0, this->frameContext.rcvBuffer, MAX_BLE_XFER_PAYLOAD_SIZE) != ESP_FAIL)
                {
                    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_NEW_CUSTOM_RECV, &this->frameContext.curCustomSeqSlot, sizeof(this->frameContext.curCustomSeqSlot), DEFAULT_NOTIFY_WAIT_DURATION);
                    ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_XFER_NEW_CUSTOM_RECV event ret=%s", esp_err_to_name(ret));
                    ResetFrameContext(this);
                    ret = ESP_OK;
                }
                else
                {
                    ESP_LOGE(TAG, "Update of custom led sequence %d failed", this->frameContext.curCustomSeqSlot);
                }
            }
            else if (this->frameContext.fileType == FILE_TYPE_SETTINGS_FILE)
            {
                ESP_LOGI(TAG, "Updating settings");
                ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_NEW_SETTINGS_RECV, (void *)this->frameContext.rcvBuffer, MIN(this->frameContext.frameBytesReceived, MAX_BLE_XFER_PAYLOAD_SIZE), DEFAULT_NOTIFY_WAIT_DURATION);
                ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_XFER_NEW_SETTINGS_RECV event ret=%s", esp_err_to_name(ret));
                ResetFrameContext(this);
                ret = ESP_OK;
            }
            else if (this->frameContext.fileType == FILE_TYPE_TEST)
            {
                ESP_LOGI(TAG, "Pairing Successful");
                ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_NEW_PAIR_RECV, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
                ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_XFER_NEW_PAIR_RECV event ret=%s", esp_err_to_name(ret));
                ResetFrameContext(this);
                ret = ESP_OK;
            }
            else
            {
                ESP_LOGE(TAG, "Invalid file type %d", this->frameContext.fileType);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Invalid JSON");
        }
        
    }
    else
    {
        ESP_LOGI(TAG, "Frame already processed");
    }

    if (ret == ESP_OK)
    {
        ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_COMPLETE, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
        ESP_LOGI(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_XFER_COMPLETE event ret=%s", esp_err_to_name(ret));
    }
    else
    {
        ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_FAILED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
        ESP_LOGE(TAG, "NotificationDispatcher_NotifyEvent for NOTIFICATION_EVENTS_BLE_XFER_FAILED event ret=%s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t VerifyAllFramesPresent(BleControl *this)
{
    esp_err_t ret = ESP_FAIL;
    int count = 0;
    assert(this);
    if (this->frameContext.curNumFrames > 0)
    {
        for (int i = 0; i < this->frameContext.curNumFrames; i++)
        {
            count += this->frameContext.frameReceived[i];
        }
        if (count == this->frameContext.curNumFrames)
        {
            ret = ESP_OK;
        }
    }
    return ret;
}

static esp_err_t BleXferDisconnectedAction(BleControl *this)
{
    esp_err_t ret = ESP_FAIL;
    assert(this);
    ESP_LOGI(TAG, "On Disconnect. frameBytesReceived = %d", this->frameContext.frameBytesReceived);

    ResetFrameContext(this);

    ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_DISCONNECTED, NULL, 0, DEFAULT_NOTIFY_WAIT_DURATION);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to notify event for NOTIFICATION_EVENTS_BLE_DISCONNECTED event. ret=%s", esp_err_to_name(ret));
    }

    // Disable BLE through disable task
    if(this->bleXferDisableSem)
    {
        if(xSemaphoreGive(this->bleXferDisableSem) != pdTRUE)
        {
            ESP_LOGE(TAG, "BleXferDisconnectedAction: Failed to release timeout task");
        }
    }
    return ret;
}

static esp_err_t BleXferReceiveDataAction(BleControl *this, const char * data, int size, bool final)
{
    esp_err_t ret = ESP_OK;
    assert(this);
    
    if (!this->frameContext.configFrameProcessed && size == CONFIG_FRAME_HEADER_SIZE)
    {
        uint16_t curFrame  = (((uint16_t)data[0] << 8) | data[1]); // 0 based frame index
        uint16_t numFrames = (((uint16_t)data[2] << 8) | data[3]); // 0 based frame count
        uint16_t frameLen  = (((uint16_t)data[4] << 8) | data[5]);
        uint8_t  fileType  = (uint8_t)data[6];
        uint8_t  *pairId   = (uint8_t*)&data[7];

        if (curFrame == 0 && numFrames == 0 && frameLen > DATA_FRAME_HEADER_SIZE && frameLen < DATA_FRAME_MAX_SIZE)
        {
            if (memcmp(this->pUserSettings->settings.pairId, pairId, PAIR_ID_SIZE))
            {
                ESP_LOGI(TAG, "Pairing to new device. mobileId = %s", pairId);
                UserSettings_SetPairId(this->pUserSettings, pairId);
            }
        }
        else if (curFrame == 0 && numFrames > 0 && frameLen > DATA_FRAME_HEADER_SIZE && frameLen < DATA_FRAME_MAX_SIZE)
        {
            this->frameContext.configFrameProcessed = true;
            this->frameContext.frameReceived[curFrame] = 1;
            this->frameContext.curNumFrames = numFrames+1; // store 1 based frame count
            this->frameContext.frameLen = frameLen;
            this->frameContext.fileType = fileType;
            if (memcmp(this->pUserSettings->settings.pairId, pairId, PAIR_ID_SIZE))
            {
                ESP_LOGI(TAG, "Pairing to new device. pairId = %s", pairId);
                UserSettings_SetPairId(this->pUserSettings, pairId);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Invalid frame count");
        }
    }
    else
    {
        if (size > DATA_FRAME_HEADER_SIZE)
        {
            uint16_t curFrame = (((uint16_t)data[0] << 8) | data[1]);
            int curFrameSize = this->frameContext.frameLen - DATA_FRAME_HEADER_SIZE;
            int curBufferOffset = (curFrame - 1) * curFrameSize; // -1 because the data frames begin at 1
            const char *frameBuffer = &data[DATA_FRAME_HEADER_SIZE];

            if ((curBufferOffset + curFrameSize < MAX_BLE_XFER_PAYLOAD_SIZE) && (curFrame < MAX_BLE_FRAMES))
            {
                ESP_LOGI(TAG, "Loading frame %d data at offset %d:%d", curFrame, curBufferOffset, curBufferOffset+curFrameSize);
                uint32_t percentComplete;

                // avoid divide by zero
                if (this->frameContext.curNumFrames == 0)
                {
                    percentComplete = 100;
                }
                else
                {
                    percentComplete = (((curFrame+1)*100)/this->frameContext.curNumFrames);
                }

                ret = NotificationDispatcher_NotifyEvent(this->pNotificationDispatcher, NOTIFICATION_EVENTS_BLE_XFER_PERCENT_CHANGED, &percentComplete, sizeof(percentComplete), DEFAULT_NOTIFY_WAIT_DURATION);
                memcpy((void*)&this->frameContext.rcvBuffer[curBufferOffset], frameBuffer, curFrameSize);
                this->frameContext.frameReceived[curFrame] = 1;
                this->frameContext.frameBytesReceived += curFrameSize;
                if (VerifyAllFramesPresent(this) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Processing completed file. file size=%d", this->frameContext.frameBytesReceived);
                    ret = ProcessTransferedFile(this);
                    if (ret == ESP_FAIL)
                    {
                        ESP_LOGE(TAG, "BleXferReceiveDataAction: ProcessTransferedFile failed ret=%s", esp_err_to_name(ret));
                    }
                    ret = BleXferDisconnectedAction(this);
                    if (ret == ESP_FAIL)
                    {
                        ESP_LOGE(TAG, "BleXferReceiveDataAction: BleXferDisconnectedAction failed ret=%s", esp_err_to_name(ret));
                    }
                }
            }
            else
            {
                ret = ESP_FAIL;
                ESP_LOGE(TAG, "Frame would exceed maximum file size. curBufferOffset = %d, size = %d", curBufferOffset, size);
            }
        }
        else
        {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Frame has insufficient data. size = %d", size);
        }
    }
    return ret;
}

static esp_err_t BleXferWriteEventAction(BleControl * this, esp_gatt_if_t gattsIf, PrepareTypeEnv *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    esp_err_t ret = ESP_FAIL;
    esp_gatt_status_t status = ESP_GATT_OK;
    assert(this);
    if (param->write.need_rsp)
    {
        if (param->write.is_prep)
        {
            if (prepare_write_env->prepareBuf == NULL) 
            {
                prepare_write_env->prepareBuf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE*sizeof(uint8_t));
                prepare_write_env->prepareLen = 0;
                if (prepare_write_env->prepareBuf == NULL)
                {
                    ESP_LOGE(TAG, "Gatt_server prep no mem");
                    status = ESP_GATT_NO_RESOURCES;
                    ret = ESP_FAIL;
                }
            }
            else 
            {
                if(param->write.offset > PREPARE_BUF_MAX_SIZE) 
                {
                    status = ESP_GATT_INVALID_OFFSET;
                    ESP_LOGE(TAG, "Write offset exceeds buff max size");
                }
                else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) 
                {
                    status = ESP_GATT_INVALID_ATTR_LEN;
                    ESP_LOGE(TAG, "Write offset exceeds buff max size");
                }
            }

            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)heap_caps_malloc(sizeof(esp_gatt_rsp_t), MALLOC_CAP_SPIRAM);
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gattsIf, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            ESP_ERROR_CHECK(response_err);
            
            heap_caps_free(gatt_rsp);
            if (status == ESP_GATT_OK)
            {
                ret = ESP_OK;
                memcpy(prepare_write_env->prepareBuf + param->write.offset,
                    param->write.value,
                    param->write.len);
                prepare_write_env->prepareLen += param->write.len;
            }
            else
            {
                ret = ESP_FAIL;
            }
        }
        else
        {
            // JER: This logic might need revisited
            // EMP: ESP_ERROR_CHECK failed: esp_err_t 0x103SP_ERR_INVALID_STATE
            ret = esp_ble_gatts_send_response(gattsIf, param->write.conn_id, param->write.trans_id, status, NULL);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Send response error");
            }
        }
    }
    return ret;
}

static esp_err_t BleXferExecWriteEventAction(BleControl *this, PrepareTypeEnv *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    esp_err_t ret = ESP_OK; // JER: Need to revisit how to tell if there are any errors here
    assert(this);
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
    {
        ret = BleXferReceiveDataAction(this, (const char *)prepare_write_env->prepareBuf, prepare_write_env->prepareLen, false);
        //esp_log_buffer_hex(TAG, prepare_write_env->prepareBuf, prepare_write_env->prepareLen);
    }
    else
    {
        ESP_LOGI(TAG,"ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepareBuf)
    {
        ESP_LOGI(TAG,"Freeing prepare_write_env->prepareBuf");
        free(prepare_write_env->prepareBuf);
        prepare_write_env->prepareBuf = NULL;
    }
    prepare_write_env->prepareLen = 0;
    return ret;
}

static void BleXferGapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    BleControl * this = BleControl_GetInstance();
    assert(this);
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        this->bleXferParameters.advConfigDone &= (~adv_config_flag);
        if (this->bleXferParameters.advConfigDone == 0){
            esp_ble_gap_start_advertising(&this->bleXferParameters.advParams);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        this->bleXferParameters.advConfigDone &= (~SCAN_RSP_CONFIG_FLAG);
        if (this->bleXferParameters.advConfigDone == 0){
            esp_ble_gap_start_advertising(&this->bleXferParameters.advParams);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(TAG, "Stop adv successfully");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

void refresh_service_uuid(BleControl *this)
{
    // Overwrite with base service UUID
    memcpy(ADV_SERVICE_UUID128, ADV_BASE_SERVICE_UUID128, sizeof(ADV_SERVICE_UUID128));

    // If pairID is set, update the UUID with the pairID
    // ASSUMPTION that pairID byte is 0x7b when set
    if (this && this->pUserSettings && this->pUserSettings->settings.pairId[0] != '\0')
    {
        ESP_LOGI(TAG, "BLE Pair ID: %s", this->pUserSettings->settings.pairId);
        update_uuid_with_pairId(ADV_SERVICE_UUID128, this->pUserSettings->settings.pairId);
    }

    // Setup response/advertisement data
    this->bleXferParameters.advData.p_service_uuid = (uint8_t *)ADV_SERVICE_UUID128;
    this->bleXferParameters.scanRspData.p_service_uuid = (uint8_t *)ADV_SERVICE_UUID128;
}

static void BleXferGattsProfileAEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gattsIf, esp_ble_gatts_cb_param_t *param)
{
    esp_err_t ret = ESP_OK;
    BleControl *this = BleControl_GetInstance();
    assert(this);
    switch (event) 
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "REGISTER_APP_EVT, status %d, appId %d", param->reg.status, param->reg.app_id);
        refresh_service_uuid(this);
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceId.is_primary = true;
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceId.id.inst_id = 0x00;
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceId.id.uuid.len = ESP_UUID_LEN_128;
        memcpy(this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceId.id.uuid.uuid.uuid128, ADV_SERVICE_UUID128, sizeof(this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceId.id.uuid.uuid.uuid128));

        ret = esp_ble_gap_set_device_name(this->bleName);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "set device name failed, error code = %s", esp_err_to_name(ret));
        }
#ifdef CONFIG_SET_RAW_ADV_DATA
        ret = esp_ble_gap_config_adv_data_raw(RAW_ADV_DATA, sizeof(RAW_ADV_DATA));
        if (ret)
        {
            ESP_LOGE(TAG, "config raw adv data failed, error code = %s ", esp_err_to_name(ret));
        }
        this->bleXferParameters.advConfigDone |= adv_config_flag;
        ret = esp_ble_gap_config_scan_rsp_data_raw(RAW_SCAN_RSP_DATA, sizeof(RAW_SCAN_RSP_DATA));
        if (ret)
        {
            ESP_LOGE(TAG, "config raw scan rsp data failed, error code = %s", esp_err_to_name(ret));
        }
        this->bleXferParameters.advConfigDone |= SCAN_RSP_CONFIG_FLAG;
#else
        //config adv data
        ret = esp_ble_gap_config_adv_data(&this->bleXferParameters.advData);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "config adv data failed, error code = %s", esp_err_to_name(ret));
        }
        this->bleXferParameters.advConfigDone |= adv_config_flag;
        //config scan response data
        ret = esp_ble_gap_config_adv_data(&this->bleXferParameters.scanRspData);
        if (ret)
        {
            ESP_LOGE(TAG, "config scan response data failed, error code = %s", esp_err_to_name(ret));
        }
        this->bleXferParameters.advConfigDone |= SCAN_RSP_CONFIG_FLAG;
#endif
        ret = esp_ble_gatts_create_service(gattsIf, &this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceId, GATTS_NUM_HANDLE_TEST_A);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "config scan response data failed, error code = %s", esp_err_to_name(ret));
        }
        break;
    case ESP_GATTS_READ_EVT: 
    {
        ESP_LOGI(TAG, "GATT_READ_EVT, connId %d, trans_id %lu, handle %d", param->read.conn_id, param->read.trans_id, param->read.handle);
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        
        int printed_bytes = 0;
        int length = 0;
        
        // Write badgeID
        length = 8;
        memcpy((void*)&rsp.attr_value.value+printed_bytes, this->pUserSettings->badgeId, length);
        printed_bytes += length;
        
        // Write Settings
        unsigned char settings_pack[] = {this->pUserSettings->settings.vibrationEnabled << 1 | this->pUserSettings->settings.soundEnabled};
        length = 1;
        memcpy((void*)&rsp.attr_value.value+printed_bytes, settings_pack, length);
        printed_bytes += length;

        // Write Badge_Type
        length = 1;
        int coded_badge_type = GetBadgeType();
        memcpy((void*)&rsp.attr_value.value+printed_bytes, &coded_badge_type, length);
        printed_bytes += length;
        
        // TODO: Write SongBits (12 bits) [2 bytes]
        length = 2; // 2 bytes for 12 bits
        uint16_t temp_song_spacer = 0;
        memcpy((void*)&rsp.attr_value.value+printed_bytes, &temp_song_spacer, length);
        printed_bytes += length;

        // Write Wifi_SSID
        length = sizeof(this->pUserSettings->settings.wifiSettings.ssid) / sizeof(this->pUserSettings->settings.wifiSettings.ssid[0]);
        memcpy((void*)&rsp.attr_value.value+printed_bytes, this->pUserSettings->settings.wifiSettings.ssid, length);
        printed_bytes += length;
        
        rsp.attr_value.len = printed_bytes;

        ESP_LOGI(TAG, "read value: \"%s\" size=%d", rsp.attr_value.value, rsp.attr_value.len);
        ret = esp_ble_gatts_send_response(gattsIf, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "send response error. error code = %s", esp_err_to_name(ret));
        }
        break;
    }
    case ESP_GATTS_WRITE_EVT: 
    {
        // ESP_LOGI(TAG, "GATT_WRITE_EVT, connId %d, trans_id %d, handle %d", param->write.connId, param->write.trans_id, param->write.handle);
        if (!param->write.is_prep)
        {
            // ESP_LOGI(TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
            // ESP_LOGI(TAG, "%d Received %d bytes for packet %d of %d", __LINE__, param->write.len, param->write.value[0], param->write.value[1]);
            ret = BleXferReceiveDataAction(this, (const char *)param->write.value, param->write.len, true);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "BleReceiveDataAction failed, error code = %s", esp_err_to_name(ret));
            }
            //esp_log_buffer_hex(TAG, param->write.value, param->write.len);
            if (this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].descrHandle == param->write.handle && param->write.len == 2)
            {
                uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == 0x0001)
                {
                    if (this->bleXferParameters.aProperty & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
                    {
                        ESP_LOGI(TAG, "notify enable");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++i)
                        {
                            notify_data[i] = i%0xff;
                        }
                        //the size of notify_data[] need less than MTU size
                        ret = esp_ble_gatts_send_indicate(gattsIf, param->write.conn_id, this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].charHandle,
                                                          sizeof(notify_data), notify_data, false);
                        if (ret != ESP_OK)
                        {
                            ESP_LOGE(TAG, "send A indicate error, error code = %s", esp_err_to_name(ret));
                        }
                    }
                }
                else if (descr_value == 0x0002)
                {
                    if (this->bleXferParameters.aProperty & ESP_GATT_CHAR_PROP_BIT_INDICATE)
                    {
                        ESP_LOGI(TAG, "indicate enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            indicate_data[i] = i%0xff;
                        }
                        //the size of indicate_data[] need less than MTU size
                        ret = esp_ble_gatts_send_indicate(gattsIf, param->write.conn_id, this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].charHandle,
                                                          sizeof(indicate_data), indicate_data, true);
                        if (ret != ESP_OK)
                        {
                            ESP_LOGE(TAG, "send B indicate error, error code = %s", esp_err_to_name(ret));
                        }
                    }
                }
                else if (descr_value == 0x0000)
                {
                    ESP_LOGI(TAG, "notify/indicate disable ");
                }
                else
                {
                    ESP_LOGE(TAG, "unknown descr value");
                    esp_log_buffer_hex(TAG, param->write.value, param->write.len);
                }
            }
        }
        ret = BleXferWriteEventAction(this, gattsIf, &this->bleXferParameters.aPrepareWriteEnv, param);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "BleWriteEventAction failed, error code = %s", esp_err_to_name(ret));
        }
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        // ESP_LOGI(TAG,"ESP_GATTS_EXEC_WRITE_EVT");
        ret = esp_ble_gatts_send_response(gattsIf, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ble_gatts_send_response failed, error code = %s", esp_err_to_name(ret));
        }
        ret = BleXferExecWriteEventAction(this, &this->bleXferParameters.aPrepareWriteEnv, param);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "BleExecWriteEventAction failed, error code = %s", esp_err_to_name(ret));
        }
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "CREATE_SERVICE_EVT, status %d,  serviceHandle %d", param->create.status, param->create.service_handle);
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceHandle = param->create.service_handle;
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].charUuid.len = ESP_UUID_LEN_128;
        memcpy(this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].charUuid.uuid.uuid128, GATTS_CHAR_UUID_TEST_A_128, sizeof(this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].charUuid.uuid.uuid128));
        
        ret = esp_ble_gatts_start_service(this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceHandle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ble_gatts_start_service failed, error code = %s", esp_err_to_name(ret));
        }
        this->bleXferParameters.aProperty = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
        ret = esp_ble_gatts_add_char(this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceHandle, &this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].charUuid,
                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                     this->bleXferParameters.aProperty,
                                     &this->bleXferParameters.gattsChar1Val, NULL);
        if (ret)
        {
            ESP_LOGE(TAG, "add char failed, error code = %s", esp_err_to_name(ret));
        }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
    {
        uint16_t length = 0;
        const uint8_t *prf_char;

        ESP_LOGI(TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, serviceHandle %d",
                param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].charHandle = param->add_char.attr_handle;
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].descrUuid.len = ESP_UUID_LEN_16;
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].descrUuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "esp_ble_gatts_get_attr_value error. ret=%s", esp_err_to_name(ret));
        }

        ESP_LOGI(TAG, "the gatts demo char length = %d", length);
        for(int i = 0; i < length; i++)
        {
            ESP_LOGI(TAG, "prf_char[%x] =%x",i,prf_char[i]);
        }
        ret = esp_ble_gatts_add_char_descr(this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].serviceHandle, &this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].descrUuid,
                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        if (ret)
        {
            ESP_LOGE(TAG, "add char descr failed, error code = %s", esp_err_to_name(ret));
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].descrHandle = param->add_char_descr.attr_handle;
        ESP_LOGI(TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, serviceHandle %d",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, serviceHandle %d", param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
    {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
        ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, connId %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        this->bleXferParameters.glProfileTable[PROFILE_A_APP_ID].connId = param->connect.conn_id;
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        BleXferConnectionEstablishedAction(this);
        break;
    }
    
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
        esp_ble_gap_start_advertising(&this->bleXferParameters.advParams);
        BleXferDisconnectedAction(this);
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status %d attr_handle %d", param->conf.status, param->conf.handle);
        if (param->conf.status != ESP_GATT_OK)
        {
            esp_log_buffer_hex(TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

static void BleXferGattsEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gattsIf, esp_ble_gatts_cb_param_t *param)
{
    BleControl *this = BleControl_GetInstance();
    assert(this);
    // If event is register event, store the gattsIf for each profile
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            this->bleXferParameters.glProfileTable[param->reg.app_id].gattsIf = gattsIf;
        }
        else
        {
            ESP_LOGI(TAG, "Reg app failed, appId %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    // If the gattsIf equal to profile A, call profile A cb handler,
    // so here call each profile's callback 
    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            if (gattsIf == ESP_GATT_IF_NONE || // ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function`
                gattsIf == this->bleXferParameters.glProfileTable[idx].gattsIf)
            {
                if (this->bleXferParameters.glProfileTable[idx].gattsCb)
                {
                    this->bleXferParameters.glProfileTable[idx].gattsCb(event, gattsIf, param);
                }
            }
        }
    } while (0);
}

static void BleXferDisableTimeoutEventHandler(void* arg)
{
    BleControl *this = (BleControl*) arg;
    assert(this);
    // Release disable xfer BLE task
    BleControl_DisableBleXfer(this);
}
