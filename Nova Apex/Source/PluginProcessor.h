#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaApexAudioProcessor : public juce::AudioProcessor
{
public:
    NovaApexAudioProcessor();
    ~NovaApexAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

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

    // Thread-safe metering
    std::atomic<float> inputLevelL   { 0.0f };
    std::atomic<float> inputLevelR   { 0.0f };
    std::atomic<float> outputLevelL  { 0.0f };
    std::atomic<float> outputLevelR  { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> novaGuardActivity { 0.0f };  // 0..1, how hard Nova Guard is working

    // Mode flags set from editor/UI
    std::atomic<bool> deltaMode         { false };
    std::atomic<bool> smartLoudnessMode { false };
    std::atomic<int>  ditherBits        { 0 };       // 0=off, 16, 20, 24

    float getLUFS()     const noexcept { return lufsEstimate.load(); }
    float getTruePeak() const noexcept { return truePeakEstimate.load(); }

    // Loudness match: gain to apply to bypass signal so levels match limited output
    float getSmartMatchGain() const noexcept { return smartMatchGain.load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void resetState() noexcept;

    // Drive / saturation (Nova Heat-style)
    float applyDrive (float sample, float driveAmt, int style) noexcept;

    // Low-end protect: returns a 0..1 suppression factor for GR (1 = don't reduce as much)
    float lowEndProtectFactor (float sample, int channel, float amount) noexcept;

    // Transient preserve: returns 0..1 transient weight (1 = transient present, relax GR)
    float transientWeight (float sample, int channel, float amount) noexcept;

    // Nova Guard: monitors distortion accumulation; returns adaptive release multiplier
    float novaGuardMultiplier() const noexcept;

    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    // Lookahead
    static constexpr int kMaxLookaheadSamples = 8192;
    std::array<std::vector<float>, 2> lookaheadBuf;
    std::array<int, 2>                lookaheadWritePos { 0, 0 };
    int lookaheadDelaySamples { 0 };

    // Delta: pre-limit copy
    std::array<std::vector<float>, 2> preLimitBuf;
    std::array<int, 2>                preLimitWritePos { 0, 0 };

    // Gain envelope (per channel; channel 0 used when linked)
    std::array<float, 2> gainEnv { 1.0f, 1.0f };

    // Transient detector: fast and slow envelope followers per channel
    std::array<float, 2> transientFastEnv  { 0.0f, 0.0f };
    std::array<float, 2> transientSlowEnv  { 0.0f, 0.0f };
    float transientFastCoeff  { 0.9f };
    float transientSlowCoeff  { 0.9995f };

    // Low-end protect: 1-pole LP filter state per channel
    std::array<float, 2> lowEndLPState { 0.0f, 0.0f };
    float lowEndLPCoeff { 0.99f };  // ~80Hz at 44.1kHz

    // Nova Guard: rolling distortion accumulator
    float novaGuardAccum    { 0.0f };
    float novaGuardDecay    { 0.9998f };
    float adaptiveReleaseMul { 1.0f };

    // LUFS / true-peak
    std::atomic<float> lufsEstimate    { -70.0f };
    std::atomic<float> truePeakEstimate { 0.0f };
    std::atomic<float> smartMatchGain  { 0.0f };

    float rmsAccum    { 0.0f };
    float rmsAccumOut { 0.0f };
    int   rmsCount    { 0 };
    float tpHold      { 0.0f };

    float inputPeakHoldL  { 0.0f }, inputPeakHoldR  { 0.0f };
    float outputPeakHoldL { 0.0f }, outputPeakHoldR { 0.0f };

    // Dither
    std::array<float, 2> ditherState { 0.0f, 0.0f };

    int currentProgram { 0 };

    // Oversampling for nonlinear drive stage (2x default, factor selectable)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int lastOversampleFactor { 2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaApexAudioProcessor)
};
