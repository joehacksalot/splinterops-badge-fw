/**
  * @file songs/SongOfStorms.c
  * @brief Note sequence asset for "Song of Storms".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `SongOfStorms` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes SongOfStorms = {
  "Song of Storms",
  180,  // Tempo
  43,   // Number of Notes
  {
    {NOTE_D3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_HALF, 0},

    {NOTE_D3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_HALF, 0},

    {NOTE_E4, NOTE_TYPE_QUARTER_DOT, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_E4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 0},

    {NOTE_E4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 0},

    // --

    {NOTE_A3, NOTE_TYPE_QUARTER, 0},
    {NOTE_D3, NOTE_TYPE_QUARTER, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_G3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_HALF_DOT, 0},

    {NOTE_A3, NOTE_TYPE_QUARTER, 0},
    {NOTE_D3, NOTE_TYPE_QUARTER, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_G3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_E3, NOTE_TYPE_HALF_DOT, 0},

    // --

    {NOTE_D3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_HALF, 0},
    
    {NOTE_D3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_HALF, 0},

    {NOTE_E4, NOTE_TYPE_QUARTER_DOT, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_E4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 0},

    {NOTE_E4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 0},

    // --

    {NOTE_A3, NOTE_TYPE_QUARTER, 0},
    {NOTE_D3, NOTE_TYPE_QUARTER, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_G3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 0},

    {NOTE_A3, NOTE_TYPE_QUARTER, 0},
    {NOTE_D3, NOTE_TYPE_HALF_DOT, 0},
  }
};
