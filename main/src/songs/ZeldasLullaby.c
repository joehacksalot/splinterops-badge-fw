/**
  * @file songs/ZeldasLullaby.c
  * @brief Note sequence asset for "Zelda's Lullaby".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `ZeldasLullaby` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes ZeldasLullaby = {
  "Zelda's Lullaby",
  104,  // Tempo
  28,   // Number of Notes
  {
    {NOTE_B3, NOTE_TYPE_HALF, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_A3, NOTE_TYPE_HALF_DOT, 0},

    // --

    {NOTE_B3, NOTE_TYPE_HALF, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_A3, NOTE_TYPE_HALF_DOT, 0},

    // --

    {NOTE_B3, NOTE_TYPE_HALF, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_A4, NOTE_TYPE_HALF, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_D4, NOTE_TYPE_HALF, 0},
    {NOTE_C4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},

    // --

    {NOTE_A3, NOTE_TYPE_HALF, 0},
    {NOTE_G3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_EIGHTH, 0},

    // --

    {NOTE_B3, NOTE_TYPE_HALF, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_A3, NOTE_TYPE_HALF_DOT, 0},

    // --

    {NOTE_B3, NOTE_TYPE_HALF, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_A3, NOTE_TYPE_HALF_DOT, 0},

    // --

    {NOTE_B3, NOTE_TYPE_HALF, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_A4, NOTE_TYPE_HALF, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_D5, NOTE_TYPE_HALF_DOT, 1},

    // --

    {NOTE_D5, NOTE_TYPE_HALF, 0},
  }
};
