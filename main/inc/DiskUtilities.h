
/**
 * @file DiskUtilities.h
 * @brief File system initialization and disk I/O utilities
 * 
 * This module provides essential file system operations including:
 * - FATFS file system initialization and mounting
 * - NVS (Non-Volatile Storage) initialization
 * - Generic file read operations with size validation
 * - Battery-aware file write operations
 * - Error handling for disk operations
 * - File system health monitoring
 * 
 * The disk utilities system provides a reliable foundation for persistent
 * data storage across badge power cycles and system resets.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include "BatterySensor.h"
#include "esp_err.h"

/**
 * @brief Initialize the file system
 * 
 * Initializes and mounts the FATFS file system for persistent storage.
 * Sets up the storage partition and prepares the file system for
 * read/write operations throughout the badge system.
 * 
 * @return ESP_OK on success, error code on initialization failure
 */
esp_err_t DiskUtilities_InitFs(void);

/**
 * @brief Initialize Non-Volatile Storage (NVS)
 * 
 * Initializes the NVS partition for storing configuration data,
 * user settings, and other persistent key-value pairs that survive
 * power cycles and firmware updates.
 * 
 * @return ESP_OK on success, error code on initialization failure
 */
esp_err_t DiskUtilities_InitNvs(void);

/**
 * @brief Read file from disk with validation
 * 
 * Reads a file from the file system with size validation and error handling.
 * Provides the actual bytes read for verification against expected file size.
 * 
 * @param filename Name of the file to read
 * @param buffer Output buffer to store file contents
 * @param bufferSize Size of the output buffer
 * @param pReadBytes Pointer to store actual bytes read
 * @param expectedFileSize Expected file size for validation
 * @return ESP_OK on success, error code on read failure or size mismatch
 */
extern esp_err_t ReadFileFromDisk(char *filename, char *buffer, int bufferSize, int * pReadBytes, int expectedFileSize);

/**
 * @brief Write file to disk with battery check
 * 
 * Writes data to a file on the file system with battery level validation.
 * Prevents file corruption by checking battery level before write operations.
 * 
 * @param pBatterySensor Battery sensor for power level validation
 * @param filename Name of the file to write
 * @param buffer Data buffer to write to file
 * @param bufferSize Size of data to write
 * @return ESP_OK on success, error code on write failure or low battery
 */
extern esp_err_t WriteFileToDisk(BatterySensor * pBatterySensor, char *filename, char *buffer, int bufferSize);

#endif // FILESYSTEM_H_
