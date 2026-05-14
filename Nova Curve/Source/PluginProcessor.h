#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaCurveAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int spectrumBins = 96;
    static constexpr int maxBands = 24;

    NovaCurveAudioProcessor();
    ~NovaCurveAudioProcessor() override;

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

    juce::String getUiStateAsJson() const;
    void applyUiStateFromJson (const juce::String& jsonText);
    void applyRealtimeParamFromUi (const juce::String& key, int bandIndex, float value);

    const std::array<std::atomic<float>, spectrumBins>& getPreSpectrum() const noexcept { return preSpectrum; }
    const std::array<std::atomic<float>, spectrumBins>& getPostSpectrum() const noexcept { return postSpectrum; }
    const std::array<std::atomic<float>, spectrumBins>& getReductionSpectrum() const noexcept { return reductionSpectrum; }
    int getUiStateApplyCount() const noexcept { return uiStateApplyCount.load (std::memory_order_relaxed); }
    int getUiStateLastApplyMs() const noexcept { return uiStateLastApplyMs.load (std::memory_order_relaxed); }
    int getUiStateDiagSelectedBand() const noexcept { return uiStateDiagSelectedBand.load (std::memory_order_relaxed); }
    float getUiStateDiagFrequency() const noexcept { return uiStateDiagFrequency.load (std::memory_order_relaxed); }
    float getUiStateDiagGainDb() const noexcept { return uiStateDiagGainDb.load (std::memory_order_relaxed); }
    float getUiStateDiagQ() const noexcept { return uiStateDiagQ.load (std::memory_order_relaxed); }
    int getUiStateDiagEnabled() const noexcept { return uiStateDiagEnabled.load (std::memory_order_relaxed); }
    int getUiStateDiagSolo() const noexcept { return uiStateDiagSolo.load (std::memory_order_relaxed); }
    std::atomic<float> outputPeakLevel { 0.0f };
    std::atomic<float> dynamicActivity { 0.0f };

private:
    struct BandState
    {
        std::atomic<float> enabled { 0.0f };
        std::atomic<float> type { 0.0f };      // 0 bell, 1 low shelf, 2 high shelf, 3 high pass, 4 low pass, 5 notch, 6 band pass, 7 tilt
        std::atomic<float> mode { 0.0f };      // 0 static, 1 dynamic, 2 resonance
        std::atomic<float> channel { 0.0f };   // 0 stereo, 1 mid, 2 side, 3 left, 4 right
        std::atomic<float> frequency { 1000.0f };
        std::atomic<float> gainDb { 0.0f };
        std::atomic<float> q { 1.0f };
        std::atomic<float> slope { 24.0f };
        std::atomic<float> dynRangeDb { -6.0f };
        std::atomic<float> thresholdMode { 1.0f }; // 0 auto, 1 manual
        std::atomic<float> thresholdDb { -22.0f };
        std::atomic<float> attackMs { 10.0f };
        std::atomic<float> releaseMs { 120.0f };
        std::atomic<float> ratio { 2.2f };
        std::atomic<float> solo { 0.0f };
    };

    juce::var createStateVar() const;
    void applyStateVar (const juce::var& parsed);

    void initialiseDefaultState();
    void pushSampleToAnalyzer (float preSample, float postSample) noexcept;
    void refreshAnalyzerData();

    static juce::dsp::IIR::Coefficients<float>::Ptr createBandCoefficients (
        int type,
        double sampleRate,
        float frequency,
        float q,
        float gainDb,
        float slopeDbPerOct);

    static float mapMagnitudeToNormal (float magnitude) noexcept;

    std::array<BandState, maxBands> bands;

    std::atomic<float> selectedBand { 3.0f };
    std::atomic<float> phaseMode { 1.0f };     // 0 zero latency, 1 natural, 2 linear
    std::atomic<float> qualityMode { 1.0f };   // 0 eco, 1 high, 2 ultra
    std::atomic<float> analyzerMode { 0.0f };  // 0 pre, 1 post
    std::atomic<float> harmonicLink { 0.0f };  // 0 off, 1 on
    std::atomic<float> outputGainDb { 0.0f };
    std::atomic<float> bypassed { 0.0f };
    std::atomic<float> resonanceAmount { 42.0f };

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> preFifo {};
    std::array<float, fftSize> postFifo {};
    std::array<float, fftSize * 2> preFftData {};
    std::array<float, fftSize * 2> postFftData {};
    int fftFifoIndex { 0 };
    bool fftReady { false };

    std::array<std::atomic<float>, spectrumBins> preSpectrum {};
    std::array<std::atomic<float>, spectrumBins> postSpectrum {};
    std::array<std::atomic<float>, spectrumBins> reductionSpectrum {};

    std::array<std::array<juce::dsp::IIR::Filter<float>, maxBands>, 2> eqFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, maxBands>, 2> eqFiltersStage2;
    std::array<std::array<juce::dsp::IIR::Filter<float>, maxBands>, 2> eqFiltersStage3;
    std::array<std::array<juce::dsp::IIR::Filter<float>, maxBands>, 2> eqFiltersStage4;
    std::array<std::array<juce::dsp::IIR::Filter<float>, maxBands>, 2> detectorFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, maxBands>, 2> auditionFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, maxBands>, 2> auditionFiltersStage2;
    std::array<float, maxBands> detectorEnvelopes {};
    std::array<float, maxBands> bandDynamicGainDb {};
    std::array<float, maxBands> lastDynamicGainDb {};  // For smooth transient tracking
    std::array<float, maxBands> smoothedAppliedGainDb {}; // Smoothed total gain to avoid zipper/click artifacts

    std::atomic<int> uiStateApplyCount { 0 };
    std::atomic<int> uiStateLastApplyMs { 0 };
    std::atomic<int> uiStateDiagSelectedBand { 0 };
    std::atomic<float> uiStateDiagFrequency { 0.0f };
    std::atomic<float> uiStateDiagGainDb { 0.0f };
    std::atomic<float> uiStateDiagQ { 0.0f };
    std::atomic<int> uiStateDiagEnabled { 0 };
    std::atomic<int> uiStateDiagSolo { 0 };

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> soloSourceBuffer;
    double currentSampleRate { 44100.0 };
    float transientDecay { 0.0f };  // For transient preservation

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaCurveAudioProcessor)
};
