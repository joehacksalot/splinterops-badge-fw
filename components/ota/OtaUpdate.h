#ifndef OTA_UPDATE_TASK_H
#define OTA_UPDATE_TASK_H

#include "esp_err.h"

#include "NotificationDispatcher.h"
#include "WifiClient.h"

typedef struct OtaUpdate_t
{
  WifiClient *pWifiClient;
  NotificationDispatcher * pNotificationDispatcher;
  int updateAvailableEventId;
  int downloadInitiatedEventId;
  int downloadCompletedEventId;
} OtaUpdate;

esp_err_t OtaUpdate_Init(OtaUpdate *this, WifiClient *pWifiClient, NotificationDispatcher * pNotificationDispatcher, int priority, int cpuNumber, int updateAvailableEventId, int downloadInitiatedEventId, int downloadCompletedEventId);

#endif // OTA_UPDATE_TASK_H