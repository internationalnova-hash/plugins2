#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaVoiceAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int spectrumBins = 96;

    NovaVoiceAudioProcessor();
    ~NovaVoiceAudioProcessor() override;

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

    std::atomic<float> outputPeakLevel { 0.0f };

    const std::array<std::atomic<float>, spectrumBins>& getInputSpectrum() const noexcept { return inputSpectrum; }
    const std::array<std::atomic<float>, spectrumBins>& getProblemSpectrum() const noexcept { return problemSpectrum; }
    const std::array<std::atomic<float>, spectrumBins>& getReductionSpectrum() const noexcept { return reductionSpectrum; }

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr size_t smoothingBandCount = 24;

    using IIRFilter = juce::dsp::IIR::Filter<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void initialiseBandLayout();

    void pushNextSampleIntoAnalyzer (float sample) noexcept;
    void refreshAnalyzerData();

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> analyzerFifo {};
    std::array<float, fftSize * 2> fftData {};
    int analyzerFifoIndex { 0 };
    bool nextFFTBlockReady { false };

    std::array<float, smoothingBandCount> bandFrequencies {};
    std::array<float, smoothingBandCount> bandQValues {};
    std::array<float, smoothingBandCount> bandAttackMs {};
    std::array<float, smoothingBandCount> bandReleaseMs {};
    std::array<float, smoothingBandCount> bandEnvelopes {};
    std::array<float, smoothingBandCount> bandRawReductionDb {};
    std::array<float, smoothingBandCount> bandSmoothedReductionDb {};
    std::array<float, smoothingBandCount> bandMaxReductionDb {};
    std::array<float, smoothingBandCount> bandAttackCoeff {};
    std::array<float, smoothingBandCount> bandReleaseCoeff {};

    std::array<IIRFilter, smoothingBandCount> leftProbeFilters;
    std::array<IIRFilter, smoothingBandCount> rightProbeFilters;
    std::array<IIRFilter, smoothingBandCount> leftReductionFilters;
    std::array<IIRFilter, smoothingBandCount> rightReductionFilters;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> pitchUpBuffer;
    juce::AudioBuffer<float> pitchDownBuffer;
    double currentSampleRate { 44100.0 };
    float previousWetLeft { 0.0f };
    float previousWetRight { 0.0f };
    float previousInputLeft { 0.0f };
    float previousInputRight { 0.0f };
    float modulationPhase { 0.0f };
    float pitchUpPhase { 0.0f };
    float pitchDownPhase { 0.0f };
    bool subOctavePolarityLeft { false };
    bool subOctavePolarityRight { false };
    float robotCarrierPhase { 0.0f };
    float robotEnvelope { 0.0f };

    std::array<IIRFilter, 4> formantFiltersLeft;
    std::array<IIRFilter, 4> formantFiltersRight;

    std::array<std::atomic<float>, spectrumBins> inputSpectrum {};
    std::array<std::atomic<float>, spectrumBins> problemSpectrum {};
    std::array<std::atomic<float>, spectrumBins> reductionSpectrum {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaVoiceAudioProcessor)
};
