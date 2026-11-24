#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "NovaPunchEngine.h"
#include "Presets/NovaPunchPresetManager.h"
#include "NovaLicenseGate.h"

// No message-thread reconfigure Timer here (unlike e.g. Nova Bend) - none
// of this plugin's parameters change the DSP's internal buffer sizing at
// runtime, so there's nothing that needs a deferred, allocation-safe
// reconfigure step.
class NovaPunchAudioProcessor : public juce::AudioProcessor
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-punch", "Nova Punch" };

    NovaPunchAudioProcessor();
    ~NovaPunchAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void reset() override { engine.reset(); }

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    NovaPunchPresetManager presetManager;
    void loadPreset (int index) { presetManager.applyToApvts (index, apvts); }
    void loadNextPreset() { presetManager.selectNext (apvts); }
    void loadPreviousPreset() { presetManager.selectPrevious (apvts); }
    int getCurrentPresetIndex() const noexcept { return presetManager.getCurrentIndex(); }
    int saveCurrentAsPreset (const juce::String& name) { return presetManager.saveCurrentAsUserPreset (name, apvts); }

    // --- real (not decorative) telemetry, safe to read from the message
    // thread - written once per block in processBlock(), matching this
    // catalog's established atomic-snapshot pattern (see Nova Bend's
    // PluginProcessor for the same shape). ---
    float getTransientProbability() const noexcept { return transientProbability.load(); }
    float getTransientStrength() const noexcept { return transientStrength.load(); }
    float getAttackGainDb() const noexcept { return attackGainDb.load(); }
    float getDecayGainDb() const noexcept { return decayGainDb.load(); }
    float getSustainGainDb() const noexcept { return sustainGainDb.load(); }
    float getLowActivity() const noexcept { return lowActivity.load(); }
    float getMidActivity() const noexcept { return midActivity.load(); }
    float getHighActivity() const noexcept { return highActivity.load(); }
    float getInputPeakL() const noexcept { return inputPeakL.load(); }
    float getInputPeakR() const noexcept { return inputPeakR.load(); }
    float getOutputPeakL() const noexcept { return outputPeakL.load(); }
    float getOutputPeakR() const noexcept { return outputPeakR.load(); }
    int   getCurrentRegion() const noexcept { return currentRegion.load(); }

    // Processing Activity: two parallel history series (Punch/Body),
    // matching the mockup's PROCESSING ACTIVITY display. Ring buffer;
    // index 0 is oldest.
    static constexpr int kActivityHistorySize = 64;
    float getPunchActivityHistoryAt (int indexFromOldest) const noexcept;
    float getBodyActivityHistoryAt (int indexFromOldest) const noexcept;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    nova::punch::NovaPunchEngine engine;

    double currentSampleRate { 44100.0 };

    std::atomic<float> transientProbability { 0.0f };
    std::atomic<float> transientStrength { 0.0f };
    std::atomic<float> attackGainDb { 0.0f };
    std::atomic<float> decayGainDb { 0.0f };
    std::atomic<float> sustainGainDb { 0.0f };
    std::atomic<float> lowActivity { 0.0f };
    std::atomic<float> midActivity { 0.0f };
    std::atomic<float> highActivity { 0.0f };
    std::atomic<float> inputPeakL { 0.0f }, inputPeakR { 0.0f };
    std::atomic<float> outputPeakL { 0.0f }, outputPeakR { 0.0f };
    std::atomic<int>   currentRegion { 0 };

    std::array<std::atomic<float>, kActivityHistorySize> punchActivityHistory {};
    std::array<std::atomic<float>, kActivityHistorySize> bodyActivityHistory {};
    std::atomic<int> activityHistoryWrite { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPunchAudioProcessor)
};
