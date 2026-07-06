#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class JuiceGangProcessor : public juce::AudioProcessor
{
public:
    JuiceGangProcessor();
    ~JuiceGangProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override { return "Juice Gang"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // For UI metering
    float getInputLevel()  const noexcept { return inputLevel.load(); }
    float getOutputLevel() const noexcept { return outputLevel.load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── State-variable filter (per channel) ──────────────────────────────
    struct SVFilter {
        float lp = 0.f, bp = 0.f, hp = 0.f;
        void reset() { lp = bp = hp = 0.f; }
        // Returns selected mode output; mode: 0=LP 1=HP 2=BP 3=Notch
        float process (float x, float g, float R, int mode) {
            hp = x - R * bp - lp;
            bp = g * hp + bp;
            lp = g * bp + lp;
            float notch = lp + hp;
            switch (mode) { case 1: return hp; case 2: return bp; case 3: return notch; default: return lp; }
        }
    };
    std::array<SVFilter, 2> svf;

    // ── Delay ─────────────────────────────────────────────────────────────
    static constexpr int kDelayMaxSamples = 192001; // ~4s at 48k
    std::array<float, kDelayMaxSamples> delayBufL {}, delayBufR {};
    int delayWrite = 0;
    int delayReadSamples = 0;
    // Delay filter state (HP + LP on feedback)
    juce::dsp::IIR::Filter<float> delayHpL, delayHpR, delayLpL, delayLpR;

    // ── Reverb (Schroeder-Moorer) ─────────────────────────────────────────
    struct CombFilter {
        std::vector<float> buf;
        int writePos = 0;
        float feedback = 0.f, damp = 0.f, filterState = 0.f;
        void prepare (int size) { buf.assign (size, 0.f); writePos = 0; filterState = 0.f; }
        float process (float x) {
            float out = buf[writePos];
            filterState = out * (1.f - damp) + filterState * damp;
            buf[writePos] = x + filterState * feedback;
            if (++writePos >= (int)buf.size()) writePos = 0;
            return out;
        }
    };
    struct AllpassFilter {
        std::vector<float> buf;
        int writePos = 0;
        float feedback = 0.5f;
        void prepare (int size) { buf.assign (size, 0.f); writePos = 0; }
        float process (float x) {
            float bufOut = buf[writePos];
            float output = -x + bufOut;
            buf[writePos] = x + bufOut * feedback;
            if (++writePos >= (int)buf.size()) writePos = 0;
            return output;
        }
    };
    static constexpr int kNumCombs    = 8;
    static constexpr int kNumAllpass  = 4;
    std::array<CombFilter,    kNumCombs>   combL, combR;
    std::array<AllpassFilter, kNumAllpass> apL,   apR;
    bool freezeActive = false;

    // Reverb EQ (simple biquads)
    juce::dsp::IIR::Filter<float> revEqLowCutL,  revEqLowCutR;
    juce::dsp::IIR::Filter<float> revEqLowShelfL, revEqLowShelfR;
    juce::dsp::IIR::Filter<float> revEqMidL,     revEqMidR;
    juce::dsp::IIR::Filter<float> revEqHighShelfL,revEqHighShelfR;
    juce::dsp::IIR::Filter<float> revEqHighCutL,  revEqHighCutR;

    // Pre-delay buffer
    static constexpr int kPreDelayMax = 48000; // 1s
    std::array<float, kPreDelayMax> preDelayBufL {}, preDelayBufR {};
    int preDelayWrite = 0;

    // ── LFO ───────────────────────────────────────────────────────────────
    float lfoPhase = 0.f;
    float lfoValue = 0.f;

    // ── Smoothed params ───────────────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothCutoff;
    juce::SmoothedValue<float> smoothRes, smoothDrive, smoothFilterMix;
    juce::SmoothedValue<float> smoothDelayFeedback, smoothDelayMix;
    juce::SmoothedValue<float> smoothReverbSize, smoothReverbDecay, smoothReverbMix, smoothReverbDamp;
    juce::SmoothedValue<float> smoothMasterMix, smoothMasterOut, smoothMasterIn;

    // ── Voice tag playback ────────────────────────────────────────────────
    juce::AudioBuffer<float> tagBuffer;
    int  tagPlayPos  = 0;      // sample position; -1 = done / not loaded
    bool tagDecoded  = false;

    void decodeVoiceTag (double sr);

    // ── Levels ────────────────────────────────────────────────────────────
    std::atomic<float> inputLevel { 0.f }, outputLevel { 0.f };

    double sampleRate = 44100.0;

    void updateReverbEQ();
    float getLFOValue (int shape);
    float tempoSyncedSeconds (int divIndex, double bpm) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuiceGangProcessor)
};
