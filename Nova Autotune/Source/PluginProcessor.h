#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#ifdef HAVE_RUBBERBAND
#include <rubberband/RubberBandStretcher.h>
#endif

class NovaAutotuneAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int pitchHistorySize = 256;
    static constexpr int mpmBufferSize    = 2048;

    NovaAutotuneAudioProcessor();
    ~NovaAutotuneAudioProcessor() override;

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

    // UI getters
    float getDetectedPitch()  const noexcept { return detectedPitch.load(); }
    float getCorrectedPitch() const noexcept { return correctedPitch.load(); }
    float getConfidence()     const noexcept { return pitchConfidence.load(); }

private:
    // -------------------------------------------------------------------------
    // Scale tables
    // -------------------------------------------------------------------------
    enum Scale : int { Chromatic = 0, Major = 1, Minor = 2, Pentatonic = 3, Blues = 4, Dorian = 5 };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // -------------------------------------------------------------------------
    // MPM pitch detection
    // -------------------------------------------------------------------------
    float detectMPM (const float* samples, int n);

    // -------------------------------------------------------------------------
    // Scale quantization
    // -------------------------------------------------------------------------
    float quantizeToScale (float hz);
    float midiToHz        (float midi) const;
    float hzToMidi        (float hz)   const;

    // -------------------------------------------------------------------------
    // Member state
    // -------------------------------------------------------------------------
    double currentSampleRate { 44100.0 };

    // MPM ring buffer
    std::vector<float> mpmBuf;
    int    mpmWritePos           { 0 };

    // Smoothing
    float  smoothedDetectedHz    { 0.0f };
    int    blocksSinceValidPitch { 0 };
    static constexpr int maxHoldBlocks = 20;

    // Pitch correction smoothing
    float pitchRatioSmoothed     { 1.0f };

    // 5-block median filter
    std::array<float, 5> pitchHistory5 {};
    int  ph5index = 0;

#ifdef HAVE_RUBBERBAND
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
#endif

    // UI atomics
    std::atomic<float> detectedPitch  { 0.0f };
    std::atomic<float> correctedPitch { 0.0f };
    std::atomic<float> pitchConfidence{ 0.0f };
    std::array<std::atomic<float>, pitchHistorySize> pitchHistory {};
    int historyIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAutotuneAudioProcessor)
};
