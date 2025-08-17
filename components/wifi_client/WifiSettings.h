#ifndef WIFI_SETTINGS_H_
#define WIFI_SETTINGS_H_

#define MAX_SSID_LENGTH (32)
#define MAX_PASSWORD_LENGTH (64)

typedef struct WifiSettings_t
{
    uint8_t ssid[MAX_SSID_LENGTH];
    uint8_t password[MAX_PASSWORD_LENGTH];
} WifiSettings;

#endif /* WIFI_SETTINGS_H_ */