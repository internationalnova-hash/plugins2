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

    inline float dbToLinear (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
    inline float linearToDb (float lin) noexcept { return 20.0f * std::log10 (lin + 1.0e-10f); }

    inline float makeAttackCoeff (float ms, double sr) noexcept
    {
        return std::exp (-1.0f / (static_cast<float> (sr) * ms * 0.001f));
    }

    inline float makeReleaseCoeff (float ms, double sr) noexcept
    {
        return std::exp (-1.0f / (static_cast<float> (sr) * ms * 0.001f));
    }

    // Triangular PDF dither, ~0.5 LSB at 24-bit
    inline float triangleDither (float& state) noexcept
    {
        constexpr float amplitude = 1.0f / 16777216.0f; // 2^-24
        const float r1 = static_cast<float> (rand()) / static_cast<float> (RAND_MAX);
        const float r2 = state;
        state = r1;
        return amplitude * (r1 - r2);
    }
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

    return layout;
}

const juce::String NovaApexAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaApexAudioProcessor::acceptsMidi() const { return false; }
bool NovaApexAudioProcessor::producesMidi() const { return false; }
bool NovaApexAudioProcessor::isMidiEffect() const { return false; }
double NovaApexAudioProcessor::getTailLengthSeconds() const { return 0.02; } // 20ms lookahead
int NovaApexAudioProcessor::getNumPrograms() { return 1; }
int NovaApexAudioProcessor::getCurrentProgram() { return 0; }
void NovaApexAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaApexAudioProcessor::getProgramName (int) { return {}; }
void NovaApexAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaApexAudioProcessor::resetState() noexcept
{
    gainEnv = { 1.0f, 1.0f };
    for (auto& buf : lookaheadBuf)
        std::fill (buf.begin(), buf.end(), 0.0f);
    lookaheadWritePos = { 0, 0 };
    rmsAccum = 0.0f;
    rmsCount = 0;
    tpHold = 0.0f;
    inputPeakHoldL = inputPeakHoldR = 0.0f;
    outputPeakHoldL = outputPeakHoldR = 0.0f;
    ditherState = { 0.0f, 0.0f };
}

void NovaApexAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    const float lookaheadMs = apvts.getRawParameterValue (lookaheadId)->load();
    lookaheadDelaySamples = juce::jmin (
        static_cast<int> (lookaheadMs * 0.001f * static_cast<float> (sampleRate)),
        kMaxLookaheadSamples - 1);

    resetState();
}

void NovaApexAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaApexAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input  = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}
#endif

float NovaApexAudioProcessor::processSampleLimiter (float sample, int channel,
                                                     float ceilingLinear, float inputGainLinear,
                                                     float attackCoeff, float releaseCoeff,
                                                     int style, bool link) noexcept
{
    const int envCh = link ? 0 : channel;

    // Apply input gain
    float driven = sample * inputGainLinear;

    // Analog style: tanh saturation before limiting
    if (style == 4)
    {
        const float sat = 1.6f;
        driven = std::tanh (driven * sat) / sat;
    }

    const float epsilon = 1.0e-9f;
    const float absVal  = std::abs (driven);

    // Compute instantaneous target gain reduction
    float targetGr = 1.0f;
    if (absVal > ceilingLinear)
    {
        switch (style)
        {
            case 0: // Clean: linear
                targetGr = ceilingLinear / (absVal + epsilon);
                break;
            case 1: // Punch: harder attack factor
                targetGr = ceilingLinear / (absVal * 1.08f + epsilon);
                break;
            case 2: // Smooth: softer ratio
                targetGr = std::sqrt (ceilingLinear / (absVal + epsilon));
                break;
            case 3: // Loud: more aggressive
                targetGr = ceilingLinear / (absVal * 1.15f + epsilon);
                break;
            case 4: // Analog: soft-knee
            {
                const float over = absVal - ceilingLinear;
                targetGr = (ceilingLinear + over * 0.12f) / (absVal + epsilon);
                break;
            }
            case 5: // Master: very conservative
                targetGr = (ceilingLinear + (absVal - ceilingLinear) * 0.05f) / (absVal + epsilon);
                break;
            default:
                targetGr = ceilingLinear / (absVal + epsilon);
                break;
        }
        targetGr = juce::jmin (1.0f, targetGr);
    }

    // Smooth gain envelope
    float& env = gainEnv[envCh];
    if (targetGr < env)
        env = attackCoeff * env + (1.0f - attackCoeff) * targetGr;
    else
        env = releaseCoeff * env + (1.0f - releaseCoeff) * targetGr;

    // Punch style: slower release on transients — env is already channel-tracked
    return driven * env;
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

    if (buffer.getNumSamples() == 0)
        return;

    // Read parameters
    const float gainDb       = apvts.getRawParameterValue (gainId)->load();
    const float ceilingDb    = apvts.getRawParameterValue (ceilingId)->load();
    const float outputGainDb = apvts.getRawParameterValue (outputGainId)->load();
    const float driveVal     = apvts.getRawParameterValue (driveId)->load();
    const int   style        = juce::roundToInt (apvts.getRawParameterValue (styleId)->load());
    const float attackMs     = apvts.getRawParameterValue (attackId)->load();
    const float releaseMs    = apvts.getRawParameterValue (releaseId)->load();
    const float lookaheadMs  = apvts.getRawParameterValue (lookaheadId)->load();
    const bool  link         = apvts.getRawParameterValue (linkId)->load() > 0.5f;
    const bool  dither       = apvts.getRawParameterValue (ditherId)->load() > 0.5f;

    const float inputGainLin  = dbToLinear (gainDb) * (1.0f + driveVal * 0.1f);
    const float ceilingLinear = dbToLinear (ceilingDb);
    const float outputGainLin = dbToLinear (outputGainDb);

    const float attackCoeff  = makeAttackCoeff (attackMs, currentSampleRate);
    const float releaseCoeff = makeReleaseCoeff (releaseMs, currentSampleRate);

    // Update lookahead delay length
    const int newLookahead = juce::jmin (
        static_cast<int> (lookaheadMs * 0.001f * static_cast<float> (currentSampleRate)),
        kMaxLookaheadSamples - 1);
    lookaheadDelaySamples = newLookahead;

    const int numChannels = juce::jmin (2, buffer.getNumChannels());
    const int numSamples  = buffer.getNumSamples();

    float inPeakL  = 0.0f, inPeakR  = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    float grAccum  = 0.0f;

    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
    {
        // Peak input for linked detection
        float linkedPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            linkedPeak = juce::jmax (linkedPeak, std::abs (buffer.getSample (ch, sampleIdx)));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float rawIn = buffer.getSample (ch, sampleIdx);

            // Write into lookahead buffer
            lookaheadBuf[ch][lookaheadWritePos[ch]] = rawIn;

            // Read from lookahead (delayed output)
            const int readPos = (lookaheadWritePos[ch] - lookaheadDelaySamples
                                 + kMaxLookaheadSamples) % kMaxLookaheadSamples;
            const float delayedSample = lookaheadBuf[ch][readPos];

            lookaheadWritePos[ch] = (lookaheadWritePos[ch] + 1) % kMaxLookaheadSamples;

            // Use the incoming (look-ahead) sample to compute GR, apply to delayed sample
            const float previewVal = link
                ? (linkedPeak * (rawIn >= 0.0f ? 1.0f : -1.0f))
                : rawIn;

            // Compute target GR on preview, apply envelope
            const float absPreview   = std::abs (previewVal * inputGainLin);
            const float epsilon      = 1.0e-9f;
            float targetGr = 1.0f;
            if (absPreview > ceilingLinear)
            {
                switch (style)
                {
                    case 0: targetGr = ceilingLinear / (absPreview + epsilon); break;
                    case 1: targetGr = ceilingLinear / (absPreview * 1.08f + epsilon); break;
                    case 2: targetGr = std::sqrt (ceilingLinear / (absPreview + epsilon)); break;
                    case 3: targetGr = ceilingLinear / (absPreview * 1.15f + epsilon); break;
                    case 4:
                    {
                        const float over = absPreview - ceilingLinear;
                        targetGr = (ceilingLinear + over * 0.12f) / (absPreview + epsilon);
                        break;
                    }
                    case 5: targetGr = (ceilingLinear + (absPreview - ceilingLinear) * 0.05f) / (absPreview + epsilon); break;
                    default: targetGr = ceilingLinear / (absPreview + epsilon); break;
                }
                targetGr = juce::jmin (1.0f, targetGr);
            }

            const int envCh = link ? 0 : ch;
            float& env = gainEnv[envCh];
            if (targetGr < env)
                env = attackCoeff * env + (1.0f - attackCoeff) * targetGr;
            else
                env = releaseCoeff * env + (1.0f - releaseCoeff) * targetGr;

            // Apply: input gain + GR + analog saturation
            float out = delayedSample * inputGainLin * env;

            if (style == 4) // Analog: tanh
            {
                const float sat = 1.4f;
                out = std::tanh (out * sat) / sat;
            }

            // Output gain
            out *= outputGainLin;

            // Hard clip safety
            out = juce::jlimit (-ceilingLinear, ceilingLinear, out);

            // Dither
            if (dither)
                out += triangleDither (ditherState[ch]);

            buffer.setSample (ch, sampleIdx, out);

            // Metering accumulation
            const float inAbs  = std::abs (rawIn);
            const float outAbs = std::abs (out);
            grAccum += juce::jmax (0.0f, linearToDb (env));

            rmsAccum += rawIn * rawIn;
            ++rmsCount;
            tpHold = juce::jmax (tpHold, outAbs);

            if (ch == 0) { inPeakL = juce::jmax (inPeakL, inAbs); outPeakL = juce::jmax (outPeakL, outAbs); }
            else         { inPeakR = juce::jmax (inPeakR, inAbs); outPeakR = juce::jmax (outPeakR, outAbs); }
        }
    }

    if (numChannels < 2)
    {
        inPeakR  = inPeakL;
        outPeakR = outPeakL;
    }

    // Smoothed peak decay
    const float decayCoeff = 0.9995f;
    inputPeakHoldL  = juce::jmax (inPeakL,  inputPeakHoldL  * decayCoeff);
    inputPeakHoldR  = juce::jmax (inPeakR,  inputPeakHoldR  * decayCoeff);
    outputPeakHoldL = juce::jmax (outPeakL, outputPeakHoldL * decayCoeff);
    outputPeakHoldR = juce::jmax (outPeakR, outputPeakHoldR * decayCoeff);

    inputLevelL.store  (inputPeakHoldL);
    inputLevelR.store  (inputPeakHoldR);
    outputLevelL.store (outputPeakHoldL);
    outputLevelR.store (outputPeakHoldR);

    const float avgGr = (numSamples * numChannels) > 0
                        ? grAccum / static_cast<float> (numSamples * numChannels)
                        : 0.0f;
    gainReductionDb.store (juce::jmin (0.0f, avgGr));

    // Rolling LUFS (simple RMS approximation over ~400ms integration)
    constexpr int lufsIntegrationSamples = 17640; // ~400ms @ 44.1kHz
    if (rmsCount >= lufsIntegrationSamples)
    {
        const float rms = std::sqrt (rmsAccum / static_cast<float> (rmsCount));
        const float lufs = linearToDb (rms) - 0.691f; // approximate K-weighting offset
        lufsEstimate.store (lufs);

        truePeakEstimate.store (linearToDb (tpHold));
        tpHold   = 0.0f;
        rmsAccum = 0.0f;
        rmsCount = 0;
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
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaApexAudioProcessor();
}
