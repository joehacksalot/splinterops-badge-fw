
#ifndef BLECONTROL_SERVICECHAR_FILETRANSFER_H_
#define BLECONTROL_SERVICECHAR_FILETRANSFER_H_

#include "BleControl.h"

esp_err_t _BleControl_GetFileTransferReadResponse(BleControl *this, uint8_t * buffer, uint32_t size, uint16_t * pLength);
void BleControl_SetTouchSensorActive(BleControl *this, uint32_t touchSensorIndex, bool active);
void _BleControl_ResetFrameContext(BleControl *this);

#endif // BLECONTROL_SERVICECHAR_FILETRANSFER_H_
