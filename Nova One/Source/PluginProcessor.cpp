#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <cmath>

namespace
{
    struct ModeProfile
    {
        float warmthBias;
        float widthBias;
        float glueBias;
        float limiterBias;
        float ceilingDb;
    };

    ModeProfile getModeProfile (int modeIndex) noexcept
    {
        switch (juce::jlimit (0, 3, modeIndex))
        {
            case 1: return { 0.18f, 0.00f, 0.18f, 0.75f, -0.82f }; // Warm
            case 2: return { 0.05f, 0.18f, 0.05f, 0.70f, -0.84f }; // Wide
            case 3: return { 0.10f, 0.06f, 0.30f, 1.00f, -0.78f }; // Loud
            default: return { 0.00f, 0.00f, 0.00f, 0.55f, -0.90f }; // Clean
        }
    }

    float computeGainReductionDb (float overDb, float ratio, float kneeDb) noexcept
    {
        if (overDb <= 0.0f)
            return 0.0f;

        const auto slope = 1.0f - (1.0f / juce::jmax (1.0f, ratio));

        if (kneeDb <= 0.0f)
            return slope * overDb;

        const auto kneeStart = -0.5f * kneeDb;
        const auto kneeEnd = 0.5f * kneeDb;

        if (overDb <= kneeStart)
            return 0.0f;

        if (overDb < kneeEnd)
        {
            const auto x = overDb - kneeStart;
            return slope * (x * x) / (2.0f * kneeDb);
        }

        return slope * overDb;
    }

    float applySoftCeiling (float sample, float ceilingLinear) noexcept
    {
        const auto safeCeiling = juce::jmax (0.05f, ceilingLinear);
        const auto normalised = sample / safeCeiling;
        const auto shaped = std::tanh (normalised * 0.82f) / std::tanh (0.82f);
        return shaped * safeCeiling;
    }
}

NovaOneAudioProcessor::NovaOneAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("NovaOne"), createParameterLayout())
{
}

NovaOneAudioProcessor::~NovaOneAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaOneAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::tone, 1 },
        "Tone",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        5.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::glue, 1 },
        "Glue",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        3.5f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::weight, 1 },
        "Weight",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        4.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::air, 1 },
        "Air",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        5.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::width, 1 },
        "Width",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        4.5f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1); }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::mix, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
        100.0f,
        "%",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::outputGain, 1 },
        "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f),
        0.0f,
        "dB",
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int)
        {
            const auto sign = value >= 0.0f ? "+" : "";
            return sign + juce::String (value, 1) + " dB";
        }));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::finishAmount, 1 },
        "Finish",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::modePreset, 1 },
        "Mode",
        juce::StringArray { "Clean", "Warm", "Wide", "Loud" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::meterView, 1 },
        "Meter View",
        juce::StringArray { "OUT", "LU" },
        0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::autoLevel, 1 },
        "Auto Level",
        false));

    // UI-state only: the four independent macro-dimension amounts (0-100%) that the
    // Nova One web UI blends into tone/glue/weight/air/width/output_gain above. Not
    // read anywhere in processBlock; persisted purely so the knob positions for all
    // four dimensions survive a plugin reopen without an ambiguous many-to-one guess.
    const auto addDimensionAmount = [&layout] (const char* id, const char* name)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 },
            name,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f),
            0.0f,
            "%",
            juce::AudioProcessorParameter::genericParameter,
            [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }));
    };

    addDimensionAmount (ParameterIDs::cleanAmount, "Clean Amount");
    addDimensionAmount (ParameterIDs::warmAmount, "Warm Amount");
    addDimensionAmount (ParameterIDs::wideAmount, "Wide Amount");
    addDimensionAmount (ParameterIDs::loudAmount, "Loud Amount");

    return layout;
}

const juce::String NovaOneAudioProcessor::getName() const
{
    return "Nova One";
}

bool NovaOneAudioProcessor::acceptsMidi() const
{
    return false;
}

bool NovaOneAudioProcessor::producesMidi() const
{
    return false;
}

bool NovaOneAudioProcessor::isMidiEffect() const
{
    return false;
}

double NovaOneAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaOneAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaOneAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaOneAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NovaOneAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NovaOneAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NovaOneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // One-pole lowpass coefficient for the stereo widener's mono-safe crossover.
    // Content below this frequency is left at its original (unwidened) amount so
    // Wide's side-channel boost can't widen true mono-compatibility-critical bass.
    constexpr float sideLowCrossoverHz = 150.0f;
    sideLowCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * sideLowCrossoverHz
                              / static_cast<float> (currentSampleRate));

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));

    tiltLowShelf.prepare (spec);
    tiltHighShelf.prepare (spec);
    weightShelf.prepare (spec);
    lowTightFilter.prepare (spec);
    airShelf.prepare (spec);
    airSmoothFilter.prepare (spec);
    outputTrim.prepare (spec);
    outputTrim.setRampDurationSeconds (0.03);

    tiltLowShelf.reset();
    tiltHighShelf.reset();
    weightShelf.reset();
    lowTightFilter.reset();
    airShelf.reset();
    airSmoothFilter.reset();
    outputTrim.reset();

    dryBuffer.setSize (static_cast<int> (spec.numChannels), samplesPerBlock, false, false, true);
    dryBuffer.clear();

    compressorEnvelope = 0.0f;
    compressorGainReductionDb = 0.0f;
    limiterGainReductionDb = 0.0f;
    outputPeakHold = 0.0f;
    outputRmsHold = 0.0f;
    inputPeakHold = 0.0f;
    inputRmsHold = 0.0f;
    sideLowState = 0.0f;

    outputPeakLevel.store (0.0f);
    outputRmsLevel.store (0.0f);
    limiterReductionLevel.store (0.0f);
    outputIsHot.store (false);
    inputPeakLevel.store (0.0f);
    inputRmsLevel.store (0.0f);
}

void NovaOneAudioProcessor::releaseResources()
{
    dryBuffer.setSize (0, 0);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaOneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainOutput == layouts.getMainInputChannelSet();
}
#endif

void NovaOneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    if (dryBuffer.getNumChannels() < totalNumOutputChannels || dryBuffer.getNumSamples() < buffer.getNumSamples())
        dryBuffer.setSize (totalNumOutputChannels, buffer.getNumSamples(), false, false, true);

    for (auto channel = 0; channel < totalNumOutputChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, buffer.getNumSamples());

    {
        const auto inputChannels = juce::jmax (1, totalNumOutputChannels);
        float inputPeakAccumulator = 0.0f;
        double inputSquaredSum = 0.0;

        for (int channel = 0; channel < inputChannels; ++channel)
        {
            const auto* channelData = dryBuffer.getReadPointer (channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto sampleValue = channelData[sample];
                inputPeakAccumulator = juce::jmax (inputPeakAccumulator, std::abs (sampleValue));
                inputSquaredSum += static_cast<double> (sampleValue) * static_cast<double> (sampleValue);
            }
        }

        const auto inputRms = std::sqrt (inputSquaredSum / static_cast<double> (buffer.getNumSamples() * inputChannels));
        const auto inputRmsNormalised = juce::jlimit (0.0f, 1.0f, static_cast<float> (inputRms * 2.1));

        inputPeakHold = juce::jmax (inputPeakAccumulator, inputPeakHold * 0.93f);
        inputRmsHold = juce::jmax (inputRmsNormalised, inputRmsHold * 0.92f);

        inputPeakLevel.store (inputPeakHold);
        inputRmsLevel.store (inputRmsHold);
    }

    const auto tone = apvts.getRawParameterValue (ParameterIDs::tone)->load();
    const auto glue = apvts.getRawParameterValue (ParameterIDs::glue)->load();
    const auto weight = apvts.getRawParameterValue (ParameterIDs::weight)->load();
    const auto air = apvts.getRawParameterValue (ParameterIDs::air)->load();
    const auto width = apvts.getRawParameterValue (ParameterIDs::width)->load();
    const auto mix = apvts.getRawParameterValue (ParameterIDs::mix)->load();
    const auto outputGainDb = apvts.getRawParameterValue (ParameterIDs::outputGain)->load();
    const auto finishAmount = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (ParameterIDs::finishAmount)->load());
    const auto modeIndex = static_cast<int> (apvts.getRawParameterValue (ParameterIDs::modePreset)->load());

    const auto mode = getModeProfile (modeIndex);

    const auto toneNorm = juce::jlimit (0.0f, 1.0f, tone / 10.0f);
    const auto glueNorm = juce::jlimit (0.0f, 1.0f, glue / 10.0f);
    const auto weightNorm = juce::jlimit (0.0f, 1.0f, weight / 10.0f);
    const auto airNorm = juce::jlimit (0.0f, 1.0f, air / 10.0f);
    const auto widthNorm = juce::jlimit (0.0f, 1.0f, width / 10.0f);
    const auto mixNorm = juce::jlimit (0.0f, 1.0f, mix / 100.0f);

    const auto tiltDb = -3.0f + (toneNorm * 6.0f) + ((modeIndex == 1) ? -0.25f : (modeIndex == 2 ? 0.25f : 0.0f));
    const auto lowTiltGainDb = -tiltDb * 0.5f;
    const auto highTiltGainDb = tiltDb * 0.5f;

    const auto weightDb = -1.5f + (weightNorm * 3.5f) + (mode.warmthBias * 1.2f);
    const auto tightenDb = -((glueNorm * 1.3f) + (finishAmount * 0.35f * (1.0f + mode.glueBias)));
    const auto airDb = -1.0f + (airNorm * 4.0f) + (modeIndex == 2 ? 0.25f : 0.0f);
    const auto smoothingCutoff = juce::jlimit (5500.0f, 19000.0f,
                                               18000.0f - (airNorm * 2200.0f)
                                                          - (glueNorm * 1300.0f)
                                                          - (finishAmount * (900.0f + (800.0f * mode.limiterBias))));

    *tiltLowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 220.0f, 0.72f, juce::Decibels::decibelsToGain (lowTiltGainDb));
    *tiltHighShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 2600.0f, 0.72f, juce::Decibels::decibelsToGain (highTiltGainDb));
    *weightShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, 82.0f, 0.82f, juce::Decibels::decibelsToGain (weightDb));
    *lowTightFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, 210.0f, 0.85f, juce::Decibels::decibelsToGain (tightenDb));
    *airShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 12000.0f, 0.72f, juce::Decibels::decibelsToGain (airDb));
    *airSmoothFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, smoothingCutoff, 0.78f);

    juce::dsp::AudioBlock<float> wetBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> wetContext (wetBlock);

    tiltLowShelf.process (wetContext);
    tiltHighShelf.process (wetContext);
    weightShelf.process (wetContext);
    lowTightFilter.process (wetContext);
    airShelf.process (wetContext);
    airSmoothFilter.process (wetContext);

    const auto thresholdDb = -6.0f - (glueNorm * 12.0f) - (mode.glueBias * 2.0f);
    const auto ratio = 1.5f + (glueNorm * 2.0f) + (mode.glueBias * 0.45f);
    const auto attackMs = juce::jlimit (18.0f, 40.0f, 30.0f - (mode.limiterBias * 5.0f));
    const auto releaseMs = 120.0f + (glueNorm * 230.0f) + (finishAmount * 40.0f * mode.limiterBias);
    const auto kneeDb = 6.0f + finishAmount;

    const auto attackCoeff = std::exp (-1.0f / (0.001f * attackMs * static_cast<float> (currentSampleRate)));
    const auto releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * static_cast<float> (currentSampleRate)));

    const auto harmonicDrive = 1.0f + (glueNorm * 0.03f) + (mode.warmthBias * 0.04f)
                             + (finishAmount * 0.05f);
    const auto harmonicBlend = juce::jlimit (0.08f, 0.22f,
                                             0.08f + (glueNorm * 0.05f) + (finishAmount * 0.04f));
    const auto sideBoost = 1.0f + (widthNorm * (0.30f + mode.widthBias)) + (finishAmount * 0.05f);

    // Continuous blend between "finish off" and "finish fully on" so engaging the
    // limiter (via Loud's finish_amount ramp) never snaps/clicks the texture.
    const auto limiterThresholdDb = juce::jmap (finishAmount, 0.5f,
                                                -3.2f - (glueNorm * 0.9f) - (mode.limiterBias * 0.4f));
    const auto limiterThresholdLinear = juce::Decibels::decibelsToGain (limiterThresholdDb);
    const auto ceilingDb = juce::jmap (finishAmount, -0.5f, -0.8f);
    const auto ceilingLinear = juce::Decibels::decibelsToGain (ceilingDb);
    const auto finishDrive = juce::jmap (finishAmount, 1.0f, 1.03f + (glueNorm * 0.03f));
    const auto preLimiterGain = juce::Decibels::decibelsToGain (juce::jmap (finishAmount, 0.0f, 3.0f));
    const auto limiterRatio = 2.6f + (0.5f * mode.limiterBias);
    const auto limiterKneeDb = juce::jmap (finishAmount, 0.0f, 3.0f);
    const auto limiterAttackRate = juce::jmap (finishAmount, 0.32f, 0.16f);
    const auto limiterReleaseRate = juce::jmap (finishAmount, 0.006f, 0.010f);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto left = buffer.getSample (0, sample);
        auto right = totalNumInputChannels > 1 ? buffer.getSample (1, sample) : left;

        const auto detector = juce::jmax (std::abs (left), std::abs (right)) + 1.0e-6f;
        if (detector > compressorEnvelope)
            compressorEnvelope = (attackCoeff * compressorEnvelope) + ((1.0f - attackCoeff) * detector);
        else
            compressorEnvelope = (releaseCoeff * compressorEnvelope) + ((1.0f - releaseCoeff) * detector);

        const auto detectorDb = juce::Decibels::gainToDecibels (compressorEnvelope, -160.0f);
        const auto targetCompGrDb = computeGainReductionDb (detectorDb - thresholdDb, ratio, kneeDb);

        if (targetCompGrDb > compressorGainReductionDb)
            compressorGainReductionDb += (targetCompGrDb - compressorGainReductionDb) * 0.14f;
        else
            compressorGainReductionDb += (targetCompGrDb - compressorGainReductionDb) * 0.025f;

        const auto compGain = juce::Decibels::decibelsToGain (-compressorGainReductionDb);
        left *= compGain;
        right *= compGain;

        const auto saturatedLeft = std::tanh (left * harmonicDrive);
        const auto saturatedRight = std::tanh (right * harmonicDrive);
        left = juce::jmap (harmonicBlend, left, saturatedLeft);
        right = juce::jmap (harmonicBlend, right, saturatedRight);

        if (totalNumInputChannels > 1)
        {
            const auto mid = 0.5f * (left + right);
            const auto side = 0.5f * (left - right);
            sideLowState = (sideLowCoeff * sideLowState) + ((1.0f - sideLowCoeff) * side);
            const auto sideHigh = side - sideLowState;
            const auto widenedSide = sideLowState + (sideHigh * sideBoost);
            left = mid + widenedSide;
            right = mid - widenedSide;
        }

        left *= finishDrive;
        right *= finishDrive;
        left *= preLimiterGain;
        right *= preLimiterGain;

        const auto peak = juce::jmax (std::abs (left), std::abs (right));
        auto targetLimiterGrDb = 0.0f;

        if (peak > limiterThresholdLinear)
        {
            const auto overDb = juce::Decibels::gainToDecibels (peak / limiterThresholdLinear, 0.0f);
            targetLimiterGrDb = juce::jlimit (0.0f, 6.0f,
                                              computeGainReductionDb (overDb, limiterRatio, limiterKneeDb));
        }

        if (peak > ceilingLinear)
            targetLimiterGrDb = juce::jmax (targetLimiterGrDb,
                                            juce::Decibels::gainToDecibels (peak / ceilingLinear, 0.0f));

        if (targetLimiterGrDb > limiterGainReductionDb)
            limiterGainReductionDb += (targetLimiterGrDb - limiterGainReductionDb) * limiterAttackRate;
        else
            limiterGainReductionDb += (targetLimiterGrDb - limiterGainReductionDb) * limiterReleaseRate;

        const auto limiterGain = juce::Decibels::decibelsToGain (-limiterGainReductionDb);
        left *= limiterGain;
        right *= limiterGain;

        left = applySoftCeiling (left, ceilingLinear);
        right = applySoftCeiling (right, ceilingLinear);

        buffer.setSample (0, sample, left);
        if (totalNumInputChannels > 1)
            buffer.setSample (1, sample, right);
    }


    const auto numChannels = juce::jmax (1, totalNumOutputChannels);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* wetData = buffer.getWritePointer (channel);
        const auto* dryData = dryBuffer.getReadPointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            wetData[sample] = (dryData[sample] * (1.0f - mixNorm)) + (wetData[sample] * mixNorm);
    }

    outputTrim.setGainDecibels (outputGainDb);
    outputTrim.process (wetContext);

    float peakAccumulator = 0.0f;
    double squaredSum = 0.0;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* channelData = buffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto sampleValue = channelData[sample];
            peakAccumulator = juce::jmax (peakAccumulator, std::abs (sampleValue));
            squaredSum += static_cast<double> (sampleValue) * static_cast<double> (sampleValue);
        }
    }

    const auto rms = std::sqrt (squaredSum / static_cast<double> (buffer.getNumSamples() * numChannels));
    const auto rmsNormalised = juce::jlimit (0.0f, 1.0f, static_cast<float> (rms * 2.1));

    outputPeakHold = juce::jmax (peakAccumulator, outputPeakHold * 0.93f);
    outputRmsHold = juce::jmax (rmsNormalised, outputRmsHold * 0.92f);

    outputPeakLevel.store (outputPeakHold);
    outputRmsLevel.store (outputRmsHold);
    limiterReductionLevel.store (juce::jlimit (0.0f, 1.0f, limiterGainReductionDb / 6.0f));
    outputIsHot.store (outputPeakHold > 0.92f);
}

bool NovaOneAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NovaOneAudioProcessor::createEditor()
{
    return new NovaOneAudioProcessorEditor (*this);
}

void NovaOneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void NovaOneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaOneAudioProcessor();
}
