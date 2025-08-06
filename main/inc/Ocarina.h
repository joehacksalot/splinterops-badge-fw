/**
 * @file Ocarina.h
 * @brief Interactive musical instrument and song recognition system
 * 
 * This module provides ocarina-style musical gameplay functionality including:
 * - Touch-based musical note input system
 * - Song pattern recognition and matching
 * - Unlockable song library management
 * - Musical key mapping to touch sensors
 * - Circular buffer for note sequence tracking
 * - Integration with badge audio synthesis
 * - Achievement system for discovered songs
 * 
 * The ocarina system transforms the badge into an interactive musical instrument,
 * allowing users to play melodies and unlock new songs through pattern recognition.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef OCARINA_H_
#define OCARINA_H_

#include "freertos/semphr.h"
#include "esp_err.h"

#include "CircularBuffer.h"
#include "NotificationDispatcher.h"
#include "Song.h"
#include "UserSettings.h"

#define OCARINA_SONG_MAX_NAME_LENGTH 32
#define OCARINA_MAX_SONG_KEYS 8
#define OCARINA_NUM_SONGS 12

typedef enum OcarinaKey_t
{
  OCARINA_KEY_L = 0,  // D3
  UNUSED_KEY_E3,
  OCARINA_KEY_R,      // F3
  UNUSED_KEY_G3,
  OCARINA_KEY_Y,      // A4
  OCARINA_KEY_X,      // B4
  UNUSED_KEY_C4,
  OCARINA_KEY_A,      // D4
  UNUSED_KEY_E4,
} OcarinaKey;


typedef struct OcarinaKeySet_t {
  const char Name[OCARINA_SONG_MAX_NAME_LENGTH];
  int NumKeys;
  Song Song;
  OcarinaKey Keys[OCARINA_MAX_SONG_KEYS];
} OcarinaKeySet;


typedef struct OcarinaSongStatus_t
{
    bool unlocked;
} OcarinaSongStatus;


typedef struct Ocarina_t
{
    bool initialized;
    bool enabled;
    CircularBuffer ocarinaKeys;
    OcarinaSongStatus songStatus[OCARINA_NUM_SONGS];
    NotificationDispatcher* pNotificationDispatcher;
} Ocarina;


/**
 * @brief Initialize the ocarina musical instrument system
 * 
 * Initializes the interactive ocarina gameplay system including song
 * recognition, key sequence tracking, and musical pattern matching.
 * Sets up the circular buffer for key sequences and integrates with
 * the notification system for song completion events.
 * 
 * @param this Pointer to Ocarina instance to initialize
 * @param pNotificationDispatcher Notification system for ocarina events
 * @return ESP_OK on success, error code on failure
 */
esp_err_t Ocarina_Init(Ocarina* this, NotificationDispatcher* pNotificationDispatcher);

/**
 * @brief Enable or disable ocarina mode
 * 
 * Controls whether the ocarina system is active and processing touch
 * inputs for musical gameplay. When disabled, touch inputs will not
 * be interpreted as musical notes.
 * 
 * @param this Pointer to Ocarina instance
 * @param enabled True to enable ocarina mode, false to disable
 * @return ESP_OK on success, error code on failure
 */
esp_err_t Ocarina_SetModeEnabled(Ocarina *this, bool enabled);

#endif // OCARINA_H_