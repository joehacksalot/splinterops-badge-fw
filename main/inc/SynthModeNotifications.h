
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
