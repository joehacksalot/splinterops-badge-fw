/**
 * @file NotificationDispatcher.h
 * @brief Event-driven notification system for inter-component communication
 * 
 * This module provides the core event system that enables loose coupling between
 * badge components. It implements a publish-subscribe pattern using ESP-IDF's
 * event system to facilitate communication between:
 * - Touch sensor events
 * - BLE connection and data events
 * - Game state changes
 * - WiFi connectivity events
 * - OTA update notifications
 * - Audio/song playback events
 * - Interactive game actions
 * 
 * The notification dispatcher serves as the central nervous system of the badge,
 * allowing components to communicate without direct dependencies.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef NOTIFICATION_DISPATCHER_H_
#define NOTIFICATION_DISPATCHER_H_

#include "esp_event.h"

#define DEFAULT_NOTIFY_WAIT_DURATION 100 //ms

typedef enum NotificationEvents_e
{
    NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION,
    NOTIFICATION_EVENTS_TOUCH_ACTION_CMD,
    NOTIFICATION_EVENTS_TOUCH_ENABLED,
    NOTIFICATION_EVENTS_TOUCH_DISABLED,
    NOTIFICATION_EVENTS_BLE_SERVICE_ENABLED,
    NOTIFICATION_EVENTS_BLE_SERVICE_DISABLED,
    NOTIFICATION_EVENTS_BLE_SERVICE_CONNECTED,
    NOTIFICATION_EVENTS_BLE_DROPPED,
    NOTIFICATION_EVENTS_BLE_SERVICE_DISCONNECTED,
    NOTIFICATION_EVENTS_BLE_FILE_SERVICE_PERCENT_CHANGED,
    NOTIFICATION_EVENTS_BLE_FILE_COMPLETE,
    NOTIFICATION_EVENTS_BLE_FILE_FAILED,
    NOTIFICATION_EVENTS_BLE_FILE_SETTINGS_RECVD,
    NOTIFICATION_EVENTS_BLE_FILE_LEDJSON_RECVD,
    NOTIFICATION_EVENTS_BLE_NEW_PAIR_RECV,
    NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED, // TODO: Ethan to send this, Jose to catch/process this
    NOTIFICATION_EVENTS_GAME_EVENT_JOINED,
    NOTIFICATION_EVENTS_GAME_EVENT_ENDED,
    NOTIFICATION_EVENTS_FIRST_TIME_POWER_ON,
    NOTIFICATION_EVENTS_WIFI_HEARTBEAT_READY_TO_SEND, // TODO: Jose to send this, Shaun to catch/process this
    NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV, // TODO: Shaun to send this. Jose to catch/process this
    NOTIFICATION_EVENTS_SEND_HEARTBEAT,
    NOTIFICATION_EVENTS_WIFI_ENABLED,
    NOTIFICATION_EVENTS_WIFI_DISABLED,
    NOTIFICATION_EVENTS_WIFI_CONNECTED,
    NOTIFICATION_EVENTS_WIFI_DISCONNECTED,
    NOTIFICATION_EVENTS_OTA_REQUIRED,
    NOTIFICATION_EVENTS_OTA_DOWNLOAD_INITIATED,
    NOTIFICATION_EVENTS_OTA_DOWNLOAD_COMPLETE,
    NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE,
    NOTIFICATION_EVENTS_PLAY_SONG,
    NOTIFICATION_EVENTS_SONG_NOTE_ACTION,
    NOTIFICATION_EVENTS_OCARINA_SONG_MATCHED,
    NOTIFICATION_EVENTS_INTERACTIVE_GAME_STATE_CHANGE,
    NOTIFICATION_EVENTS_INTERACTIVE_GAME_ACTION
} NotificationEvent;

typedef struct NotificationDispatcher_t
{
    esp_event_loop_handle_t eventLoopHandle;
    esp_event_base_t notificationEventBase;
    SemaphoreHandle_t notifyMutex;
} NotificationDispatcher;

/**
 * @brief Initialize the notification dispatcher system
 * 
 * Initializes the ESP-IDF event loop system for inter-component communication.
 * Sets up the event base, creates mutexes for thread-safe operations, and
 * prepares the system for event registration and dispatching.
 * 
 * @param this Pointer to NotificationDispatcher instance to initialize
 * @return ESP_OK on success, error code on failure
 */
esp_err_t NotificationDispatcher_Init(NotificationDispatcher *this);

/**
 * @brief Dispatch a notification event to registered handlers
 * 
 * Sends an event notification to all registered handlers for the specified
 * event type. Supports optional data payload and configurable wait timeout
 * for event processing completion.
 * 
 * @param this Pointer to NotificationDispatcher instance
 * @param notificationEvent Type of event to dispatch
 * @param data Optional data payload to send with the event
 * @param dataSize Size of the data payload in bytes
 * @param waitDurationMSec Maximum time to wait for event processing (ms)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t NotificationDispatcher_NotifyEvent(NotificationDispatcher *this, NotificationEvent notificationEvent, void *data, int dataSize, uint32_t waitDurationMSec);

/**
 * @brief Register an event handler for specific notification events
 * 
 * Registers a callback function to handle specific types of notification
 * events. Multiple handlers can be registered for the same event type,
 * and they will all be called when the event is dispatched.
 * 
 * @param this Pointer to NotificationDispatcher instance
 * @param notificationEvent Type of event to handle
 * @param eventHandler Callback function to handle the event
 * @param eventHandlerArgs Optional arguments to pass to the handler
 * @return ESP_OK on success, error code on failure
 */
esp_err_t NotificationDispatcher_RegisterNotificationEventHandler(NotificationDispatcher *this, NotificationEvent notificationEvent, esp_event_handler_t eventHandler, void *eventHandlerArgs);



#endif // NOTIFICATION_DISPATCHER_H_
