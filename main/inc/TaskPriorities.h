/**
 * @file TaskPriorities.h
 * @brief FreeRTOS task priority definitions for badge system coordination
 * 
 * This header defines the priority hierarchy for all FreeRTOS tasks in the badge system.
 * Priority values increase with importance (higher numbers = higher priority) following
 * FreeRTOS conventions. The priority assignment ensures proper system responsiveness:
 * 
 * - **Critical Real-time (21-15)**: BLE control, LED control, touch sensors
 * - **System Core (14-10)**: System state, audio synthesis, network communication
 * - **Application Logic (9-6)**: Game state, notifications, console
 * - **Background Services (5-1)**: User settings, statistics, battery monitoring
 * 
 * This priority structure maintains responsive user interaction while ensuring
 * background services operate without interfering with real-time operations.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef TASK_PRIORITIES_H_
#define TASK_PRIORITIES_H_

// FreeRTOS priority increases with larger integers
#define BLE_CONTROL_TASK_PRIORITY           21
#define LED_CONTROL_TASK_PRIORITY           15
#define TOUCH_SENSOR_TASK_PRIORITY          14
#define SYSTEM_STATE_TASK_PRIORITY          13
#define SYNTH_MODE_TASK_PRIORITY            12
#define HTTP_GAME_CLIENT_TASK_PRIORITY      11
#define WIFI_CONTROL_TASK_PRIORITY          10
#define GAME_STATE_TASK_PRIORITY            9
#define CAPT_DNS_TASK_PRIORITY              8
#define NOTIFICATIONS_TASK_PRIORITY         7
#define BLE_DISABLE_TASK_PRIORITY           6
#define CONSOLE_TASK_PRIORITY               5
#define USER_SETTINGS_TASK_PRIORITY         4
#define BADGE_STAT_TASK_PRIORITY            3
#define OTA_UPDATE_TASK_PRIORITY            2
#define BATT_SENSE_TASK_PRIORITY            1

#endif // TASK_PRIORITIES_H_