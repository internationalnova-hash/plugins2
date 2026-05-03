#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <cmath>

namespace
{
    float normaliseToUnit (float value, float min, float max) noexcept
    {
        if (max <= min)
            return 0.0f;

        return juce::jlimit (0.0f, 1.0f, (value - min) / (max - min));
    }

    float shapeLog (float t, float amount) noexcept
    {
        const float clamped = juce::jlimit (0.0f, 1.0f, t);
        const float safeAmount = juce::jmax (0.01f, amount);
        return std::log1p (clamped * safeAmount) / std::log1p (safeAmount);
    }

    float remapShaped (float value, float inMin, float inMax, float outMin, float outMax, float shapeAmount) noexcept
    {
        const float t = normaliseToUnit (value, inMin, inMax);
        const float shaped = shapeLog (t, shapeAmount);
        return outMin + (outMax - outMin) * shaped;
    }

    float applyGlobalVoicesCurveNorm (int selectedVoices) noexcept
    {
        const float v = static_cast<float> (juce::jlimit (1, 32, selectedVoices));

        if (v <= 4.0f)
            return remapShaped (v, 1.0f, 4.0f, 0.0f, 0.40f, 8.0f);

        if (v <= 12.0f)
            return remapShaped (v, 4.0f, 12.0f, 0.40f, 0.72f, 2.7f);

        if (v <= 24.0f)
            return remapShaped (v, 12.0f, 24.0f, 0.72f, 0.90f, 1.2f);

        return remapShaped (v, 24.0f, 32.0f, 0.90f, 1.0f, 0.7f);
    }

    float applyPresetVoicesCurveNorm (int selectedVoices, int styleIndex) noexcept
    {
        const float v = static_cast<float> (juce::jlimit (1, 32, selectedVoices));
        const float global = applyGlobalVoicesCurveNorm (selectedVoices);

        switch (juce::jlimit (0, 7, styleIndex))
        {
            case 0: // Pop Lead Stack behavior
            {
                if (v <= 8.0f)
                    return global;

                const float aboveEight = normaliseToUnit (v, 8.0f, 32.0f);
                const float capped = 0.60f + 0.25f * std::pow (aboveEight, 1.6f);
                return juce::jmin (global, capped);
            }

            case 1: // R&B Backgrounds
            {
                if (v <= 12.0f)
                    return remapShaped (v, 1.0f, 12.0f, 0.0f, 0.70f, 1.0f);

                return remapShaped (v, 12.0f, 32.0f, 0.70f, 1.0f, 1.3f);
            }

            case 2: // Trap Wide Stack
            {
                if (v <= 4.0f)
                    return remapShaped (v, 1.0f, 4.0f, 0.0f, 0.45f, 4.0f);

                if (v <= 8.0f)
                    return remapShaped (v, 4.0f, 8.0f, 0.45f, 0.78f, 7.0f);

                return remapShaped (v, 8.0f, 32.0f, 0.78f, 1.0f, 0.25f);
            }

            case 3: // Gospel Lift
            {
                if (v <= 12.0f)
                    return remapShaped (v, 1.0f, 12.0f, 0.0f, 0.45f, 1.8f);

                if (v <= 24.0f)
                    return remapShaped (v, 12.0f, 24.0f, 0.45f, 0.88f, 4.0f);

                return remapShaped (v, 24.0f, 32.0f, 0.88f, 1.0f, 1.0f);
            }

            case 6: // Ambient Cloud
            {
                return juce::jlimit (0.0f, 1.0f, std::pow (global, 0.72f));
            }

            default:
                return global;
        }
    }

    float applyGlobalWidthCurveNorm (float widthPercent) noexcept
    {
        const float w = juce::jlimit (0.0f, 200.0f, widthPercent);

        if (w <= 40.0f)
            return remapShaped (w, 0.0f, 40.0f, 0.0f, 0.40f, 1.2f);

        if (w <= 80.0f)
            return remapShaped (w, 40.0f, 80.0f, 0.40f, 0.92f, 4.0f);

        if (w <= 120.0f)
            return remapShaped (w, 80.0f, 120.0f, 0.92f, 1.35f, 3.0f);

        return remapShaped (w, 120.0f, 200.0f, 1.35f, 1.62f, 0.45f);
    }

    float applyPresetWidthCurveNorm (float widthPercent, int styleIndex) noexcept
    {
        const float w = juce::jlimit (0.0f, 200.0f, widthPercent);
        float curved = applyGlobalWidthCurveNorm (w);

        switch (juce::jlimit (0, 7, styleIndex))
        {
            case 0: // Pop: fast 30-80, slow after
                curved = w <= 80.0f ? remapShaped (w, 0.0f, 80.0f, 0.0f, 1.02f, 4.6f)
                                   : remapShaped (w, 80.0f, 200.0f, 1.02f, 1.55f, 0.55f);
                break;

            case 1: // R&B: starts wider
                curved = juce::jmax (0.62f, curved);
                break;

            case 2: // Trap: aggressive push
                curved = juce::jlimit (0.0f, 2.0f, 0.10f + (curved * 1.22f));
                break;

            case 3: // Gospel: wide default
                curved = juce::jlimit (0.0f, 2.0f, curved + 0.15f);
                break;

            case 6: // Ambient: always wide, push toward max
                curved = juce::jlimit (0.0f, 2.0f, 1.15f + 0.78f * normaliseToUnit (curved, 0.0f, 2.0f));
                break;

            default:
                break;
        }

        return juce::jlimit (0.0f, 2.0f, curved);
    }

    float applyGlobalHumanizeCurveNorm (float humanizePercent) noexcept
    {
        const float h = juce::jlimit (0.0f, 100.0f, humanizePercent);

        if (h <= 20.0f)
            return remapShaped (h, 0.0f, 20.0f, 0.0f, 0.14f, 0.9f);

        if (h <= 50.0f)
            return remapShaped (h, 20.0f, 50.0f, 0.14f, 0.58f, 4.6f);

        if (h <= 80.0f)
            return remapShaped (h, 50.0f, 80.0f, 0.58f, 0.85f, 2.2f);

        return remapShaped (h, 80.0f, 100.0f, 0.85f, 1.0f, 0.8f);
    }

    float applyPresetHumanizeCurveNorm (float humanizePercent, int styleIndex) noexcept
    {
        const float h = juce::jlimit (0.0f, 100.0f, humanizePercent);
        float curved = applyGlobalHumanizeCurveNorm (h);

        switch (juce::jlimit (0, 7, styleIndex))
        {
            case 0: // Pop: very compressed, max effective ~35%
                curved = juce::jmin (curved, 0.35f);
                break;

            case 1: // R&B: expanded 30-60
                if (h >= 30.0f && h <= 60.0f)
                {
                    const float mid = normaliseToUnit (h, 30.0f, 60.0f);
                    curved += 0.12f * std::sin (mid * juce::MathConstants<float>::pi);
                }
                break;

            case 2: // Trap: low sensitivity
                curved = juce::jmin (0.52f, curved * 0.70f);
                break;

            case 3: // Gospel: responsive 40-80
                if (h >= 40.0f && h <= 80.0f)
                    curved += 0.18f * normaliseToUnit (h, 40.0f, 80.0f);
                break;

            case 6: // Ambient: high sensitivity
                curved = juce::jlimit (0.0f, 1.0f, std::pow (curved, 0.72f));
                break;

            default:
                break;
        }

        return juce::jlimit (0.0f, 1.0f, curved);
    }

    float applyGlobalMixCurveNorm (float mixPercent) noexcept
    {
        const float m = juce::jlimit (0.0f, 100.0f, mixPercent);

        if (m <= 30.0f)
            return remapShaped (m, 0.0f, 30.0f, 0.0f, 0.22f, 1.0f);

        if (m <= 70.0f)
            return remapShaped (m, 30.0f, 70.0f, 0.22f, 0.78f, 4.2f);

        return remapShaped (m, 70.0f, 100.0f, 0.78f, 0.94f, 0.45f);
    }

    struct StyleProfile
    {
        juce::Array<int> intervals;
        float widthBias;
        float humanizeBias;
        float toneBias;
        float textureBias;
    };

    StyleProfile getStyleProfile (int styleIndex)
    {
        switch (juce::jlimit (0, 7, styleIndex))
        {
            case 1: return { juce::Array<int> { 0, 3, -3, 9 }, 1.15f, 1.05f, -0.08f, 0.12f }; // R&B
            case 2: return { juce::Array<int> { 0, -12, 12, 7 }, 1.30f, 0.92f, 0.06f, 0.10f }; // Trap
            case 3: return { juce::Array<int> { 4, 7, 12, 9, 11 }, 1.18f, 1.20f, 0.02f, 0.18f }; // Gospel
            case 4: return { juce::Array<int> { 0, 4, 12, 7 }, 1.12f, 1.00f, 0.03f, 0.08f }; // Latin
            case 5: return { juce::Array<int> { 0, 4, 7, 12, 9 }, 1.35f, 1.25f, -0.02f, 0.28f }; // Choir
            case 6: return { juce::Array<int> { 12, 7, 5 }, 1.50f, 1.35f, -0.16f, 0.35f }; // Ambient
            case 7: return { juce::Array<int> { 0, 0, 0, 12 }, 1.22f, 0.85f, 0.0f, 0.05f }; // Wide Doubles
            default: return { juce::Array<int> { 0, 12, 4, 7 }, 1.0f, 1.0f, 0.0f, 0.10f }; // Pop
        }
    }

    float centsToRatio (float cents) noexcept
    {
        return std::pow (2.0f, cents / 1200.0f);
    }

    float saturateSample (float sample) noexcept
    {
        return std::tanh (sample * 1.4f);
    }
}

NovaHarmonyAudioProcessor::NovaHarmonyAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaHarmony"), createParameterLayout())
{
}

NovaHarmonyAudioProcessor::~NovaHarmonyAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaHarmonyAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParameterIDs::voices, 1 },
        "Voices",
        1,
        32,
        8));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::width, 1 },
        "Width",
        juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f),
        75.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::humanize, 1 },
        "Humanize",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        35.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        60.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::style, 1 },
        "Style",
        juce::StringArray {
            "Pop Stack",
            "R&B Stack",
            "Trap Stack",
            "Gospel Stack",
            "Latin Stack",
            "Choir",
            "Ambient",
            "Wide Doubles"
        },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::tone, 1 },
        "Tone",
        juce::StringArray { "Dark", "Natural", "Bright" },
        1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::keyMode, 1 },
        "Key Mode",
        juce::StringArray { "Auto", "Major", "Minor", "Chromatic" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::keyNote, 1 },
        "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
        0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::lowLatency, 1 },
        "Low Latency",
        false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::qualityMode, 1 },
        "Quality Mode",
        juce::StringArray { "Preview", "Process" },
        0));

    return layout;
}

const juce::String NovaHarmonyAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaHarmonyAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NovaHarmonyAudioProcessor::producesMidi() const
{
    return false;
}

bool NovaHarmonyAudioProcessor::isMidiEffect() const
{
    return false;
}

double NovaHarmonyAudioProcessor::getTailLengthSeconds() const
{
    return 0.1;
}

int NovaHarmonyAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaHarmonyAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaHarmonyAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NovaHarmonyAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NovaHarmonyAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaHarmonyAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));

    toneLowMidCut.prepare (spec);
    tonePresence.prepare (spec);
    toneHighShelf.prepare (spec);
    mudCut.prepare (spec);

    toneLowMidCut.reset();
    tonePresence.reset();
    toneHighShelf.reset();
    mudCut.reset();

    delayBuffer.setSize (2, maxDelaySamples, false, true, true);
    delayBuffer.clear();
    delayWritePosition = 0;

    dryBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), samplesPerBlock, false, true, true);
    dryBuffer.clear();

    toneState = {};

    for (auto& voice : voiceStates)
    {
        voice.phaseA = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
        voice.phaseB = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
        voice.gainJitter = juce::Random::getSystemRandom().nextFloat();
    }

    sampleCounter = 0;
    inputEnvelope = 0.0f;
    zeroCrossings = 0;
    keyWindowSamples = 0;
    previousDetectorSample = 0.0f;
    detectedKeyNote = 0.0f;
    peakHold = 0.0f;
    outputPeakLevel.store (0.0f);
    outputIsHot.store (false);
}

void NovaHarmonyAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaHarmonyAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void NovaHarmonyAudioProcessor::updateKeyDetection (float monoSample) noexcept
{
    if ((previousDetectorSample < 0.0f && monoSample >= 0.0f)
        || (previousDetectorSample > 0.0f && monoSample <= 0.0f))
    {
        ++zeroCrossings;
    }

    previousDetectorSample = monoSample;
    ++keyWindowSamples;

    const int analysisWindow = static_cast<int> (currentSampleRate * 0.12);
    if (keyWindowSamples < juce::jmax (256, analysisWindow))
        return;

    const float duration = static_cast<float> (keyWindowSamples) / static_cast<float> (currentSampleRate);
    const float estimatedHz = (0.5f * static_cast<float> (zeroCrossings)) / juce::jmax (0.0001f, duration);

    zeroCrossings = 0;
    keyWindowSamples = 0;

    if (estimatedHz < 40.0f || estimatedHz > 1200.0f)
        return;

    const float midi = 69.0f + 12.0f * std::log2 (estimatedHz / 440.0f);
    const float wrapped = std::fmod (midi + 120.0f, 12.0f);
    detectedKeyNote = 0.965f * detectedKeyNote + 0.035f * wrapped;
}

float NovaHarmonyAudioProcessor::getDetectedKeyNote() const noexcept
{
    return detectedKeyNote;
}

int NovaHarmonyAudioProcessor::getActiveVoiceCount (int selectedVoices, int styleIndex, bool lowLatencyOn) const noexcept
{
    const float voiceCurveNorm = applyPresetVoicesCurveNorm (selectedVoices, styleIndex);
    const int curvedVoices = juce::jlimit (1, 32, static_cast<int> (std::round (1.0f + voiceCurveNorm * 31.0f)));
    return lowLatencyOn ? juce::jlimit (1, 12, curvedVoices) : curvedVoices;
}

void NovaHarmonyAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    dryBuffer.makeCopyOf (buffer, true);

    const auto styleIndex = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::style)->load());
    const auto toneIndex = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::tone)->load());
    const auto keyMode = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::keyMode)->load());
    const auto keyNote = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::keyNote)->load());
    const auto qualityMode = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::qualityMode)->load());
    const auto lowLatencyOn = apvts.getRawParameterValue (ParameterIDs::lowLatency)->load() > 0.5f;

    const auto requestedVoices = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::voices)->load());
    const float rawWidthPercent = apvts.getRawParameterValue (ParameterIDs::width)->load();
    const float rawHumanizePercent = apvts.getRawParameterValue (ParameterIDs::humanize)->load();
    const float rawMixPercent = apvts.getRawParameterValue (ParameterIDs::mix)->load();

    const float voiceCurveNorm = applyPresetVoicesCurveNorm (requestedVoices, styleIndex);
    const auto activeVoices = getActiveVoiceCount (requestedVoices, styleIndex, lowLatencyOn);

    float widthNorm = applyPresetWidthCurveNorm (rawWidthPercent, styleIndex);
    float humanizeNorm = applyPresetHumanizeCurveNorm (rawHumanizePercent, styleIndex);
    const float mixNorm = applyGlobalMixCurveNorm (rawMixPercent);

    // Secret-sauce interactions: denser stacks spread and randomize more, style-tuned.
    float voicesToWidth = 0.12f;
    float voicesToHumanize = 0.06f;
    switch (juce::jlimit (0, 7, styleIndex))
    {
        case 0: voicesToWidth = 0.18f; voicesToHumanize = 0.03f; break; // Pop
        case 1: voicesToWidth = 0.14f; voicesToHumanize = 0.09f; break; // R&B
        case 2: voicesToWidth = 0.20f; voicesToHumanize = 0.02f; break; // Trap
        case 3: voicesToWidth = 0.16f; voicesToHumanize = 0.12f; break; // Gospel
        case 6: voicesToWidth = 0.24f; voicesToHumanize = 0.18f; break; // Ambient
        default: break;
    }
    widthNorm = juce::jlimit (0.0f, 2.0f, widthNorm + voicesToWidth * voiceCurveNorm);
    humanizeNorm = juce::jlimit (0.0f, 1.0f, humanizeNorm + voicesToHumanize * voiceCurveNorm);

    const auto styleProfile = getStyleProfile (styleIndex);
    const float styleHumanize = juce::jlimit (0.65f, 1.45f, styleProfile.humanizeBias);
    const float styleWidth = juce::jlimit (0.70f, 1.45f, styleProfile.widthBias);

    const float gainCompensation = 1.0f / std::sqrt (static_cast<float> (juce::jmax (1, activeVoices)));

    float toneLowMidDb = 0.0f;
    float tonePresenceDb = 0.0f;
    float toneHighDb = 0.0f;

    if (toneIndex == 0)
    {
        toneLowMidDb = -1.5f;
        toneHighDb = -3.0f;
    }
    else if (toneIndex == 2)
    {
        toneLowMidDb = -1.2f;
        tonePresenceDb = 1.8f;
        toneHighDb = 3.2f;
    }

    toneLowMidDb += styleProfile.toneBias * 2.5f;
    toneHighDb += styleProfile.toneBias * 1.7f;

    const float mudDrive = juce::jlimit (0.0f, 1.0f, (static_cast<float> (activeVoices) - 8.0f) / 24.0f);
    const float mudCutDb = -1.2f - (mudDrive * 4.0f);

    *toneLowMidCut.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        currentSampleRate, 300.0f, 0.85f, juce::Decibels::decibelsToGain (toneLowMidDb));

    *tonePresence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        currentSampleRate, 4100.0f, 0.90f, juce::Decibels::decibelsToGain (tonePresenceDb));

    *toneHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        currentSampleRate, 6200.0f, 0.70f, juce::Decibels::decibelsToGain (toneHighDb));

    *mudCut.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        currentSampleRate, 280.0f, 0.72f, juce::Decibels::decibelsToGain (mudCutDb));

    const float detectedNote = keyMode == 0 ? getDetectedKeyNote() : static_cast<float> (keyNote);
    const float scaleBias = keyMode == 2 ? -0.02f : (keyMode == 1 ? 0.02f : 0.0f);

    auto hpStep = [this] (float input, float cutoff, float& state)
    {
        const float a = std::exp (-2.0f * juce::MathConstants<float>::pi * cutoff / static_cast<float> (currentSampleRate));
        state = (a * state) + ((1.0f - a) * input);
        return input - state;
    };

    juce::AudioBuffer<float> harmonyBuffer;
    harmonyBuffer.setSize (2, buffer.getNumSamples(), false, false, true);
    harmonyBuffer.clear();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float inL = dryBuffer.getSample (0, sample);
        const float inR = dryBuffer.getNumChannels() > 1 ? dryBuffer.getSample (1, sample) : inL;
        const float mono = 0.5f * (inL + inR);

        inputEnvelope = 0.995f * inputEnvelope + 0.005f * std::abs (mono);
        updateKeyDetection (mono);

        delayBuffer.setSample (0, delayWritePosition, inL);
        delayBuffer.setSample (1, delayWritePosition, inR);

        float coreL = 0.0f, coreR = 0.0f;
        float supportL = 0.0f, supportR = 0.0f;
        float textureL = 0.0f, textureR = 0.0f;

        const float keyOffset = (detectedNote / 12.0f) + scaleBias;
        const float qualityBoost = qualityMode == 1 ? 1.0f : 0.6f;

        for (int voiceIndex = 0; voiceIndex < activeVoices; ++voiceIndex)
        {
            const bool isCore = voiceIndex < 4;
            const bool isSupport = voiceIndex >= 4 && voiceIndex < 16;
            const bool isTexture = !isCore && !isSupport;

            if (lowLatencyOn && isTexture)
                continue;

            auto& voice = voiceStates[static_cast<size_t> (voiceIndex)];

            const int interval = styleProfile.intervals[voiceIndex % styleProfile.intervals.size()];
            const float intervalWeight = 1.0f + 0.03f * static_cast<float> (interval);

            const float groupHumanizeScale = isCore ? 0.32f : (isSupport ? 0.72f : 1.15f);
            const float groupWidthScale = isCore ? 0.20f : (isSupport ? 0.62f : 1.0f);
            const float groupTimingBaseMs = isCore ? 3.0f : (isSupport ? 12.0f : 26.0f);

            const float humanizeAmount = humanizeNorm * styleHumanize * groupHumanizeScale;
            const float widthAmount = juce::jlimit (0.0f, 2.0f, widthNorm * styleWidth * groupWidthScale);

            const float voiceStepA = 0.0006f + 0.00012f * static_cast<float> ((voiceIndex % 7) + 1);
            const float voiceStepB = 0.0011f + 0.00013f * static_cast<float> ((voiceIndex % 11) + 1);
            voice.phaseA += voiceStepA;
            voice.phaseB += voiceStepB;

            if (voice.phaseA > juce::MathConstants<float>::twoPi)
                voice.phaseA -= juce::MathConstants<float>::twoPi;
            if (voice.phaseB > juce::MathConstants<float>::twoPi)
                voice.phaseB -= juce::MathConstants<float>::twoPi;

            const float phaseMix = 0.7f * std::sin (voice.phaseA + keyOffset)
                                 + 0.3f * std::sin (voice.phaseB * intervalWeight);

            const float pitchCents = (isCore ? 5.0f : (isSupport ? 12.0f : 24.0f)) * humanizeAmount * phaseMix;
            const float pitchRatio = centsToRatio (pitchCents);
            const float microDelayMs = (widthAmount > 1.0f ? (widthAmount - 1.0f) * 12.0f : 0.0f) * (isCore ? 0.25f : (isSupport ? 0.6f : 1.0f));
            const float timingMs = groupTimingBaseMs + 14.0f * humanizeAmount * (0.5f + 0.5f * std::abs (phaseMix));
            const float baseDelayMs = timingMs + microDelayMs + static_cast<float> (std::abs (interval)) * 0.28f;

            const int delaySamples = juce::jlimit (0, maxDelaySamples - 2,
                                                   static_cast<int> ((baseDelayMs * 0.001f * static_cast<float> (currentSampleRate))
                                                                      * (1.0f / pitchRatio)));

            int readPos = delayWritePosition - delaySamples;
            while (readPos < 0)
                readPos += maxDelaySamples;
            readPos %= maxDelaySamples;

            float voiceL = delayBuffer.getSample (0, readPos);
            float voiceR = delayBuffer.getSample (1, readPos);

            const float panBase = (static_cast<float> (voiceIndex) / static_cast<float> (juce::jmax (1, activeVoices - 1))) * 2.0f - 1.0f;
            const float pan = juce::jlimit (-1.0f, 1.0f, panBase * widthAmount);
            const float leftPan = std::sqrt (0.5f * (1.0f - pan));
            const float rightPan = std::sqrt (0.5f * (1.0f + pan));

            const float dynamicJitter = 0.85f + 0.35f * voice.gainJitter;
            const float textureThin = isTexture ? juce::jlimit (0.6f, 1.0f, 1.0f - 0.3f * mudDrive) : 1.0f;
            const float perVoiceGain = gainCompensation * dynamicJitter * textureThin * qualityBoost;

            voiceL *= perVoiceGain * leftPan;
            voiceR *= perVoiceGain * rightPan;

            if (isCore)
            {
                coreL += voiceL;
                coreR += voiceR;
            }
            else if (isSupport)
            {
                supportL += voiceL;
                supportR += voiceR;
            }
            else
            {
                textureL += voiceL;
                textureR += voiceR;
            }
        }

        coreL = hpStep (coreL, 90.0f, toneState.hpCoreL);
        coreR = hpStep (coreR, 90.0f, toneState.hpCoreR);
        supportL = hpStep (supportL, 120.0f, toneState.hpSupportL);
        supportR = hpStep (supportR, 120.0f, toneState.hpSupportR);

        const float textureHp = activeVoices >= 24 ? 180.0f : 150.0f;
        textureL = hpStep (textureL, textureHp, toneState.hpTextureL);
        textureR = hpStep (textureR, textureHp, toneState.hpTextureR);

        harmonyBuffer.setSample (0, sample, coreL + supportL + textureL);
        harmonyBuffer.setSample (1, sample, coreR + supportR + textureR);

        delayWritePosition = (delayWritePosition + 1) % maxDelaySamples;
        ++sampleCounter;
    }

    juce::dsp::AudioBlock<float> harmonyBlock (harmonyBuffer);
    juce::dsp::ProcessContextReplacing<float> harmonyContext (harmonyBlock);

    toneLowMidCut.process (harmonyContext);
    tonePresence.process (harmonyContext);
    toneHighShelf.process (harmonyContext);
    mudCut.process (harmonyContext);

    // Higher wet mix applies slight auto trim to keep dry/wet balance musical.
    const float mixAutoTrimDb = -(1.8f * std::pow (mixNorm, 1.12f))
                              - (mixNorm > 0.7f ? (mixNorm - 0.7f) * 4.0f : 0.0f);
    const float harmonyLimiter = juce::Decibels::decibelsToGain (mixAutoTrimDb);

    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* output = buffer.getWritePointer (channel);
        const auto* dry = dryBuffer.getReadPointer (channel);
        const auto* wet = harmonyBuffer.getReadPointer (juce::jmin (1, channel));

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float harmony = wet[sample] * harmonyLimiter;
            harmony = saturateSample (harmony);
            const float out = (dry[sample] * (1.0f - mixNorm)) + (harmony * mixNorm);
            output[sample] = out;
            peak = juce::jmax (peak, std::abs (out));
        }
    }

    peakHold = peak > peakHold ? peak : peakHold * 0.94f;
    outputPeakLevel.store (juce::jlimit (0.0f, 1.0f, peakHold));
    outputIsHot.store (peak >= 0.98f);
}

bool NovaHarmonyAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaHarmonyAudioProcessor::createEditor()
{
    return new NovaHarmonyAudioProcessorEditor (*this);
}

void NovaHarmonyAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaHarmonyAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaHarmonyAudioProcessor();
}
