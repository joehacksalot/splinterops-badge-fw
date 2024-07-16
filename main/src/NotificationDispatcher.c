
#include <stdint.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "NotificationDispatcher.h"
#include "SystemState.h"
#include "TaskPriorities.h"

// Size of notification queue before dropping messages
#define NOTIFICATION_QUEUE_SIZE (100)
#define MUTEX_WAIT_DURATION_MS  (100)

// Internal Constants
static const char * TAG = "ND";

esp_err_t NotificationDispatcher_Init(NotificationDispatcher *this)
{
    static bool initialized = false;
    esp_err_t ret = false;

    assert(this);

    if (initialized == false)
    {
        initialized = true;
        memset(this, 0, sizeof(*this));
        this->notificationEventBase = "NotificationEventBase";

        // Create the event loop
        esp_event_loop_args_t touchTaskArgs =
        {
            .queue_size = NOTIFICATION_QUEUE_SIZE,
            .task_name = "NotificationsEventLoop",
            .task_priority = NOTIFICATIONS_TASK_PRIORITY,
            .task_stack_size = configMINIMAL_STACK_SIZE * 3,
            .task_core_id = APP_CPU_NUM
        };
        ESP_ERROR_CHECK(esp_event_loop_create(&touchTaskArgs, &this->eventLoopHandle));

        // Create mutex for notifies
        this->notifyMutex = xSemaphoreCreateMutex();
        assert(this->notifyMutex);

        ret = ESP_OK;
    }
    return ret;
}

esp_err_t NotificationDispatcher_NotifyEvent(NotificationDispatcher *this, NotificationEvent notificationEvent, void *data, int dataSize, uint32_t waitDurationMSec)
{
    assert(this);
    assert(this->eventLoopHandle);
    assert(this->notificationEventBase);
    assert(this->notifyMutex);

    esp_err_t ret = ESP_FAIL;

    if(xSemaphoreTake(this->notifyMutex, pdMS_TO_TICKS(MUTEX_WAIT_DURATION_MS)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
    }
    else
    {
        ret = esp_event_post_to(this->eventLoopHandle,
                                this->notificationEventBase,
                                notificationEvent,
                                data,
                                dataSize,
                                pdMS_TO_TICKS(waitDurationMSec));       // Time to wait for full queue before timing out
        switch(ret)
        {
            case ESP_OK:
                ESP_LOGD(TAG, "Notification (%d) Posted", notificationEvent);
                break;
            case ESP_ERR_TIMEOUT:
                ESP_LOGE(TAG, "Notification Queue Full");
                break;
            case ESP_ERR_INVALID_ARG:
                ESP_LOGE(TAG, "Invalid base or event");
                break;
        }

        if(xSemaphoreGive(this->notifyMutex) != pdTRUE)
        {
            ESP_LOGE(TAG, "Failed to give mutex");
        }
    }

    return ret;
}

esp_err_t NotificationDispatcher_RegisterNotificationEventHandler(NotificationDispatcher *this, NotificationEvent notificationEvent, esp_event_handler_t eventHandler, void *eventHandlerArgs)
{
    assert(this);
    return esp_event_handler_instance_register_with(this->eventLoopHandle,
                                                    this->notificationEventBase,
                                                    notificationEvent,
                                                    eventHandler,
                                                    eventHandlerArgs,
                                                    NULL);
}
