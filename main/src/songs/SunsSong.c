/**
  * @file songs/SunsSong.c
  * @brief Note sequence asset for "Sun's Song".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `SunsSong` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes SunsSong = {
  "Sun's Song",
  150,  // Tempo
  16,   // Number of Notes
  {
    {NOTE_A3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},
    {NOTE_REST, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},
    {NOTE_REST, NOTE_TYPE_EIGHTH, 0},

    // --

    {NOTE_A3, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_B3, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_C4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_E4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_G4, NOTE_TYPE_SIXTEENTH, 1},
    {NOTE_G4, NOTE_TYPE_HALF_DOT, 1},

    // --
    
    {NOTE_G4, NOTE_TYPE_HALF_DOT, 0},
  }
};