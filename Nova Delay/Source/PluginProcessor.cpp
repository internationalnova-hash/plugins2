#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <array>
#include <cmath>

namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;

    struct ModeProfile
    {
        float minHpHz;
        float maxLpHz;
        float saturationScale;
        float wowScale;
        float flutterScale;
        float hissAmount;
        float clockGritAmount;
        float hfLossPerRepeat;
        float timingSmearSamples;
        float stereoWidthScale;
        float feedbackCompression;
        float feedbackDarken;
    };

    float hzToOnePoleCoeff (float cutoffHz, float sampleRate) noexcept
    {
        const auto safeCutoff = juce::jlimit (10.0f, 20000.0f, cutoffHz);
        const auto omega = 2.0f * kPi * safeCutoff / juce::jmax (1.0f, sampleRate);
        return 1.0f - std::exp (-omega);
    }

    float clampSafeFeedback (float feedbackPercent) noexcept
    {
        return juce::jlimit (0.0f, 95.0f, feedbackPercent) / 100.0f;
    }

    float softClip (float x) noexcept
    {
        return std::tanh (x);
    }

    float modeWeight (float modeValue, float center) noexcept
    {
        return juce::jmax (0.0f, 1.0f - std::abs (modeValue - center));
    }

    ModeProfile getBlendedModeProfile (float modeValue) noexcept
    {
        constexpr ModeProfile analog {
            80.0f, 12000.0f, 0.65f, 0.60f, 0.55f,
            0.00002f, 0.0f, 0.06f, 0.35f, 0.85f,
            0.08f, 0.12f
        };

        constexpr ModeProfile tape {
            100.0f, 8000.0f, 1.00f, 1.20f, 1.35f,
            0.00022f, 0.00012f, 0.25f, 2.20f, 0.95f,
            0.16f, 0.28f
        };

        constexpr ModeProfile bbd {
            150.0f, 4500.0f, 1.12f, 0.80f, 0.85f,
            0.00042f, 0.00048f, 0.40f, 1.30f, 0.62f,
            0.30f, 0.42f
        };

        const float wAnalog = modeWeight (modeValue, 0.0f);
        const float wTape = modeWeight (modeValue, 1.0f);
        const float wBbd = modeWeight (modeValue, 2.0f);
        const float wSum = juce::jmax (0.0001f, wAnalog + wTape + wBbd);

        auto blend = [wAnalog, wTape, wBbd, wSum] (float a, float t, float b) noexcept
        {
            return ((a * wAnalog) + (t * wTape) + (b * wBbd)) / wSum;
        };

        return ModeProfile {
            blend (analog.minHpHz, tape.minHpHz, bbd.minHpHz),
            blend (analog.maxLpHz, tape.maxLpHz, bbd.maxLpHz),
            blend (analog.saturationScale, tape.saturationScale, bbd.saturationScale),
            blend (analog.wowScale, tape.wowScale, bbd.wowScale),
            blend (analog.flutterScale, tape.flutterScale, bbd.flutterScale),
            blend (analog.hissAmount, tape.hissAmount, bbd.hissAmount),
            blend (analog.clockGritAmount, tape.clockGritAmount, bbd.clockGritAmount),
            blend (analog.hfLossPerRepeat, tape.hfLossPerRepeat, bbd.hfLossPerRepeat),
            blend (analog.timingSmearSamples, tape.timingSmearSamples, bbd.timingSmearSamples),
            blend (analog.stereoWidthScale, tape.stereoWidthScale, bbd.stereoWidthScale),
            blend (analog.feedbackCompression, tape.feedbackCompression, bbd.feedbackCompression),
            blend (analog.feedbackDarken, tape.feedbackDarken, bbd.feedbackDarken)
        };
    }
}

NovaDelayAudioProcessor::NovaDelayAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaDelay"), createParameterLayout())
{
}

NovaDelayAudioProcessor::~NovaDelayAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::preset, 1 },
        "Preset",
        juce::StringArray { "Vintage Echo", "Modern Tape", "BBD Lo-Fi", "Dub Space" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::delayTimeSync, 1 },
        "Delay Time Sync",
        juce::StringArray { "1/32", "1/16", "1/8", "1/8D", "1/8T", "1/4", "1/4D", "1/4T", "1/2", "1/1" },
        5));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::delayTimeFreeMs, 1 },
        "Delay Time Free",
        juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f),
        500.0f,
        "ms",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 0) + " ms"; }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::syncEnabled, 1 },
        "Sync",
        true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::feedback, 1 },
        "Feedback",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        35.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        40.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::tone, 1 },
        "Tone",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        55.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::wowFlutter, 1 },
        "Wow Flutter",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        25.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::saturation, 1 },
        "Saturation",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        30.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::mode, 1 },
        "Mode",
        juce::StringArray { "Analog", "Tape", "BBD" },
        1));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::pingPong, 1 },
        "Ping Pong",
        false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::stereo, 1 },
        "Stereo",
        true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::lofi, 1 },
        "Lo-Fi",
        false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::freeze, 1 },
        "Freeze",
        false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::hpFilterHz, 1 },
        "HP Filter",
        juce::NormalisableRange<float> (20.0f, 800.0f, 1.0f, 0.45f),
        120.0f,
        "Hz"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::lpFilterHz, 1 },
        "LP Filter",
        juce::NormalisableRange<float> (1000.0f, 18000.0f, 1.0f, 0.38f),
        6000.0f,
        "Hz"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::ducking, 1 },
        "Ducking",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        20.0f,
        "%"));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::delayModel, 1 },
        "Delay Model",
        juce::StringArray { "Vintage Tape", "Warm Bucket", "Modern Analog", "Echo Chamber" },
        0));

    return layout;
}

const juce::String NovaDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaDelayAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NovaDelayAudioProcessor::producesMidi() const
{
    return false;
}

bool NovaDelayAudioProcessor::isMidiEffect() const
{
    return false;
}

double NovaDelayAudioProcessor::getTailLengthSeconds() const
{
    return 4.0;
}

int NovaDelayAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaDelayAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NovaDelayAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NovaDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    const auto maxDelaySamples = static_cast<int> (std::ceil (currentSampleRate * 2.5));
    delayBufferSize = juce::jmax (maxDelaySamples, samplesPerBlock * 2);
    delayBuffer.setSize (2, delayBufferSize, false, true, true);
    delayBuffer.clear();
    writePosition = 0;

    delayTimeMsSmoothed.reset (currentSampleRate, 0.05);
    feedbackSmoothed.reset (currentSampleRate, 0.03);
    mixSmoothed.reset (currentSampleRate, 0.03);
    toneSmoothed.reset (currentSampleRate, 0.05);
    wowFlutterSmoothed.reset (currentSampleRate, 0.05);
    saturationSmoothed.reset (currentSampleRate, 0.04);
    hpSmoothed.reset (currentSampleRate, 0.05);
    lpSmoothed.reset (currentSampleRate, 0.05);
    duckingSmoothed.reset (currentSampleRate, 0.03);
    freezeMixSmoothed.reset (currentSampleRate, 0.09);
    modeBlendSmoothed.reset (currentSampleRate, 0.12);

    delayTimeMsSmoothed.setCurrentAndTargetValue (500.0f);
    feedbackSmoothed.setCurrentAndTargetValue (0.35f);
    mixSmoothed.setCurrentAndTargetValue (0.40f);
    toneSmoothed.setCurrentAndTargetValue (0.55f);
    wowFlutterSmoothed.setCurrentAndTargetValue (0.25f);
    saturationSmoothed.setCurrentAndTargetValue (0.30f);
    hpSmoothed.setCurrentAndTargetValue (120.0f);
    lpSmoothed.setCurrentAndTargetValue (6000.0f);
    duckingSmoothed.setCurrentAndTargetValue (0.20f);
    freezeMixSmoothed.setCurrentAndTargetValue (0.0f);
    modeBlendSmoothed.setCurrentAndTargetValue (1.0f);

    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    duckEnvelope = 0.0f;
    inputPeakHoldL = 0.0f;
    inputPeakHoldR = 0.0f;
    outputPeakHoldL = 0.0f;
    outputPeakHoldR = 0.0f;
    inputPeakHold = 0.0f;
    outputPeakHold = 0.0f;
    inputPeakLevelL.store (0.0f);
    inputPeakLevelR.store (0.0f);
    outputPeakLevelL.store (0.0f);
    outputPeakLevelR.store (0.0f);
    inputPeakLevel.store (0.0f);
    outputPeakLevel.store (0.0f);
    outputIsHot.store (false);

    for (auto& state : delayStates)
        state = {};
}

void NovaDelayAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

double NovaDelayAudioProcessor::getHostBpm() const noexcept
{
    if (auto* playhead = getPlayHead())
    {
        if (auto position = playhead->getPosition())
        {
            if (auto bpm = position->getBpm())
                return juce::jlimit (30.0, 240.0, *bpm);
        }
    }

    return 120.0;
}

float NovaDelayAudioProcessor::getSyncDelayMs (int syncIndex, double bpm) const noexcept
{
    static constexpr std::array<float, 10> beatDivisions {
        0.125f, 0.25f, 0.5f, 0.75f, 0.333333f,
        1.0f, 1.5f, 0.666667f, 2.0f, 4.0f
    };

    const auto beats = beatDivisions[static_cast<size_t> (juce::jlimit (0, 9, syncIndex))];
    const auto quarterMs = static_cast<float> (60000.0 / juce::jmax (30.0, bpm));
    return juce::jlimit (20.0f, 2000.0f, beats * quarterMs);
}

float NovaDelayAudioProcessor::readDelaySample (int channel, float delaySamples) const noexcept
{
    if (delayBufferSize <= 1)
        return 0.0f;

    const auto wrappedDelay = juce::jlimit (1.0f, static_cast<float> (delayBufferSize - 2), delaySamples);
    float readPos = static_cast<float> (writePosition) - wrappedDelay;

    while (readPos < 0.0f)
        readPos += static_cast<float> (delayBufferSize);

    while (readPos >= static_cast<float> (delayBufferSize))
        readPos -= static_cast<float> (delayBufferSize);

    const int indexA = static_cast<int> (readPos);
    const int indexB = (indexA + 1) % delayBufferSize;
    const float frac = readPos - static_cast<float> (indexA);

    const auto* data = delayBuffer.getReadPointer (channel);
    return juce::jmap (frac, data[indexA], data[indexB]);
}

float NovaDelayAudioProcessor::applySaturation (float x, float amount) const noexcept
{
    const auto drive = 1.0f + (amount * 5.0f);
    const auto compressed = std::tanh (x * drive) / std::tanh (drive);
    return juce::jmap (juce::jlimit (0.0f, 1.0f, amount), x, compressed);
}

float NovaDelayAudioProcessor::processDelayPathSample (int channel, float sample, float hpHz, float lpHz, float toneHz,
                                                       float feedbackNorm, bool lofiOn, float lofiAmount,
                                                       float modeDarkness, float modeGrit) noexcept
{
    auto& state = delayStates[static_cast<size_t> (juce::jlimit (0, 1, channel))];

    const auto toneCoeff = hzToOnePoleCoeff (toneHz, static_cast<float> (currentSampleRate));
    state.toneLp += toneCoeff * (sample - state.toneLp);
    float y = state.toneLp;

    const auto hpCoeff = hzToOnePoleCoeff (hpHz, static_cast<float> (currentSampleRate));
    state.hpLp += hpCoeff * (y - state.hpLp);
    y = y - state.hpLp;

    const auto lpCoeff = hzToOnePoleCoeff (lpHz, static_cast<float> (currentSampleRate));
    state.lpOut += lpCoeff * (y - state.lpOut);
    y = state.lpOut;

    const float harshCenterHz = 4000.0f;
    const auto antiHarshLpCoeff = hzToOnePoleCoeff (harshCenterHz, static_cast<float> (currentSampleRate));
    state.antiHarshLp += antiHarshLpCoeff * (y - state.antiHarshLp);
    float antiHarshHp = y - state.antiHarshLp;
    state.antiHarshHpLp += antiHarshLpCoeff * (antiHarshHp - state.antiHarshHpLp);
    const auto harshBand = antiHarshHp - state.antiHarshHpLp;

    const float harshReduction = juce::jmap (feedbackNorm, 0.0f, 1.0f, 0.08f, 0.42f + (modeDarkness * 0.2f));
    y -= harshBand * harshReduction;

    if (lofiOn)
    {
        const int holdSamples = juce::jlimit (2, 12, static_cast<int> (2.0f + (lofiAmount * 10.0f)));
        if (state.lofiCounter <= 0)
        {
            state.lofiCounter = holdSamples;
            const float bitDepth = juce::jmap (lofiAmount, 0.0f, 1.0f, 14.0f, 10.0f);
            const float levels = std::pow (2.0f, bitDepth);
            state.lofiHold = std::round (y * levels) / levels;
        }

        --state.lofiCounter;
        y = juce::jmap (0.78f, y, state.lofiHold);
    }

    const float bodyComp = 1.0f / (1.0f + (std::abs (y) * (0.12f + (modeGrit * 0.45f))));
    y *= bodyComp;

    return y;
}

void NovaDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    const float modeTarget = apvts.getRawParameterValue (ParameterIDs::mode)->load();
    const bool syncEnabled = apvts.getRawParameterValue (ParameterIDs::syncEnabled)->load() > 0.5f;
    const auto syncIndex = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::delayTimeSync)->load());
    const auto freeDelayMs = apvts.getRawParameterValue (ParameterIDs::delayTimeFreeMs)->load();

    const auto targetDelayMs = syncEnabled ? getSyncDelayMs (syncIndex, getHostBpm()) : freeDelayMs;

    delayTimeMsSmoothed.setTargetValue (targetDelayMs);
    feedbackSmoothed.setTargetValue (clampSafeFeedback (apvts.getRawParameterValue (ParameterIDs::feedback)->load()));
    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::mix)->load() / 100.0f));
    toneSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::tone)->load() / 100.0f));
    wowFlutterSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::wowFlutter)->load() / 100.0f));
    saturationSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::saturation)->load() / 100.0f));
    hpSmoothed.setTargetValue (apvts.getRawParameterValue (ParameterIDs::hpFilterHz)->load());
    lpSmoothed.setTargetValue (apvts.getRawParameterValue (ParameterIDs::lpFilterHz)->load());
    duckingSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::ducking)->load() / 100.0f));
    modeBlendSmoothed.setTargetValue (juce::jlimit (0.0f, 2.0f, modeTarget));

    const bool pingPongOn = apvts.getRawParameterValue (ParameterIDs::pingPong)->load() > 0.5f;
    const bool stereoOn = apvts.getRawParameterValue (ParameterIDs::stereo)->load() > 0.5f;
    const bool lofiOn = apvts.getRawParameterValue (ParameterIDs::lofi)->load() > 0.5f;
    const bool freezeOn = apvts.getRawParameterValue (ParameterIDs::freeze)->load() > 0.5f;

    freezeMixSmoothed.setTargetValue (freezeOn ? 1.0f : 0.0f);

    const float attackCoeff = std::exp (-1.0f / (0.0075f * static_cast<float> (currentSampleRate)));
    const float releaseCoeff = std::exp (-1.0f / (0.24f * static_cast<float> (currentSampleRate)));

    float blockInputPeakL = 0.0f;
    float blockInputPeakR = 0.0f;
    float blockOutputPeakL = 0.0f;
    float blockOutputPeakR = 0.0f;

    auto* left = buffer.getWritePointer (0);
    auto* right = totalNumInputChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inL = left[sample];
        const float inR = right != nullptr ? right[sample] : inL;
        const float inMono = 0.5f * (inL + inR);

        blockInputPeakL = juce::jmax (blockInputPeakL, std::abs (inL));
        blockInputPeakR = juce::jmax (blockInputPeakR, std::abs (inR));

        const float delayMs = delayTimeMsSmoothed.getNextValue();
        const float feedbackNormRaw = feedbackSmoothed.getNextValue();
        const float mixNorm = mixSmoothed.getNextValue();
        const float toneNorm = toneSmoothed.getNextValue();
        const float wowNorm = wowFlutterSmoothed.getNextValue();
        const float satNorm = saturationSmoothed.getNextValue();
        const float hpHzUser = hpSmoothed.getNextValue();
        const float lpHzUser = lpSmoothed.getNextValue();
        const float duckingNorm = duckingSmoothed.getNextValue();
        const float freezeMix = freezeMixSmoothed.getNextValue();
        const float modeBlendValue = modeBlendSmoothed.getNextValue();
        const auto modeProfile = getBlendedModeProfile (modeBlendValue);

        const float wowRateHz = juce::jmap (wowNorm * modeProfile.wowScale, 0.0f, 1.0f, 0.1f, 1.0f);
        const float flutterRateHz = juce::jmap (wowNorm * modeProfile.flutterScale, 0.0f, 1.0f, 3.0f, 9.0f);
        wowPhase += (2.0f * kPi * wowRateHz) / static_cast<float> (currentSampleRate);
        flutterPhase += (2.0f * kPi * flutterRateHz) / static_cast<float> (currentSampleRate);

        if (wowPhase > 2.0f * kPi) wowPhase -= 2.0f * kPi;
        if (flutterPhase > 2.0f * kPi) flutterPhase -= 2.0f * kPi;

        auto& leftState = delayStates[0];
        auto& rightState = delayStates[1];

        if (leftState.clockCounter <= 0)
        {
            leftState.clockCounter = juce::jlimit (2, 16, static_cast<int> (2.0f + (modeProfile.clockGritAmount * 22000.0f)));
            leftState.clockHold = randomGenerator.nextFloat() * 2.0f - 1.0f;
        }

        if (rightState.clockCounter <= 0)
        {
            rightState.clockCounter = juce::jlimit (2, 16, static_cast<int> (2.0f + (modeProfile.clockGritAmount * 24000.0f)));
            rightState.clockHold = randomGenerator.nextFloat() * 2.0f - 1.0f;
        }

        --leftState.clockCounter;
        --rightState.clockCounter;

        const float wowSignal = std::sin (wowPhase);
        const float flutterSignal = 0.55f * std::sin (flutterPhase) + 0.45f * std::sin (flutterPhase * 1.77f);
        const float modulationDepth = wowNorm * (2.0f + 4.0f * std::abs (wowSignal) + 1.5f * flutterSignal);
        const float modSamples = modulationDepth * (0.45f + modeProfile.wowScale);

        const float baseDelaySamples = juce::jlimit (1.0f, static_cast<float> (delayBufferSize - 2), delayMs * 0.001f * static_cast<float> (currentSampleRate));
        const float stereoOffset = stereoOn ? juce::jmap (wowNorm, 0.0f, 1.0f, 5.0f, 25.0f) * modeProfile.stereoWidthScale : 0.0f;

        const float smearLTarget = (randomGenerator.nextFloat() * 2.0f - 1.0f) * modeProfile.timingSmearSamples;
        const float smearRTarget = (randomGenerator.nextFloat() * 2.0f - 1.0f) * modeProfile.timingSmearSamples;
        leftState.timingSmear += 0.0009f * (smearLTarget - leftState.timingSmear);
        rightState.timingSmear += 0.0009f * (smearRTarget - rightState.timingSmear);

        float delaySamplesL = baseDelaySamples + modSamples + leftState.timingSmear + (stereoOn ? -stereoOffset : 0.0f);
        float delaySamplesR = baseDelaySamples - modSamples + rightState.timingSmear + (stereoOn ? stereoOffset : 0.0f);

        delaySamplesL = juce::jlimit (1.0f, static_cast<float> (delayBufferSize - 2), delaySamplesL);
        delaySamplesR = juce::jlimit (1.0f, static_cast<float> (delayBufferSize - 2), delaySamplesR);

        float delayedL = readDelaySample (0, delaySamplesL);
        float delayedR = readDelaySample (1, delaySamplesR);

        const float toneHzBase = juce::jmap (toneNorm, 0.0f, 1.0f, 2500.0f, 16000.0f);
        const float progressiveDarken = 1.0f - (feedbackNormRaw * modeProfile.feedbackDarken);
        const float toneHz = juce::jlimit (1400.0f, 16000.0f, toneHzBase * progressiveDarken);
        const float hpHz = juce::jmax (hpHzUser, modeProfile.minHpHz);
        const float lpHz = juce::jmin (lpHzUser, modeProfile.maxLpHz);
        const float modeDarkness = feedbackNormRaw * modeProfile.hfLossPerRepeat;
        const float modeGrit = modeProfile.clockGritAmount * 2100.0f;

        delayedL = processDelayPathSample (0, delayedL, hpHz, lpHz, toneHz, feedbackNormRaw, lofiOn, wowNorm, modeDarkness, modeGrit);
        delayedR = processDelayPathSample (1, delayedR, hpHz, lpHz, toneHz, feedbackNormRaw, lofiOn, wowNorm, modeDarkness, modeGrit);

        const float hissGain = modeProfile.hissAmount * (0.35f + (feedbackNormRaw * 0.65f));
        delayedL += (randomGenerator.nextFloat() * 2.0f - 1.0f) * hissGain;
        delayedR += (randomGenerator.nextFloat() * 2.0f - 1.0f) * hissGain;

        delayedL += leftState.clockHold * modeProfile.clockGritAmount * (0.45f + (feedbackNormRaw * 0.55f));
        delayedR += rightState.clockHold * modeProfile.clockGritAmount * (0.45f + (feedbackNormRaw * 0.55f));

        const float detector = std::abs (inMono);
        if (detector > duckEnvelope)
            duckEnvelope = (attackCoeff * duckEnvelope) + ((1.0f - attackCoeff) * detector);
        else
            duckEnvelope = (releaseCoeff * duckEnvelope) + ((1.0f - releaseCoeff) * detector);

        const float duckGain = 1.0f - (duckingNorm * juce::jlimit (0.0f, 1.0f, duckEnvelope * 2.2f) * 0.88f);

        const float wetPreDuckL = delayedL;
        const float wetPreDuckR = delayedR;

        const float wetL = wetPreDuckL * duckGain;
        const float wetR = wetPreDuckR * duckGain;

        const float mixAngle = mixNorm * (kPi * 0.5f);
        const float dryGain = std::cos (mixAngle);
        const float wetGain = std::sin (mixAngle);

        const float outL = (inL * dryGain) + (wetL * wetGain);
        const float outR = (inR * dryGain) + (wetR * wetGain);

        left[sample] = outL;
        if (right != nullptr)
            right[sample] = outR;

        blockOutputPeakL = juce::jmax (blockOutputPeakL, std::abs (outL));
        blockOutputPeakR = juce::jmax (blockOutputPeakR, std::abs (outR));

        const float freezeFeedbackBoost = juce::jmap (freezeMix, 0.0f, 1.0f, 0.0f, 0.62f);
        const float feedbackNorm = juce::jlimit (0.0f, 0.985f, feedbackNormRaw + freezeFeedbackBoost);

        float fbInL = wetPreDuckL;
        float fbInR = wetPreDuckR;

        if (pingPongOn)
        {
            const float width = stereoOn ? 0.95f : 0.70f;
            const float cross = width;
            const float self = 1.0f - (0.45f * width);
            const float pingL = (wetPreDuckR * cross) + (wetPreDuckL * self * 0.35f);
            const float pingR = (wetPreDuckL * cross) + (wetPreDuckR * self * 0.35f);
            fbInL = pingL;
            fbInR = pingR;
        }

        const float satAmount = juce::jlimit (0.0f, 1.0f, satNorm * modeProfile.saturationScale);
        fbInL = applySaturation (fbInL, satAmount);
        fbInR = applySaturation (fbInR, satAmount);

        const float fbLevel = 0.5f * (std::abs (fbInL) + std::abs (fbInR));
        const float compGain = 1.0f / (1.0f + (fbLevel * (2.8f * modeProfile.feedbackCompression)));
        fbInL *= compGain;
        fbInR *= compGain;

        fbInL = softClip (fbInL * 1.1f);
        fbInR = softClip (fbInR * 1.1f);

        const float freezeInputScale = 1.0f - (0.98f * freezeMix);
        const float inToDelayL = inL * freezeInputScale;
        const float inToDelayR = inR * freezeInputScale;

        auto* delayWriteL = delayBuffer.getWritePointer (0);
        auto* delayWriteR = delayBuffer.getWritePointer (1);

        float writeL = inToDelayL + (fbInL * feedbackNorm);
        float writeR = inToDelayR + (fbInR * feedbackNorm);

        writeL = softClip (writeL);
        writeR = softClip (writeR);

        delayWriteL[writePosition] = writeL;
        delayWriteR[writePosition] = writeR;

        ++writePosition;
        if (writePosition >= delayBufferSize)
            writePosition = 0;
    }

    inputPeakHoldL = blockInputPeakL > inputPeakHoldL ? blockInputPeakL : inputPeakHoldL * 0.92f;
    inputPeakHoldR = blockInputPeakR > inputPeakHoldR ? blockInputPeakR : inputPeakHoldR * 0.92f;
    outputPeakHoldL = blockOutputPeakL > outputPeakHoldL ? blockOutputPeakL : outputPeakHoldL * 0.92f;
    outputPeakHoldR = blockOutputPeakR > outputPeakHoldR ? blockOutputPeakR : outputPeakHoldR * 0.92f;

    const auto inL = juce::jlimit (0.0f, 1.0f, inputPeakHoldL);
    const auto inR = juce::jlimit (0.0f, 1.0f, inputPeakHoldR);
    const auto outL = juce::jlimit (0.0f, 1.0f, outputPeakHoldL);
    const auto outR = juce::jlimit (0.0f, 1.0f, outputPeakHoldR);

    inputPeakLevelL.store (inL);
    inputPeakLevelR.store (inR);
    outputPeakLevelL.store (outL);
    outputPeakLevelR.store (outR);

    // Keep aggregate mono peaks for any existing UI fallback.
    inputPeakHold = juce::jmax (inL, inR);
    outputPeakHold = juce::jmax (outL, outR);
    inputPeakLevel.store (inputPeakHold);
    outputPeakLevel.store (outputPeakHold);
    outputIsHot.store (juce::jmax (blockOutputPeakL, blockOutputPeakR) >= 0.985f);
}

bool NovaDelayAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaDelayAudioProcessor::createEditor()
{
    return new NovaDelayAudioProcessorEditor (*this);
}

void NovaDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NovaDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaDelayAudioProcessor();
}
