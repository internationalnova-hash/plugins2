#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>

// Global master effects chain (spec sections 13-14): EQ -> Saturation ->
// Compression -> Chorus -> Delay -> Reverb -> Output. Runs ONCE per block
// on the whole mixed voice-pool output (unlike NovaInstrumentLayer/
// NovaSynthVoice, which are per-sample per-voice) - this is deliberately a
// simple block-based chain, matching how a global insert-effects rack
// normally works.
//
// Reuses proven formulas from _shared/NovaAIMasterCore/DSP (saturation's
// tanh-drive-blend technique, the compressor's soft-knee gain-reduction
// curve, the limiter's soft-ceiling safety net) reimplemented against this
// module's own plain params rather than depending on NovaMasteringPlan -
// same "reuse the algorithm, not the entangled class" approach used
// throughout this build. Reverb/Chorus/Delay use JUCE's own dsp:: building
// blocks directly (spec section 18: reuse existing Nova DSP where
// practical, but do not embed those plugins' full UIs/classes).
struct NovaEffectsParams
{
    bool eqActive = true;
    float eqLowGainDb = 0.f, eqMidGainDb = 0.f, eqHighGainDb = 0.f;
    float eqLowFreq = 200.f, eqMidFreq = 1000.f, eqHighFreq = 5000.f;

    bool saturationActive = true;
    float saturationDrive = 0.f; // 0-1
    float saturationMix = 0.f;   // 0-1

    bool compressionActive = true;
    float compThresholdDb = -18.f, compRatio = 2.f, compAttackMs = 10.f, compReleaseMs = 150.f, compMakeupDb = 0.f;

    bool chorusActive = true;
    float chorusRateHz = 1.f, chorusDepth = 0.25f, chorusMix = 0.f;

    bool delayActive = true;
    float delayTimeMs = 350.f, delayFeedback = 0.3f, delayMix = 0.f;

    bool reverbActive = true;
    float reverbRoomSize = 0.5f, reverbDamping = 0.5f, reverbMix = 0.f, reverbWidth = 1.f;

    float outputCeilingDb = -0.3f; // final safety soft-ceiling, always on
};

// TONE/MOTION/SPACE/CHARACTER (spec sections 12-14). These are the primary
// user-facing sound-design controls - each drives several internal effects
// parameters at once. On/off toggles for each effect stay under explicit
// user/preset control via setParams(); macros only drive the continuous
// "amount" parameters, so a user can still fully disable an effect
// regardless of macro position.
struct NovaMacroValues
{
    float tone = 0.5f;      // dark(0) <-> bright(1)
    float motion = 0.5f;    // still(0) <-> moving(1)
    float space = 0.5f;     // dry(0) <-> huge(1)
    float character = 0.5f; // clean(0) <-> rich(1)
};

class NovaEffectsChain
{
public:
    NovaEffectsChain() = default;

    void prepare (double sampleRate, int maxBlockSize, int numChannels = 2);
    void reset() noexcept;

    void setParams (const NovaEffectsParams& p) noexcept;
    const NovaEffectsParams& getParams() const noexcept { return params; }

    void setMacros (const NovaMacroValues& macros) noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    void updateEQCoefficients() noexcept;
    void processEQ (juce::AudioBuffer<float>& buffer) noexcept;
    void processSaturation (juce::AudioBuffer<float>& buffer) noexcept;
    void processCompression (juce::AudioBuffer<float>& buffer) noexcept;
    void processDelay (juce::AudioBuffer<float>& buffer) noexcept;
    void processOutputCeiling (juce::AudioBuffer<float>& buffer) noexcept;

    NovaEffectsParams params;
    NovaMacroValues macroValues;

    struct EQBand { juce::dsp::IIR::Filter<float> filter; };
    static constexpr int kNumEqBands = 3; // low shelf, mid peak, high shelf
    std::array<std::array<EQBand, kNumEqBands>, 2> eqBands; // [channel][band]

    std::unique_ptr<juce::dsp::Oversampling<float>> saturationOversampler;

    float compEnvelope = 0.f, compGainReductionDb = 0.f;
    float compAttackCoeff = 0.f, compReleaseCoeff = 0.f;

    float outputLimiterGrDb = 0.f; // fast peak limiter ahead of the soft-ceiling safety curve

    juce::dsp::Chorus<float> chorus;
    juce::dsp::DelayLine<float> delayLine { 1 << 17 }; // ~3s at 44.1kHz, plenty for a musical delay

    juce::Reverb reverb;

    double sampleRate = 44100.0;
    int preparedNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaEffectsChain)
};
