
// #ifndef BLE_SPEC_H
// #define BLE_SPEC_H

// #include <stdlib.h>
// #include "esp_gap_ble_api.h"

// #define BLE_ADV_DATA_MAX_SIZE (31)

// // Shaun TODO: Add local copy of these to repo
// // References:
// //      https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1721324589188
// //      https://www.bluetooth.org/docman/handlers/DownloadDoc.ashx?doc_id=519976

// typedef struct BleAdvertising_FlagsBits_t
// {
//     uint8_t LE_Limited_Discoverable_Mode : 1;
//     uint8_t LE_General_Discoverable_Mode : 1;
//     uint8_t BR_EDR_Not_Supported : 1;
//     uint8_t Simultaneous_LE_BR_EDR_Capable : 1;
//     uint8_t Previously_Used : 1;
//     uint8_t Reserved : 3;
// } BleAdvertising_FlagsBits;

// typedef struct BleAdvertising_Block_t
// {
//     uint8_t type;  // Must be set to BLE_ADV_TYPE_SHORT_NAME or BLE_ADV_TYPE_COMPLETE_NAME
//     uint8_t length;
//     uint8_t *data;
//     uint8_t dataLen;
// } BleAdvertising_Block;

// uint8_t BleAdvertising_Block_Serialize(esp_ble_adv_data_type type, uint8_t *pData, uint8_t dataLen, uint8_t *pBuffer, uint8_t bufferLen);

// int16_t BleAdvertising_Block_Deserialize(uint8_t *pBuffer, uint8_t bufferLen, esp_ble_adv_data_type *type, uint8_t **pData, uint8_t *dataLen);

// #endif // 