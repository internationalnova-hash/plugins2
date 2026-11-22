#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// One flat parameter snapshot spanning both factory (hardcoded) and user
// (saved to disk) presets - same "combined browsable index" pattern as
// Nova Pitch's NovaPitchPresetManager / Nova Desk EQ's NovaDeskPresetManager.
struct NovaTapePreset
{
    juce::String name;
    float input = 0.0f;
    float tapeAmount = 65.0f;
    int   tapeType = 0;
    int   ips = 1;
    float bias = 0.0f;
    float wowFlutter = 2.4f;
    float noise = 1.8f;
    float lowCut = 30.0f;
    float highCut = 18000.0f;
    float mix = 50.0f;
    float output = -1.3f;
    float trim = 0.0f;
};

class NovaTapePresetManager
{
public:
    NovaTapePresetManager();

    int getNumPresets() const noexcept { return (int) factoryPresets.size() + (int) userPresets.size(); }
    const NovaTapePreset& getPreset (int index) const noexcept;
    bool isUserPreset (int index) const noexcept { return index >= (int) factoryPresets.size(); }

    void applyPreset (int index, juce::AudioProcessorValueTreeState& apvts) const;
    int saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts);

    static juce::File getUserPresetsFolder();

private:
    void scanUserPresets();

    std::vector<NovaTapePreset> factoryPresets;
    std::vector<NovaTapePreset> userPresets;
};
