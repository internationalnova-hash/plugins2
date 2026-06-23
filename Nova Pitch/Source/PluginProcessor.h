#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaPitchAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int pitchHistorySize = 256;
    static constexpr int yinBufferSize    = 2048;

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

    float getDetectedPitch()  const noexcept { return detectedPitch.load(); }
    float getCorrectedPitch() const noexcept { return correctedPitch.load(); }
    float getConfidence()     const noexcept { return pitchConfidence.load(); }
    const std::array<std::atomic<float>, pitchHistorySize>& getPitchHistory() const noexcept { return pitchHistory; }

private:
    enum Scale : int { Chromatic = 0, Major = 1, Minor = 2, Pentatonic = 3, Blues = 4 };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float detectYIN (const float* samples, int n, float* d, float* cmnd);
    int   quantizeToScale (float hz);
    float midiToHz (int midi) const;
    float hzToMidi (float hz) const;

    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    // YIN pitch detection
    std::vector<float> yinBuf;
    std::vector<float> yinLinear;
    std::vector<float> yinD;
    std::vector<float> yinCmnd;

    int   yinWritePos           { 0 };
    float smoothedDetectedHz    { 0.0f };
    int   blocksSinceValidPitch { 0 };
    static constexpr int maxHoldBlocks = 20;

    std::array<float, 5> pitchHistory5 {};
    int ph5index = 0;

    std::atomic<float> detectedPitch   { 0.0f };
    std::atomic<float> correctedPitch  { 0.0f };
    std::atomic<float> pitchConfidence { 0.0f };
    std::array<std::atomic<float>, pitchHistorySize> pitchHistory {};
    int historyIndex { 0 };

    bool  correctionActive { false };
    int   lastTargetMidi   { -1 };
    float noteTargetRatio  { 1.0f };

    // ---------------------------------------------------------------
    // Dual-head pitch shifter (TD-PSOLA style)
    //
    // Read head A moves through the delay line at `ratio` samples per
    // output sample (Doppler pitch shift).  When the read/write gap
    // drifts by more than one pitch period from the target latency, we
    // crossfade to a new head B that is one pitch period away — because
    // the signal is periodic, that position looks identical, making the
    // jump inaudible.  No FFT, no phase accumulation, no choir effect.
    // ---------------------------------------------------------------
    static constexpr int kDlSize    = 16384;   // delay-line size (power of 2)
    static constexpr int kDlMask    = kDlSize - 1;
    static constexpr int kDlLatency = 2048;    // declared plugin latency (samples)

    std::array<float, kDlSize> dlBufL  {};
    std::array<float, kDlSize> dlBufR  {};

    int   dlWrite    { 0 };
    float dlReadA    { 0.0f };
    float dlReadB    { 0.0f };
    bool  dlXfading  { false };
    float dlXfade    { 0.0f };   // 0 = fully head-A, 1 = fully head-B
    float dlXfadeInc { 0.0f };   // increment per sample

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessor)
};
