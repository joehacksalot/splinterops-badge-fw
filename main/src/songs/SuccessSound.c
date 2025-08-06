/**
  * @file songs/SuccessSound.c
  * @brief Note sequence asset for "Success Sound".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `SuccessSound` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes SuccessSound = {
  "Success Sound",
  240,   // Tempo
  6,   // Number of Notes
  {
    {NOTE_A5, NOTE_TYPE_EIGHTH, 1},
    {NOTE_B5, NOTE_TYPE_EIGHTH, 1},
    {NOTE_D6, NOTE_TYPE_EIGHTH, 1},
    {NOTE_E6, NOTE_TYPE_EIGHTH, 1},
    {NOTE_A6, NOTE_TYPE_QUARTER, 1},
    {NOTE_REST, NOTE_TYPE_QUARTER, 1}
  }
};
