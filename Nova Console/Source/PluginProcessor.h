#pragma once

#include <array>
#include <atomic>
#include <random>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaConsoleAudioProcessor : public juce::AudioProcessor
{
public:
    NovaConsoleAudioProcessor();
    ~NovaConsoleAudioProcessor() override;

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

    float getInputMeter() const noexcept { return inputMeter.load(); }
    float getOutputMeter() const noexcept { return outputMeter.load(); }
    float getGainReductionMeter() const noexcept { return gainReductionMeter.load(); }

private:
    enum class ConsoleMode : int { clean = 0, british, tube, tape, gold, modern };
    enum class QualityMode : int { eco = 0, mix, master };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateLinearStageCoefficients();

    void processPreamp (juce::AudioBuffer<float>& buffer, ConsoleMode mode, int osFactor);
    void processFilters (juce::AudioBuffer<float>& buffer);
    void processEq (juce::AudioBuffer<float>& buffer, ConsoleMode mode);
    void processCompressor (juce::AudioBuffer<float>& buffer, ConsoleMode mode, QualityMode quality);
    void processGate (juce::AudioBuffer<float>& buffer);
    void processAnalogEngine (juce::AudioBuffer<float>& buffer, ConsoleMode mode, int osFactor, QualityMode quality);

    static float modeWarmth (ConsoleMode mode) noexcept;
    static float modePresence (ConsoleMode mode) noexcept;

    juce::dsp::StateVariableTPTFilter<float> hpf[2];
    juce::dsp::StateVariableTPTFilter<float> lpf[2];

    juce::dsp::IIR::Filter<float> lowShelf[2];
    juce::dsp::IIR::Filter<float> lowMidPeak[2];
    juce::dsp::IIR::Filter<float> highMidPeak[2];
    juce::dsp::IIR::Filter<float> highShelf[2];
    juce::dsp::IIR::Filter<float> airShelf[2];

    // Preamp smoothing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> colorSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> trimSmoothed;
    
    // Input/Output smoothing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputSmoothed;
    
    // EQ smoothing (gains: 30-40ms, frequencies: 40-60ms, Q: 10-20ms)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowQSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowMidSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowMidFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowMidQSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highMidSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highMidFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highMidQSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highQSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airFreqSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airQSmoothed;
    
    // Filter frequency smoothing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> hpfSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lpfSmoothed;
    
    // Compressor smoothing (40-50ms for threshold/release, light 10-20ms for attack/ratio, 30ms for mix/makeup/punch)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compThresholdSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compRatioSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compAttackSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compReleaseSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compMixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compMakeupSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> compPunchSmoothed;
    
    // Gate smoothing (40-50ms, plus attack/hold state)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateThresholdSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateReleaseSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateRangeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateAttackSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gateHoldSmoothed;
    
    // Analog engine smoothing (50ms)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> heatSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> depthSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driftSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> noiseSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> crosstalkSmoothed;

    float compressorDetector = 0.0f;
    float compressorGainState = 1.0f;
    std::array<float, 2> compressorPunchMemory { 0.0f, 0.0f };
    std::array<float, 2> gateEnv { 0.0f, 0.0f };
    std::array<int32_t, 2> gateHoldCounter { 0, 0 };
    std::array<float, 2> gatePreviousEnv { 0.0f, 0.0f };
    std::array<float, 2> driftState { 0.0f, 0.0f };
    std::array<float, 2> preampPrevInput { 0.0f, 0.0f };
    std::array<float, 2> analogPrevInput { 0.0f, 0.0f };
    std::array<float, 2> analogToneMemory { 0.0f, 0.0f };

    float lastHpfHz = -1.0f;
    float lastLpfHz = -1.0f;
    float lastLowDb = 999.0f;
    float lastLowMidDb = 999.0f;
    float lastHighMidDb = 999.0f;
    float lastHighDb = 999.0f;
    float lastAirDb = 999.0f;
    float lastLowFreq = -1.0f;
    float lastLowMidFreq = -1.0f;
    float lastHighMidFreq = -1.0f;
    float lastHighFreq = -1.0f;
    float lastAirFreq = -1.0f;
    float lastLowQ = -1.0f;
    float lastLowMidQ = -1.0f;
    float lastHighMidQ = -1.0f;
    float lastHighQ = -1.0f;
    float lastAirQ = -1.0f;
    int lastHpfSlope = -1;
    int lastLpfSlope = -1;
    int lastLowMode = -1;
    int lastHighMode = -1;
    int lastAirMode = -1;

    std::minstd_rand rng;
    std::uniform_real_distribution<float> randDist { -1.0f, 1.0f };

    double currentSampleRate = 44100.0;

    std::atomic<float> inputMeter { 0.0f };
    std::atomic<float> outputMeter { 0.0f };
    std::atomic<float> gainReductionMeter { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaConsoleAudioProcessor)
};
