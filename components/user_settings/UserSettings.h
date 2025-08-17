#ifndef USER_SETTINGS_H_
#define USER_SETTINGS_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "BatterySensor.h"
#include "WifiSettings.h"
#include "GameTypes.h"
#include "Badge.h"

typedef struct UserSettingsFile_t
{
    uint32_t selectedIndex;
    uint8_t soundEnabled;
    uint8_t vibrationEnabled;
    uint8_t pairId[PAIR_ID_SIZE];
    WifiSettings wifiSettings;
    uint8_t reserved;
} UserSettingsFile;

typedef struct UserSettings_t
{
    UserSettingsFile settings;
    bool updateNeeded;
    SemaphoreHandle_t mutex;
    BatterySensor *pBatterySensor;
} UserSettings;

esp_err_t UserSettings_Init(UserSettings *this, BatterySensor * pBatterySensor, int userSettingsPriority);

esp_err_t UserSettings_UpdateFromJson(UserSettings *this, uint8_t * settingsJson);

esp_err_t UserSettings_SetPairId(UserSettings *this, uint8_t * pairId);

esp_err_t UserSettings_SetSelectedIndex(UserSettings *this, uint32_t selectedIndex);

#endif // USER_SETTINGS_H_