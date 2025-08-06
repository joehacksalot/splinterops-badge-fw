/**
  * @file songs/BonusBonus.c
  * @brief Note sequence asset for "Bonus Bonus".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `BonusBonus` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes BonusBonus = {
  "Bonus Bonus",
  114,  // Tempo
  65,   // Number of Notes
  {
    {NOTE_C4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},

    //--

    {NOTE_A4, NOTE_TYPE_EIGHTH_DOT, 0},
    {NOTE_A4, NOTE_TYPE_SIXTEENTH, 1},
    {NOTE_A4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},

    //--

    {NOTE_G4, NOTE_TYPE_EIGHTH_DOT, 0},
    {NOTE_G4, NOTE_TYPE_SIXTEENTH, 1},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_E4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},

    //--

    {NOTE_F4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_E4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_E4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_C4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C4, NOTE_TYPE_EIGHTH, 0},

    //--

    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_F4, NOTE_TYPE_HALF, 0},
    {NOTE_C4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},

    //--

    {NOTE_A4, NOTE_TYPE_EIGHTH_DOT, 0},
    {NOTE_A4, NOTE_TYPE_SIXTEENTH, 1},
    {NOTE_A4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},

    //--

    {NOTE_C5, NOTE_TYPE_QUARTER, 0},
    {NOTE_E4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_E4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},

    //--

    {NOTE_F4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_E4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_E4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_C4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C4, NOTE_TYPE_EIGHTH, 0},

    //--

    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_F4, NOTE_TYPE_HALF, 0},
  }
};