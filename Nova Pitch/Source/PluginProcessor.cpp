#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <limits>

namespace
{
void appendDiagnosticLine (const juce::String& line)
{
    const auto documentsLogFile = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Documents")
        .getChildFile ("NovaPitch")
        .getChildFile ("nova_pitch_diag.log");

    documentsLogFile.getParentDirectory().createDirectory();
    documentsLogFile.appendText (line + "\n");

    const auto tempLogFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("nova_pitch_diag.log");
    tempLogFile.appendText (line + "\n");
}
}

constexpr std::array<int, 12> NovaPitchAudioProcessor::chromaticScale;
constexpr std::array<int, 7> NovaPitchAudioProcessor::majorScale;
constexpr std::array<int, 7> NovaPitchAudioProcessor::minorScale;
constexpr std::array<int, 5> NovaPitchAudioProcessor::pentatonicScale;
constexpr std::array<int, 6> NovaPitchAudioProcessor::bluesScale;

juce::AudioProcessorValueTreeState::ParameterLayout NovaPitchAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        "key", "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        "scale", "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor", "Pentatonic", "Blues" },
        0));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "tolerance", "Tolerance",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "amount", "Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "confidenceThreshold", "Confidence",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "vibrato", "Vibrato",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        "formant", "Formant",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        "lowLatency", "Low Latency",
        false));

    return layout;
}

NovaPitchAudioProcessor::NovaPitchAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    yinBuffer.resize (yinBufferSize, 0.0f);
    for (auto& p : pitchHistory)
        p.store (0.0f);
}

NovaPitchAudioProcessor::~NovaPitchAudioProcessor()
{
}

const juce::String NovaPitchAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NovaPitchAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NovaPitchAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NovaPitchAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NovaPitchAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NovaPitchAudioProcessor::getNumPrograms()
{
    return 1;
}

int NovaPitchAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NovaPitchAudioProcessor::setCurrentProgram (int)
{
}

const juce::String NovaPitchAudioProcessor::getProgramName (int)
{
    return {};
}

void NovaPitchAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void NovaPitchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::ignoreUnused (samplesPerBlock);
    initializePitchShift();
    currentLatencySamples = normalPitchDelaySamples;
    setLatencySamples (currentLatencySamples);
    analysisScratch.clear();
    analysisScratch.reserve (static_cast<size_t> (juce::jmax (samplesPerBlock, 512)));
    
        // Cover full vocal range: bass to soprano head voice (60 Hz – 1 kHz).
        // Old cap of 400 Hz made tauMin=110 samples, making every note above G4 undetectable.
        minPitchHz = 60.0f;
        maxPitchHz = 1000.0f;
    analysisInterval = static_cast<int>(sampleRate / 20.0); // Analyze ~20 times per second
    smoothedDetectedHz = 0.0f;
    lastValidDetectedHz = 0.0f;
    blocksSinceValidPitch = 0;
    retuneLfoPhase = 0.0f;
    retuneLfoJitter = 0.0f;
    outputCompGain = 1.0f;
    targetPitchRatio = 1.0f;
    activePitchRatio = 1.0f;
    wetMixSmoothed = 0.0f;
    lockedTargetMidi = -1;
    lockedTargetAge = 0;
    pendingTargetMidi = -1;
    pendingTargetStreak = 0;
    targetSwitchCooldownBlocks = 0;
    diagWindowSamples = 0;
    diagWindowBlocks = 0;
    diagWindowDetectEvalBlocks = 0;
    diagWindowDetectValidBlocks = 0;
    diagWindowRatioComputedBlocks = 0;
    diagWindowUnityReturnBlocks = 0;
    diagWindowLockSwitches = 0;
    diagWindowTrackingLostBlocks = 0;
    diagWindowLargeRatioStepBlocks = 0;
    diagWindowAppliedCentsAbsSum = 0.0;
    diagWindowTargetCentsAbsSum = 0.0;
    diagWindowInputRmsSum = 0.0;
    diagWindowInputRmsCount = 0;
    diagPrevActivePitchRatio = 1.0f;
    diagWindowDetectedHzSum = 0.0;
    diagWindowDetectedHzCount = 0;
    diagLastLockedTargetHz = 0.0f;
}

void NovaPitchAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaPitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
   #endif
}
#endif

void NovaPitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    int numSamples = buffer.getNumSamples();
    diagWindowBlocks++;
    diagWindowSamples += static_cast<std::uint64_t> (juce::jmax (0, numSamples));
    auto* channelL = buffer.getWritePointer (0);
    auto* channelR = totalNumInputChannels > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool lowLatencyMode = apvts.getRawParameterValue ("lowLatency")->load() >= 0.5f;
    const float amountValue = apvts.getRawParameterValue ("amount")->load();
    const float toleranceValue = apvts.getRawParameterValue ("tolerance")->load();
    const float confidenceValue = apvts.getRawParameterValue ("confidenceThreshold")->load();
    const float vibratoValue = apvts.getRawParameterValue ("vibrato")->load();
    const float formantValue = apvts.getRawParameterValue ("formant")->load();
    const float amountNorm = juce::jlimit (0.0f, 1.0f, amountValue / 100.0f);
    // UI semantics: 0 = slow, 100 = fast.
    const float retuneSpeedNorm = amountNorm;
    const int desiredLatencySamples = lowLatencyMode ? lowLatencyPitchDelaySamples : normalPitchDelaySamples;
    juce::ignoreUnused (toleranceValue, confidenceValue);

    if (desiredLatencySamples != currentLatencySamples)
    {
        currentLatencySamples = desiredLatencySamples;
        setLatencySamples (currentLatencySamples);

        // Re-anchor read heads when latency mode changes so we don't spend
        // seconds drifting from an invalid delay target (audible skip/delay artifacts).
        for (int ch = 0; ch < 2; ++ch)
        {
            const int write = pitchWriteIndex[static_cast<size_t> (ch)];
            float newRead = static_cast<float> (write - currentLatencySamples);
            while (newRead < 0.0f)
                newRead += static_cast<float> (pitchShiftBufferSize);
            while (newRead >= static_cast<float> (pitchShiftBufferSize))
                newRead -= static_cast<float> (pitchShiftBufferSize);

            pitchReadPos[static_cast<size_t> (ch)] = newRead;
            pitchOutputSmoother[static_cast<size_t> (ch)] = 0.0f;
            pitchDryBlendSmoothed[static_cast<size_t> (ch)] = 0.0f;
        }
    }

    // Debug: Log parameter values every 100 blocks to diagnose engine engagement
    static int debugCounter = 0;
    if (++debugCounter % 100 == 0)
    {
        DBG("Nova Pitch params: amount=" << amountValue << " tolerance=" << toleranceValue 
            << " confidence=" << confidenceValue << " vibrato=" << vibratoValue << " formant=" << formantValue);
    }

    // Mono-focused detection, stereo-safe processing.
    if (analysisScratch.size() < static_cast<size_t> (numSamples))
        analysisScratch.resize (static_cast<size_t> (numSamples), 0.0f);

    float* analysisData = analysisScratch.data();
    if (channelR != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
            analysisData[i] = 0.5f * (channelL[i] + channelR[i]);
    }
    else
    {
        std::copy (channelL, channelL + numSamples, analysisData);
    }

    const float inputRms = computeBufferRms (buffer);

    if (dryScratchL.size() < static_cast<size_t> (numSamples))
        dryScratchL.resize (static_cast<size_t> (numSamples), 0.0f);
    std::copy (channelL, channelL + numSamples, dryScratchL.data());
    if (channelR != nullptr)
    {
        if (dryScratchR.size() < static_cast<size_t> (numSamples))
            dryScratchR.resize (static_cast<size_t> (numSamples), 0.0f);
        std::copy (channelR, channelR + numSamples, dryScratchR.data());
    }

    // Track faster when Retune Speed is high (amount near 100).
    const bool fastRetuneTracking = retuneSpeedNorm > 0.70f;
    const int intervalDivider = (lowLatencyMode || fastRetuneTracking) ? 1 : 2;

    // Perform pitch detection periodically
    if (blockCount % intervalDivider == 0)
    {
        diagWindowDetectEvalBlocks++;
        float detectedHz = detectPitchYIN (analysisData, numSamples);
        const bool fastCorrectionMode = retuneSpeedNorm > 0.70f;
        // Always smooth detector output; disabling smoothing in hard mode caused
        // large frame-to-frame ratio jumps and audible skip bursts.
        detectedHz = smoothDetectedPitch (detectedHz, inputRms, lowLatencyMode || fastRetuneTracking);
        detectedPitch.store (detectedHz);

        bool hasUsablePitch = detectedHz > minPitchHz - 10.0f && detectedHz < maxPitchHz + 10.0f;

        if (hasUsablePitch)
        {
            diagWindowDetectValidBlocks++;
            lastValidDetectedHz = detectedHz;
            blocksSinceValidPitch = 0;
        }
        else
        {
            ++blocksSinceValidPitch;

            // Hard-tune should not collapse to dry passthrough on brief detector misses.
            // Hold the most recent valid estimate for a short window while signal is present.
            const bool canHoldLastPitch = fastCorrectionMode
                && inputRms > 0.001f
                && lastValidDetectedHz > minPitchHz - 10.0f
                && lastValidDetectedHz < maxPitchHz + 10.0f
                && blocksSinceValidPitch <= 12;

            if (canHoldLastPitch)
            {
                detectedHz = lastValidDetectedHz;
                hasUsablePitch = true;
                pitchConfidence.store (juce::jmax (pitchConfidence.load(), 0.20f));
            }
        }

        if (hasUsablePitch)
        {
            const int candidateMidiNote = quantizeToScale (detectedHz);
            const float detectedMidi = 69.0f + 12.0f * std::log2 (juce::jmax (1.0f, detectedHz) / 440.0f);

            if (targetSwitchCooldownBlocks > 0)
                --targetSwitchCooldownBlocks;

            if (lockedTargetMidi < 0)
            {
                lockedTargetMidi = candidateMidiNote;
                lockedTargetAge = 0;
                pendingTargetMidi = -1;
                pendingTargetStreak = 0;
                targetSwitchCooldownBlocks = 0;
            }
            else if (candidateMidiNote != lockedTargetMidi)
            {
                ++lockedTargetAge;

                if (candidateMidiNote == pendingTargetMidi)
                    ++pendingTargetStreak;
                else
                {
                    pendingTargetMidi = candidateMidiNote;
                    pendingTargetStreak = 1;
                }

                // Ignore implausibly large jumps in one analysis step in fast/hard mode.
                // These are usually detector octave flips that sound like garble artifacts.
                if (std::abs (candidateMidiNote - lockedTargetMidi) > 5)
                {
                    // Keep existing lock until detector stabilizes.
                }
                else
                {
                // Keep strong hysteresis in fast mode to prevent adjacent-note ping-pong.
                const float switchHysteresis = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.95f, 0.75f);
                const float detectRateHz = static_cast<float> (currentSampleRate)
                    / static_cast<float> (juce::jmax (1, numSamples * intervalDivider));
                const float minHoldSeconds = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.22f, 0.16f);
                const int minHoldBlocks = static_cast<int> (std::round (
                    juce::jmax (3.0f, detectRateHz * minHoldSeconds)));
                const bool switchUp = candidateMidiNote > lockedTargetMidi
                                   && detectedMidi > static_cast<float> (lockedTargetMidi) + switchHysteresis;
                const bool switchDown = candidateMidiNote < lockedTargetMidi
                                     && detectedMidi < static_cast<float> (lockedTargetMidi) - switchHysteresis;
                const bool confidentSwitch = pitchConfidence.load() > 0.45f;
                const float stableSeconds = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.10f, 0.14f);
                const int requiredStableHits = static_cast<int> (std::round (
                    juce::jmax (2.0f, detectRateHz * stableSeconds)));
                if ((switchUp || switchDown)
                    && confidentSwitch
                    && lockedTargetAge >= minHoldBlocks
                    && pendingTargetStreak >= requiredStableHits
                    && targetSwitchCooldownBlocks == 0)
                {
                    lockedTargetMidi = candidateMidiNote;
                    lockedTargetAge = 0;
                    pendingTargetMidi = -1;
                    pendingTargetStreak = 0;
                    const float cooldownSeconds = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.10f, 0.12f);
                    targetSwitchCooldownBlocks = static_cast<int> (std::round (
                        juce::jmax (2.0f, detectRateHz * cooldownSeconds)));
                    diagWindowLockSwitches++;
                }
                }
            }
            else
            {
                ++lockedTargetAge;
                pendingTargetMidi = -1;
                pendingTargetStreak = 0;
            }

            const int targetMidiNote = lockedTargetMidi;
            const float targetHz = getTargetPitchHz (targetMidiNote);

            // Align detector octave to the selected target note before ratio computation.
            // This prevents persistent octave-low detections from forcing near-max upward shifts.
            float octaveAlignedDetectedHz = detectedHz;
            if (targetHz > 1.0f)
            {
                while (octaveAlignedDetectedHz < targetHz * 0.70710678f)
                    octaveAlignedDetectedHz *= 2.0f;
                while (octaveAlignedDetectedHz > targetHz * 1.41421356f)
                    octaveAlignedDetectedHz *= 0.5f;
            }

            float pitchRatio = computeRetuneRatio (octaveAlignedDetectedHz, targetHz, inputRms, lowLatencyMode);
            diagWindowRatioComputedBlocks++;
            if (std::abs (pitchRatio - 1.0f) < 0.001f)
                diagWindowUnityReturnBlocks++;

            if (vibratoValue > 0.001f)
            {
                applyVibrato (pitchRatio, static_cast<float> (currentSampleRate), numSamples,
                              vibratoValue);
            }
            // Keep correction bounded to a stable range for the current time-domain shifter.
            // Wider ranges were producing read-head stress and fastest-mode skip artifacts.
            targetPitchRatio = juce::jlimit (0.82f, 1.22f, pitchRatio);
            const float correctedHz = octaveAlignedDetectedHz * pitchRatio;
            correctedPitch.store (correctedHz);

            if (debugCounter % 100 == 2)
            {
                DBG("Nova Pitch: detectedHz=" << detectedHz << " targetHz=" << targetHz << " pitchRatio=" << pitchRatio);
            }
        }
        else
        {
            if (debugCounter % 100 == 2)
            {
                DBG("Nova Pitch: detectedHz=" << detectedHz << " out of range=[" << (minPitchHz - 10.0f) << "-" << (maxPitchHz + 10.0f) << "]");
            }

            // Do not hard-reset target on short detector dropouts; this causes ratio ping-pong
            // and is perceived as skip/burst artifacts at fastest retune.
            const int dropoutHoldBlocks = lowLatencyMode ? 18 : 30;
            if (blocksSinceValidPitch <= dropoutHoldBlocks)
            {
                pitchConfidence.store (juce::jmax (0.06f, pitchConfidence.load() * 0.96f));
            }
            else
            {
                targetPitchRatio = 1.0f;
                correctedPitch.store (0.0f);
                pitchConfidence.store (0.0f);
                lockedTargetMidi = -1;
                lockedTargetAge = 0;
                pendingTargetMidi = -1;
                pendingTargetStreak = 0;
                targetSwitchCooldownBlocks = 0;
            }
        }
    }

    // AutoTune/MetaTune architecture:
    // - Retune knob = SPEED only (LP filter glide time toward target)
    // - Correction is always 100% toward target note — no depth scaling
    // - Shifted audio replaces input entirely — no wet/dry blend (blend causes doubling)
    const float trackingConfidence = juce::jlimit (0.0f, 1.0f, pitchConfidence.load());
    const bool signalTooLow = inputRms < 0.0002f;
    // Go to dry bypass immediately when confidence is zero — no grace period.
    // A 24-block grace window caused the shifter to run 24 blocks without a valid pitch
    // then abruptly switch to dry, producing an audible skip/comb artifact every playback start.
    const bool trackingLost = signalTooLow || trackingConfidence < 0.01f;
    if (trackingLost)
        diagWindowTrackingLostBlocks++;

    const float desiredWet = trackingLost ? 0.0f : 1.0f;
    const float wetSlew = trackingLost ? 0.14f : 0.03f;
    wetMixSmoothed += (desiredWet - wetMixSmoothed) * wetSlew;
    const float wetMix = juce::jlimit (0.0f, 1.0f, wetMixSmoothed);

    // Retune stays active across the full knob range; knob controls speed only.
    // Single smooth LP filter toward target — no per-block clamping (eliminates double-limiting oscillations).
    if (! trackingLost)
    {
        const float speedCoeff = lowLatencyMode
            ? juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.03f, 0.10f)
            : juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.025f, 0.085f);

        // Smooth target-ratio motion first, then apply retune-speed glide.
        const float targetSmoothing = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.08f, 0.20f);
        targetRatioSmoothed += (targetPitchRatio - targetRatioSmoothed) * targetSmoothing;

        // Single control law: LP toward target followed by hard slew-limit.
        // This avoids the prior double-update that could still jump too far in one block.
        const float desiredRatio = activePitchRatio + (targetRatioSmoothed - activePitchRatio) * speedCoeff;
        const float maxStep = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.0012f, 0.0030f);
        const float step = juce::jlimit (-maxStep, maxStep, desiredRatio - activePitchRatio);
        activePitchRatio += step;

        activePitchRatio = juce::jlimit (0.82f, 1.22f, activePitchRatio);

        const float ratioStep = std::abs (activePitchRatio - diagPrevActivePitchRatio);
        if (ratioStep > 0.008f)
            diagWindowLargeRatioStepBlocks++;
        diagPrevActivePitchRatio = activePitchRatio;

        const float appliedCents = std::abs (1200.0f * std::log2 (juce::jmax (0.001f, activePitchRatio)));
        const float targetCents = std::abs (1200.0f * std::log2 (juce::jmax (0.001f, targetPitchRatio)));
        diagWindowAppliedCentsAbsSum += static_cast<double> (appliedCents);
        diagWindowTargetCentsAbsSum += static_cast<double> (targetCents);

        // Apply shift only when tracking is valid.
        processCircularBufferPitchShift (channelL, numSamples, activePitchRatio, 0, lowLatencyMode, retuneSpeedNorm);
        if (channelR != nullptr)
            processCircularBufferPitchShift (channelR, numSamples, activePitchRatio, 1, lowLatencyMode, retuneSpeedNorm);
    }
    else
    {
        // Hard safety path: when tracking is lost, bypass shifter entirely to avoid skip/delay artifacts.
        activePitchRatio += (1.0f - activePitchRatio) * 0.06f;
        targetRatioSmoothed += (1.0f - targetRatioSmoothed) * 0.08f;
        activePitchRatio = 1.0f;
        targetRatioSmoothed = 1.0f;

        std::copy (dryScratchL.data(), dryScratchL.data() + numSamples, channelL);
        if (channelR != nullptr)
            std::copy (dryScratchR.data(), dryScratchR.data() + numSamples, channelR);
    }

    if (! trackingLost)
    {
        applyFormantShaper (channelL, numSamples, formantValue, 0);
        if (channelR != nullptr)
            applyFormantShaper (channelR, numSamples, formantValue, 1);
    }

    // Track input RMS and detected pitch every block for diagnostics.
    diagWindowInputRmsSum += static_cast<double> (inputRms);
    ++diagWindowInputRmsCount;
    const float diagDetHz = detectedPitch.load();
    if (diagDetHz > 0.0f) { diagWindowDetectedHzSum += static_cast<double> (diagDetHz); ++diagWindowDetectedHzCount; }
    if (lockedTargetMidi >= 0) diagLastLockedTargetHz = getTargetPitchHz (lockedTargetMidi);

    if (! trackingLost && wetMix < 0.999f)
    {
        const float dryMix = 1.0f - wetMix;
        auto* dryL = dryScratchL.data();
        for (int i = 0; i < numSamples; ++i)
            channelL[i] = channelL[i] * wetMix + dryL[i] * dryMix;

        if (channelR != nullptr)
        {
            auto* dryR = dryScratchR.data();
            for (int i = 0; i < numSamples; ++i)
                channelR[i] = channelR[i] * wetMix + dryR[i] * dryMix;
        }
    }

    if (! trackingLost)
        applyOutputManagement (buffer, inputRms);

    const std::uint64_t diagMinSamples = static_cast<std::uint64_t> (juce::jmax (1.0, currentSampleRate * 2.0));
    if (diagWindowSamples >= diagMinSamples)
    {
        const double windowSeconds = static_cast<double> (diagWindowSamples) / juce::jmax (1.0, currentSampleRate);
        const double detectValidRatio = (diagWindowDetectEvalBlocks > 0)
            ? static_cast<double> (diagWindowDetectValidBlocks) / static_cast<double> (diagWindowDetectEvalBlocks)
            : 0.0;
        const double unityReturnRatio = (diagWindowRatioComputedBlocks > 0)
            ? static_cast<double> (diagWindowUnityReturnBlocks) / static_cast<double> (diagWindowRatioComputedBlocks)
            : 0.0;
        const double lockSwitchRate = (windowSeconds > 1.0e-6)
            ? static_cast<double> (diagWindowLockSwitches) / windowSeconds
            : 0.0;
        const double trackingLostRatio = (diagWindowBlocks > 0)
            ? static_cast<double> (diagWindowTrackingLostBlocks) / static_cast<double> (diagWindowBlocks)
            : 0.0;
        const double largeStepRatio = (diagWindowBlocks > 0)
            ? static_cast<double> (diagWindowLargeRatioStepBlocks) / static_cast<double> (diagWindowBlocks)
            : 0.0;
        const double avgAppliedCents = (diagWindowBlocks > 0)
            ? diagWindowAppliedCentsAbsSum / static_cast<double> (diagWindowBlocks)
            : 0.0;
        const double avgTargetCents = (diagWindowBlocks > 0)
            ? diagWindowTargetCentsAbsSum / static_cast<double> (diagWindowBlocks)
            : 0.0;
        const double avgInputRms = (diagWindowInputRmsCount > 0)
            ? diagWindowInputRmsSum / static_cast<double> (diagWindowInputRmsCount)
            : 0.0;

        const double avgDetectedHz = (diagWindowDetectedHzCount > 0)
            ? diagWindowDetectedHzSum / static_cast<double> (diagWindowDetectedHzCount)
            : 0.0;
        const float lockedTargetHz = diagLastLockedTargetHz;

        juce::ignoreUnused (detectValidRatio, unityReturnRatio, lockSwitchRate,
                            trackingLostRatio, largeStepRatio, avgAppliedCents, avgTargetCents, avgInputRms,
                            avgDetectedHz, lockedTargetHz);

        const juce::String diagLine = juce::String ("Nova Pitch DIAG")
            + " secs=" + juce::String (windowSeconds, 2)
            + " detectValidRatio=" + juce::String (detectValidRatio, 3)
            + " unityReturnRatio=" + juce::String (unityReturnRatio, 3)
            + " lockSwitchRateHz=" + juce::String (lockSwitchRate, 3)
            + " trackingLostRatio=" + juce::String (trackingLostRatio, 3)
            + " largeRatioStepRatio=" + juce::String (largeStepRatio, 3)
            + " avgAppliedCents=" + juce::String (avgAppliedCents, 1)
            + " avgTargetCents=" + juce::String (avgTargetCents, 1)
            + " avgInputRms=" + juce::String (avgInputRms, 4)
            + " avgDetectedHz=" + juce::String (avgDetectedHz, 1)
            + " lockedTargetHz=" + juce::String (lockedTargetHz, 1)
            + " speedNorm=" + juce::String (retuneSpeedNorm, 3)
            + " lowLatency=" + juce::String (lowLatencyMode ? 1 : 0);

        DBG (diagLine);
        appendDiagnosticLine (juce::Time::getCurrentTime().toString (true, true) + " " + diagLine);

        diagWindowSamples = 0;
        diagWindowBlocks = 0;
        diagWindowDetectEvalBlocks = 0;
        diagWindowDetectValidBlocks = 0;
        diagWindowRatioComputedBlocks = 0;
        diagWindowUnityReturnBlocks = 0;
        diagWindowLockSwitches = 0;
        diagWindowTrackingLostBlocks = 0;
        diagWindowLargeRatioStepBlocks = 0;
        diagWindowAppliedCentsAbsSum = 0.0;
        diagWindowTargetCentsAbsSum = 0.0;
        diagWindowInputRmsSum = 0.0;
        diagWindowInputRmsCount = 0;
        diagWindowDetectedHzSum = 0.0;
        diagWindowDetectedHzCount = 0;
    }

    blockCount++;
}

float NovaPitchAudioProcessor::smoothDetectedPitch (float rawDetectedHz, float signalRms, bool lowLatencyMode)
{
    if (rawDetectedHz <= 0.0f)
        return smoothedDetectedHz;

    if (smoothedDetectedHz <= 0.0f)
    {
        smoothedDetectedHz = rawDetectedHz;
        return smoothedDetectedHz;
    }

    const float energy = juce::jlimit (0.0f, 1.0f, signalRms * 4.0f);
    const float delta = std::abs (rawDetectedHz - smoothedDetectedHz);
    const float movement = juce::jlimit (0.0f, 1.0f, delta / 35.0f);

    // Auto behavior: track fast on unstable/live-like movement, smooth on steady playback-like content.
    float alphaFast = lowLatencyMode ? 0.82f : 0.68f;
    float alphaSlow = lowLatencyMode ? 0.36f : 0.18f;
    float alpha = juce::jlimit (alphaSlow, alphaFast, alphaSlow + movement * 0.45f + energy * 0.15f);

    smoothedDetectedHz += (rawDetectedHz - smoothedDetectedHz) * alpha;
    return smoothedDetectedHz;
}

float NovaPitchAudioProcessor::computeRetuneRatio (float detectedHz, float targetHz, float /*signalRms*/, bool /*lowLatencyMode*/)
{
    const float amountNorm = apvts.getRawParameterValue ("amount")->load() / 100.0f;
    const float retuneSpeedNorm = juce::jlimit (0.0f, 1.0f, amountNorm);

    const float toleranceNorm = apvts.getRawParameterValue ("tolerance")->load() / 100.0f;

    const float fullRatio = targetHz / juce::jmax (1.0f, detectedHz);

    // Tolerance window: if singer is already close enough, keep ratio at unity.
    const float centsError = std::abs (1200.0f * std::log2 (juce::jmax (0.001f, std::abs (fullRatio))));
    const float toleranceScale = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 1.0f, 0.15f);
    float toleranceCents = toleranceNorm * 45.0f * toleranceScale;
    // In fastest mode, disable the tolerance dead-zone so tuning remains clearly active.
    if (retuneSpeedNorm > 0.90f)
        toleranceCents = 0.0f;
    if (centsError < toleranceCents)
        return 1.0f;

    // MetaTune-style behavior: always compute full correction ratio,
    // then let downstream glide speed determine how quickly we reach it.
    return juce::jlimit (0.50f, 2.00f, fullRatio);
}

void NovaPitchAudioProcessor::applyVibrato (float& pitchRatio, float sampleRate, int numSamples, float vibratoParam)
{
    // Effective vibrato range lock: 0..30 usable, mapped from 0..100 UI value.
    const float vibEffective = juce::jlimit (0.0f, 30.0f, vibratoParam * 0.3f);
    if (vibEffective <= 0.001f)
        return;

    const float depth = (vibEffective / 30.0f);
    const float rateHz = 4.0f + depth * 2.6f; // natural low, stylized high
    const float randomness = 0.015f * depth;
    retuneLfoJitter = 0.94f * retuneLfoJitter + randomness * juce::Random::getSystemRandom().nextFloat();

    const float phaseAdvance = juce::MathConstants<float>::twoPi * rateHz * (static_cast<float> (numSamples) / sampleRate);
    retuneLfoPhase = std::fmod (retuneLfoPhase + phaseAdvance, juce::MathConstants<float>::twoPi);

    const float lfo = std::sin (retuneLfoPhase + retuneLfoJitter * juce::MathConstants<float>::twoPi);
    const float centsDepth = 4.0f + depth * 18.0f;
    const float ratioDepth = std::pow (2.0f, (centsDepth / 1200.0f) * lfo) - 1.0f;
    pitchRatio *= (1.0f + ratioDepth);
}

void NovaPitchAudioProcessor::applyFormantShaper (float* channelData, int numSamples, float formantParam, int channelIndex)
{
    // Formant knob semantics: 0 must be neutral (no coloration/artifacts).
    const float amount = juce::jlimit (0.0f, 1.0f, formantParam / 100.0f);
    if (amount < 0.01f)
        return;

    const float a = juce::jlimit (-0.16f, 0.16f, amount * 0.16f);
    const float mix = juce::jlimit (0.0f, 0.22f, amount * 0.22f);

    auto& s1 = formantAllPassState[static_cast<size_t> (channelIndex)][0];
    auto& s2 = formantAllPassState[static_cast<size_t> (channelIndex)][1];

    for (int i = 0; i < numSamples; ++i)
    {
        float x = channelData[i];
        // Two-stage all-pass vocal-tract shaper (phase/formant emphasis, not simple EQ gain).
        float y1 = -a * x + s1;
        s1 = x + a * y1;

        float y2 = -a * y1 + s2;
        s2 = y1 + a * y2;

        channelData[i] = x * (1.0f - mix) + y2 * mix;
    }
}

void NovaPitchAudioProcessor::applyOutputManagement (juce::AudioBuffer<float>& buffer, float inputRms)
{
    const float outRms = computeBufferRms (buffer);
    if (inputRms > 1.0e-6f && outRms > 1.0e-6f)
    {
        // Keep plugin-enabled level perceptually equal or slightly above bypass.
        // A small fixed make-up factor avoids the recurring "enabled sounds quieter" report.
        const float rmsRatio = inputRms / outRms;
        const float targetGain = juce::jlimit (1.02f, 1.40f, juce::jmax (1.02f, rmsRatio * 1.02f));
        outputCompGain = 0.65f * outputCompGain + 0.35f * targetGain;
    }
    else
    {
        outputCompGain += (1.0f - outputCompGain) * 0.05f;
    }

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < samples; ++i)
            data[i] *= outputCompGain;
    }
}

float NovaPitchAudioProcessor::computeBufferRms (const juce::AudioBuffer<float>& buffer) const
{
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return 0.0f;

    double sum = 0.0;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < samples; ++i)
            sum += static_cast<double> (data[i] * data[i]);
    }

    const double denom = static_cast<double> (channels * samples);
    return static_cast<float> (std::sqrt (sum / juce::jmax (1.0, denom)));
}

float NovaPitchAudioProcessor::detectPitchYIN (const float* samples, int numSamples)
{
    // Accumulate samples into analysis window.
    for (int i = 0; i < numSamples && yinWriteIndex < yinBufferSize; ++i)
        yinBuffer[static_cast<size_t> (yinWriteIndex++)] = samples[i];

    if (yinWriteIndex < yinBufferSize)
        return detectedPitch.load();

    auto advanceAnalysisWindow = [&]()
    {
        const int halfBuf = yinBufferSize / 2;
        std::copy (yinBuffer.begin() + halfBuf, yinBuffer.end(), yinBuffer.begin());
        yinWriteIndex = halfBuf;
    };

    const int halfBuf = yinBufferSize / 2;
    const int tauMin  = juce::jmax (2, static_cast<int> (currentSampleRate / maxPitchHz));
    const int tauMax  = juce::jmin (halfBuf - 1, static_cast<int> (currentSampleRate / minPitchHz));

    if (tauMin >= tauMax)
    {
        advanceAnalysisWindow();
        return detectedPitch.load();
    }

    // Lightweight voiced gate to skip expensive work on near-silence windows.
    double windowEnergy = 0.0;
    for (int i = 0; i < halfBuf; ++i)
    {
        const float x = yinBuffer[static_cast<size_t> (i)];
        windowEnergy += static_cast<double> (x) * static_cast<double> (x);
    }
    const float windowRms = static_cast<float> (std::sqrt (windowEnergy / static_cast<double> (juce::jmax (1, halfBuf))));
    if (windowRms < 2.0e-4f)
    {
        advanceAnalysisWindow();
        return detectedPitch.load();
    }

    auto finalizeDetectedHz = [&] (float hz, float confidence) -> float
    {
        pitchConfidence.store (juce::jlimit (0.0f, 1.0f, confidence));
        pitchHistory[static_cast<size_t> (historyIndex)].store (hz);
        historyIndex = (historyIndex + 1) % pitchHistorySize;

        advanceAnalysisWindow();
        return hz;
    };

    auto foldIntoRange = [&] (float hz) -> float
    {
        if (! std::isfinite (hz) || hz <= 0.0f)
            return 0.0f;

        const float minHz = minPitchHz - 10.0f;
        const float maxHz = maxPitchHz + 10.0f;

        while (hz > maxHz)
            hz *= 0.5f;
        while (hz < minHz)
            hz *= 2.0f;

        return (hz >= minHz && hz <= maxHz) ? hz : 0.0f;
    };

    auto alignToReferenceOctave = [&] (float hz) -> float
    {
        hz = foldIntoRange (hz);
        if (hz <= 0.0f)
            return 0.0f;

        float referenceHz = 0.0f;
        if (lastValidDetectedHz > minPitchHz - 10.0f && lastValidDetectedHz < maxPitchHz + 10.0f)
            referenceHz = lastValidDetectedHz;
        else if (smoothedDetectedHz > minPitchHz - 10.0f && smoothedDetectedHz < maxPitchHz + 10.0f)
            referenceHz = smoothedDetectedHz;
        else if (detectedPitch.load() > minPitchHz - 10.0f && detectedPitch.load() < maxPitchHz + 10.0f)
            referenceHz = detectedPitch.load();

        if (referenceHz <= 0.0f)
            return hz;

        auto octaveDistance = [] (float a, float b)
        {
            return std::abs (std::log2 (juce::jmax (1.0f, a) / juce::jmax (1.0f, b)));
        };

        float bestHz = hz;
        float bestDist = octaveDistance (hz, referenceHz);

        float up = hz;
        while (up * 2.0f <= maxPitchHz + 10.0f)
        {
            up *= 2.0f;
            const float d = octaveDistance (up, referenceHz);
            if (d < bestDist)
            {
                bestDist = d;
                bestHz = up;
            }
        }

        float down = hz;
        while (down * 0.5f >= minPitchHz - 10.0f)
        {
            down *= 0.5f;
            const float d = octaveDistance (down, referenceHz);
            if (d < bestDist)
            {
                bestDist = d;
                bestHz = down;
            }
        }

        return foldIntoRange (bestHz);
    };

    auto estimateByAutoCorrelation = [&]() -> float
    {
        // Autocorrelation-first and decimated for real-time stability in fast-retune mode.
        double mean = 0.0;
        for (int i = 0; i < halfBuf; ++i)
            mean += static_cast<double> (yinBuffer[static_cast<size_t> (i)]);
        mean /= static_cast<double> (juce::jmax (1, halfBuf));

        int bestTau = tauMin;
        float bestCorr = -1.0f;
        constexpr int sampleStep = 2;
        constexpr int tauStep = 2;

        for (int tau = tauMin; tau < tauMax; tau += tauStep)
        {
            double xy = 0.0;
            double xx = 0.0;
            double yy = 0.0;

            for (int i = 0; i < halfBuf; i += sampleStep)
            {
                const float x = yinBuffer[static_cast<size_t> (i)] - static_cast<float> (mean);
                const float y = yinBuffer[static_cast<size_t> (i + tau)] - static_cast<float> (mean);
                xy += static_cast<double> (x) * static_cast<double> (y);
                xx += static_cast<double> (x) * static_cast<double> (x);
                yy += static_cast<double> (y) * static_cast<double> (y);
            }

            const double denom = std::sqrt (juce::jmax (1.0e-12, xx * yy));
            const float corr = static_cast<float> (xy / denom);
            if (std::isfinite (corr) && corr > bestCorr)
            {
                bestCorr = corr;
                bestTau = tau;
            }
        }

        const float rawHz = static_cast<float> (currentSampleRate) / static_cast<float> (juce::jmax (1, bestTau));
        const float hz = alignToReferenceOctave (rawHz);
        if (hz > 0.0f && bestCorr > 0.005f)
        {
            const float conf = juce::jlimit (0.06f, 0.90f, (bestCorr - 0.005f) / 0.50f);
            return finalizeDetectedHz (hz, conf);
        }

        return 0.0f;
    };

    auto estimateByZeroCrossing = [&]() -> float
    {
        // Count both upward AND downward crossings for a noise-tolerant estimate.
        // Total crossings / 2 gives the number of full cycles in halfBuf samples.
        int upCrossings = 0;
        int downCrossings = 0;
        float prev = yinBuffer[0];
        for (int i = 1; i < halfBuf; ++i)
        {
            const float x = yinBuffer[static_cast<size_t> (i)];
            if (prev <= 0.0f && x > 0.0f) ++upCrossings;
            if (prev >= 0.0f && x < 0.0f) ++downCrossings;
            prev = x;
        }
        const int totalCrossings = upCrossings + downCrossings;
        // totalCrossings/2 full cycles in halfBuf samples
        const float rawHz = (totalCrossings > 0)
            ? static_cast<float> (totalCrossings) * 0.5f * static_cast<float> (currentSampleRate)
              / static_cast<float> (juce::jmax (1, halfBuf))
            : 0.0f;
        const float hz = alignToReferenceOctave (rawHz);
        if (hz > 0.0f)
        {
            return finalizeDetectedHz (hz, 0.12f);
        }

        // Last-resort fallback: keep smoothed estimate alive when signal has energy.
        if (smoothedDetectedHz > minPitchHz - 10.0f && smoothedDetectedHz < maxPitchHz + 10.0f)
        {
            pitchConfidence.store (juce::jmax (pitchConfidence.load(), 0.08f));
            return smoothedDetectedHz;
        }

        // Emergency voiced fallback: prefer last known valid/estimated pitch when input energy is present.
        if (windowRms > 0.004f)
        {
            float fallbackHz = 0.0f;
            if (lastValidDetectedHz > minPitchHz - 10.0f && lastValidDetectedHz < maxPitchHz + 10.0f)
                fallbackHz = lastValidDetectedHz;
            else if (detectedPitch.load() > minPitchHz - 10.0f && detectedPitch.load() < maxPitchHz + 10.0f)
                fallbackHz = detectedPitch.load();
            else
                fallbackHz = 220.0f;

            return finalizeDetectedHz (fallbackHz, 0.06f);
        }

        advanceAnalysisWindow();
        return detectedPitch.load();
    };

    const float autoCorrHz = estimateByAutoCorrelation();
    if (autoCorrHz > 0.0f)
        return autoCorrHz;

    return estimateByZeroCrossing();
}

float NovaPitchAudioProcessor::getYINThreshold() const noexcept
{
    const float confidence = juce::jlimit (0.0f, 1.0f,
                                           apvts.getRawParameterValue ("confidenceThreshold")->load() / 100.0f);

    // Keep YIN threshold in a practical range. Very high thresholds (close to 1.0)
    // cause false locks and octave errors, which become skip artifacts at fast retune.
    // Higher confidence setting = stricter detection.
    return juce::jmap (confidence, 0.0f, 1.0f, 0.22f, 0.08f);
}

int NovaPitchAudioProcessor::quantizeToScale (float pitchHz)
{
    const float safeHz = juce::jmax (1.0f, pitchHz);
    const float detectedMidi = 69.0f + 12.0f * std::log2 (safeHz / 440.0f);
    const int centerMidi = static_cast<int> (std::round (detectedMidi));
    const int key = static_cast<int> (apvts.getRawParameterValue ("key")->load());
    const auto scale = static_cast<Scale> (apvts.getRawParameterValue ("scale")->load());

    auto inScale = [&] (int semitoneFromKey)
    {
        const int degree = ((semitoneFromKey % 12) + 12) % 12;

        switch (scale)
        {
            case Chromatic:  return true;
            case Major:      return std::find (majorScale.begin(), majorScale.end(), degree) != majorScale.end();
            case Minor:      return std::find (minorScale.begin(), minorScale.end(), degree) != minorScale.end();
            case Pentatonic: return std::find (pentatonicScale.begin(), pentatonicScale.end(), degree) != pentatonicScale.end();
            case Blues:      return std::find (bluesScale.begin(), bluesScale.end(), degree) != bluesScale.end();
            default:         return true;
        }
    };

    int bestMidi = centerMidi;
    float bestDistance = std::numeric_limits<float>::max();

    for (int midi = centerMidi - 24; midi <= centerMidi + 24; ++midi)
    {
        const int semitoneFromKey = midi - key;
        if (! inScale (semitoneFromKey))
            continue;

        const float distance = std::abs (static_cast<float> (midi) - detectedMidi);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestMidi = midi;
        }
    }

    return bestMidi;
}

float NovaPitchAudioProcessor::getTargetPitchHz (int midiNote) const
{
    return 440.0f * std::pow (2.0f, (static_cast<float> (midiNote) - 69.0f) / 12.0f);
}

void NovaPitchAudioProcessor::initializePitchShift()
{
    for (auto& channel : pitchDelay)
        for (auto& s : channel)
            s = 0.0f;

    // Initialize for single-head delay-resampling shifter:
    // write is placed ahead of read by the normal delay amount so startup
    // begins on a valid anchor and avoids initial warble/skip artifacts.
    pitchReadPos[0] = 0.0f;
    pitchReadPos[1] = 0.0f;
    pitchCrossfadePhase[0] = 0.0f;
    pitchCrossfadePhase[1] = 0.0f;
    pitchWriteIndex[0] = normalPitchDelaySamples;
    pitchWriteIndex[1] = normalPitchDelaySamples;
    pitchShiftRatioSmoothed[0] = 1.0f;
    pitchShiftRatioSmoothed[1] = 1.0f;
    pitchOutputSmoother[0] = 0.0f;
    pitchOutputSmoother[1] = 0.0f;
    pitchDryBlendSmoothed[0] = 0.0f;
    pitchDryBlendSmoothed[1] = 0.0f;
    formantAllPassState = {};
}

void NovaPitchAudioProcessor::processCircularBufferPitchShift (float* channelData, int numSamples, float pitchRatio,
                                                              int channelIndex, bool lowLatencyMode, float retuneSpeedNorm)
{
    const int channel = juce::jlimit (0, 1, channelIndex);
    auto& channelDelay = pitchDelay[static_cast<size_t> (channel)];
    auto& readPos = pitchReadPos[static_cast<size_t> (channel)];
    auto& crossfadePhase = pitchCrossfadePhase[static_cast<size_t> (channel)];
    auto& writeIdx = pitchWriteIndex[static_cast<size_t> (channel)];
    auto& ratioSmoothed = pitchShiftRatioSmoothed[static_cast<size_t> (channel)];
    auto& outputSmoother = pitchOutputSmoother[static_cast<size_t> (channel)];
    auto& dryBlendSmoothed = pitchDryBlendSmoothed[static_cast<size_t> (channel)];

    const int bufferSize = pitchShiftBufferSize;
    const float clampedRatio = juce::jlimit (0.82f, 1.22f, pitchRatio);
    const float ratioDelta = std::abs (clampedRatio - ratioSmoothed);
    const bool hardTuneMode = retuneSpeedNorm > 0.90f;
    // Keep ratio changes smooth enough to avoid time-domain read-head jumps.
    const float ratioSmoothing = hardTuneMode
        ? 0.10f
        : juce::jlimit (0.08f, 0.20f, 0.08f + ratioDelta * 0.30f);
    ratioSmoothed += (clampedRatio - ratioSmoothed) * ratioSmoothing;
    const float effectiveRatio = juce::jlimit (0.82f, 1.22f, ratioSmoothed);

    // Stable time-domain resampling core.
    // This is intentionally simpler than the prior granular OLA path to prioritize skip-free output.
    const int baseDelaySamples = lowLatencyMode ? lowLatencyPitchDelaySamples : normalPitchDelaySamples;

    auto wrapPos = [bufferSize] (float p)
    {
        while (p < 0.0f)
            p += static_cast<float> (bufferSize);
        while (p >= static_cast<float> (bufferSize))
            p -= static_cast<float> (bufferSize);
        return p;
    };

    auto sampleAt = [&] (float pos)
    {
        const float wrapped = wrapPos (pos);
        const int i0 = static_cast<int> (wrapped);
        const int i1 = (i0 + 1) % bufferSize;
        const int im1 = (i0 - 1 + bufferSize) % bufferSize;
        const int i2 = (i0 + 2) % bufferSize;
        const float frac = wrapped - static_cast<float> (i0);
        const float ym1 = channelDelay[static_cast<size_t> (im1)];
        const float y0 = channelDelay[static_cast<size_t> (i0)];
        const float y1 = channelDelay[static_cast<size_t> (i1)];
        const float y2 = channelDelay[static_cast<size_t> (i2)];

        // Cubic Hermite interpolation lowers aliasy/grainy texture vs linear interpolation.
        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    };

    // Two-head overlap-crossfade shifter.
    // This removes discontinuous read-head anchor corrections that manifest as skip bursts.
    const int windowSamples = juce::jlimit (96, juce::jmax (97, baseDelaySamples - 32),
                                            static_cast<int> (std::round (baseDelaySamples * 0.65f)));
    float phase = crossfadePhase;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = channelData[i];
        channelDelay[static_cast<size_t> (writeIdx)] = inputSample;

        const float unityDelta = std::abs (effectiveRatio - 1.0f);
        const float desiredAnchor = wrapPos (static_cast<float> (writeIdx - baseDelaySamples));

        // Anchor moves at +1 sample per sample due to writeIdx increment.
        // Add only the delta-speed component so net read speed becomes effectiveRatio.
        phase += (effectiveRatio - 1.0f);
        const float windowF = static_cast<float> (windowSamples);
        while (phase >= windowF)
            phase -= windowF;
        while (phase < 0.0f)
            phase += windowF;

        const float t = phase / windowF;
        const float readA = wrapPos (desiredAnchor + phase);
        const float readB = wrapPos (readA - windowF);
        const float sampleA = sampleAt (readA);
        const float sampleB = sampleAt (readB);

        const float gainA = std::sin ((1.0f - t) * juce::MathConstants<float>::halfPi);
        const float gainB = std::sin (t * juce::MathConstants<float>::halfPi);
        const float shifted = sampleA * gainA + sampleB * gainB;

        // Light smoothing only to avoid zippering while preserving audibility.
        const float baseAlpha = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.88f, 0.98f);
        const float alphaBoost = juce::jlimit (0.0f, 0.02f, unityDelta * 0.30f);
        const float outputAlpha = juce::jlimit (0.86f, 0.995f, baseAlpha + alphaBoost);
        outputSmoother += (shifted - outputSmoother) * outputAlpha;

        // Disable dry-assist mixing. Mixing dry and delayed pitch-shift paths can comb-filter,
        // causing both perceived skipping and apparent level loss.
        dryBlendSmoothed += (0.0f - dryBlendSmoothed) * 0.25f;
        const float dryBlend = 0.0f;
        channelData[i] = outputSmoother * (1.0f - dryBlend) + inputSample * dryBlend;

        // Keep legacy readPos coherent for state continuity/debug visibility.
        readPos = readA;

        writeIdx = (writeIdx + 1) % bufferSize;
    }

    crossfadePhase = phase;
}

void NovaPitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NovaPitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaPitchAudioProcessor();
}

//==============================================================================
juce::AudioProcessorEditor* NovaPitchAudioProcessor::createEditor()
{
    return new NovaPitchAudioProcessorEditor (*this);
}

bool NovaPitchAudioProcessor::hasEditor() const
{
    return true;
}
