/**
 * @file SynthModeNotifications.h
 * @brief Audio synthesis event notification data structures and types
 * 
 * This header defines the notification data structures for audio synthesis events:
 * - Song playback event notifications with song data
 * - Note change event notifications for real-time audio feedback
 * - Song state transitions (start, stop, tone changes)
 * - Integration with the notification dispatcher system
 * - Event-driven audio system coordination
 * 
 * These notification types enable decoupled communication between the audio
 * synthesis system and other badge components for synchronized audio-visual effects.
 * 
 * @author Badge Development Team
 * @date 2024
 */

#ifndef SYNTH_MODE_NOTIFICATIONS_H_
#define SYNTH_MODE_NOTIFICATIONS_H_

#include "Notes.h"
#include "Song.h"

typedef struct PlaySongEventNotificationData_t
{
    Song song;
} PlaySongEventNotificationData;

typedef enum SongNoteChangeType_t
{
    SONG_NOTE_CHANGE_TYPE_SONG_START,
    SONG_NOTE_CHANGE_TYPE_TONE_START,
    SONG_NOTE_CHANGE_TYPE_TONE_STOP,
    SONG_NOTE_CHANGE_TYPE_SONG_STOP
} SongNoteChangeType;

typedef struct SongNoteChangeEventNotificationData_t
{
    Song song;
    SongNoteChangeType action;
    NoteName note;
} SongNoteChangeEventNotificationData;

#endif // SYNTH_MODE_NOTIFICATIONS_H_
