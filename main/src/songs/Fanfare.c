/**
  * @file songs/Fanfare.c
  * @brief Note sequence asset for "Fanfare".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `Fanfare` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes Fanfare = {
  "Fanfare",
  144,  // Tempo
  81,   // Number of Notes
  {
    {NOTE_D5, NOTE_TYPE_QUARTER_TRIPLET, 0},
    {NOTE_D5, NOTE_TYPE_QUARTER_TRIPLET, 0},
    {NOTE_D5, NOTE_TYPE_QUARTER_TRIPLET, 0},
    {NOTE_D5, NOTE_TYPE_QUARTER, 0},
    {NOTE_AS4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C5, NOTE_TYPE_QUARTER, 0},

    //--

    {NOTE_D5, NOTE_TYPE_QUARTER_TRIPLET_DOUBLE, 0},
    {NOTE_C5, NOTE_TYPE_QUARTER_TRIPLET, 0},
    {NOTE_D5, NOTE_TYPE_HALF_DOT, 0},

    //--

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},

    //--

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_FS4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_E4, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_E4, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},

    //--

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D5, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_D5, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},

    //--

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_FS4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_E4, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_E4, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 1},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},

    //--

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D5, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_D5, NOTE_TYPE_WHOLE, 0},
  }
};
