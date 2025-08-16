#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "console_system.h"
#include "Console.h"
#include "Utilities.h"

static const char *TAG = "console_system";

int get_version(int argc, char **argv)
{
    esp_chip_info_t info;
    esp_chip_info(&info);

    uint32_t size_flash_chip;
    esp_flash_get_size(NULL, &size_flash_chip);

    printf("Build info: %s %s\r\n", __DATE__, __TIME__);
    printf("IDF Version:%s\r\n", esp_get_idf_version());
    printf("Chip info:\r\n");
    printf("\tModel:%s\r\n", info.model == CHIP_ESP32 ? "ESP32" : "Unknown");
    printf("\tCores:%d\r\n", info.cores);
    printf("\tFeature:%s%s%s%s%lu%s\r\n",
           info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
           info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
           info.features & CHIP_FEATURE_BT ? "/BT" : "",
           info.features & CHIP_FEATURE_EMB_FLASH ? "/Embedded-Flash:" : "/External-Flash:",
           size_flash_chip / (1024 * 1024), " MB");
    printf("\tRevision number:%d\r\n", info.revision);
    return 0;
}

static int get_free_mem(int argc, char **argv)
{
    printf("Total available heap: %lu\n", esp_get_free_heap_size());
    printf("Total heap watermark: %lu\n", esp_get_minimum_free_heap_size());
    printf("Available internal heap size: %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("Internal heap watermark: %d\n", heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    printf("Available external heap size: %d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("External heap watermark: %d\n", heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
    return 0;
}

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
static int get_task_info(int argc, char **argv)
{
    char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * sizeof(TaskStatus_t));
    if (task_list_buffer == NULL)
    {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}
#endif // CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS

#define LINE_SIZE 64
#if CONFIG_CONSOLE_STORE_HISTORY
static int get_history(int argc, char **argv)
{
    char line[LINE_SIZE] = {0};
    FILE * pFile = fopen(HISTORY_PATH, "r");
    if(pFile != NULL)
    {
        while(fgets(line,LINE_SIZE,pFile) != NULL)
        {
            printf("%s",line);
        }
        printf("\n");
        fclose(pFile);
    }
    else
    {
        printf("Failed to open history file\n");
    }

    return 0;
}
#endif

static int cat_file(int argc, char **argv)
{
    if (argc == 2)
    {
        FILE * pFile = fopen(argv[1], "rb");
        if(pFile)
        {
            char line[LINE_SIZE] = {0};
            while(fgets(line,LINE_SIZE,pFile) != NULL)
            {
                printf("%s",line);
            }
            printf("\n");

            fclose(pFile);
        }
        else
        {
            ESP_LOGI(TAG, "Unable to open %s", argv[1]);
        }
    }
    else if((argc == 3) && (strcmp(argv[2],"hex") == 0))
    {
        FILE * pFile = fopen(argv[1], "rb");
        if(pFile)
        {
            char line[LINE_SIZE] = {0};
            unsigned int bytesRead = fread(line, 1, LINE_SIZE, pFile);
            do
            {
                printf("Data(%d): ", bytesRead);
                for(unsigned int i = 0; i < bytesRead; ++i)
                {
                    printf("%02X", line[i]);
                }
            } while((bytesRead = fread(line, 1, LINE_SIZE, pFile)) == LINE_SIZE);
            printf("\n");

            fclose(pFile);
        }
        else
        {
            ESP_LOGI(TAG, "Unable to open %s", argv[1]);
        }
    }

    return 0;
}

void register_system_basic(void)
{
    const esp_console_cmd_t version_cmd = 
    {
        .command = "version",
        .help = "Print chip version and feature information",
        .hint = NULL,
        .func = &get_version,
    };

#if CONFIG_CONSOLE_STORE_HISTORY
    const esp_console_cmd_t history_cmd = 
    {
        .command = "history",
        .help = "Print console history",
        .hint = NULL,
        .func = &get_history,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&history_cmd));
#endif

    ESP_ERROR_CHECK(esp_console_cmd_register(&version_cmd));
}

void register_system_dev(void)
{
    const esp_console_cmd_t free_cmd = 
    {
        .command = "free",
        .help = "Prints current size of free heap",
        .hint = NULL,
        .func = &get_free_mem,
    };

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
    const esp_console_cmd_t task_info_cmd = 
    {
        .command = "task_info",
        .help = "Prints information about running tasks",
        .hint = NULL,
        .func = &get_task_info,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&task_info_cmd));
#endif

    const esp_console_cmd_t cat_file_cmd =
    {
        .command = "cat",
        .help = "Attempts to read the file in the filesystem",
        .hint = NULL,
        .func = &cat_file,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&free_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cat_file_cmd));
}