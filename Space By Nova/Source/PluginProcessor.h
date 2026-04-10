#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class SpaceByNovaAudioProcessor : public juce::AudioProcessor
{
public:
    SpaceByNovaAudioProcessor();
    ~SpaceByNovaAudioProcessor() override;

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

private:
    using DelayLine = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static float clamp01 (float value) noexcept;

    juce::Reverb reverb;
    DelayLine preDelayLeft { 96000 };
    DelayLine preDelayRight { 96000 };
    DelayLine decorrelationDelay { 4096 };

    juce::dsp::StateVariableTPTFilter<float> wetToneLeft;
    juce::dsp::StateVariableTPTFilter<float> wetToneRight;
    juce::dsp::StateVariableTPTFilter<float> wetBodyLeft;
    juce::dsp::StateVariableTPTFilter<float> wetBodyRight;
    juce::dsp::StateVariableTPTFilter<float> earlyToneLeft;
    juce::dsp::StateVariableTPTFilter<float> earlyToneRight;
    juce::dsp::StateVariableTPTFilter<float> earlyBodyLeft;
    juce::dsp::StateVariableTPTFilter<float> earlyBodyRight;

    juce::LinearSmoothedValue<float> smoothedSpace;
    juce::LinearSmoothedValue<float> smoothedAir;
    juce::LinearSmoothedValue<float> smoothedDepth;
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedWidth;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> earlyBuffer;

    std::vector<float> earlyTapBufferLeft;
    std::vector<float> earlyTapBufferRight;
    int earlyTapWriteIndex { 0 };
    int earlyTapBufferSize { 0 };
    float earlyDiffuseStateLeft { 0.0f };
    float earlyDiffuseStateRight { 0.0f };

    double currentSampleRate { 44100.0 };
    float motionPhase { 0.0f };
    float haloPhase { 1.7f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceByNovaAudioProcessor)
};
