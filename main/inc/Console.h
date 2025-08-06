/**
 * @file Console.h
 * @brief Interactive console interface for debugging and configuration
 * 
 * This module provides a command-line interface for badge debugging and
 * configuration including:
 * - Interactive command processing
 * - Command history storage and retrieval
 * - FATFS integration for persistent history
 * - Runtime configuration and status queries
 * - Development and debugging utilities
 * 
 * The console system enables developers to interact with the badge firmware
 * during development and provides users with advanced configuration options.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef CONSOLE_TASK_H
#define CONSOLE_TASK_H

#include "esp_err.h"
#include "DiskDefines.h"

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY
#define HISTORY_PATH MOUNT_PATH "/history.txt"
#endif // CONFIG_CONSOLE_STORE_HISTORY

/**
 * @brief Initialize the interactive console system
 * 
 * Initializes the command-line interface including command registration,
 * history management, FATFS integration for persistent history storage,
 * and UART communication for interactive debugging and configuration.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t Console_Init(void);

#endif // CONSOLE_TASK_H