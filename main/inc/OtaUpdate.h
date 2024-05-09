#ifndef OTA_UPDATE_TASK_H
#define OTA_UPDATE_TASK_H

#include "esp_err.h"

#include "NotificationDispatcher.h"
#include "WifiClient.h"

typedef struct OtaUpdate_t
{
  WifiClient *pWifiClient;
  NotificationDispatcher * pNotificationDispatcher;
} OtaUpdate;

esp_err_t OtaUpdate_Init(OtaUpdate *this, WifiClient *pWifiClient, NotificationDispatcher * pNotificationDispatcher);

#endif // OTA_UPDATE_TASK_H