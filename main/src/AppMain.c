/**
 * @file AppMain.c
 * @brief Main application entry point for the badge firmware
 * 
 * This file contains the primary entry point for the ESP-IDF application.
 * It initializes the central SystemState and starts all badge subsystems
 * including BLE, LED control, touch sensors, audio synthesis, and networking.
 * 
 * The application follows an event-driven architecture where the SystemState
 * coordinates all component interactions through the notification dispatcher.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#include "SystemState.h"

void app_main()
{
   SystemState *systemState = SystemState_GetInstance();
   SystemState_Init(systemState);
}
