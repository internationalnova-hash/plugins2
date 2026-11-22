#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "NovaDeskPreset.h"

// Lightweight preset browser (build spec section 30): holds a selection
// index into the factory library and applies a preset to the real APVTS
// via the same setValueNotifyingHost() path a host automating the plugin
// would use - there is no separate "preset engine," a preset IS just a
// set of APVTS values. User presets (saved via the UI's SAVE button) are
// appended after the factory library in the same browsable index space,
// so listPresets/loadPreset/next/previous all work on either kind without
// the UI needing to know the difference.
class NovaDeskPresetManager
{
public:
    NovaDeskPresetManager();

    int getNumPresets() const noexcept;
    const NovaDeskPreset& getPreset (int index) const noexcept;
    int getCurrentIndex() const noexcept { return currentIndex; }
    bool isUserPreset (int index) const noexcept;

    void applyToApvts (int index, juce::AudioProcessorValueTreeState& apvts);
    void selectNext (juce::AudioProcessorValueTreeState& apvts);
    void selectPrevious (juce::AudioProcessorValueTreeState& apvts);

    // Restores which preset is "selected" (for the browser's display/
    // highlight) WITHOUT touching APVTS values - used when recalling
    // session state, where the real parameter values are already
    // restored separately and may have been tweaked since the preset was
    // first loaded.
    void setSelectedIndexOnly (int index) noexcept { currentIndex = juce::jlimit (0, juce::jmax (0, getNumPresets() - 1), index); }

    // Captures the CURRENT apvts values into a new user preset, writes it
    // to disk (so it survives across sessions), appends it to the
    // browsable list, and selects it. Returns the new preset's index, or
    // -1 if the name was empty or the file couldn't be written.
    int saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts);

    static juce::File getUserPresetsFolder();

private:
    void scanUserPresets();

    int currentIndex = 0;
    std::vector<NovaDeskPreset> userPresets;
};
