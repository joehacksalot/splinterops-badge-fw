/**
 * @file Notes.c
 * @brief Musical note utilities: frequency lookup and note decomposition.
 *
 * Provides a compile-time lookup table mapping `NoteName` enumeration values to
 * their corresponding frequencies (Hz), and helpers to retrieve a note's
 * frequency as well as decompose a `NoteName` into base note and octave
 * components (`NoteParts`). This module is used by the audio/synth systems.
 *
 * Notes
 * - The `NoteFrequencies` array must align exactly with `NoteName` ordering.
 * - Invalid note values return 0.0 Hz and log a message.
 */

#include "esp_log.h"

#include "Notes.h"

static const char * TAG = "NOTE";

/**
 * @var NoteFrequencies
 * @brief Lookup table mapping `NoteName` to frequency in Hz.
 *
 * Array index corresponds to the `NoteName` enum value. The first entry is
 * the REST (0 Hz). All subsequent entries map enharmonic spellings (e.g.,
 * CS vs DF) to the same frequency.
 */
const float NoteFrequencies[NOTE_TOTAL_NUM_NOTES] = 
{
  FREQ_NOTE_REST,
  FREQ_NOTE_C0,
  FREQ_NOTE_CS0,
  FREQ_NOTE_DF0,
  FREQ_NOTE_D0,
  FREQ_NOTE_DS0,
  FREQ_NOTE_EF0,
  FREQ_NOTE_E0,
  FREQ_NOTE_F0,
  FREQ_NOTE_FS0,
  FREQ_NOTE_GF0,
  FREQ_NOTE_G0,
  FREQ_NOTE_GS0,
  FREQ_NOTE_AF0,
  FREQ_NOTE_A0,
  FREQ_NOTE_AS0,
  FREQ_NOTE_BF0,
  FREQ_NOTE_B0,
  FREQ_NOTE_C1,
  FREQ_NOTE_CS1,
  FREQ_NOTE_DF1,
  FREQ_NOTE_D1,
  FREQ_NOTE_DS1,
  FREQ_NOTE_EF1,
  FREQ_NOTE_E1,
  FREQ_NOTE_F1,
  FREQ_NOTE_FS1,
  FREQ_NOTE_GF1,
  FREQ_NOTE_G1,
  FREQ_NOTE_GS1,
  FREQ_NOTE_AF1,
  FREQ_NOTE_A1,
  FREQ_NOTE_AS1,
  FREQ_NOTE_BF1,
  FREQ_NOTE_B1,
  FREQ_NOTE_C2,
  FREQ_NOTE_CS2,
  FREQ_NOTE_DF2,
  FREQ_NOTE_D2,
  FREQ_NOTE_DS2,
  FREQ_NOTE_EF2,
  FREQ_NOTE_E2,
  FREQ_NOTE_F2,
  FREQ_NOTE_FS2,
  FREQ_NOTE_GF2,
  FREQ_NOTE_G2,
  FREQ_NOTE_GS2,
  FREQ_NOTE_AF2,
  FREQ_NOTE_A2,
  FREQ_NOTE_AS2,
  FREQ_NOTE_BF2,
  FREQ_NOTE_B2,
  FREQ_NOTE_C3,
  FREQ_NOTE_CS3,
  FREQ_NOTE_DF3,
  FREQ_NOTE_D3,
  FREQ_NOTE_DS3,
  FREQ_NOTE_EF3,
  FREQ_NOTE_E3,
  FREQ_NOTE_F3,
  FREQ_NOTE_FS3,
  FREQ_NOTE_GF3,
  FREQ_NOTE_G3,
  FREQ_NOTE_GS3,
  FREQ_NOTE_AF3,
  FREQ_NOTE_A3,
  FREQ_NOTE_AS3,
  FREQ_NOTE_BF3,
  FREQ_NOTE_B3,
  FREQ_NOTE_C4,
  FREQ_NOTE_CS4,
  FREQ_NOTE_DF4,
  FREQ_NOTE_D4,
  FREQ_NOTE_DS4,
  FREQ_NOTE_EF4,
  FREQ_NOTE_E4,
  FREQ_NOTE_F4,
  FREQ_NOTE_FS4,
  FREQ_NOTE_GF4,
  FREQ_NOTE_G4,
  FREQ_NOTE_GS4,
  FREQ_NOTE_AF4,
  FREQ_NOTE_A4,
  FREQ_NOTE_AS4,
  FREQ_NOTE_BF4,
  FREQ_NOTE_B4,
  FREQ_NOTE_C5,
  FREQ_NOTE_CS5,
  FREQ_NOTE_DF5,
  FREQ_NOTE_D5,
  FREQ_NOTE_DS5,
  FREQ_NOTE_EF5,
  FREQ_NOTE_E5,
  FREQ_NOTE_F5,
  FREQ_NOTE_FS5,
  FREQ_NOTE_GF5,
  FREQ_NOTE_G5,
  FREQ_NOTE_GS5,
  FREQ_NOTE_AF5,
  FREQ_NOTE_A5,
  FREQ_NOTE_AS5,
  FREQ_NOTE_BF5,
  FREQ_NOTE_B5,
  FREQ_NOTE_C6,
  FREQ_NOTE_CS6,
  FREQ_NOTE_DF6,
  FREQ_NOTE_D6,
  FREQ_NOTE_DS6,
  FREQ_NOTE_EF6,
  FREQ_NOTE_E6,
  FREQ_NOTE_F6,
  FREQ_NOTE_FS6,
  FREQ_NOTE_GF6,
  FREQ_NOTE_G6,
  FREQ_NOTE_GS6,
  FREQ_NOTE_AF6,
  FREQ_NOTE_A6,
  FREQ_NOTE_AS6,
  FREQ_NOTE_BF6,
  FREQ_NOTE_B6,
  FREQ_NOTE_C7,
  FREQ_NOTE_CS7,
  FREQ_NOTE_DF7,
  FREQ_NOTE_D7,
  FREQ_NOTE_DS7,
  FREQ_NOTE_EF7,
  FREQ_NOTE_E7,
  FREQ_NOTE_F7,
  FREQ_NOTE_FS7,
  FREQ_NOTE_GF7,
  FREQ_NOTE_G7,
  FREQ_NOTE_GS7,
  FREQ_NOTE_AF7,
  FREQ_NOTE_A7,
  FREQ_NOTE_AS7,
  FREQ_NOTE_BF7,
  FREQ_NOTE_B7,
  FREQ_NOTE_C8,
  FREQ_NOTE_CS8,
  FREQ_NOTE_DF8,
  FREQ_NOTE_D8,
  FREQ_NOTE_DS8,
  FREQ_NOTE_EF8,
  FREQ_NOTE_E8,
  FREQ_NOTE_F8,
  FREQ_NOTE_FS8
};

/**
 * @brief Get the frequency for a given note.
 *
 * @param note A value from `NoteName` enum.
 * @return Frequency in Hz, or 0.0 if the note is invalid.
 */
float GetNoteFrequency(NoteName note)
{
  if (note < NOTE_TOTAL_NUM_NOTES)
  {
    return NoteFrequencies[note];
  }
  
  ESP_LOGI(TAG, "Invalid note: %d", note);
  return 0.0;
}

/**
 * @brief Decompose a `NoteName` into base note and octave.
 *
 * Maps enharmonic spellings to the canonical base representation
 * (e.g., DF -> CS). For REST, base and octave are set to NONE.
 *
 * @param note A value from `NoteName` enum.
 * @return `NoteParts` with `.base` and `.octave` fields set.
 */
NoteParts GetNoteParts(NoteName note)
{
  NoteParts parts;
  switch (note)
  {
    case NOTE_REST:
      parts.base = NOTE_BASE_NONE;
      parts.octave = NOTE_OCTAVE_NONE;
      break;
    case NOTE_C0:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_CS0:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_DF0:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_D0:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_DS0:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_EF0:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_E0:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_F0:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_FS0:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_GF0:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_G0:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_GS0:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_AF0:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_A0:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_AS0:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_BF0:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_B0:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_0;
      break;
    case NOTE_C1:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_CS1:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_DF1:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_D1:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_DS1:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_EF1:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_E1:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_F1:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_FS1:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_GF1:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_G1:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_GS1:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_AF1:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_A1:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_AS1:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_BF1:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_B1:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_1;
      break;
    case NOTE_C2:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_CS2:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_DF2:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_D2:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_DS2:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_EF2:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_E2:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_F2:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_FS2:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_GF2:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_G2:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_GS2:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_AF2:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_A2:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_AS2:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_BF2:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_B2:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_2;
      break;
    case NOTE_C3:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_CS3:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_DF3:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_D3:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_DS3:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_EF3:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_E3:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_F3:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_FS3:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_GF3:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_G3:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_GS3:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_AF3:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_A3:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_AS3:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_BF3:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_B3:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_3;
      break;
    case NOTE_C4:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_CS4:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_DF4:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_D4:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_DS4:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_EF4:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_E4:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_F4:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_FS4:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_GF4:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_G4:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_GS4:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_AF4:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_A4:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_AS4:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_BF4:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_B4:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_4;
      break;
    case NOTE_C5:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_CS5:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_DF5:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_D5:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_DS5:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_EF5:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_E5:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_F5:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_FS5:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_GF5:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_G5:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_GS5:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_AF5:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_A5:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_AS5:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_BF5:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_B5:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_5;
      break;
    case NOTE_C6:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_CS6:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_DF6:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_D6:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_DS6:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_EF6:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_E6:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_F6:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_FS6:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_GF6:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_G6:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_GS6:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_AF6:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_A6:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_AS6:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_BF6:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_B6:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_6;
      break;
    case NOTE_C7:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_CS7:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_DF7:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_D7:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_DS7:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_EF7:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_E7:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_F7:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_FS7:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_GF7:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_G7:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_GS7:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_AF7:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_A7:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_AS7:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_BF7:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_B7:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_7;
      break;
    case NOTE_C8:
      parts.base = NOTE_BASE_C;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_CS8:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_DF8:
      parts.base = NOTE_BASE_CS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_D8:
      parts.base = NOTE_BASE_D;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_DS8:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_EF8:
      parts.base = NOTE_BASE_DS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_E8:
      parts.base = NOTE_BASE_E;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_F8:
      parts.base = NOTE_BASE_F;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_FS8:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_GF8:
      parts.base = NOTE_BASE_FS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_G8:
      parts.base = NOTE_BASE_G;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_GS8:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_AF8:
      parts.base = NOTE_BASE_GS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_A8:
      parts.base = NOTE_BASE_A;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_AS8:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_BF8:
      parts.base = NOTE_BASE_AS;
      parts.octave = NOTE_OCTAVE_8;
      break;
    case NOTE_B8:
      parts.base = NOTE_BASE_B;
      parts.octave = NOTE_OCTAVE_8;
    break;
  default:
    break;
  };
  return parts;
}