
#include "Song.h"

const SongNotes SongOfTime = {
  "Song of Time",
  104,  // Tempo
  18,   // Number of Notes
  {
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_D4, NOTE_TYPE_HALF, 0},
    {NOTE_F4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_D4, NOTE_TYPE_HALF, 0},
    {NOTE_F4, NOTE_TYPE_QUARTER, 0},

    // --

    {NOTE_A4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_C5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_QUARTER, 0},
    {NOTE_G4, NOTE_TYPE_QUARTER, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_G4, NOTE_TYPE_EIGHTH, 0},

    // --

    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},
    {NOTE_C4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_E4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER, 1},

    //--

    {NOTE_D4, NOTE_TYPE_HALF, 0},
  }
};
