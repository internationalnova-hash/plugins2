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

    // UI getters
    float getDetectedPitch()  const noexcept { return detectedPitch.load(); }
    float getCorrectedPitch() const noexcept { return correctedPitch.load(); }
    float getConfidence()     const noexcept { return pitchConfidence.load(); }
    const std::array<std::atomic<float>, pitchHistorySize>& getPitchHistory() const noexcept { return pitchHistory; }

private:
    // -------------------------------------------------------------------------
    // Scale tables
    // -------------------------------------------------------------------------
    enum Scale : int { Chromatic = 0, Major = 1, Minor = 2, Pentatonic = 3, Blues = 4 };

    static constexpr std::array<int, 12> chromaticScale  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    static constexpr std::array<int, 7>  majorScale      { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr std::array<int, 7>  minorScale      { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr std::array<int, 5>  pentatonicScale { 0, 2, 4, 7, 9 };
    static constexpr std::array<int, 6>  bluesScale      { 0, 3, 5, 6, 7, 10 };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // -------------------------------------------------------------------------
    // Pitch detection (YIN)
    // -------------------------------------------------------------------------
    float detectYIN (const float* samples, int n);

    // -------------------------------------------------------------------------
    // Scale quantization
    // -------------------------------------------------------------------------
    int   quantizeToScale (float hz);
    float midiToHz        (int   midi) const;
    float hzToMidi        (float hz)   const;

    // -------------------------------------------------------------------------
    // Simple two-pointer crossfading pitch shifter (grain-based, no FFT)
    // -------------------------------------------------------------------------
    struct SimplePitchShifter
    {
        static constexpr int N    = 8192;   // ring buffer size (must be power of 2)
        static constexpr int MASK = N - 1;

        std::array<float, N> buf {};
        int   writePos = 0;
        // Relative positions: negative means "this many samples behind the write pointer"
        float readArel = -(N / 2.0f);          // nominal: half-buffer behind
        float readBrel = -(N * 3.0f / 4.0f);  // B starts one more quarter behind

        void reset() noexcept
        {
            buf.fill (0.0f);
            writePos = 0;
            readArel = -(N / 2.0f);
            readBrel = -(N * 3.0f / 4.0f);
        }

        float process (float input, float ratio) noexcept
        {
            // Write
            buf[writePos & MASK] = input;
            ++writePos;

            // Each reader gains `(ratio-1)` relative to the write pointer per sample.
            // At ratio=1: no movement relative to write.
            // At ratio>1: reader drifts toward write (distance shrinks).
            // At ratio<1: reader drifts away (distance grows).
            readArel += (ratio - 1.0f);
            readBrel += (ratio - 1.0f);

            // Clamp: keep relative position in safe range
            // Too close to write (> -N/8): jump back half-buffer to avoid reading the future
            // Too far from write (< -7N/8): jump forward to avoid reading stale data
            constexpr float minDist = -(N / 8.0f);      // closest allowed = -1024
            constexpr float maxDist = -(7.0f * N / 8.0f); // furthest allowed = -7168
            constexpr float jump    = -(N / 2.0f);       // jump amount = -4096

            if (readArel > minDist) readArel += jump;
            if (readArel < maxDist) readArel -= jump;
            if (readBrel > minDist) readBrel += jump;
            if (readBrel < maxDist) readBrel -= jump;

            // Cross-fade weights: fade a reader out as it approaches minDist (jump zone)
            constexpr float fadeWidth = N / 8.0f;        // 1024 samples to fade
            float wA = juce::jlimit (0.0f, 1.0f, (minDist - readArel) / fadeWidth);
            float wB = juce::jlimit (0.0f, 1.0f, (minDist - readBrel) / fadeWidth);
            float totalW = wA + wB;
            if (totalW < 1e-6f) { wA = 0.5f; wB = 0.5f; }
            else { wA /= totalW; wB /= totalW; }

            // Interpolated buffer reads
            auto lerp = [this](float rel) -> float
            {
                float abs = (float)writePos + rel - 1.0f;  // -1 because writePos already incremented
                float wrapped = abs - (float)N * std::floor (abs / (float)N);
                int i0 = (int)wrapped & MASK;
                int i1 = (i0 + 1) & MASK;
                float frac = wrapped - (float)(int)wrapped;
                return buf[i0] * (1.0f - frac) + buf[i1] * frac;
            };

            return lerp (readArel) * wA + lerp (readBrel) * wB;
        }
    };

    SimplePitchShifter spL, spR;   // one per channel

    // -------------------------------------------------------------------------
    // Member state
    // -------------------------------------------------------------------------
    double currentSampleRate { 44100.0 };

    // YIN state
    std::vector<float> yinBuf;
    int    yinWritePos          { 0 };
    float  smoothedDetectedHz   { 0.0f };
    int    blocksSinceValidPitch{ 0 };
    static constexpr int maxHoldBlocks = 20;

    // Pitch correction smoothing
    float pitchRatioSmoothed    { 1.0f };

    // Pitch detection history for median filter (last 5 blocks)
    std::array<float, 5> pitchHistory5 {};
    int  ph5index = 0;

    // UI atomics
    std::atomic<float> detectedPitch  { 0.0f };
    std::atomic<float> correctedPitch { 0.0f };
    std::atomic<float> pitchConfidence{ 0.0f };
    std::array<std::atomic<float>, pitchHistorySize> pitchHistory {};
    int historyIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessor)
};
