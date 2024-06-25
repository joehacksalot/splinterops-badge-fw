
#include "Song.h"

const SongNotes SunsSong = {
  "Sun's Song",
  100,  // Tempo
  14,   // Number of Notes
  {
    {NOTE_A3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER_DOT, 0},
    {NOTE_REST, NOTE_TYPE_EIGHTH, 0},

    // --

    {NOTE_A3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_QUARTER_DOT, 0},
    {NOTE_REST, NOTE_TYPE_EIGHTH, 0},

    // --

    {NOTE_C4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_E4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F4, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_G4, NOTE_TYPE_HALF, 1},

    // --
    
    {NOTE_G4, NOTE_TYPE_HALF_DOT, 0},
  }
};