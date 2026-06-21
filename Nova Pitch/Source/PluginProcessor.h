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

    // YIN uses pre-allocated arrays passed in — no heap allocs on audio thread
    float detectYIN (const float* samples, int n, float* d, float* cmnd);
    int   quantizeToScale (float hz);
    float midiToHz (int midi) const;
    float hzToMidi (float hz) const;

    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    // YIN buffers — all pre-allocated in prepareToPlay, zero heap on audio thread
    std::vector<float> yinBuf;
    std::vector<float> yinLinear;
    std::vector<float> yinD;
    std::vector<float> yinCmnd;

    int   yinWritePos           { 0 };
    float smoothedDetectedHz    { 0.0f };
    int   blocksSinceValidPitch { 0 };
    static constexpr int maxHoldBlocks = 20;

    float pitchRatioSmoothed { 1.0f };

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
    // Dual-head crossfade pitch shifter
    // Replaces RubberBand — no external library, no latency surprises.
    //
    // Two read heads advance through a circular input buffer at `ratio`
    // speed.  When the active head drifts too close to the write head,
    // we crossfade to the inactive head which has been repositioned at
    // the standard delay.  This hides the reset discontinuity.
    // ---------------------------------------------------------------
    static constexpr int   kShiftBufSize   = 16384;   // must be power of 2
    static constexpr int   kShiftBufMask   = kShiftBufSize - 1;
    static constexpr int   kInitialDelay   = 1024;    // ~21 ms @ 48 kHz (reported to DAW)
    static constexpr int   kCrossfadeLen   = 512;     // ~10 ms crossfade window
    static constexpr float kMinDelay       = static_cast<float> (kCrossfadeLen + 32);

    std::array<float, kShiftBufSize> shiftBufL {};
    std::array<float, kShiftBufSize> shiftBufR {};

    int   shiftWritePos  { 0 };
    float shiftReadPos1  { 0.0f };   // head 1 (active)
    float shiftReadPos2  { 0.0f };   // head 2 (fading in during crossfade)
    int   shiftXfade     { -1 };     // -1 = not crossfading; 0..kCrossfadeLen-1 = active

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessor)
};
