#include "NovaBendFactoryPresets.h"

// Factory library for Nova Bend's "PITCH -> FORMANT -> DONE" identity, plus
// its Flatten monotone effect. Link semantics matter for how these were
// tuned (see NovaBendEngine::process): with Link OFF, Formant is an
// independent offset - small values here read as a natural, corrected
// timbre shift. With Link ON, Formant is ADDED to the effective pitch shift,
// so Formant=0 + Link=ON means the formants track the pitch 1:1 - the
// classic "tape speed" character (unmistakable chipmunk/giant color),
// which is what the Extreme category is built around.
const std::vector<NovaBendPreset>& getNovaBendFactoryPresets()
{
    static const std::vector<NovaBendPreset> presets = {
        { .id = "default", .displayName = "Default", .category = "Init",
          .pitchSemitones = 0.0f, .formantSemitones = 0.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "male_deepen", .displayName = "Male Deepen", .category = "Character",
          .pitchSemitones = -4.0f, .formantSemitones = -2.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "female_lift", .displayName = "Female Lift", .category = "Character",
          .pitchSemitones = 4.0f, .formantSemitones = 2.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "subtle_male", .displayName = "Subtle Male", .category = "Character",
          .pitchSemitones = -2.0f, .formantSemitones = -1.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 85.0f, .qualityIndex = 1, .bypass = false },

        { .id = "subtle_female", .displayName = "Subtle Female", .category = "Character",
          .pitchSemitones = 2.0f, .formantSemitones = 1.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 85.0f, .qualityIndex = 1, .bypass = false },

        { .id = "androgynous_shift", .displayName = "Androgynous Shift", .category = "Character",
          .pitchSemitones = 1.0f, .formantSemitones = 2.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 70.0f, .qualityIndex = 1, .bypass = false },

        { .id = "giant", .displayName = "Giant", .category = "Extreme",
          .pitchSemitones = -12.0f, .formantSemitones = 0.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = true,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "chipmunk", .displayName = "Chipmunk", .category = "Extreme",
          .pitchSemitones = 12.0f, .formantSemitones = 0.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = true,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "demon_voice", .displayName = "Demon Voice", .category = "Extreme",
          .pitchSemitones = -7.0f, .formantSemitones = -4.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = true,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "helium_squeak", .displayName = "Helium Squeak", .category = "Extreme",
          .pitchSemitones = 9.0f, .formantSemitones = 3.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = true,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "formant_widen", .displayName = "Formant Widen", .category = "Formant",
          .pitchSemitones = 0.0f, .formantSemitones = 6.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "formant_narrow", .displayName = "Formant Narrow", .category = "Formant",
          .pitchSemitones = 0.0f, .formantSemitones = -6.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "deep_formant_only", .displayName = "Deep Formant Only", .category = "Formant",
          .pitchSemitones = 0.0f, .formantSemitones = -9.0f, .flatten = false,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "flatten_low_c2", .displayName = "Flatten Low C2", .category = "Flatten",
          .pitchSemitones = 0.0f, .formantSemitones = 0.0f, .flatten = true,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 1, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "flatten_mid_c3", .displayName = "Flatten Mid C3", .category = "Flatten",
          .pitchSemitones = 0.0f, .formantSemitones = 0.0f, .flatten = true,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 2, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },

        { .id = "flatten_high_c4", .displayName = "Flatten High C4", .category = "Flatten",
          .pitchSemitones = 0.0f, .formantSemitones = 0.0f, .flatten = true,
          .flattenNoteIndex = 0, .flattenOctaveIndex = 3, .link = false,
          .mixPercent = 100.0f, .qualityIndex = 1, .bypass = false },
    };
    return presets;
}
