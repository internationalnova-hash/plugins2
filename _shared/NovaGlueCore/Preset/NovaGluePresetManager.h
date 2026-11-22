#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Preset manager for Nova Glue, adapted from
// _shared/NovaGlowCore/Preset/NovaGlowPresetManager.h - same shape (baked
// factory presets + disk-based user presets, one JSON file per preset).
// Built from V1 per the user's own spec, which lists "presets/state
// architecture" inside the V1 stop-point rather than as a follow-up.
// bypass is deliberately NOT part of a preset - transport state, not sonic
// character, the same reasoning Glow uses for its own bypass/meterMode.
class NovaGluePresetManager
{
public:
    struct Values
    {
        float thresholdDb = -10.0f;
        int   attackChoice = 4;   // 0.1/0.3/1/3/10/30 ms
        int   releaseChoice = 4;  // 0.1/0.3/0.6/1.2 s, 4 = AUTO
        float makeupDb = 0.0f;
        int   ratioChoice = 0;    // 0=2:1, 1=4:1, 2=10:1
        float scHpfHz = 20.0f;
        float mixPercent = 100.0f;
        float gluePercent = 45.0f;
        float drivePercent = 0.0f;
        bool  autoGain = true;
        bool  stereoLink = true;
        int   oversamplingChoice = 1; // Off/2x/4x
    };

    struct Preset
    {
        juce::String name;
        bool isFactory = true;
        Values values;
        juce::File sourceFile; // empty for factory presets
    };

    NovaGluePresetManager();

    void scanUserPresets();

    int getNumPresets() const noexcept { return static_cast<int> (presets.size()); }
    const Preset& getPreset (int index) const noexcept;
    juce::StringArray getPresetNames() const;
    juce::Array<bool> getFactoryFlags() const;

    int getCurrentIndex() const noexcept { return currentIndex; }
    void setCurrentIndex (int index) noexcept;
    void selectNext() noexcept;
    void selectPrevious() noexcept;

    void applyTo (juce::AudioProcessorValueTreeState& apvts) const;
    static Values captureFrom (const juce::AudioProcessorValueTreeState& apvts);

    bool saveUserPreset (const juce::String& name, const Values& values);

    static juce::File getUserPresetDirectory();

private:
    std::vector<Preset> presets;
    int currentIndex = 0;

    static juce::var valuesToVar (const juce::String& name, const Values& v);
    static bool valuesFromVar (const juce::var& v, juce::String& outName, Values& outValues);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGluePresetManager)
};
