/**
 * @file songs/ChestSound.c
 * @brief Note sequence asset for "Chest Sound".
 *
 * Defines a `SongNotes` constant with the title, tempo, and ordered note
 * events used by the audio/synth playback system. See `Song.h` for structure
 * details and playback integration.
 *
 * Usage
 * - Reference the symbol `ChestSound` to schedule or play this melody.
 *
 * Notes
 * - All timing is encoded via `NoteType`; rests use `NOTE_REST` entries.
 */
#include "Song.h"

const SongNotes ChestSound = {
  "Chest Sound",
  80,   // Tempo
  4,   // Number of Notes
  {
    {NOTE_A4, NOTE_TYPE_QUARTER_TRIPLET, 1},
    {NOTE_AS4, NOTE_TYPE_QUARTER_TRIPLET, 1},
    {NOTE_B4, NOTE_TYPE_QUARTER_TRIPLET, 1},
    {NOTE_C5, NOTE_TYPE_QUARTER, 1}
  }
};
