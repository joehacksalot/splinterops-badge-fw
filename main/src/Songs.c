
#include <stdio.h>
#include "esp_log.h"
#include "Song.h"

static const char * TAG = "SONG";

extern const SongNotes SecretSound;
extern const SongNotes SuccessSound;
extern const SongNotes ChestSound;
extern const SongNotes ZeldaOpening;
extern const SongNotes ZeldaTheme;
extern const SongNotes ZeldasLullaby;
extern const SongNotes EponasSong;
extern const SongNotes SongOfStorms;
extern const SongNotes SariasSong;
extern const SongNotes SunsSong;
extern const SongNotes SongOfTime;
extern const SongNotes MinuetOfForest;
extern const SongNotes BoleroOfFire;
extern const SongNotes SerenadeOfWater;
extern const SongNotes NocturneOfShadow;
extern const SongNotes RequiemOfSpirit;
extern const SongNotes PreludeOfLight;
extern const SongNotes Bonus;

static const SongNotes *pSongs[NUM_SONGS] = 
{
    &SecretSound,
    &SuccessSound,
    &ChestSound,
    &ZeldaOpening,
    &ZeldaTheme,
    &ZeldasLullaby,
    &EponasSong,
    &SariasSong,
    &SunsSong,
    &SongOfTime,
    &SongOfStorms,
    &MinuetOfForest,
    &BoleroOfFire,
    &SerenadeOfWater,
    &NocturneOfShadow,
    &RequiemOfSpirit,
    &PreludeOfLight,
    &Bonus,
};

// Function to calculate the duration of a note in milliseconds
// 'tempo' is the tempo in beats per minute
// 'noteType' is the type of the note as a fraction of a whole note
// For example, 1 for a whole note, 1/2 for a half note, 1/4 for a quarter note, etc.
int GetNoteTypeInMilliseconds(int tempo, float noteType)
{
    if (tempo <= 0 || noteType <= 0.0)
    {
        ESP_LOGI(TAG, "Error: Tempo and note type must be positive integers.\n");
        return -1;  // Error case
    }

    // Calculate the duration of one beat in milliseconds
    double beatDuration = 60000.0 / tempo;

    // Calculate the note duration based on the type of the note
    double NoteType = beatDuration * 4.0 * noteType;

    return (int)NoteType;
}

const SongNotes * GetSong(Song song)
{
    if (song >= 0 && song < NUM_SONGS)
    {
       return pSongs[song];
    }
    else
    {
        return NULL;
    }
    
}
