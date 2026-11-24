#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TwoTrackEngine.h"
#include "NovaLicenseGate.h"

struct TwoTrackPreset
{
    const char* name;
    tt::Settings s;
};

/** Factory presets, also exposed to hosts as programs. */
const std::vector<TwoTrackPreset>& twoTrackPresets();

/** Parameter IDs in UI order (bypass excluded). */
const juce::StringArray& twoTrackParamIds();

class NovaTwoTrackAudioProcessor : public juce::AudioProcessor
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-2track", "Nova 2-Track" };

    NovaTwoTrackAudioProcessor();

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
    std::atomic<int> currentProgram { 0 };

    // ── Meters (peaks since the editor last read them) ──
    std::atomic<float> inL { 0 }, inR { 0 }, outL { 0 }, outR { 0 }, overDb { 0 };
    std::atomic<float> correlation { 1.0f };
    std::atomic<float> punchDb { 0 }, energyDb { 0 };

    // Full-rate output samples for the spectrum view (editor FFTs the newest 2048).
    static constexpr int kFftRing = 4096;
    std::array<std::atomic<float>, kFftRing> fftL {}, fftR {};
    std::atomic<int> fftWrite { 0 };
    static float take (std::atomic<float>& a) { return a.exchange (0.0f); }

    // Output samples for the stereo-image scope (decimated ring).
    static constexpr int kScopeSize = 1024;
    std::array<std::atomic<float>, kScopeSize> scopeL {}, scopeR {};
    std::atomic<int> scopeWrite { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    tt::Settings readSettings() const;

    tt::TwoTrackEngine engine;
    juce::AudioParameterBool* bypassParam = nullptr;

    juce::AudioBuffer<float> dryBuffer, delayRing, monoScratch;
    int ringPos = 0, scopeDecim = 0;
    juce::LinearSmoothedValue<float> wetAmount;
    double corrLL = 1.0e-9, corrRR = 1.0e-9, corrLR = 0.0, corrCoef = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaTwoTrackAudioProcessor)
};
