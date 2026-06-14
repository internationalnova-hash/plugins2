#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <complex>
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
    // Phase vocoder
    // -------------------------------------------------------------------------
    struct PVChannel
    {
        std::vector<float>               inputBuf;   // ring buffer for input
        int                              inputWritePos { 0 };
        std::vector<float>               outputBuf;  // overlap-add accumulation
        int                              outputWritePos { 0 };
        int                              outputReadPos  { 0 };
        std::vector<float>               lastAnalysisPhase;
        std::vector<float>               synthesisPhase;
        std::vector<std::complex<float>> fftBuf;
        int                              samplesSinceLastHop { 0 };

        void reset (int fftSize)
        {
            int bufSize = fftSize * 4;
            inputBuf.assign  (bufSize, 0.0f);
            outputBuf.assign (bufSize, 0.0f);
            lastAnalysisPhase.assign (fftSize / 2 + 1, 0.0f);
            synthesisPhase.assign    (fftSize / 2 + 1, 0.0f);
            fftBuf.assign            (fftSize,          { 0.0f, 0.0f });
            inputWritePos  = 0;
            outputWritePos = 0;
            outputReadPos  = 0;
            samplesSinceLastHop = 0;
        }
    };

    void initPV (bool lowLatency);
    void processPVChannel (PVChannel& ch, const float* in, float* out, int numSamples, float ratio);
    void runPVFrame (PVChannel& ch, float ratio);

    // -------------------------------------------------------------------------
    // Formant envelope helpers
    // -------------------------------------------------------------------------
    void computeSpectralEnvelope (const std::vector<float>& mag, std::vector<float>& env, int smoothBins);

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

    // Pitch ratio smoothing
    float pitchRatioSmoothed    { 1.0f };

    // PV engine
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float>              hannWindow;
    int  currentFftSize  { 2048 };
    int  currentHopSize  { 512 };
    int  currentFftOrder { 11 };
    PVChannel pvL, pvR;

    // UI atomics
    std::atomic<float> detectedPitch  { 0.0f };
    std::atomic<float> correctedPitch { 0.0f };
    std::atomic<float> pitchConfidence{ 0.0f };
    std::array<std::atomic<float>, pitchHistorySize> pitchHistory {};
    int historyIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessor)
};
