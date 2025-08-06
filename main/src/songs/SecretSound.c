/**
  * @file songs/SecretSound.c
  * @brief Note sequence asset for "Secret Sound".
  *
  * Defines a `SongNotes` constant with the title, tempo, and ordered note
  * events used by the audio/synth playback system. See `Song.h` for structure
  * details and playback integration.
  *
  * Usage
  * - Reference the symbol `SecretSound` to schedule or play this melody.
  *
  * Notes
  * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
  */
#include "Song.h"

const SongNotes SecretSound = {
  "Secret Sound",
  120,  // Tempo
  8,   // Number of Notes
  {
    {NOTE_GF6, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_F6, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_D6, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_AF5, NOTE_TYPE_SIXTEENTH, 0},

    {NOTE_G5, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_EF6, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_G6, NOTE_TYPE_SIXTEENTH, 0},
    {NOTE_B6, NOTE_TYPE_SIXTEENTH, 0},
  }
};