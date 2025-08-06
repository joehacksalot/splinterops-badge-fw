/**
  * @file songs/EponasSong.c
  * @brief Note sequence asset for "Epona's Song".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `EponasSong` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes EponasSong = {
  "Epona's Song",
  100,  // Tempo
  42,   // Number of Notes
  {
    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 1},
    {NOTE_A3, NOTE_TYPE_SIXTEENTH, 0},

    // --

    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 1},
    {NOTE_A3, NOTE_TYPE_SIXTEENTH, 0},

    // --

    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_QUARTER, 1},

    // --

    {NOTE_B3, NOTE_TYPE_QUARTER, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 1},
    {NOTE_A3, NOTE_TYPE_SIXTEENTH, 0},

    // --

    {NOTE_REST, NOTE_TYPE_QUARTER, 0},
    {NOTE_FS3, NOTE_TYPE_QUARTER, 0},
    {NOTE_F3, NOTE_TYPE_QUARTER, 0},

    // --
    {NOTE_FS3, NOTE_TYPE_QUARTER, 0},
    {NOTE_CS4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_HALF, 1},
    {NOTE_B3, NOTE_TYPE_SIXTEENTH, 0},

    // --

    {NOTE_D4, NOTE_TYPE_HALF, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},
    {NOTE_CS4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},

    // --

    {NOTE_A3, NOTE_TYPE_HALF, 1},
    {NOTE_A3, NOTE_TYPE_SIXTEENTH, 0},

    // --

    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 1},
    {NOTE_A3, NOTE_TYPE_SIXTEENTH, 0},

    // +++

    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 1},
    {NOTE_A3, NOTE_TYPE_SIXTEENTH, 0},

    // --

    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_QUARTER, 1},

    // --

    {NOTE_B3, NOTE_TYPE_QUARTER, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 1},
    {NOTE_A3, NOTE_TYPE_SIXTEENTH, 0},
  }
};
