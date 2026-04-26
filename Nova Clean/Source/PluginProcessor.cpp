#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

NovaCleanV2AudioProcessor::NovaCleanV2AudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

NovaCleanV2AudioProcessor::~NovaCleanV2AudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NovaCleanV2AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> ("presetIndex", "Selected Preset",
        juce::StringArray {
            "Vocal Clean (Default)",
            "Mouth Clicks",
            "Heavy Vocal Clicks",
            "Soft Preserve",
            "Rap Transient Safe",
            "Crackle Repair",
            "Digital Glitch Fix",
            "Background Artifact Assist",
            "Aggressive Clean",
            "Emergency Repair"
        }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> ("mode", "Mode",
        juce::StringArray { "Vocal", "Digital", "Crackle" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("clean", "Clean",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 42.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("preserve", "Preserve",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 86.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("outputGain", "Output Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), -1.92f));

    layout.add (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("lowLatency", "Low Latency", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("listenRemoved", "Listen Removed", false));
    layout.add (std::make_unique<juce::AudioParameterBool> ("advanced", "Advanced", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("sensitivity", "Sensitivity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("clickSize", "Click Size",
        juce::StringArray { "Micro", "Short", "Medium" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("freqFocus", "Frequency Focus",
        juce::StringArray { "High", "Mid", "Full" }, 2));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("strength", "Strength",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 88.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("shape", "Shape",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 80.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("interpolation", "Interpolation",
        juce::StringArray { "Basic", "Smart" }, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("vocalProtect", "Vocal Protect",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 84.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("transientGuard", "Transient Guard",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 60.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> ("hqMode", "HQ Mode", true));

    return layout;
}

const juce::String NovaCleanV2AudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaCleanV2AudioProcessor::acceptsMidi() const { return false; }
bool NovaCleanV2AudioProcessor::producesMidi() const { return false; }
bool NovaCleanV2AudioProcessor::isMidiEffect() const { return false; }
double NovaCleanV2AudioProcessor::getTailLengthSeconds() const { return 0.0; }

int NovaCleanV2AudioProcessor::getNumPrograms() { return 1; }
int NovaCleanV2AudioProcessor::getCurrentProgram() { return 0; }
void NovaCleanV2AudioProcessor::setCurrentProgram (int) {}
const juce::String NovaCleanV2AudioProcessor::getProgramName (int) { return {}; }
void NovaCleanV2AudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaCleanV2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    dryBuffer.setSize (2, samplesPerBlock);
    removedBuffer.setSize (2, samplesPerBlock);

    for (auto& s : channelStates)
        s = ChannelState{};

    mixSmoothed.reset (sampleRate, 0.08);
    outputGainSmoothed.reset (sampleRate, 0.06);
    bypassSmoothed.reset (sampleRate, 0.02);
    listenRemovedSmoothed.reset (sampleRate, 0.02);

    mixSmoothed.setCurrentAndTargetValue (1.0f);
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
    bypassSmoothed.setCurrentAndTargetValue (0.0f);
    listenRemovedSmoothed.setCurrentAndTargetValue (0.0f);
}

void NovaCleanV2AudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaCleanV2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void NovaCleanV2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (dryBuffer.getNumSamples() < numSamples)
    {
        dryBuffer.setSize (2, numSamples, false, false, true);
        removedBuffer.setSize (2, numSamples, false, false, true);
    }

    const int channels = juce::jmin (2, totalNumInputChannels);
    for (int ch = 0; ch < channels; ++ch)
    {
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        removedBuffer.clear (ch, 0, numSamples);
    }

    const auto mode = static_cast<Mode> (static_cast<int> (apvts.getRawParameterValue ("mode")->load()));
    const auto clickSize = static_cast<ClickSize> (static_cast<int> (apvts.getRawParameterValue ("clickSize")->load()));
    const auto freqFocus = static_cast<FrequencyFocus> (static_cast<int> (apvts.getRawParameterValue ("freqFocus")->load()));
    const auto interpolation = static_cast<InterpolationMode> (static_cast<int> (apvts.getRawParameterValue ("interpolation")->load()));

    const float cleanNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("clean")->load() / 100.0f);
    const float preserveNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("preserve")->load() / 100.0f);
    const float sensitivityNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("sensitivity")->load() / 100.0f);
    const float strengthNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("strength")->load() / 100.0f);
    const float shapeNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("shape")->load() / 100.0f);
    const float vocalProtectNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("vocalProtect")->load() / 100.0f);
    const float transientGuardNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("transientGuard")->load() / 100.0f);

    const bool lowLatency = apvts.getRawParameterValue ("lowLatency")->load() > 0.5f;
    const bool hqMode = apvts.getRawParameterValue ("hqMode")->load() > 0.5f && !lowLatency;

    const float effectiveSensitivity = juce::jlimit (0.0f, 1.0f, sensitivityNorm * 0.55f + cleanNorm * 0.65f);
    const float effectiveRepair = juce::jlimit (0.0f, 1.0f, strengthNorm * 0.65f + cleanNorm * 0.45f);

    const float baseThreshold = 0.11f;
    const float shapedSensitivity = std::pow (effectiveSensitivity, 1.15f);
    float threshold = juce::jlimit (0.01f, 0.85f, baseThreshold + (1.0f - shapedSensitivity) * 0.34f);

    if (mode == Digital)
        threshold *= 0.85f;
    else if (mode == Crackle)
        threshold *= 0.72f;

    if (clickSize == Micro)
        threshold *= 0.86f;
    else if (clickSize == Medium)
        threshold *= 1.12f;

    const float protectScale = 1.0f - 0.35f * preserveNorm;
    const float cleanBoost = juce::jmap (cleanNorm, 0.0f, 1.0f, 0.65f, 1.0f);
    const float repairDepth = juce::jlimit (0.0f, 1.0f, effectiveRepair * protectScale * cleanBoost);

    const float focusHigh = (freqFocus == High ? 1.35f : (freqFocus == Mid ? 0.82f : 1.0f));
    const float focusMid  = (freqFocus == Mid ? 1.3f : 1.0f);

    int removedEvents = 0;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        auto* dry = dryBuffer.getReadPointer (ch);
        auto* removed = removedBuffer.getWritePointer (ch);

        auto& state = channelStates[static_cast<size_t> (ch)];
        // holdSamples lives in state — persists across buffer boundaries

        const int winLen = (clickSize == Micro ? 1 : (clickSize == Short ? 2 : 4));

        for (int i = 0; i < numSamples; ++i)
        {
            const float in   = x[i];
            const float next = (i + 1 < numSamples) ? dry[i + 1] : state.prevIn1;
            const float post = (i + 2 < numSamples) ? dry[i + 2] : next;

            const float d1 = in - state.prevIn1;
            const float d2 = state.prevIn1 - state.prevIn2;
            const float curvature = std::abs (d1 - d2);
            const float slope     = std::abs (d1);

            // Fast and slow energy envelopes
            state.energyEnv     = 0.995f  * state.energyEnv     + 0.005f  * (in * in);
            state.hfEnv         = 0.985f  * state.hfEnv         + 0.015f  * curvature;
            state.slowEnergyEnv = 0.9997f * state.slowEnergyEnv + 0.0003f * (in * in);

            const float energy     = std::sqrt (state.energyEnv     + 1.0e-9f);
            const float slowEnergy = std::sqrt (state.slowEnergyEnv + 1.0e-9f);
            const float hfRatio    = curvature / (std::sqrt (state.hfEnv + 1.0e-9f) + 1.0e-6f);
            const float clickiness = curvature / (slope + 1.0e-6f);

            // Amplitude pop: sudden energy burst vs long-term level
            const float popRatio  = energy / (slowEnergy + 1.0e-6f);
            const bool  amplitudePop = (popRatio > 4.2f) && (slope > energy * 0.55f);

            // Digital impulse: isolated sample — both neighbours are quiet
            const float neighborMax = juce::jmax (std::abs (state.prevIn1), std::abs (next));
            const bool  digitalImpulse = (std::abs (in) > neighborMax * 3.5f) && (std::abs (in) > 0.003f);

            float score = 0.0f;
            score += 0.40f * juce::jlimit (0.0f, 4.0f, hfRatio)                        * focusHigh;
            score += 0.30f * juce::jlimit (0.0f, 4.0f, clickiness)                     * focusMid;
            score += 0.18f * juce::jlimit (0.0f, 4.0f, slope / (energy + 1.0e-5f));
            score += 0.12f * juce::jlimit (0.0f, 3.0f, popRatio - 1.0f);  // pop contribution

            if (mode == Digital)
                score += 0.25f * juce::jlimit (0.0f, 3.0f, slope / (std::abs (next) + 1.0e-5f));
            else if (mode == Crackle)
                score += 0.20f * juce::jlimit (0.0f, 3.0f, hfRatio);

            const bool likelyConsonant  = (slope > 0.015f && clickiness < 0.35f);
            const bool protectTransient = likelyConsonant && transientGuardNorm > 0.35f;
            const bool protectVocal     = (clickiness < 0.55f && vocalProtectNorm > 0.45f);

            // Hard impulses always bypass vocal/transient protection
            const bool hardImpulse    = digitalImpulse || amplitudePop || (score > threshold * 1.85f);
            const bool protectedEvent = !hardImpulse && (protectTransient || protectVocal);
            const bool detected       = (score > threshold || digitalImpulse || amplitudePop) && !protectedEvent;

            if (detected)
                state.holdSamples = winLen;

            float repaired = in;
            if (state.holdSamples > 0)
            {
                float interpTarget;
                const float interpBase = 0.5f * (state.prevOut + next);

                if (interpolation == Smart && hqMode)
                {
                    // Catmull-Rom cubic: P0=prevIn2, P1=prevOut, P2=next, P3=post
                    // t=0 → prevOut (before click), t=1 → next (after click)
                    const float t  = 1.0f - static_cast<float> (state.holdSamples) / static_cast<float> (winLen);
                    const float t2 = t * t;
                    const float t3 = t2 * t;
                    interpTarget = 0.5f * (
                          (2.0f * state.prevOut)
                        + (-state.prevIn2 + next)       * t
                        + (2.0f*state.prevIn2 - 5.0f*state.prevOut + 4.0f*next - post) * t2
                        + (-state.prevIn2 + 3.0f*state.prevOut - 3.0f*next + post)     * t3
                    );

                    // Shape controls the Smart+HQ character:
                    // low shape -> more transparent (closer to source), high shape -> smoother repair.
                    const float transparentTarget = 0.75f * in + 0.25f * interpBase;
                    const float shapeBlend = juce::jlimit (0.0f, 1.0f, shapeNorm);
                    interpTarget = juce::jmap (shapeBlend, transparentTarget, interpTarget);
                }
                else
                {
                    const float smartBlend = (interpolation == Smart
                        ? juce::jlimit (0.0f, 1.0f, 0.35f + 0.5f * shapeNorm)
                        : 0.2f);
                    interpTarget = juce::jmap (smartBlend, interpBase, 0.35f * in + 0.65f * interpBase);
                }

                repaired = in + (interpTarget - in) * repairDepth;

                removed[i] = in - repaired;
                if (detected)
                    ++removedEvents;

                --state.holdSamples;
            }
            else
            {
                removed[i] = 0.0f;
            }

            x[i] = repaired;
            state.prevIn2 = state.prevIn1;
            state.prevIn1 = in;
            state.prevOut = repaired;
        }
    }

    const float mixNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("mix")->load() / 100.0f);
    const float gainDb = apvts.getRawParameterValue ("outputGain")->load();
    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const bool listenRemoved = apvts.getRawParameterValue ("listenRemoved")->load() > 0.5f;

    mixSmoothed.setTargetValue (mixNorm);
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainDb));
    bypassSmoothed.setTargetValue (bypass ? 1.0f : 0.0f);
    listenRemovedSmoothed.setTargetValue (listenRemoved ? 1.0f : 0.0f);

    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    float removedEnergy = 0.0f;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        auto* dry = dryBuffer.getReadPointer (ch);
        auto* removed = removedBuffer.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = dry[i];
            const float wetSample = x[i];
            const float removedSample = removed[i];

            const float mixNow = mixSmoothed.getNextValue();
            const float gainNow = outputGainSmoothed.getNextValue();
            const float bypassNow = bypassSmoothed.getNextValue();
            const float listenNow = listenRemovedSmoothed.getNextValue();

            const float blended = drySample * (1.0f - mixNow) + wetSample * mixNow;
            const float normalOut = blended * gainNow;
            const float removedOut = removedSample * (2.8f * gainNow);
            const float modeOut = normalOut * (1.0f - listenNow) + removedOut * listenNow;
            const float finalOut = drySample * bypassNow + modeOut * (1.0f - bypassNow);

            x[i] = finalOut;

            if (ch == 0)
            {
                inPeakL = juce::jmax (inPeakL, std::abs (drySample));
                outPeakL = juce::jmax (outPeakL, std::abs (finalOut));
            }
            else
            {
                inPeakR = juce::jmax (inPeakR, std::abs (drySample));
                outPeakR = juce::jmax (outPeakR, std::abs (finalOut));
            }

            removedEnergy += removedSample * removedSample;
        }
    }

    inputMeterL.store (0.82f * inputMeterL.load() + 0.18f * inPeakL);
    inputMeterR.store (0.82f * inputMeterR.load() + 0.18f * inPeakR);
    outputMeterL.store (0.82f * outputMeterL.load() + 0.18f * outPeakL);
    outputMeterR.store (0.82f * outputMeterR.load() + 0.18f * outPeakR);

    const float removedNorm = juce::jlimit (0.0f, 1.0f, std::sqrt (removedEnergy / juce::jmax (1, numSamples * channels)) * 16.0f);
    removedAmountNorm.store (0.75f * removedAmountNorm.load() + 0.25f * removedNorm);
    clicksRemovedCount.store (removedEvents);
}

bool NovaCleanV2AudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* NovaCleanV2AudioProcessor::createEditor() { return new NovaCleanV2AudioProcessorEditor (*this); }

void NovaCleanV2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaCleanV2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaCleanV2AudioProcessor();
}
