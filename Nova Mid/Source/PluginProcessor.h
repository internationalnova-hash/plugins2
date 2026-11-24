#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MidEngine.h"
#include "NovaLicenseGate.h"

struct MidPreset
{
    const char* name;
    float push, ceiling, output;
};

/** Factory presets, also exposed to hosts as programs. */
const std::vector<MidPreset>& midPresets();

class NovaMidAudioProcessor : public juce::AudioProcessor
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-mid", "Nova Mid" };

    NovaMidAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioParameterBool* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    // Meters for the editor: peaks since the editor last read them.
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, overDb { 0.0f };
    float takeInPeak()  { return inPeak.exchange (0.0f); }
    float takeOutPeak() { return outPeak.exchange (0.0f); }
    float takeOverDb()  { return overDb.exchange (0.0f); }

    std::atomic<int> currentProgram { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    nm::MidEngine engine;
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* pushParam = nullptr;
    std::atomic<float>* ceilingParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    // Bypass crossfades to a latency-matched dry signal so toggling never clicks or jumps in time.
    juce::AudioBuffer<float> dryBuffer, delayRing;
    int ringPos = 0;
    juce::LinearSmoothedValue<float> wetAmount;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaMidAudioProcessor)
};
