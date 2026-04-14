#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaPitchAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int spectrumBins = 96;
    static constexpr int yinBufferSize = 2048;
    static constexpr int pitchHistorySize = 256;

    NovaPitchAudioProcessor();
    ~NovaPitchAudioProcessor() override;

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

    // Getters for UI visualization
    float getDetectedPitch() const noexcept { return detectedPitch.load(); }
    float getCorrectedPitch() const noexcept { return correctedPitch.load(); }
    float getConfidence() const noexcept { return pitchConfidence.load(); }
    const std::array<std::atomic<float>, pitchHistorySize>& getPitchHistory() const noexcept { return pitchHistory; }

private:
    enum Scale : int
    {
        Chromatic = 0,
        Major = 1,
        Minor = 2,
        Pentatonic = 3,
        Blues = 4,
        NumScales
    };

    // Scale quantization lookup tables
    static constexpr std::array<int, 12> chromaticScale { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    static constexpr std::array<int, 7> majorScale { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr std::array<int, 7> minorScale { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr std::array<int, 5> pentatonicScale { 0, 2, 4, 7, 9 };
    static constexpr std::array<int, 6> bluesScale { 0, 3, 5, 6, 7, 10 };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // YIN Pitch Detection Algorithm
    float detectPitchYIN (const float* samples, int numSamples);
    float detectPitchZeroCrossingFallback() const;
    float getYINThreshold() const noexcept;
    
    // Pitch correction
    void correctPitch (float* channelData, int numSamples, float detectedHz, float targetHz);
    int quantizeToScale (float pitchHz);
    float getTargetPitchHz (int midiNote) const;

    // Circular buffer resampling pitch shift
    void initializePitchShift();
    void processCircularBufferPitchShift (float* channelData, int numSamples, float pitchRatio, int channelIndex);

    // DSP systems
    float smoothDetectedPitch (float rawDetectedHz, float signalRms, bool lowLatencyMode);
    float computeRetuneRatio (float detectedHz, float targetHz, float signalRms, bool lowLatencyMode);
    void applyVibrato (float& pitchRatio, float sampleRate, int numSamples, float vibratoParam);
    void applyFormantShaper (float* channelData, int numSamples, float formantParam, int channelIndex);
    void applyOutputManagement (juce::AudioBuffer<float>& buffer, float inputRms);
    float computeBufferRms (const juce::AudioBuffer<float>& buffer) const;

    // Member variables
    std::vector<float> yinBuffer;
    int yinWriteIndex { 0 };
    
    std::array<std::array<float, yinBufferSize>, 2> pitchDelay {};
    std::array<float, 2> pitchReadPos { 0.0f, 0.0f };
    std::array<int, 2> pitchWriteIndex { 0, 0 };
    std::array<std::array<float, 2>, 2> formantAllPassState {};

    std::array<std::atomic<float>, pitchHistorySize> pitchHistory {};
    int historyIndex { 0 };

    std::atomic<float> detectedPitch { 0.0f };
    std::atomic<float> correctedPitch { 0.0f };
    std::atomic<float> pitchConfidence { 0.0f };

    double currentSampleRate { 44100.0 };
    float minPitchHz { 50.0f };
    float maxPitchHz { 400.0f };
    int blockCount { 0 };
    int analysisInterval { 2048 };
    float smoothedDetectedHz { 0.0f };
    float retuneLfoPhase { 0.0f };
    float retuneLfoJitter { 0.0f };
    float outputCompGain { 1.0f };
    float targetPitchRatio { 1.0f };
    float activePitchRatio { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessor)
};
