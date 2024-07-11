
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "console_badge.h"
// #include "batt_sense_task.h"
// #include "touch_actions_task.h"
// #include "TouchSensor.h"
// #include "system_State.h"
// #include "LedControl.h"
// #include "LedSequences.h"
// #include "ble_config.h"

// int ble_cmd_func(int argc, char **argv)
// {
//     if (argc == 2)
//     {
//         if (strcmp(argv[1],"on") == 0)
//         {
//             // SetSystemState(SYSTEM_STATE_BLE_ENABLE_INDICATOR); //TODO
//             ble_enable();
//         }
//         else if (strcmp(argv[1],"off") == 0)
//         {
//             // SetSystemState(SYSTEM_STATE_USER_JSON); //TODO
//             ble_disable();
//         }
//         else
//         {
//             printf("invalid syntax\n");
//         }
//     }
//     printf("BLE Enabled = %d\n", ble_enabled());
//     return 0;
// }

// int led_cmd_func(int argc, char **argv)
// {
//     if (((argc == 2) && (strcmp(argv[1],"up") == 0)))
//     {
//         CycleSelectedLedSequence(true);
//     }
//     else if ((argc == 2) && (strcmp(argv[1],"down") == 0))
//     {
//         CycleSelectedLedSequence(false);
//     }

//     printf("Led user sequence = %d\n", GetCurrentLedSequenceIndex());
//     return 0;
// }

// int ss_cmd_func(int argc, char **argv)
// {
//     if (argc == 2)
//     {
//         if (strcmp(argv[1],"user") == 0)
//         {
//             SetSystemState(SYSTEM_STATE_USER_JSON);
//         }
//         else if (strcmp(argv[1],"batt") == 0)
//         {
//             SetSystemState(SYSTEM_STATE_SHOW_BATTERY_LEVEL);
//         }
//         else if (strcmp(argv[1],"ble_en") == 0)
//         {
//             SetSystemState(SYSTEM_STATE_BLE_ENABLE_INDICATOR);
//         }
//         else if (strcmp(argv[1],"ble_xfer") == 0)
//         {
//             SetSystemState(SYSTEM_STATE_BLE_XFER_INDICATOR);
//         }
//         else if (strcmp(argv[1],"error") == 0)
//         {
//             SetSystemState(SYSTEM_STATE_ERROR_INDICATOR);
//         }
//         else if (strcmp(argv[1],"success") == 0)
//         {
//             SetSystemState(SYSTEM_STATE_SUCCESS_INDICATOR);
//         }
//         else
//         {
//             printf("invalid syntax\n");
//         }
//     }
//     printf("System State = %d\n", GetSystemState());
//     return 0;
// }

// int batt_cmd_func(int argc, char **argv)
// {
//     printf("battery percent:%.1f\r\n", GetBatteryPercent());
//     printf("battery voltage:%.1f\r\n", get_battery_voltage());
//     return 0;
// }

// int touch_func(int argc, char **argv)
// {
//     if ((argc == 3) && (strcmp(argv[1],"verbose") == 0 ))
//     {
//         if (strcmp(argv[2],"on") == 0 )
//         {
//             SetTouchSensorVerbose(true);
//         }
//         else if (strcmp(argv[2],"off") == 0 )
//         {
//             SetTouchSensorVerbose(false);
//         }
//     }
//     else
//     {
//         for (int i = 0; i < TOUCH_SENSOR_NUM_BUTTONS; i++)
//         {
//             printf("Touch %d: %d\n", i, GetTouchSensorActive(i));
//         }
//     }
//     return 0;
// }

void register_badge_commands(void)
{
    // const esp_console_cmd_t batt_cmd = 
    // {
    //     .command = "batt",
    //     .help = "Prints the current battery status",
    //     .hint = NULL,
    //     .func = &batt_cmd_func,
    // };
    // ESP_ERROR_CHECK(esp_console_cmd_register(&batt_cmd));

    // const esp_console_cmd_t touch_cmd = 
    // {
    //     .command = "touch",
    //     .help = "Prints touch active array or controls spamming print verbose. touch [verbose on|off]",
    //     .hint = NULL,
    //     .func = &touch_func,
    // };
    // ESP_ERROR_CHECK(esp_console_cmd_register(&touch_cmd));

    // const esp_console_cmd_t ss_cmd = 
    // {
    //     .command = "ss",
    //     .help = "Sets current system state",
    //     .hint = NULL,
    //     .func = &ss_cmd_func,
    // };
    // ESP_ERROR_CHECK(esp_console_cmd_register(&ss_cmd));

    // const esp_console_cmd_t ble_cmd = 
    // {
    //     .command = "ble",
    //     .help = "Sets ble enable",
    //     .hint = NULL,
    //     .func = &ble_cmd_func,
    // };
    // ESP_ERROR_CHECK(esp_console_cmd_register(&ble_cmd));
}