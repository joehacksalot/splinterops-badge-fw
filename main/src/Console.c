/* Console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_fat.h"

#include "DiskUtilities.h"
#include "console_system.h"
#include "Console.h"
#include "TaskPriorities.h"
#include "Utilities.h"

#ifdef CONFIG_ESP_CONSOLE_USB_CDC
#error Incompatible with USB CDC console
#endif // CONFIG_ESP_CONSOLE_USB_CDC

#define PROMPT_STR CONFIG_IDF_TARGET

#define CONSOLE_DELAY_MS 10


// Internal Function Declarations
static void ConsoleTask(void *pvParameters);

// Internal Constants
static const char* TAG = "ConsoleTask";

// Prompt to be printed before each line.
// This can be customized, made dynamic, etc.
const char* prompt = LOG_COLOR_I PROMPT_STR "> " LOG_RESET_COLOR;


esp_err_t Console_Init(void)
{
    esp_err_t ret = ESP_OK;

    // Drain stdout before reconfiguring it
    fflush(stdout);
    fsync(fileno(stdout));

    // Disable buffering on stdin
    setvbuf(stdin, NULL, _IONBF, 0);

    // Minicom, screen, idf_monitor send CR when ENTER key is pressed
    uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    // Move the caret to the beginning of the next line on '\n'
    uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    // Configure UART. Note that REF_TICK is used so that the baud rate remains
    // correct while APB frequency is changing in light sleep mode.
    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        .source_clk = UART_SCLK_REF_TICK,
#else
        .source_clk = UART_SCLK_XTAL,
#endif
    };
    // Install UART driver for interrupt-driven reads and writes
    ret = uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed installing UART driver: %s", esp_err_to_name(ret));
    }
    ret = uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed installing UART driver: %s", esp_err_to_name(ret));
    }

    // Tell VFS to use UART driver
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    // Initialize the console
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
            .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ret = esp_console_init(&console_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed installing UART driver: %s", esp_err_to_name(ret));
    }

    // Configure linenoise line completion library
    // Enable multiline editing. If not set, long commands will scroll within
    // single line.
    linenoiseSetMultiLine(1);

    // Tell linenoise where to get command completions and hints
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    // Set command history size
    linenoiseHistorySetMaxLen(100);

    // Set command maximum length
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);

    // Don't return empty lines
    linenoiseAllowEmpty(false);

#if CONFIG_CONSOLE_STORE_HISTORY
    // Load command history from filesystem
    linenoiseHistoryLoad(HISTORY_PATH);
#endif
#if CONFIG_CONSOLE_STORE_HISTORY
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

    // Register commands
    ret = esp_console_register_help_command();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed installing UART driver: %s", esp_err_to_name(ret));
    }
    register_system_basic();

#if CONFIG_DEBUG_FEATURES
    register_system_dev();
#endif

    // register_badge_commands();

    printf("\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n"
           "Press Enter or Ctrl+C will terminate the console environment.\n");

    // Figure out if the terminal supports escape sequences
    int probe_status = linenoiseProbe();
    if (probe_status) 
    { // zero indicates success
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
#if CONFIG_LOG_COLORS
        // Since the terminal doesn't support escape sequences,
        // don't use color codes in the prompt.
        prompt = PROMPT_STR "> ";
#endif //CONFIG_LOG_COLORS
    }
    
    assert(xTaskCreatePinnedToCore(ConsoleTask, "ConsoleTask", configMINIMAL_STACK_SIZE * 2, NULL, CONSOLE_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);
    return ret;
}

static void ConsoleTask(void *pvParameters)
{
    // Main loop 
    while(true) 
    {
        // Get a line using linenoise.
        // The line is returned when ENTER is pressed.
        char* line = linenoise(prompt);
        if (line == NULL) 
        {
            continue;
        }
        // Add the command to the history if not empty
        if (strlen(line) > 0) 
        {
            linenoiseHistoryAdd(line);
#if CONFIG_CONSOLE_STORE_HISTORY
            // Save command history to filesystem
            linenoiseHistorySave(HISTORY_PATH);
#endif
        }

        // Try to run the command
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND)
        {
            printf("Unrecognized command\n");
        }
        else if (err == ESP_ERR_INVALID_ARG) 
        {
            // command was empty
            printf("EMPTY COMMAND\n");
        }
        else if (err == ESP_OK && ret != ESP_OK) 
        {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        }
        else if (err != ESP_OK)
        {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }

        // linenoise allocates line buffer on the heap, so need to free it
        linenoiseFree(line);

        vTaskDelay(pdMS_TO_TICKS(CONSOLE_DELAY_MS));
    }

    ESP_LOGE(TAG, "Error or end-of-input, terminating console");
    esp_err_t ret = esp_console_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed deinitializing console: %s", esp_err_to_name(ret));
    }
}
