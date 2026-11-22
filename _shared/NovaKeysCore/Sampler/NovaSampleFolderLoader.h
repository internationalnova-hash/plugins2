#pragma once

#include "NovaSamplerEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>

// Loads a folder of note-named WAV files (e.g. "..._C4_...wav") into a
// NovaSamplerEngine's zone list (message-thread/file-I/O only - never
// called from the audio thread). Splits the full MIDI key range evenly
// between the loaded root notes so the whole keyboard is covered even
// though the test library only has a handful of notes per instrument
// (spec section 2: "samples must map intelligently across the keyboard").
//
// Filenames are expected to contain a note name token like "C4", "G#1",
// "A#3" (case-insensitive, standard scientific pitch notation with C4 =
// MIDI 60, matching this UI's own keyboard labeling). Files without a
// recognizable note token are skipped, not treated as an error - a
// partially-named folder still loads whatever it can.
class NovaSampleFolderLoader
{
public:
    // Returns the number of zones successfully added.
    static int loadFolderIntoEngine (const juce::File& folder, NovaSamplerEngine& engine, int roundRobinGroup = 0);

    // Parses a note name token (e.g. "C4", "G#1") to a MIDI note number.
    // Returns -1 if no valid note name is found anywhere in the string.
    static int parseNoteNameFromFilename (const juce::String& filename);

private:
    static juce::AudioFormatManager& getFormatManager();
};
