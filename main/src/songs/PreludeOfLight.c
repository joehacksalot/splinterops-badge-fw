/**
  * @file songs/PreludeOfLight.c
  * @brief Note sequence asset for "Prelude of Light".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `PreludeOfLight` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
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
