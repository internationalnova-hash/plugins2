#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Preset manager for Nova Glow. Unlike a sample-based synth (see Nova
// Bass's NovaBassPresetManager, which needs an external asset "Vault" for
// sample references/licensing), every one of Glow's 13 parameters is a
// plain APVTS float/choice/bool - a preset here is nothing more than a
// named snapshot of those values, so a lightweight scheme is the right
// size: factory presets are baked into this .cpp (same shape as Nova
// Ride's NovaRidePresetManager), and user presets are simple one-preset-
// per-file JSON documents scanned from a small per-plugin directory in
// Application Support. bypass and meterMode are deliberately NOT part of
// a preset - they're transport/monitoring state (is the unit currently
// bypassed, what the tech is currently looking at), not sonic character,
// the same way a real hardware unit's front-panel settings wouldn't
// include "which meter am I currently reading."
class NovaGlowPresetManager
{
public:
    struct Values
    {
        float peakReduction = 30.0f;
        float gain = 50.0f;
        int   compressLimit = 0; // 0=Compress, 1=Limit
        float scHpf = 30.0f;
        float emphasis = 0.0f;
        float glow = 45.0f;
        int   analogMode = 1; // 0=Clean, 1=Vintage, 2=Hot
        float mix = 100.0f;
        float stereoLink = 100.0f;
        float noise = 0.0f;
        int   oversampling = 1; // 0=Off, 1=2x, 2=4x
    };

    struct Preset
    {
        juce::String name;
        bool isFactory = true;
        Values values;
        juce::File sourceFile; // empty for factory presets
    };

    NovaGlowPresetManager();

    // Scans the user preset directory (created if missing) and appends any
    // *.novaglowpreset files found to the list, after the factory presets.
    // Message-thread only - call once from the editor/processor
    // constructor, never from the audio thread.
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

    // Writes a new *.novaglowpreset JSON file, adds it to the in-memory
    // list (as the new current preset), and returns true on success. Name
    // collisions are disambiguated with a numeric suffix rather than
    // overwriting an existing user preset silently.
    bool saveUserPreset (const juce::String& name, const Values& values);

    static juce::File getUserPresetDirectory();

private:
    std::vector<Preset> presets;
    int currentIndex = 0;

    static juce::var valuesToVar (const juce::String& name, const Values& v);
    static bool valuesFromVar (const juce::var& v, juce::String& outName, Values& outValues);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaGlowPresetManager)
};
