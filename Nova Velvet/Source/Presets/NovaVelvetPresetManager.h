#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "NovaVelvetPreset.h"

// Lightweight preset browser: a preset IS just a set of APVTS values,
// applied via the same setValueNotifyingHost() path a host automating the
// plugin would use. User presets (saved via the UI's SAVE button) are
// appended after the factory library in the same browsable index space.
class NovaVelvetPresetManager
{
public:
    NovaVelvetPresetManager();

    int getNumPresets() const noexcept;
    const NovaVelvetPreset& getPreset (int index) const noexcept;
    int getCurrentIndex() const noexcept { return currentIndex; }
    bool isUserPreset (int index) const noexcept;

    void applyToApvts (int index, juce::AudioProcessorValueTreeState& apvts);
    void selectNext (juce::AudioProcessorValueTreeState& apvts);
    void selectPrevious (juce::AudioProcessorValueTreeState& apvts);

    void setSelectedIndexOnly (int index) noexcept { currentIndex = juce::jlimit (0, juce::jmax (0, getNumPresets() - 1), index); }

    int saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts);

    static juce::File getUserPresetsFolder();

private:
    void scanUserPresets();

    int currentIndex = 0;
    std::vector<NovaVelvetPreset> userPresets;
};
