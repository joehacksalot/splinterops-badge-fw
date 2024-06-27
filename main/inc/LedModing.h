#ifndef LED_MODING_H_
#define LED_MODING_H_

#include <stdio.h>

#include "LedControl.h"

typedef struct LedModing_t
{
    bool touchActive;
    bool gameEventActive;
    bool bleEnabled;
    bool bleConnected;
    bool batteryIndicatorActive;
    bool ledSequencePreviewActive;
    bool ledGameStatusActive;
    bool songActiveStatus;
    bool bleXferInProgress;
    bool networkTestActive;
    LedStatusIndicator curStatusIndicator;
    LedControl *pLedControl;
} LedModing;

esp_err_t LedModing_Init(LedModing *this, LedControl *pLedControl);
esp_err_t LedModing_SetTouchActive(LedModing *this, bool active);
esp_err_t LedModing_SetGameEventActive(LedModing *this, bool active);
esp_err_t LedModing_SetBatteryIndicatorActive(LedModing *this, bool active);
esp_err_t LedModing_SetBleXferActive(LedModing *this, bool active);
esp_err_t LedModing_SetBleXferEnableActive(LedModing *this, bool active);
esp_err_t LedModing_SetBleConnectedActive(LedModing *this, bool active);
esp_err_t LedModing_SetLedSequencePreviewActive(LedModing *this, bool active);
esp_err_t LedModing_SetGameEventActive(LedModing *this, bool active);
esp_err_t LedModing_SetGameStatusActive(LedModing *this, bool active);
esp_err_t LedModing_SetNetworkTestActive(LedModing *this, bool active);
esp_err_t LedModing_SetSongActiveStatusActive(LedModing *this, bool active);
esp_err_t LedModing_SetStatusIndicator(LedModing *this, LedStatusIndicator ledStatusIndicator);

esp_err_t LedModing_SetLedCustomSequence(LedModing *this, int newCustomIndex);
esp_err_t LedModing_CycleSelectedLedSequence(LedModing *this, bool direction);


#endif // LED_MODING_H_