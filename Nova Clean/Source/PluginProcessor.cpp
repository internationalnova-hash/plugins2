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
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 62.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("preserve", "Preserve",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 72.0f));
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
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 95.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("shape", "Shape",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 80.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("interpolation", "Interpolation",
        juce::StringArray { "Basic", "Smart" }, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> ("vocalProtect", "Vocal Protect",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 72.0f));
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
    lookAheadSamples = juce::jmax (1, static_cast<int> (std::round (0.005 * currentSampleRate))); // 5 ms
    setLatencySamples (lookAheadSamples);

    const float dt = 1.0f / static_cast<float> (juce::jmax (1.0, currentSampleRate));
    const float rc = 1.0f / (2.0f * juce::MathConstants<float>::pi * 3000.0f);
    detectHpAlpha = rc / (rc + dt);

    dryBuffer.setSize (2, samplesPerBlock);
    removedBuffer.setSize (2, samplesPerBlock);

    for (auto& s : channelStates)
        s = ChannelState{};

    // Listen Removed uses dry-vs-processed nulling. Keep a small delay line so
    // we can compensate any algorithmic sample offset while staying RT-safe.
    listenRemovedDelayBufferSize = juce::jmax (2048, samplesPerBlock * 4);
    listenRemovedAlignSamples = 0;
    for (int ch = 0; ch < 2; ++ch)
    {
        listenRemovedDryDelay[static_cast<size_t> (ch)].assign (static_cast<size_t> (listenRemovedDelayBufferSize), 0.0f);
        listenRemovedDryWriteIndex[static_cast<size_t> (ch)] = 0;
    }

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

    const float cleanNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("clean")->load() / 100.0f);
    const float preserveNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("preserve")->load() / 100.0f);
    const float sensitivityNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("sensitivity")->load() / 100.0f);
    const float strengthNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("strength")->load() / 100.0f);
    const float shapeNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("shape")->load() / 100.0f);
    const float effectiveSensitivity = juce::jlimit (0.0f, 1.0f, sensitivityNorm * 0.45f + cleanNorm * 0.95f);
    const float effectiveRepair = juce::jlimit (0.0f, 1.0f, strengthNorm * 0.55f + cleanNorm * 0.95f);

    const float baseThreshold = 0.07f;
    const float shapedSensitivity = std::pow (effectiveSensitivity, 1.15f);
    float threshold = juce::jlimit (0.01f, 0.85f, baseThreshold + (1.0f - shapedSensitivity) * 0.26f);

    if (mode == Digital)
        threshold *= 0.85f;
    else if (mode == Crackle)
        threshold *= 0.72f;

    if (clickSize == Micro)
        threshold *= 0.86f;
    else if (clickSize == Medium)
        threshold *= 1.12f;

    const float protectScale = 1.0f - 0.22f * preserveNorm;
    const float cleanBoost = juce::jmap (cleanNorm, 0.0f, 1.0f, 0.85f, 1.25f);
    const float repairDepth = juce::jlimit (0.20f, 1.0f, effectiveRepair * protectScale * cleanBoost);

    const float freqScale = (freqFocus == High ? 0.9f : (freqFocus == Mid ? 1.0f : 1.12f));

    int removedEvents = 0;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        auto* removed = removedBuffer.getWritePointer (ch);

        auto& state = channelStates[static_cast<size_t> (ch)];
        for (int i = 0; i < numSamples; ++i)
        {
            const float in = x[i];

            // Detection sidechain: 3 kHz high-pass.
            const float hp = detectHpAlpha * (state.hpPrevY + in - state.hpPrevX);
            state.hpPrevX = in;
            state.hpPrevY = hp;

            state.rawLookAhead.push_back (in);
            state.hpLookAhead.push_back (hp);

            float repairedOut = 0.0f;
            float removedOut = 0.0f;

            const int requiredFuture = juce::jmax (lookAheadSamples, 6);
            if (static_cast<int> (state.rawLookAhead.size()) > requiredFuture)
            {
                const float curr = state.rawLookAhead[0];
                const float hp0 = state.hpLookAhead[0];
                const float hp1 = state.hpLookAhead[1];
                const float hp2 = state.hpLookAhead[2];
                const float hp3 = state.hpLookAhead[3];
                const float hp4 = state.hpLookAhead[4];

                const float slopePrev = hp0 - state.prevHp1;
                const float slopePrev2 = state.prevHp1 - state.prevHp2;
                const float slopeNext = hp1 - hp0;

                const float localRef = juce::jmax (std::abs (state.prevHp1), std::abs (hp1), std::abs (hp2), 1.0e-6f);
                const float curvatureJump = std::abs (slopePrev - slopePrev2);
                const float slopeFlip = std::abs (slopePrev - slopeNext);
                const bool strongSpike = std::abs (hp0) > (localRef * juce::jmap (cleanNorm, 0.0f, 1.0f, 2.6f, 1.9f));
                const bool suddenDiscontinuity = curvatureJump > (localRef * 1.35f) && slopeFlip > (localRef * 1.10f);
                const bool multiSampleEdge = std::abs (hp0 - hp3) > (localRef * 1.55f * freqScale)
                    || std::abs (hp0 - hp4) > (localRef * 1.55f * freqScale);

                const bool detected = (strongSpike || (suddenDiscontinuity && multiSampleEdge))
                    && (std::abs (hp0) > threshold * 0.0045f);

                if (detected && ! state.repairActive)
                {
                    const float severity = juce::jlimit (0.0f, 1.0f, (std::abs (hp0) / juce::jmax (1.0e-6f, localRef) - 1.4f) / 2.8f);
                    const int repairLen = juce::jlimit (2, 5,
                        2 + static_cast<int> (std::round (severity * 3.0f * juce::jmap (cleanNorm, 0.0f, 1.0f, 0.8f, 1.2f))));

                    const int rightA = juce::jmin (static_cast<int> (state.rawLookAhead.size()) - 1, repairLen + 1);
                    const int rightB = juce::jmin (static_cast<int> (state.rawLookAhead.size()) - 1, repairLen + 2);

                    state.repairActive = true;
                    state.repairTotal = repairLen;
                    state.repairIndex = 0;
                    state.repairP0 = state.prevRaw2;
                    state.repairP1 = state.prevRaw1;
                    state.repairP2 = state.rawLookAhead[static_cast<size_t> (rightA)];
                    state.repairP3 = state.rawLookAhead[static_cast<size_t> (rightB)];
                    ++removedEvents;
                }

                if (state.repairActive)
                {
                    const float t = static_cast<float> (state.repairIndex + 1)
                        / static_cast<float> (juce::jmax (1, state.repairTotal + 1));
                    const float t2 = t * t;
                    const float t3 = t2 * t;

                    float spline = 0.5f * (
                          (2.0f * state.repairP1)
                        + (-state.repairP0 + state.repairP2) * t
                        + (2.0f * state.repairP0 - 5.0f * state.repairP1 + 4.0f * state.repairP2 - state.repairP3) * t2
                        + (-state.repairP0 + 3.0f * state.repairP1 - 3.0f * state.repairP2 + state.repairP3) * t3);

                    // Shape: 0 = more transparent, 1 = smoother replacement.
                    const float shapeBlend = juce::jlimit (0.0f, 1.0f, shapeNorm);
                    const float transparent = 0.65f * curr + 0.35f * spline;
                    const float interpTarget = juce::jmap (shapeBlend, transparent, spline);

                    repairedOut = curr + (interpTarget - curr) * repairDepth;
                    removedOut = curr - repairedOut;

                    ++state.repairIndex;
                    if (state.repairIndex >= state.repairTotal)
                        state.repairActive = false;
                }
                else
                {
                    repairedOut = curr;
                    removedOut = 0.0f;
                }

                x[i] = repairedOut;
                removed[i] = removedOut;

                state.prevRaw2 = state.prevRaw1;
                state.prevRaw1 = curr;
                state.prevHp2 = state.prevHp1;
                state.prevHp1 = hp0;

                state.rawLookAhead.pop_front();
                state.hpLookAhead.pop_front();
            }
            else
            {
                // Startup/pipeline fill for lookahead latency.
                x[i] = 0.0f;
                removed[i] = 0.0f;
            }
        }
    }

    listenRemovedAlignSamples = lookAheadSamples;

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
        auto& dryDelay = listenRemovedDryDelay[static_cast<size_t> (ch)];
        int& dryDelayWrite = listenRemovedDryWriteIndex[static_cast<size_t> (ch)];

        for (int i = 0; i < numSamples; ++i)
        {
            const float drySample = dry[i];
            const float wetSample = x[i];

            // Sample-accurate dry alignment for true nulling in Listen Removed mode.
            dryDelay[static_cast<size_t> (dryDelayWrite)] = drySample;
            int dryRead = dryDelayWrite - listenRemovedAlignSamples;
            if (dryRead < 0)
                dryRead += listenRemovedDelayBufferSize;
            const float alignedDrySample = dryDelay[static_cast<size_t> (dryRead)];
            dryDelayWrite = (dryDelayWrite + 1) % listenRemovedDelayBufferSize;

            const float mixNow = mixSmoothed.getNextValue();
            const float gainNow = outputGainSmoothed.getNextValue();
            const float bypassNow = bypassSmoothed.getNextValue();
            const float listenNow = listenRemovedSmoothed.getNextValue();

            const float blended = alignedDrySample * (1.0f - mixNow) + wetSample * mixNow;
            const float normalOut = blended * gainNow;
            const float removedNullSample = alignedDrySample - wetSample;
            const float removedOut = removedNullSample * gainNow;
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

            removedEnergy += removedNullSample * removedNullSample;
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
