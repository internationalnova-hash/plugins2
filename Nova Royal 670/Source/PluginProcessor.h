#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NovaRoyal670Engine.h"
#include <atomic>

// NovaRoyal670Engine drives all DSP (validated standalone via the headless
// console-app harnesses in this Source/ directory: DetectorValidation,
// TubeTransformerValidation, EngineIntegrationValidation). The engine is
// stereo-only by design (both Stereo and M/S processing need a channel
// pair); a mono host bus is adapted here by duplicating into a scratch
// stereo buffer and averaging back down on the way out, rather than
// teaching the Core engine about mono - see isBusesLayoutSupported().
class NovaRoyal670Processor : public juce::AudioProcessor
{
public:
    NovaRoyal670Processor();
    ~NovaRoyal670Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Royal 670"; }
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

    juce::AudioProcessorValueTreeState apvts;

    // Written once per block on the audio thread, read by the editor's
    // 30Hz timer to drive the twin VU needles and the GR LED ladder.
    std::atomic<float> telemetryVuLeftOrMid { 0.0f };
    std::atomic<float> telemetryVuRightOrSide { 0.0f };
    std::atomic<float> telemetryGainReductionDb { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Re-prepares the engine (and the mono scratch buffer) - called from
    // prepareToPlay and whenever the Oversampling parameter changes,
    // never from inside processBlock (reallocating the oversampler is not
    // RT-safe).
    void reprepareEngine();

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int lastPreparedOversamplingStages = 1;

    nova::royal670::NovaRoyal670Engine engine;
    juce::AudioBuffer<float> monoScratchBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRoyal670Processor)
};
