
#ifndef SONG_H_
#define SONG_H_

#include <stdint.h>
#include "Notes.h"

#define SONG_MAX_NAME_LENGTH (32)
#define SONG_MAX_NOTES       (256)


typedef float NoteType;
#define NOTE_TYPE_WHOLE            (1.0)
#define NOTE_TYPE_HALF             (1.0/2.0)
#define NOTE_TYPE_QUARTER          (1.0/4.0)
#define NOTE_TYPE_EIGHTH           (1.0/8.0)
#define NOTE_TYPE_SIXTEENTH        (1.0/16.0)
#define NOTE_TYPE_THIRTY_SECOND    (1.0/32.0)
#define NOTE_TYPE_SIXTY_FOURTH     (1.0/64.0)
#define NOTE_TYPE_HALF_DOT         (NOTE_TYPE_HALF+NOTE_TYPE_QUARTER)
#define NOTE_TYPE_HALF_DOT_DOT     (NOTE_TYPE_HALF_DOT+NOTE_TYPE_EIGHTH)
#define NOTE_TYPE_QUARTER_DOT      (NOTE_TYPE_QUARTER+NOTE_TYPE_EIGHTH)
#define NOTE_TYPE_QUARTER_DOT_DOT  (NOTE_TYPE_QUARTER_DOT+NOTE_TYPE_SIXTEENTH)
#define NOTE_TYPE_EIGHTH_DOT       (NOTE_TYPE_EIGHTH+NOTE_TYPE_SIXTEENTH)
#define NOTE_TYPE_QUARTER_TRIPLET  (NOTE_TYPE_QUARTER/3.0)
#define NOTE_TYPE_QUARTER_TRIPLET_DOUBLE  (NOTE_TYPE_QUARTER_TRIPLET * 2.0)

typedef struct Note_t
{
    NoteName note;
    NoteType noteType;
    int slur;
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
    SONG_SECRET_SOUND,
    SONG_SUCCESS_SOUND,
    SONG_ZELDA_THEME,
    SONG_ZELDAS_LULLABY,
    SONG_EPONAS_SONG,
    SONG_SARIAS_SONG,
    SONG_SUNS_SONG,
    SONG_SONG_OF_TIME,
    SONG_SONG_OF_STORMS,
    SONG_MINUET_OF_FOREST,
    NUM_SONGS
} Song;

const SongNotes * GetSong(Song song);
int GetNoteTypeInMilliseconds(int tempo, float noteType);

#endif // SONG_H_