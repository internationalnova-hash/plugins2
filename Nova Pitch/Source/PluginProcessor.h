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
    int   pitchLockBlocks  { 0 };

    // ---------------------------------------------------------------
    // Granular OLA pitch shifter — 8× overlap for low phasiness
    // ---------------------------------------------------------------
    static constexpr int kGrainSize    = 512;
    static constexpr int kHopSize      = 64;    // 8× overlap (was 128 = 4×)
    static constexpr int kInitialDelay = 1536;
    static constexpr int kGrainInMask  = 4096 - 1;
    static constexpr int kGrainOutMask = 2048 - 1;

    std::array<float, 4096> grainInL  {};
    std::array<float, 4096> grainInR  {};
    std::array<float, 2048> grainOutL {};
    std::array<float, 2048> grainOutR {};
    std::array<float, kGrainSize> grainWin {};

    int grainInWrite  { 0 };
    int grainOutWrite { 0 };
    int grainOutRead  { 0 };
    int grainHop      { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessor)
};
