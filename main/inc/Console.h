#ifndef CONSOLE_TASK_H
#define CONSOLE_TASK_H

#include "esp_err.h"
#include "DiscUtils.h"

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY
#define HISTORY_PATH MOUNT_PATH "/history.txt"
#endif // CONFIG_CONSOLE_STORE_HISTORY

esp_err_t Console_Init(void);

#endif // CONSOLE_TASK_H