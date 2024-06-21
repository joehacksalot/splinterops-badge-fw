
#ifndef SONG_H_
#define SONG_H_

#include <stdint.h>
#include "Notes.h"

#define SONG_MAX_NAME_LENGTH (32)
#define SONG_MAX_NOTES       (256)


typedef enum NoteType_t
{
    NOTE_TYPE_WHOLE = 1,
    NOTE_TYPE_HALF = 2,
    NOTE_TYPE_HALF_DOT = 3,
    NOTE_TYPE_QUARTER = 4,
    NOTE_TYPE_QUARTER_DOT = 6,
    NOTE_TYPE_EIGHTH = 8,
    NOTE_TYPE_SIXTEENTH = 16,
    NOTE_TYPE_THIRTY_SECOND = 32,
    NOTE_TYPE_SIXTY_FOURTH = 64,
} NoteType;

typedef struct Note_t
{
    NoteName note;
    NoteType duration; 
} Note;

typedef struct SongNotes_t
{
    const char songName[SONG_MAX_NAME_LENGTH];
    int tempo;
    int numNotes;
    Note notes[SONG_MAX_NOTES];
} SongNotes;

typedef enum Song_e
{
    SONG_NONE = -1,
    SONG_EPONAS_SONG,
    SONG_SONG_OF_STORMS,
    NUM_SONGS
} Song;

SongNotes EponasSong;
SongNotes SongOfStorms;

SongNotes * GetSong(Song song);
int GetNoteTypeInMilliseconds(int tempo, int noteType);

#endif // SONG_H_