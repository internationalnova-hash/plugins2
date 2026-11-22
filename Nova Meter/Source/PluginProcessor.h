#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MeterEngine.h"
#include "Profiles.h"
#include "ReferenceAnalyzer.h"

class NovaMeterAudioProcessor : public juce::AudioProcessor
{
public:
    NovaMeterAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    tz::MeterEngine& engine() { return meterEngine; }

    //==============================================================================
    // Editor-facing state. Message thread only.
    tz::Profile working;            // targets in use (possibly edited)
    juce::String activeProfile;     // profile `working` was loaded from
    juce::String viewName { "live" }; // live | targets | reference
    tz::ProfileStore profiles;

    tz::ReferenceStats reference;   // stats only; the audio itself is not stored
    tz::ReferenceAnalyzer referenceAnalyzer;

    /** Moves a finished analysis into `reference`. Returns true if one arrived. */
    bool collectReference();

    bool isModified() const;
    void loadProfile (const juce::String& name);

    /** Bumped whenever the host restores state, so an open editor can re-sync. */
    std::atomic<int> stateGeneration { 0 };

private:
    tz::MeterEngine meterEngine;
    juce::AudioBuffer<float> doubleScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMeterAudioProcessor)
};
