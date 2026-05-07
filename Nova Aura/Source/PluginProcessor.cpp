#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr auto midAuraId = "mid_aura";
    constexpr auto highAuraId = "high_aura";
    constexpr auto mixId = "mix";
    constexpr auto safeId = "safe";
    constexpr auto wideId = "wide";
    constexpr auto lowLatencyId = "low_latency";

    inline float clampUnit (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }
    inline float fastLerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
}

NovaAuraAudioProcessor::NovaAuraAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, juce::Identifier ("NovaAura"), createParameterLayout())
{
}

NovaAuraAudioProcessor::~NovaAuraAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaAuraAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { midAuraId, 1 },
        "Mid Aura",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        42.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { highAuraId, 1 },
        "High Aura",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        56.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mixId, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { safeId, 1 }, "Safe", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { wideId, 1 }, "Wide", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { lowLatencyId, 1 }, "Low Latency", false));

    return layout;
}

const juce::String NovaAuraAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaAuraAudioProcessor::acceptsMidi() const { return false; }
bool NovaAuraAudioProcessor::producesMidi() const { return false; }
bool NovaAuraAudioProcessor::isMidiEffect() const { return false; }
double NovaAuraAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int NovaAuraAudioProcessor::getNumPrograms() { return 1; }
int NovaAuraAudioProcessor::getCurrentProgram() { return 0; }
void NovaAuraAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaAuraAudioProcessor::getProgramName (int) { return {}; }
void NovaAuraAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaAuraAudioProcessor::resetState() noexcept
{
    presenceLpLo = { 0.0f, 0.0f };
    presenceLpHi = { 0.0f, 0.0f };
    airLp = { 0.0f, 0.0f };
    harshEnv = { 0.0f, 0.0f };
    transientFast = { 0.0f, 0.0f };
    transientSlow = { 0.0f, 0.0f };
    outputSmooth = { 0.0f, 0.0f };
}

void NovaAuraAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    oversampler2x.reset();
    oversampler2x.initProcessing (static_cast<size_t> (samplesPerBlock));
    oversampler4x.reset();
    oversampler4x.initProcessing (static_cast<size_t> (samplesPerBlock));
    resetState();
}

void NovaAuraAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaAuraAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return input == output;
}
#endif

void NovaAuraAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalInputChannels = getTotalNumInputChannels();
    const int totalOutputChannels = getTotalNumOutputChannels();

    for (int ch = totalInputChannels; ch < totalOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    const float midAura = apvts.getRawParameterValue (midAuraId)->load() / 100.0f;
    const float highAura = apvts.getRawParameterValue (highAuraId)->load() / 100.0f;
    const float mix = apvts.getRawParameterValue (mixId)->load() / 100.0f;
    const bool safe = apvts.getRawParameterValue (safeId)->load() > 0.5f;
    const bool wide = apvts.getRawParameterValue (wideId)->load() > 0.5f;
    const bool lowLatency = apvts.getRawParameterValue (lowLatencyId)->load() > 0.5f;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();

    const float sr = static_cast<float> (currentSampleRate);
    const float c2k = std::exp (-1.0f / (1.0f / (2.0f * juce::MathConstants<float>::pi * 2000.0f) * sr));
    const float c6k = std::exp (-1.0f / (1.0f / (2.0f * juce::MathConstants<float>::pi * 6000.0f) * sr));
    const float c8k = std::exp (-1.0f / (1.0f / (2.0f * juce::MathConstants<float>::pi * 8000.0f) * sr));
    const float harshAtk = std::exp (-1.0f / (0.0008f * sr));
    const float harshRel = std::exp (-1.0f / (0.045f * sr));
    const float trFastCoeff = std::exp (-1.0f / (0.0012f * sr));
    const float trSlowCoeff = std::exp (-1.0f / (0.020f * sr));

    const float midDb = fastLerp (0.0f, 5.0f, midAura);
    const float highDb = fastLerp (0.0f, 7.0f, highAura);
    const float midGain = juce::Decibels::decibelsToGain (midDb);
    const float highGain = juce::Decibels::decibelsToGain (highDb);

    const float safeReduce = safe ? 0.22f : 0.0f;
    const float deEssStrength = (0.16f + highAura * 0.34f + midAura * 0.1f) * (safe ? 1.35f : 1.0f);
    const float smoothStrength = (0.02f + (midAura + highAura) * 0.05f) + (safe ? 0.06f : 0.0f);

    float inPeakL = 0.0f;
    float inPeakR = 0.0f;
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;
    float auraAccum = 0.0f;
    float harshAccum = 0.0f;

    for (int s = 0; s < samples; ++s)
    {
        float dryL = buffer.getSample (0, s);
        float dryR = channels > 1 ? buffer.getSample (1, s) : dryL;

        float wetBase[2] { 0.0f, 0.0f };
        float enh[2] { 0.0f, 0.0f };

        for (int ch = 0; ch < channels; ++ch)
        {
            const float x = (ch == 0) ? dryL : dryR;
            const float absX = std::abs (x);

            transientFast[ch] = trFastCoeff * transientFast[ch] + (1.0f - trFastCoeff) * absX;
            transientSlow[ch] = trSlowCoeff * transientSlow[ch] + (1.0f - trSlowCoeff) * absX;
            const float transient = clampUnit ((transientFast[ch] - transientSlow[ch]) * 6.0f);

            presenceLpLo[ch] = c2k * presenceLpLo[ch] + (1.0f - c2k) * x;
            presenceLpHi[ch] = c6k * presenceLpHi[ch] + (1.0f - c6k) * x;
            airLp[ch] = c8k * airLp[ch] + (1.0f - c8k) * x;

            const float presenceBand = presenceLpHi[ch] - presenceLpLo[ch];
            const float airBand = x - airLp[ch];

            const float harshSample = std::abs (airBand * 1.1f + presenceBand * 0.28f);
            const float hc = harshSample > harshEnv[ch] ? harshAtk : harshRel;
            harshEnv[ch] = hc * harshEnv[ch] + (1.0f - hc) * harshSample;

            const float harshGate = clampUnit ((harshEnv[ch] - 0.08f) * 6.0f);
            const float dynamicProtect = clampUnit (deEssStrength * harshGate);

            const float midDynamic = 1.0f + midAura * 0.55f + transient * (0.14f + midAura * 0.24f);
            float midEnh = presenceBand * (midGain - 1.0f) * midDynamic;
            midEnh += (std::tanh (presenceBand * (1.2f + midAura * 2.2f)) - presenceBand) * (0.24f + midAura * 0.38f);
            midEnh *= (1.0f - dynamicProtect * 0.52f);

            const float highDynamic = 1.0f + highAura * 0.72f + transient * (0.10f + highAura * 0.12f);
            float airEnh = airBand * (highGain - 1.0f) * highDynamic;
            airEnh += (std::tanh (airBand * (1.0f + highAura * 2.8f)) - airBand) * (0.20f + highAura * 0.42f);
            airEnh *= (1.0f - dynamicProtect * (0.74f + highAura * 0.18f));

            if (safe)
            {
                const float safeSat = std::tanh ((midEnh + airEnh) * 0.9f);
                midEnh = fastLerp (midEnh, safeSat * 0.58f, 0.22f + safeReduce);
                airEnh = fastLerp (airEnh, safeSat * 0.42f, 0.26f + safeReduce);
            }

            const float enhancer = midEnh + airEnh;
            float y = x + enhancer;

            outputSmooth[ch] = 0.82f * outputSmooth[ch] + 0.18f * y;
            y = fastLerp (y, outputSmooth[ch], clampUnit (smoothStrength));

            wetBase[ch] = y;
            enh[ch] = enhancer;

            auraAccum += std::abs (enhancer);
            harshAccum += harshEnv[ch];
        }

        if (channels > 1 && wide)
        {
            const float mid = 0.5f * (enh[0] + enh[1]);
            float side = 0.5f * (enh[0] - enh[1]);
            const float width = 1.0f + midAura * 0.12f + highAura * 0.35f;
            side *= width;

            const float enhL = mid + side;
            const float enhR = mid - side;
            wetBase[0] = dryL + enhL;
            wetBase[1] = dryR + enhR;
        }

        if (lowLatency)
        {
            wetBase[0] = fastLerp (wetBase[0], dryL + enh[0], 0.62f);
            if (channels > 1)
                wetBase[1] = fastLerp (wetBase[1], dryR + enh[1], 0.62f);
        }

        wetBase[0] = std::tanh (wetBase[0] / 1.02f) * 1.02f;
        if (channels > 1)
            wetBase[1] = std::tanh (wetBase[1] / 1.02f) * 1.02f;

        wetBase[0] = fastLerp (dryL, wetBase[0], mix);
        if (channels > 1)
            wetBase[1] = fastLerp (dryR, wetBase[1], mix);

        buffer.setSample (0, s, wetBase[0]);
        if (channels > 1)
            buffer.setSample (1, s, wetBase[1]);

        inPeakL = juce::jmax (inPeakL, std::abs (dryL));
        outPeakL = juce::jmax (outPeakL, std::abs (wetBase[0]));
        if (channels > 1)
        {
            inPeakR = juce::jmax (inPeakR, std::abs (dryR));
            outPeakR = juce::jmax (outPeakR, std::abs (wetBase[1]));
        }
    }

    if (channels < 2)
    {
        inPeakR = inPeakL;
        outPeakR = outPeakL;
    }

    const float denom = static_cast<float> (juce::jmax (1, samples * channels));
    inputPeakL.store (inPeakL);
    inputPeakR.store (inPeakR);
    outputPeakL.store (outPeakL);
    outputPeakR.store (outPeakR);
    auraIntensity.store (juce::jlimit (0.0f, 1.0f, auraAccum / denom * 2.2f));
    harshnessAmount.store (juce::jlimit (0.0f, 1.0f, harshAccum / denom * 1.8f));
}

bool NovaAuraAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NovaAuraAudioProcessor::createEditor() { return new NovaAuraAudioProcessorEditor (*this); }

void NovaAuraAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaAuraAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaAuraAudioProcessor();
}
