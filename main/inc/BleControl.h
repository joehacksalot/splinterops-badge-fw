
#ifndef BLE_CONFIG_H_
#define BLE_CONFIG_H_

#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"

#include "GameState.h"
#include "NotificationDispatcher.h"
#include "UserSettings.h"

#define DATA_FRAME_HEADER_SIZE 2
#define DATA_FRAME_MAX_SIZE 500
#define CONFIG_FRAME_HEADER_SIZE 15
#define MAX_BLE_FRAMES 1024
#define PROFILE_NUM 1 //2
#define PROFILE_A_APP_ID 0
#define EVENT_ADV_MAGIC_NUMBER 0x1337
//#define PROFILE_B_APP_ID 1

typedef struct AdvertisingHeader_t {
    uint8_t flags[3];
    uint8_t length;
    uint8_t type;
    uint16_t magicNum;
}__attribute__((packed)) AdvertisingHeader;

typedef struct EventAdvertisingPayload_t {
    uint8_t badgeId[BADGE_ID_SIZE];
    uint8_t eventId[EVENT_ID_SIZE];
}__attribute__((packed)) EventAdvertisingPayload;

typedef struct EventAdvertisingPacket_t {
    AdvertisingHeader eventAdvHeader;
    EventAdvertisingPayload eventAdvPayload;
}__attribute__((packed)) EventAdvertisingPacket;

typedef struct FrameContext_t
{
    bool configFrameProcessed;
    bool fileProcessed;
    bool frameInProgress;
    uint8_t fileType;
    int curNumFrames;
    int frameLen;
    int curCustomSeqSlot;
    int frameBytesReceived;
    uint8_t frameReceived[MAX_BLE_FRAMES];
    const char * rcvBuffer;
} FrameContext;

typedef struct GattsProfileInst_t
{
    esp_gatts_cb_t gattsCb;
    uint16_t gattsIf;
    uint16_t appId;
    uint16_t connId;
    uint16_t serviceHandle;
    esp_gatt_srvc_id_t serviceId;
    uint16_t charHandle;
    esp_bt_uuid_t charUuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descrHandle;
    esp_bt_uuid_t descrUuid;
} BleGattsProfileInst;

typedef struct PrepareTypeEnv_t 
{
    uint8_t *prepareBuf;
    int prepareLen;
} PrepareTypeEnv;

typedef struct BleXferParameters_t
{
    BleGattsProfileInst glProfileTable[PROFILE_NUM];
    PrepareTypeEnv aPrepareWriteEnv;
    PrepareTypeEnv bPrepareWriteEnv;
    esp_gatt_char_prop_t aProperty;
    esp_gatt_char_prop_t bProperty;
    uint8_t advConfigDone;
    esp_ble_adv_params_t advParams;
    esp_ble_adv_data_t advData;
    esp_ble_adv_data_t scanRspData;
    esp_attr_value_t gattsChar1Val;
} BleXferParameters;

typedef struct BleControl_t
{
    bool bleEnabled;
    bool bleXferEnabled;
    bool bleXferPairMode;
    bool bleXferRefreshInProgress;
    bool bleXferAppRegistered;
    bool bleScanActive;
    char eventIdB64[EVENT_ID_B64_SIZE];
    esp_timer_handle_t bleXferDisableTimerHandle;  // Timeout timer to disable BLE if not used
    esp_timer_create_args_t bleXferDisableTimerHandleArgs;
    BleXferParameters bleXferParameters;
    FrameContext frameContext;
    SemaphoreHandle_t bleXferDisableSem;
    NotificationDispatcher *pNotificationDispatcher;
    UserSettings *pUserSettings;
    GameState *pGameState;
    EventAdvertisingPayload eventAdvPayload;
    char bleName[24];
} BleControl;

bool IsEventAdvertisingPacket(uint8_t *advData, uint8_t advDataLen);
bool IsAppAdvertisingPacket(uint8_t *advData, uint8_t advDataLen);
esp_err_t SetEventAdvertisingData(BleControl *this);
BleControl * BleControl_GetInstance();
esp_err_t BleControl_Init(BleControl *this, NotificationDispatcher *pNotificationDispatcher, UserSettings *pUserSettings, GameState *pGameSettings);
esp_err_t BleControl_UpdateEventId(BleControl *this, char *newEventId);
esp_err_t BleControl_RegisterEventModeCallback(BleControl *this);
esp_err_t BleControl_EnableEventMode(BleControl *this);
esp_err_t BleControl_EnableBle(BleControl *this);
esp_err_t BleControl_DisableBle(BleControl *this);
esp_err_t BleControl_EnableBleXfer(BleControl *this, bool genericMode);
esp_err_t BleControl_DisableBleXfer(BleControl *this);
bool BleControl_BleXferEnabled(BleControl *this);

#endif // BLE_CONFIG_H_
