
// #include <string.h>
// #include "BleSpec.h"

// uint8_t BleAdvertising_Block_Serialize(esp_ble_adv_data_type type, uint8_t *pData, uint8_t dataLen, uint8_t *pBuffer, uint8_t bufferLen)
// {
//     const uint8_t LENGTH_BYTE_LENGTH = sizeof(uint8_t);
//     const uint8_t TYPE_BYTE_LENGTH = sizeof(uint8_t);
//     uint8_t writeSize = LENGTH_BYTE_LENGTH + TYPE_BYTE_LENGTH + dataLen;
//     assert(pBuffer);
//     assert(bufferLen >= writeSize);

//     pBuffer[0] = TYPE_BYTE_LENGTH + dataLen; // Length Byte, includes type byte and data bytes
//     pBuffer[1] = type;                       // Type Byte
//     memcpy(&pBuffer[2], pData, dataLen);     // Data Bytes
//     return writeSize;
// }

// // 
// // Deserializes a BleAdvertising_Block from a buffer.
// // 
// // @param pBuffer   - in -  pointer to the buffer containing the serialized data
// // @param bufferLen - in -  length of the buffer
// // @param type      - out - pointer to a variable to store the deserialized esp_ble_adv_data_type
// // @param pData     - out - pointer will be set to the offset into pBuffer where the first data section in pBuffer is
// // @param dataLen   - out - pointer to a length of the first data section in pBuffer
// // 
// // @return the number of bytes deserialized from pBuffer. -1 if failed to deserialize
// // 
// // @throws None
// // 
// int16_t BleAdvertising_Block_Deserialize(uint8_t *pBuffer, uint8_t bufferLen, esp_ble_adv_data_type *type, uint8_t **pData, uint8_t *dataLen)
// {
//     const uint8_t MIN_DATA_LENGTH = 1;
//     const uint8_t LENGTH_BYTE_LENGTH = sizeof(uint8_t);
//     const uint8_t TYPE_BYTE_LENGTH = sizeof(uint8_t);
//     assert(pBuffer);
//     if (bufferLen < LENGTH_BYTE_LENGTH + TYPE_BYTE_LENGTH + MIN_DATA_LENGTH)
//     {
//         return -1;
//     }

//     int16_t lengthByteValue = pBuffer[0];         // first byte is length, includes type byte and data bytes
//     *type = (esp_ble_adv_data_type)pBuffer[1];    // second byte is type, defined in esp_ble_adv_data_type
//     *pData = &pBuffer[2];                         // remaining bytes are data bytes
//     *dataLen = lengthByteValue - TYPE_BYTE_LENGTH;
//     return LENGTH_BYTE_LENGTH + lengthByteValue;
// }