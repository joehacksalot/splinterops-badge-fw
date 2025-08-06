/**
 * @file LedSequences.h
 * @brief LED sequence management and custom pattern storage system
 * 
 * This module provides comprehensive LED sequence management including:
 * - Predefined status sequences (BLE enable, transfer, error, success)
 * - Custom LED sequence storage and retrieval
 * - Share code generation for sequence sharing
 * - JSON-based sequence configuration
 * - Battery-aware sequence optimization
 * - Dynamic sequence loading and updating
 * - Memory management for large custom sequences
 * 
 * The LED sequences system enables rich visual feedback and customizable
 * light patterns for enhanced badge user experience.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef LED_SEQUENCES_H_
#define LED_SEQUENCES_H_

#include "esp_check.h"
#include "BatterySensor.h"

typedef enum LedStatusSequence_e
{
  LED_STATUS_SEQUENCE_BLE_ENABLE = 0,
  LED_STATUS_SEQUENCE_BLE_XFER,
  LED_STATUS_SEQUENCE_ERROR,
  LED_STATUS_SEQUENCE_SUCCESS,

  NUM_LED_STATUS_SEQUENCES,
} LedStatusSequence;

#define NUM_SHARECODE_BYTES 7

esp_err_t LedSequences_Init(BatterySensor *pBatterySensor);
/**
 * @brief Get the total number of LED sequences
 * 
 * Returns the total count of all LED sequences including built-in
 * and custom sequences available in the system.
 * 
 * @return Total number of LED sequences
 */
int LedSequences_GetNumLedSequences(void);

/**
 * @brief Get the offset index for custom LED sequences
 * 
 * Returns the starting index offset where custom LED sequences begin
 * in the sequence array, separating them from built-in sequences.
 * 
 * @return Index offset for custom sequences
 */
int LedSequences_GetCustomLedSequencesOffset(void);

/**
 * @brief Get the number of custom LED sequences
 * 
 * Returns the count of user-created custom LED sequences that can
 * be modified, shared, and synchronized between badges.
 * 
 * @return Number of custom LED sequences
 */
int LedSequences_GetNumCustomLedSequences(void);

/**
 * @brief Get the number of status LED sequences
 * 
 * Returns the count of system status LED sequences used for
 * indicating badge states, notifications, and system feedback.
 * 
 * @return Number of status LED sequences
 */
int LedSequences_GetNumStatusSequences(void);

/**
 * @brief Get LED sequence JSON data by index
 * 
 * Retrieves the JSON representation of an LED sequence by its index.
 * The JSON contains timing, color, and pattern information for playback.
 * 
 * @param index Index of the LED sequence to retrieve
 * @return Pointer to JSON string, or NULL if index is invalid
 */
const char * LedSequences_GetLedSequenceJson(int index);

/**
 * @brief Update a custom LED sequence
 * 
 * Updates or creates a custom LED sequence with new JSON data.
 * Validates the sequence format and stores it persistently for
 * future use and badge-to-badge sharing.
 * 
 * @param index Index of the custom sequence to update
 * @param sequence JSON string containing the new sequence data
 * @param sequence_size Size of the sequence data in bytes
 * @return ESP_OK on success, error code on validation or storage failure
 */
esp_err_t LedSequences_UpdateCustomLedSequence(int index, const char * const sequence, int sequence_size);

/**
 * @brief Get LED sequence index from custom index
 * 
 * Converts a custom sequence index to the corresponding global
 * LED sequence index for unified sequence management.
 * 
 * @param custom_index Index within the custom sequences array
 * @return Global LED sequence index
 */
int GetLedSeqIndexByCustomIndex(int custom_index);

/**
 * @brief Get shareable code for custom LED sequence
 * 
 * Generates a shareable code or identifier for a custom LED sequence
 * that can be used for badge-to-badge sequence sharing and synchronization.
 * 
 * @param index Index of the custom LED sequence
 * @return Pointer to shareable code string, or NULL if invalid
 */
char * LedSequences_GetCustomLedSequenceSharecode(int index);

#define MAX_CUSTOM_LED_SEQUENCE_SIZE (128*1024)

#endif // LED_SEQUENCES_H_