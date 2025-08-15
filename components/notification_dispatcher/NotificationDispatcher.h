
#ifndef NOTIFICATION_DISPATCHER_H_
#define NOTIFICATION_DISPATCHER_H_

#include "esp_event.h"

#define DEFAULT_NOTIFY_WAIT_DURATION 100 //ms

typedef struct NotificationDispatcher_t
{
    esp_event_loop_handle_t eventLoopHandle;
    esp_event_base_t notificationEventBase;
    SemaphoreHandle_t notifyMutex;
} NotificationDispatcher;

esp_err_t NotificationDispatcher_Init(NotificationDispatcher *this, esp_event_loop_args_t *touchTaskArgs);
esp_err_t NotificationDispatcher_NotifyEvent(NotificationDispatcher *this, int notificationEvent, void *data, int dataSize, uint32_t waitDurationMSec);
esp_err_t NotificationDispatcher_RegisterNotificationEventHandler(NotificationDispatcher *this, int notificationEvent, esp_event_handler_t eventHandler, void *eventHandlerArgs);



#endif // NOTIFICATION_DISPATCHER_H_
