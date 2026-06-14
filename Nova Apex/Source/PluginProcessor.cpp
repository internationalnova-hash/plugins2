#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>

namespace
{
    constexpr auto gainId              = "gain";
    constexpr auto ceilingId           = "ceiling";
    constexpr auto outputGainId        = "outputGain";
    constexpr auto driveId             = "drive";
    constexpr auto styleId             = "style";
    constexpr auto attackId            = "attack";
    constexpr auto releaseId           = "release";
    constexpr auto lookaheadId         = "lookahead";
    constexpr auto oversampleId        = "oversample";
    constexpr auto linkId              = "link";
    constexpr auto ditherId            = "dither";
    constexpr auto transientPreserveId = "transientPreserve";
    constexpr auto lowEndProtectId     = "lowEndProtect";
    constexpr auto loudnessTargetId    = "loudnessTarget";
    constexpr auto ispProtectId        = "ispProtect";
    constexpr auto smartReleaseId      = "smartRelease";
    constexpr auto safeModeId          = "safeMode";

    inline float dbToLinear (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
    inline float linearToDb (float lin) noexcept { return 20.0f * std::log10 (lin + 1.0e-10f); }

    inline float makeCoeff (float ms, double sr) noexcept
    {
        if (ms <= 0.0f) return 0.0f;
        return std::exp (-1.0f / (static_cast<float> (sr) * ms * 0.001f));
    }

    // Triangular PDF dither, amplitude scaled to target bit depth
    inline float triangleDither (float& state, int bits) noexcept
    {
        const float amplitude = 1.0f / static_cast<float> (1 << bits);
        const float r1 = static_cast<float> (rand()) / static_cast<float> (RAND_MAX);
        const float r2 = state;
        state = r1;
        return amplitude * (r1 - r2);
    }

    // Nova Heat-style soft saturation: asymmetric with even harmonics
    inline float novaHeatSaturate (float x, float drive) noexcept
    {
        // drive 0..10 mapped to saturation depth
        const float depth = 1.0f + drive * 0.35f;
        const float s = std::tanh (x * depth);
        // Asymmetric: slight bias generates 2nd harmonic like tape/tube
        return s + 0.015f * drive * (s * s);
    }

    // 8 factory presets: { gain, drive, ceiling, outputGain, style(0-5),
    //   attack, release, lookahead, oversample(0-3), link, dither,
    //   transientPreserve, lowEndProtect, loudnessTarget }
    struct PresetData
    {
        const char* name;
        float gain, drive, ceiling, outputGain;
        int   style;
        float attack, release, lookahead;
        int   oversample;
        bool  link;
        bool  dither;
        float transientPreserve, lowEndProtect, loudnessTarget;
    };

    static const PresetData kPresets[] =
    {
        // Transparent Master — surgical, minimal colouring
        { "Transparent Master",
          0.0f, 0.0f, -0.1f, 0.0f, 0 /*Clean*/,
          0.5f, 200.0f, 7.0f, 1 /*2x*/, true, false,
          0.4f, 0.5f, -14.0f },

        // Streaming Ready — -14 LUFS, soft ceiling
        { "Streaming Ready",
          2.0f, 0.5f, -1.0f, -0.5f, 0 /*Clean*/,
          1.0f, 150.0f, 5.0f, 1, true, true,
          0.3f, 0.4f, -14.0f },

        // Loud Pop — competitive loudness with preserved transients
        { "Loud Pop",
          6.0f, 2.0f, -0.1f, 0.0f, 3 /*Loud*/,
          0.3f, 60.0f, 3.0f, 2 /*4x*/, true, false,
          0.6f, 0.3f, -9.0f },

        // Punchy Hip Hop — slow attack, big transients, sub punch
        { "Punchy Hip Hop",
          4.0f, 1.5f, -0.3f, 0.0f, 1 /*Punch*/,
          3.0f, 100.0f, 5.0f, 1, true, false,
          0.7f, 0.8f, -10.0f },

        // Analog Glue — Nova Heat saturation before limiting, warm & wide
        { "Analog Glue",
          3.0f, 5.0f, -0.3f, -0.5f, 4 /*Analog*/,
          2.0f, 120.0f, 6.0f, 2, true, false,
          0.5f, 0.6f, -11.0f },

        // Vocal Master — smooth release, preserve detail
        { "Vocal Master",
          1.0f, 0.5f, -0.5f, 0.0f, 2 /*Smooth*/,
          1.5f, 300.0f, 8.0f, 1, false, true,
          0.2f, 0.1f, -14.0f },

        // EDM Loud — maximum loudness, fast everything
        { "EDM Loud",
          8.0f, 3.0f, -0.1f, 0.0f, 3 /*Loud*/,
          0.1f, 40.0f, 2.0f, 3 /*8x*/, true, false,
          0.3f, 0.5f, -7.0f },

        // Release Ready — conservative, broadcast safe, dithered
        { "Release Ready",
          1.0f, 0.0f, -0.3f, -0.2f, 5 /*Master*/,
          1.0f, 250.0f, 10.0f, 1, true, true,
          0.5f, 0.5f, -16.0f },
    };

    static constexpr int kNumPresets = 8;
}

NovaApexAudioProcessor::NovaApexAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaApex"), createParameterLayout())
{
    for (auto& buf : lookaheadBuf)
        buf.assign (kMaxLookaheadSamples, 0.0f);
    for (auto& buf : preLimitBuf)
        buf.assign (kMaxLookaheadSamples, 0.0f);
}

NovaApexAudioProcessor::~NovaApexAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaApexAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gainId, 1 }, "Gain",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ceilingId, 1 }, "Ceiling",
        juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.1f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " dBFS"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGainId, 1 }, "Output",
        juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " dB"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveId, 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1); }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { styleId, 1 }, "Style",
        juce::StringArray { "Clean", "Punch", "Smooth", "Loud", "Analog", "Master" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float> (0.1f, 50.0f, 0.1f, 0.5f), 1.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " ms"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f), 100.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lookaheadId, 1 }, "Lookahead",
        juce::NormalisableRange<float> (0.0f, 20.0f, 0.1f), 5.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " ms"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { oversampleId, 1 }, "Oversample",
        juce::StringArray { "1x", "2x", "4x", "8x" }, 1));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { linkId, 1 }, "Link", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ditherId, 1 }, "Dither", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { transientPreserveId, 1 }, "Transient Preserve",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lowEndProtectId, 1 }, "Low-End Protect",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { loudnessTargetId, 1 }, "Loudness Target",
        juce::NormalisableRange<float> (-23.0f, -6.0f, 0.1f), -14.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (v, 1) + " LUFS"; }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ispProtectId, 1 }, "ISP Protection", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { smartReleaseId, 1 }, "Smart Release", false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { safeModeId, 1 }, "Safe Mode", false));

    return layout;
}

const juce::String NovaApexAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaApexAudioProcessor::acceptsMidi() const { return false; }
bool NovaApexAudioProcessor::producesMidi() const { return false; }
bool NovaApexAudioProcessor::isMidiEffect() const { return false; }
double NovaApexAudioProcessor::getTailLengthSeconds() const { return 0.025; }

int NovaApexAudioProcessor::getNumPrograms() { return kNumPresets; }
int NovaApexAudioProcessor::getCurrentProgram() { return currentProgram; }

void NovaApexAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentProgram = index;

    const auto& p = kPresets[index];
    apvts.getParameter (gainId)->setValueNotifyingHost (
        apvts.getParameter (gainId)->convertTo0to1 (p.gain));
    apvts.getParameter (driveId)->setValueNotifyingHost (
        apvts.getParameter (driveId)->convertTo0to1 (p.drive));
    apvts.getParameter (ceilingId)->setValueNotifyingHost (
        apvts.getParameter (ceilingId)->convertTo0to1 (p.ceiling));
    apvts.getParameter (outputGainId)->setValueNotifyingHost (
        apvts.getParameter (outputGainId)->convertTo0to1 (p.outputGain));
    apvts.getParameter (styleId)->setValueNotifyingHost (
        apvts.getParameter (styleId)->convertTo0to1 (static_cast<float> (p.style)));
    apvts.getParameter (attackId)->setValueNotifyingHost (
        apvts.getParameter (attackId)->convertTo0to1 (p.attack));
    apvts.getParameter (releaseId)->setValueNotifyingHost (
        apvts.getParameter (releaseId)->convertTo0to1 (p.release));
    apvts.getParameter (lookaheadId)->setValueNotifyingHost (
        apvts.getParameter (lookaheadId)->convertTo0to1 (p.lookahead));
    apvts.getParameter (oversampleId)->setValueNotifyingHost (
        apvts.getParameter (oversampleId)->convertTo0to1 (static_cast<float> (p.oversample)));
    apvts.getParameter (linkId)->setValueNotifyingHost (p.link ? 1.0f : 0.0f);
    apvts.getParameter (ditherId)->setValueNotifyingHost (p.dither ? 1.0f : 0.0f);
    apvts.getParameter (transientPreserveId)->setValueNotifyingHost (
        apvts.getParameter (transientPreserveId)->convertTo0to1 (p.transientPreserve));
    apvts.getParameter (lowEndProtectId)->setValueNotifyingHost (
        apvts.getParameter (lowEndProtectId)->convertTo0to1 (p.lowEndProtect));
    apvts.getParameter (loudnessTargetId)->setValueNotifyingHost (
        apvts.getParameter (loudnessTargetId)->convertTo0to1 (p.loudnessTarget));
}

const juce::String NovaApexAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < kNumPresets) return kPresets[index].name;
    return {};
}

void NovaApexAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaApexAudioProcessor::resetState() noexcept
{
    gainEnv = { 1.0f, 1.0f };
    for (auto& buf : lookaheadBuf)  std::fill (buf.begin(), buf.end(), 0.0f);
    for (auto& buf : preLimitBuf)   std::fill (buf.begin(), buf.end(), 0.0f);
    lookaheadWritePos = { 0, 0 };
    preLimitWritePos  = { 0, 0 };
    transientFastEnv  = { 0.0f, 0.0f };
    transientSlowEnv  = { 0.0f, 0.0f };
    lowEndLPState     = { 0.0f, 0.0f };
    novaGuardAccum    = 0.0f;
    adaptiveReleaseMul = 1.0f;
    rmsAccum    = 0.0f;
    rmsAccumOut = 0.0f;
    rmsCount    = 0;
    tpHold      = 0.0f;
    inputPeakHoldL = inputPeakHoldR = 0.0f;
    outputPeakHoldL = outputPeakHoldR = 0.0f;
    ditherState = { 0.0f, 0.0f };
}

void NovaApexAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    // Envelope coefficients for transient detector
    transientFastCoeff = makeCoeff (3.0f, sampleRate);
    transientSlowCoeff = makeCoeff (80.0f, sampleRate);

    // Low-end LP: ~80 Hz
    const float lpFreq = 80.0f;
    lowEndLPCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * lpFreq
                              / static_cast<float> (sampleRate));

    // Nova Guard decay: ~2 second window
    novaGuardDecay = std::exp (-1.0f / (static_cast<float> (sampleRate) * 2.0f));

    const float lookaheadMs = apvts.getRawParameterValue (lookaheadId)->load();
    lookaheadDelaySamples   = juce::jmin (
        static_cast<int> (lookaheadMs * 0.001f * static_cast<float> (sampleRate)),
        kMaxLookaheadSamples - 1);

    // Build oversampler for the nonlinear drive stage
    const int osChoice = juce::roundToInt (apvts.getRawParameterValue (oversampleId)->load());
    const int osFactor = (osChoice == 0) ? 1 : (1 << osChoice); // 1,2,4,8
    lastOversampleFactor = osFactor;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, static_cast<size_t> (osChoice == 0 ? 0 : osChoice),
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    oversampler->initProcessing (static_cast<size_t> (samplesPerBlock));

    resetState();
}

void NovaApexAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaApexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return in == out;
}
#endif

float NovaApexAudioProcessor::applyDrive (float x, float driveAmt, int style) noexcept
{
    if (driveAmt < 0.001f) return x;

    if (style == 4) // Analog: full Nova Heat saturation
        return novaHeatSaturate (x, driveAmt);

    // All other styles: subtle harmonic density — partial drive blend
    const float blend = driveAmt * 0.06f;
    return x + blend * (novaHeatSaturate (x, driveAmt * 0.4f) - x);
}

float NovaApexAudioProcessor::lowEndProtectFactor (float sample, int ch, float amount) noexcept
{
    if (amount < 0.001f) return 0.0f;

    // Track low-end energy via LP filter
    lowEndLPState[ch] = lowEndLPCoeff * lowEndLPState[ch]
                        + (1.0f - lowEndLPCoeff) * std::abs (sample);

    // High low-end energy → relax GR; return 0..1 suppression multiplier
    // A value of 1 means "don't reduce GR at all from low-end protect"
    const float lpEnergy = lowEndLPState[ch];
    const float threshold = 0.05f;
    if (lpEnergy < threshold) return 0.0f;

    const float factor = juce::jlimit (0.0f, 1.0f, (lpEnergy - threshold) / 0.3f);
    return factor * amount;
}

float NovaApexAudioProcessor::transientWeight (float sample, int ch, float amount) noexcept
{
    if (amount < 0.001f) return 0.0f;

    const float absVal = std::abs (sample);
    transientFastEnv[ch] = transientFastCoeff * transientFastEnv[ch]
                           + (1.0f - transientFastCoeff) * absVal;
    transientSlowEnv[ch] = transientSlowCoeff * transientSlowEnv[ch]
                           + (1.0f - transientSlowCoeff) * absVal;

    // Transient present when fast > slow by a meaningful margin
    const float ratio = (transientSlowEnv[ch] > 1.0e-6f)
                        ? (transientFastEnv[ch] / transientSlowEnv[ch])
                        : 1.0f;
    const float transientStrength = juce::jlimit (0.0f, 1.0f, (ratio - 1.4f) / 2.0f);
    return transientStrength * amount;
}

float NovaApexAudioProcessor::novaGuardMultiplier() const noexcept
{
    // When guard accumulator is high (lots of GR activity), slow release more
    return 1.0f + novaGuardAccum * 2.5f; // up to 3.5x release time
}

void NovaApexAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numChannels = juce::jmin (2, buffer.getNumChannels());
    const int numSamples  = buffer.getNumSamples();
    if (numSamples == 0) return;

    // Read parameters (once per block)
    const float gainDb           = apvts.getRawParameterValue (gainId)->load();
    const float ceilingDb        = apvts.getRawParameterValue (ceilingId)->load();
    const float outputGainDb     = apvts.getRawParameterValue (outputGainId)->load();
    const float driveVal         = apvts.getRawParameterValue (driveId)->load();
    const int   style            = juce::roundToInt (apvts.getRawParameterValue (styleId)->load());
    const float attackMs         = apvts.getRawParameterValue (attackId)->load();
    const float releaseMs        = apvts.getRawParameterValue (releaseId)->load();
    const float lookaheadMs      = apvts.getRawParameterValue (lookaheadId)->load();
    const int   osChoice         = juce::roundToInt (apvts.getRawParameterValue (oversampleId)->load());
    const bool  link             = apvts.getRawParameterValue (linkId)->load() > 0.5f;
    const float transientAmt     = apvts.getRawParameterValue (transientPreserveId)->load();
    const float lowEndAmt        = apvts.getRawParameterValue (lowEndProtectId)->load();
    const bool  isDelta          = deltaMode.load();
    const int   ditherBitsVal    = ditherBits.load();
    const bool  ispProtectOn     = apvts.getRawParameterValue (ispProtectId)->load() > 0.5f;
    const bool  smartReleaseOn   = apvts.getRawParameterValue (smartReleaseId)->load() > 0.5f;
    const bool  safeModeOn       = apvts.getRawParameterValue (safeModeId)->load() > 0.5f;

    const float inputGainLin  = dbToLinear (gainDb);
    // Safe Mode: pull ceiling in by 0.3 dB — more headroom, less distortion risk
    const float safeCeilingDb = safeModeOn ? ceilingDb - 0.3f : ceilingDb;
    const float ceilingLinear = dbToLinear (safeCeilingDb);
    const float outputGainLin = dbToLinear (outputGainDb);

    // Style-adjusted attack/release
    float effAttackMs  = attackMs;
    float effReleaseMs = releaseMs;
    switch (style)
    {
        case 1: // Punch: slower attack to let transient through
            effAttackMs  = attackMs * 2.5f;
            effReleaseMs = releaseMs * 0.8f;
            break;
        case 2: // Smooth: slower release
            effReleaseMs = releaseMs * 2.0f;
            break;
        case 3: // Loud: fastest everything
            effAttackMs  = attackMs * 0.5f;
            effReleaseMs = releaseMs * 0.4f;
            break;
        case 5: // Master: moderate, true-peak priority
            effAttackMs  = attackMs * 1.2f;
            effReleaseMs = releaseMs * 1.5f;
            break;
        default: break;
    }

    // Nova Guard adaptive release multiplier
    const float ngMul         = novaGuardMultiplier();
    effReleaseMs *= ngMul;

    const float attackCoeff  = makeCoeff (effAttackMs,  currentSampleRate);
    const float releaseCoeff = makeCoeff (effReleaseMs, currentSampleRate);

    // Update lookahead length
    lookaheadDelaySamples = juce::jmin (
        static_cast<int> (lookaheadMs * 0.001f * static_cast<float> (currentSampleRate)),
        kMaxLookaheadSamples - 1);

    // Rebuild oversampler if factor changed
    const int osFactor = (osChoice == 0) ? 1 : (1 << osChoice);
    if (osFactor != lastOversampleFactor)
    {
        lastOversampleFactor = osFactor;
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, static_cast<size_t> (osChoice == 0 ? 0 : osChoice),
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
        oversampler->initProcessing (static_cast<size_t> (currentBlockSize));
    }

    // ── Drive stage with oversampling ──────────────────────────────────
    // Only run oversampled path when drive is active or Analog style
    const bool needsOversample = (driveVal > 0.01f || style == 4) && osChoice > 0;

    if (needsOversample)
    {
        juce::dsp::AudioBlock<float> inputBlock (buffer);
        auto oversampledBlock = oversampler->processSamplesUp (inputBlock);

        const int osLen = static_cast<int> (oversampledBlock.getNumSamples());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = oversampledBlock.getChannelPointer (static_cast<size_t> (ch));
            for (int i = 0; i < osLen; ++i)
            {
                float driven = data[i] * inputGainLin;
                driven = applyDrive (driven, driveVal, style);
                data[i] = driven;
            }
        }
        oversampler->processSamplesDown (inputBlock);
    }
    else
    {
        // No oversampling: apply drive at native rate
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                data[i] = applyDrive (data[i] * inputGainLin, driveVal, style);
            }
        }
    }

    // ── Per-sample limiter with lookahead ─────────────────────────────
    float inPeakL  = 0.0f, inPeakR  = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    float grAccum  = 0.0f;
    float grSqAccum = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Record raw driven samples for linked peak and transient detect
        float samples[2] = { 0.0f, 0.0f };
        for (int ch = 0; ch < numChannels; ++ch)
            samples[ch] = buffer.getSample (ch, i);

        // Linked peak for sidechain
        float linkedPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            linkedPeak = juce::jmax (linkedPeak, std::abs (samples[ch]));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float rawDriven = samples[ch];

            // Store into lookahead + pre-limit buffers
            lookaheadBuf[ch][lookaheadWritePos[ch]] = rawDriven;
            preLimitBuf[ch][preLimitWritePos[ch]]   = rawDriven;

            const int readPos = (lookaheadWritePos[ch] - lookaheadDelaySamples
                                 + kMaxLookaheadSamples) % kMaxLookaheadSamples;
            const float delayed = lookaheadBuf[ch][readPos];

            lookaheadWritePos[ch] = (lookaheadWritePos[ch] + 1) % kMaxLookaheadSamples;
            preLimitWritePos[ch]  = (preLimitWritePos[ch]  + 1) % kMaxLookaheadSamples;

            // Sidechain value (current look-ahead preview)
            const float sidechain = link ? linkedPeak : std::abs (rawDriven);
            const float epsilon   = 1.0e-9f;

            // Compute limiting target gain
            float targetGr = 1.0f;
            if (sidechain > ceilingLinear)
            {
                // Safe Mode: wide soft-knee (6dB knee), capped at 12dB GR
                if (safeModeOn)
                {
                    const float over = sidechain - ceilingLinear;
                    const float knee = ceilingLinear * 0.40f; // ~6dB knee
                    if (over < knee)
                    {
                        const float t = over / knee;
                        targetGr = 1.0f - t * t * (1.0f - ceilingLinear / (sidechain + 1.0e-9f));
                    }
                    else
                    {
                        targetGr = ceilingLinear / (sidechain + 1.0e-9f);
                    }
                    // Cap GR at 12 dB to keep limiting clean
                    targetGr = juce::jmax (targetGr, dbToLinear (-12.0f));
                }
                else switch (style)
                {
                    case 0: // Clean: linear
                        targetGr = ceilingLinear / (sidechain + epsilon);
                        break;
                    case 1: // Punch: slightly harder ceiling
                        targetGr = ceilingLinear / (sidechain * 1.06f + epsilon);
                        break;
                    case 2: // Smooth: geometric (gentler ratio)
                        targetGr = std::sqrt (ceilingLinear / (sidechain + epsilon));
                        break;
                    case 3: // Loud: very aggressive
                        targetGr = ceilingLinear / (sidechain * 1.18f + epsilon);
                        break;
                    case 4: // Analog: soft-knee (~3dB knee width)
                    {
                        const float over = sidechain - ceilingLinear;
                        const float knee = ceilingLinear * 0.15f;
                        if (over < knee)
                        {
                            const float t = over / knee;
                            targetGr = 1.0f - t * t * (1.0f - ceilingLinear / (sidechain + epsilon));
                        }
                        else
                        {
                            targetGr = ceilingLinear / (sidechain + epsilon);
                        }
                        break;
                    }
                    case 5: // Master: very conservative true-peak priority
                        targetGr = (ceilingLinear + (sidechain - ceilingLinear) * 0.04f)
                                   / (sidechain + epsilon);
                        break;
                    default:
                        targetGr = ceilingLinear / (sidechain + epsilon);
                        break;
                }
                targetGr = juce::jmin (1.0f, targetGr);
            }

            // Transient preserve: relax GR on transient attacks
            const float tWeight = transientWeight (rawDriven, ch, transientAmt);
            targetGr = juce::jmin (1.0f, targetGr + (1.0f - targetGr) * tWeight * 0.6f);

            // Low-end protect: reduce GR when bass energy is high
            const float lpFactor = lowEndProtectFactor (rawDriven, ch, lowEndAmt);
            targetGr = juce::jmin (1.0f, targetGr + (1.0f - targetGr) * lpFactor * 0.5f);

            // Smooth gain envelope
            const int envCh = link ? 0 : ch;
            float& env = gainEnv[envCh];
            if (targetGr < env)
            {
                env = attackCoeff * env + (1.0f - attackCoeff) * targetGr;
            }
            else
            {
                // Smart Release: dense transients → faster release (less pumping)
                float relCoeff = releaseCoeff;
                if (smartReleaseOn && tWeight > 0.05f)
                {
                    // High transient density speeds release by up to 4x
                    const float speedup = 1.0f + tWeight * 3.0f;
                    relCoeff = std::pow (releaseCoeff, speedup);
                }
                env = relCoeff * env + (1.0f - relCoeff) * targetGr;
            }

            // Apply gain reduction to delayed sample
            float out = delayed * env;

            // Hard ceiling safety
            out = juce::jlimit (-ceilingLinear, ceilingLinear, out);

            // Output gain
            out *= outputGainLin;

            // Delta: output what the limiter removed
            if (isDelta)
            {
                const int preReadPos = (preLimitWritePos[ch] - lookaheadDelaySamples - 1
                                        + kMaxLookaheadSamples) % kMaxLookaheadSamples;
                const float preSignal = preLimitBuf[ch][preReadPos] * inputGainLin * outputGainLin;
                out = preSignal - out;
            }

            // Dither
            if (ditherBitsVal > 0)
                out += triangleDither (ditherState[ch], ditherBitsVal);

            buffer.setSample (ch, i, out);

            // Accumulate GR for metering / Nova Guard
            const float grDb  = linearToDb (env);
            grAccum  += grDb;
            grSqAccum += grDb * grDb;

            // Metering
            const float inAbs  = std::abs (rawDriven);
            const float outAbs = std::abs (out);
            rmsAccum    += rawDriven * rawDriven;
            rmsAccumOut += out * out;
            ++rmsCount;
            tpHold = juce::jmax (tpHold, outAbs);

            if (ch == 0) { inPeakL = juce::jmax (inPeakL, inAbs); outPeakL = juce::jmax (outPeakL, outAbs); }
            else         { inPeakR = juce::jmax (inPeakR, inAbs); outPeakR = juce::jmax (outPeakR, outAbs); }
        }
    }

    // ── ISP Protection ────────────────────────────────────────────────────
    // Detects intersample peaks using 4-point cubic interpolation between
    // consecutive output samples and attenuates if reconstruction exceeds ceiling.
    if (ispProtectOn && numSamples >= 4)
    {
        const float ispCeiling = dbToLinear (safeCeilingDb);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 1; i < numSamples - 2; ++i)
            {
                // Catmull-Rom cubic: interpolate midpoint between sample[i] and sample[i+1]
                const float y0 = data[i - 1];
                const float y1 = data[i];
                const float y2 = data[i + 1];
                const float y3 = (i + 2 < numSamples) ? data[i + 2] : y2;
                // t=0.5 midpoint
                const float mid = 0.5f * y1
                                + 0.5f * y2
                                - 0.0625f * (y3 - y1)
                                + 0.0625f * (y0 - y2);
                const float absMid = std::abs (mid);
                if (absMid > ispCeiling)
                {
                    const float attenuation = ispCeiling / (absMid + 1.0e-9f);
                    data[i]     *= attenuation;
                    data[i + 1] *= attenuation;
                }
            }
        }
    }

    if (numChannels < 2)
    {
        inPeakR  = inPeakL;
        outPeakR = outPeakL;
    }

    // Nova Guard: accumulate distortion stress (GR variance indicates pumping)
    {
        const int grN = numSamples * numChannels;
        if (grN > 0)
        {
            const float grMean = grAccum / static_cast<float> (grN);
            const float grVar  = (grSqAccum / static_cast<float> (grN)) - grMean * grMean;
            // Variance > threshold means pumping/distortion
            const float stress = juce::jlimit (0.0f, 1.0f, std::sqrt (juce::jmax (0.0f, grVar)) * 0.4f);
            novaGuardAccum = novaGuardDecay * novaGuardAccum + (1.0f - novaGuardDecay) * stress;

            // Adaptive release: ramp multiplier smoothly
            const float targetMul = 1.0f + novaGuardAccum * 2.5f;
            adaptiveReleaseMul = 0.999f * adaptiveReleaseMul + 0.001f * targetMul;
        }
        novaGuardActivity.store (novaGuardAccum);
    }

    // Smoothed peak decay
    constexpr float decayCoeff = 0.9995f;
    inputPeakHoldL  = juce::jmax (inPeakL,  inputPeakHoldL  * decayCoeff);
    inputPeakHoldR  = juce::jmax (inPeakR,  inputPeakHoldR  * decayCoeff);
    outputPeakHoldL = juce::jmax (outPeakL, outputPeakHoldL * decayCoeff);
    outputPeakHoldR = juce::jmax (outPeakR, outputPeakHoldR * decayCoeff);

    inputLevelL.store  (inputPeakHoldL);
    inputLevelR.store  (inputPeakHoldR);
    outputLevelL.store (outputPeakHoldL);
    outputLevelR.store (outputPeakHoldR);

    const float totalSamples = static_cast<float> (numSamples * numChannels);
    gainReductionDb.store (totalSamples > 0 ? grAccum / totalSamples : 0.0f);

    // Rolling LUFS (~400ms integration)
    constexpr int lufsIntegrationSamples = 17640;
    if (rmsCount >= lufsIntegrationSamples)
    {
        const float rmsIn  = std::sqrt (rmsAccum    / static_cast<float> (rmsCount));
        const float rmsOut = std::sqrt (rmsAccumOut / static_cast<float> (rmsCount));
        const float lufs   = linearToDb (rmsOut) - 0.691f;
        lufsEstimate.store (lufs);
        truePeakEstimate.store (linearToDb (tpHold));

        // Smart loudness match: gain difference to bring bypass to same level as output
        const float inLufs = linearToDb (rmsIn) - 0.691f;
        smartMatchGain.store (lufs - inLufs);

        tpHold      = 0.0f;
        rmsAccum    = 0.0f;
        rmsAccumOut = 0.0f;
        rmsCount    = 0;
    }
}

bool NovaApexAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* NovaApexAudioProcessor::createEditor()
{
    return new NovaApexAudioProcessorEditor (*this);
}

void NovaApexAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaApexAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaApexAudioProcessor();
}
