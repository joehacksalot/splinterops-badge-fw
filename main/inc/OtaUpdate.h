/**
 * @file OtaUpdate.h
 * @brief Over-The-Air firmware update system
 * 
 * This module provides OTA (Over-The-Air) firmware update functionality including:
 * - Remote firmware download and verification
 * - Secure firmware image validation
 * - Rollback protection and recovery mechanisms
 * - WiFi-based update delivery
 * - Progress tracking and user notification
 * - Integration with notification system for update events
 * - Automatic update checking and scheduling
 * 
 * The OTA system enables remote firmware updates without physical access
 * to the badge, ensuring users can receive bug fixes and new features.
 * 
 * @author Badge Development Team
 * @date 2024
 */

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

/**
 * @brief Initialize the Over-The-Air update system
 * 
 * Initializes the OTA update system including WiFi connectivity for
 * firmware downloads, secure update verification, and integration with
 * the notification system for update progress and status events.
 * 
 * @param this Pointer to OtaUpdate instance to initialize
 * @param pWifiClient WiFi client for firmware download connectivity
 * @param pNotificationDispatcher Notification system for OTA update events
 * @return ESP_OK on success, error code on failure
 */
esp_err_t OtaUpdate_Init(OtaUpdate *this, WifiClient *pWifiClient, NotificationDispatcher * pNotificationDispatcher);

#endif // OTA_UPDATE_TASK_H