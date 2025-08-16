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
