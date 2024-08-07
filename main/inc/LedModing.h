#ifndef LED_MODING_H_
#define LED_MODING_H_

#include <stdio.h>

#include "LedControl.h"

typedef struct LedModing_t
{
    bool touchActive;
    bool gameEventActive;
    bool bleServiceEnabled;
    bool bleConnected;
    bool otaDownloadInitiatedActive;
    bool batteryIndicatorActive;
    bool ledSequencePreviewActive;
    bool ledGameStatusActive;
    bool ledGameInteractiveActive;
    bool songActiveStatus;
    bool bleFileTransferInProgress;
    bool networkTestActive;
    bool bleReconnecting;
    // LedStatusIndicator curStatusIndicator;
    LedControl *pLedControl;
} LedModing;

esp_err_t LedModing_Init(LedModing *this, LedControl *pLedControl);
esp_err_t LedModing_SetTouchActive(LedModing *this, bool active);
esp_err_t LedModing_SetGameEventActive(LedModing *this, bool active);
esp_err_t LedModing_SetBatteryIndicatorActive(LedModing *this, bool active);
esp_err_t LedModing_SetOtaDownloadInitiatedActive(LedModing *this, bool active);
esp_err_t LedModing_SetBleReconnectingActive(LedModing *this, bool active);
esp_err_t LedModing_SetBleFileTransferIPActive(LedModing *this, bool active);
esp_err_t LedModing_SetBleServiceEnableActive(LedModing *this, bool active);
esp_err_t LedModing_SetBleConnectedActive(LedModing *this, bool active);
esp_err_t LedModing_SetLedSequencePreviewActive(LedModing *this, bool active);
esp_err_t LedModing_SetGameEventActive(LedModing *this, bool active);
esp_err_t LedModing_SetGameStatusActive(LedModing *this, bool active);
esp_err_t LedModing_SetInteractiveGameActive(LedModing *this, bool active);
esp_err_t LedModing_SetNetworkTestActive(LedModing *this, bool active);
esp_err_t LedModing_SetSongActiveStatusActive(LedModing *this, bool active);
esp_err_t LedModing_SetLedCustomSequence(LedModing *this, int newCustomIndex);
esp_err_t LedModing_CycleSelectedLedSequence(LedModing *this, bool direction);


#endif // LED_MODING_H_