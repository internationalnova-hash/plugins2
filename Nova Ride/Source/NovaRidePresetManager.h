#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Small factory-preset manager - just a table of parameter values applied to
// the APVTS. No custom/user-preset save/load in V1 (not requested).
class NovaRidePresetManager
{
public:
    struct Preset
    {
        juce::String name;
        float targetDb = 0.0f;
        int   rangeIndex = 1; // 0=±3, 1=±6, 2=±9, 3=±12
        float speedPercent = 50.0f;
        float sensitivityPercent = 50.0f;
        float outputDb = 0.0f;
        int   modeIndex = 0; // 0=Natural, 1=Tight, 2=Live
        bool  music = false;
    };

    NovaRidePresetManager();

    int getNumPresets() const noexcept { return static_cast<int> (presets.size()); }
    const Preset& getPreset (int index) const noexcept;
    juce::StringArray getPresetNames() const;

    int getCurrentIndex() const noexcept { return currentIndex; }
    void setCurrentIndex (int index) noexcept;

    void applyTo (juce::AudioProcessorValueTreeState& apvts) const;

private:
    std::vector<Preset> presets;
    int currentIndex = 0;
};
