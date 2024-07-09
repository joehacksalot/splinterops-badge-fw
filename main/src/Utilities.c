#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>

// Define the struct to hold task information
typedef struct {
    char taskName[configMAX_TASK_NAME_LEN];
    UBaseType_t coreID;
} TaskInfo_t;

// Define a global array to hold the task information
#define MAX_TASKS 30
TaskInfo_t taskInfoArray[MAX_TASKS];
int taskCount = 0;

void registerCurrentTaskInfo(void) {
    // Get the current task handle
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    
    // Get the current task name
    const char *taskName = pcTaskGetName(currentTask);
    
    // Get the current core ID
    UBaseType_t coreID = xPortGetCoreID();

    // Register the task information
    if (taskCount < MAX_TASKS) {
        strncpy(taskInfoArray[taskCount].taskName, taskName, configMAX_TASK_NAME_LEN - 1);
        taskInfoArray[taskCount].taskName[configMAX_TASK_NAME_LEN - 1] = '\0';  // Ensure null-terminated string
        taskInfoArray[taskCount].coreID = coreID;
        taskCount++;
    } else {
        ESP_LOGW("TaskInfo", "Task info array is full. Cannot register more tasks.");
    }
}

void displayTaskInfoArray(void) {
    ESP_LOGI("TaskInfo", "Displaying registered task information:");
    for (int i = 0; i < taskCount; i++) {
        ESP_LOGI("TaskInfo", "Task name: %s, Core: %d", taskInfoArray[i].taskName, taskInfoArray[i].coreID);
    }
}
