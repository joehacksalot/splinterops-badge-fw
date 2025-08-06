/**
  * @file songs/RequiemOfSpirit.c
  * @brief Note sequence asset for "Requiem of Spirit".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `RequiemOfSpirit` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes RequiemOfSpirit = {
  "Requiem of Spirit",
  80,  // Tempo
  20,   // Number of Notes
  {
    {NOTE_D4, NOTE_TYPE_QUARTER, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_F4, NOTE_TYPE_QUARTER, 0},

    //--

    {NOTE_D4, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_D3, NOTE_TYPE_QUARTER, 0},
    {NOTE_F3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D3, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A3, NOTE_TYPE_QUARTER, 0},
    {NOTE_F3, NOTE_TYPE_QUARTER, 0},

    //--

    {NOTE_D3, NOTE_TYPE_WHOLE, 0},

    //--

    {NOTE_D4, NOTE_TYPE_QUARTER, 0},
    {NOTE_F4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_D4, NOTE_TYPE_EIGHTH, 0},
    {NOTE_A4, NOTE_TYPE_QUARTER, 0},
    {NOTE_F4, NOTE_TYPE_QUARTER, 0},

    //--

    {NOTE_D4, NOTE_TYPE_HALF, 0},
    {NOTE_A3, NOTE_TYPE_HALF, 0},

    //--

    {NOTE_D3, NOTE_TYPE_WHOLE, 0},
  }
};
