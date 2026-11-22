#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// One flat parameter snapshot, spanning both factory (hardcoded) and user
// (saved to disk) presets - same "combined browsable index" pattern as
// Nova Desk EQ's NovaDeskPresetManager.
struct NovaPitchPreset
{
    juce::String name;
    int   key = 0;
    int   scale = 1;
    float tolerance = 30.0f;
    float amount = 100.0f;
    float confidenceThreshold = 70.0f;
    float vibrato = 0.0f;
    float formant = 100.0f;
    bool  lowLatency = false;
    float formantWarp = 0.0f;
    float mix = 100.0f;
    float outputGain = 0.0f;
    float range = 100.0f;
    float vibratoRate = 5.0f;
    float transients = 30.0f;
};

class NovaPitchPresetManager
{
public:
    NovaPitchPresetManager();

    int getNumPresets() const noexcept { return (int) factoryPresets.size() + (int) userPresets.size(); }
    const NovaPitchPreset& getPreset (int index) const noexcept;
    bool isUserPreset (int index) const noexcept { return index >= (int) factoryPresets.size(); }

    void applyPreset (int index, juce::AudioProcessorValueTreeState& apvts) const;

    // Builds a preset from the APVTS's current live values, slugifies/
    // de-duplicates the name, writes it to disk, and appends it to the
    // browsable index. Returns the new preset's index.
    int saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts);

    static juce::File getUserPresetsFolder();

private:
    void scanUserPresets();

    std::vector<NovaPitchPreset> factoryPresets;
    std::vector<NovaPitchPreset> userPresets;
};
