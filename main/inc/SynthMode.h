/**
 * @file SynthMode.h
 * @brief Audio synthesis and musical playback system
 * 
 * This module provides comprehensive audio synthesis functionality including:
 * - Touch-based musical note generation
 * - Song playback with tempo and octave control
 * - Real-time audio synthesis using PWM audio
 * - Song queue management for sequential playback
 * - Octave shifting for pitch control
 * - Thread-safe audio operations
 * - Integration with touch sensors for interactive music
 * - User settings integration for audio preferences
 * 
 * The synth mode transforms the badge into a musical instrument,
 * enabling users to play melodies and experience interactive audio.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef SYNTH_MODE_H_
#define SYNTH_MODE_H_

#include "freertos/semphr.h"
#include "esp_err.h"

#include "CircularBuffer.h"
#include "LedControl.h"
#include "NotificationDispatcher.h"
#include "Song.h"
#include "SynthModeNotifications.h"
#include "UserSettings.h"
#include "Utilities.h"
#include "TouchSensor.h"

typedef struct SynthMode_t
{
    bool initialized;
    bool touchSoundEnabled;
    int octaveShift;
    Song selectedSong;
    int currentNoteIdx;
    CircularBuffer songQueue;
    SemaphoreHandle_t queueMutex;
    SemaphoreHandle_t toneMutex;
    TickType_t nextNotePlayTime;
    NotificationDispatcher* pNotificationDispatcher;
    UserSettings* pUserSettings;
} SynthMode;

/**
 * @brief Initialize the audio synthesis system
 * 
 * Initializes the synthesis mode for musical playback and touch-based
 * audio generation. Sets up audio hardware, configures tone generation,
 * initializes song queues, and integrates with the notification system.
 * 
 * @param this Pointer to SynthMode instance to initialize
 * @param pNotificationDispatcher Notification system for audio events
 * @param userSettings User settings for audio preferences
 * @return ESP_OK on success, error code on failure
 */
esp_err_t SynthMode_Init(SynthMode* this, NotificationDispatcher* pNotificationDispatcher, UserSettings* userSettings);

/**
 * @brief Enable or disable touch-based sound generation
 * 
 * Controls whether touch sensor interactions generate audio tones.
 * Allows configuration of octave shifting for different musical ranges.
 * 
 * @param this Pointer to SynthMode instance
 * @param enabled True to enable touch sounds, false to disable
 * @param octaveShift Octave shift value for tone generation
 * @return ESP_OK on success, error code on failure
 */
esp_err_t SynthMode_SetTouchSoundEnabled(SynthMode *this, bool enabled, int octaveShift);

/**
 * @brief Get the current touch sound enabled state
 * 
 * Returns whether touch-based sound generation is currently enabled.
 * 
 * @param this Pointer to SynthMode instance
 * @return True if touch sounds are enabled, false otherwise
 */
bool SynthMode_GetTouchSoundEnabled(SynthMode *this);


#endif // SYNTH_MODE_H_