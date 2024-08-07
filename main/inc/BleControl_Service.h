
#ifndef BLECONTROL_SERVICE_H_
#define BLECONTROL_SERVICE_H_

#include "BleControl.h"

// Internal Type Definitions
typedef enum FileType_e
{
   FILE_TYPE_LED_SEQUENCE = 1,
   FILE_TYPE_SETTINGS_FILE = 2,
   FILE_TYPE_TEST = 3
} FileType;

typedef struct BleSettingsResponseData_t
{
    uint8_t badgeId[8];
    struct BleSettingsPackedBits_t
    {
        uint8_t soundEnabled : 1;
        uint8_t vibrationEnabled : 1;
        uint8_t unused : 6;
    }  packedSettings;
    uint8_t badgeType;
    uint16_t songBits;
    uint8_t ssid[MAX_SSID_LENGTH];
} BleFileTransferResponseData;

esp_err_t BleControl_EnableBle(BleControl *this);
esp_err_t BleControl_DisableBle(BleControl *this);
esp_err_t BleControl_EnableBleService(BleControl *this, bool genericMode, uint32_t timeoutUsec);
esp_err_t BleControl_DisableBleService(BleControl *this, bool notify);
void _BleControl_RefreshServiceUuid(BleControl *this);
void _BleControl_BleServiceDisableTimeoutEventHandler(void* arg);
void _BleControl_StartAdvertisement(BleControl *this, bool advertiseService);

#endif // BLECONTROL_SERVICE_H_