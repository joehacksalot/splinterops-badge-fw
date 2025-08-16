
#include "Song.h"

const SongNotes PreludeOfLight = {
  "Prelude of Light",
  116,  // Tempo
  18,   // Number of Notes
  {
    {NOTE_D5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER_DOT, 0},
    {NOTE_D5, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D5, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_D5, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_QUARTER_DOT, 0},
    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_B3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_EIGHTH, 1},

    //--

    {NOTE_D4, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_D4, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_D4, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_CS4, NOTE_TYPE_WHOLE, 1},
    {NOTE_CS4, NOTE_TYPE_QUARTER, 0},
  }
};
