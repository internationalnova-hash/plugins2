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
        50.0f));

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
    detMedianBuf.fill (0.0f);
    detMedianIdx  = 0;
    detMedianFull = false;
    retuneLfoPhase = 0.0f;
    retuneLfoJitter = 0.0f;
    outputCompGain = 1.0f;
    targetPitchRatio = 1.0f;
    activePitchRatio = 1.0f;
    retuneSpeedSmoothed = 0.0f;
    inputRmsSmoothed = 0.0f;
    voicedHoldBlocks = 0;
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
    // Smooth the speed control so turning the knob does not inject coefficient jumps.
    retuneSpeedSmoothed += (retuneSpeedNorm - retuneSpeedSmoothed) * 0.12f;
    const float retuneControlActive = juce::jlimit (0.0f, 1.0f, retuneSpeedSmoothed);
    const float k = retuneControlActive;
    const float kShaped = std::pow (k, 1.65f);
    const float retuneMs = 180.0f * std::pow (1.0f / 180.0f, kShaped);
    auto smoothstep = [] (float e0, float e1, float x)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / juce::jmax (1.0e-6f, e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    };
    float detectMs = retuneMs * 1.35f;
    detectMs = juce::jmap (smoothstep (0.75f, 1.0f, k), detectMs, retuneMs * 0.8f);
    const float detectSeconds = juce::jlimit (0.030f, 0.260f, detectMs * 0.001f);
    const float dtSeconds = static_cast<float> (numSamples) / static_cast<float> (juce::jmax (1.0, currentSampleRate));
    const float correctionTauSec = juce::jlimit (0.001f, 0.250f, retuneMs * 0.001f);
    const float correctionAlpha = 1.0f - std::exp (-dtSeconds / juce::jmax (1.0e-6f, correctionTauSec));
    const float hysteresisCents = 40.0f + (12.0f - 40.0f) * std::pow (k, 1.5f);
    const float switchHysteresis = hysteresisCents / 100.0f;
    const float maxSemitonesPerSecond = 10.0f * std::pow (260.0f / 10.0f, std::pow (k, 1.35f));
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

    // Keep tracking cadence fixed so changing Retune Speed cannot retime detection/locking.
    const bool fastRetuneTracking = true;
    const int intervalDivider = 1;

    // Perform pitch detection periodically
    if (blockCount % intervalDivider == 0)
    {
        diagWindowDetectEvalBlocks++;
        float rawYinHz = detectPitchYIN (analysisData, numSamples);
        const bool fastCorrectionMode = true;

        // Always smooth detector output; disabling smoothing in hard mode caused
        // large frame-to-frame ratio jumps and audible skip bursts.
        // smoothDetectedPitch uses lockedTargetMidi as reference to anchor octave and
        // suppress subharmonics — this is for audio quality only, not for lock decisions.
        float detectedHz = smoothDetectedPitch (rawYinHz, inputRms, lowLatencyMode || fastRetuneTracking);
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
            const bool hardTuneMode = retuneControlActive >= 0.90f;
            // Use the smoothed detected estimate directly for candidate selection.
            // This avoids octave-fold side effects from secondary lock-free transforms.
            const float noteSourceHz = detectedHz;
            const int candidateMidiNote = quantizeToScale (noteSourceHz);
            const float detectedMidi = 69.0f + 12.0f * std::log2 (juce::jmax (1.0f, noteSourceHz) / 440.0f);

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

                const float detectRateHz = static_cast<float> (currentSampleRate)
                    / static_cast<float> (juce::jmax (1, numSamples * intervalDivider));
                const float stableSeconds = detectSeconds;
                const int requiredStableHits = static_cast<int> (std::round (
                    juce::jmax (3.0f, detectRateHz * stableSeconds)));
                const bool strongInputForSwitch = inputRms > (hardTuneMode ? 0.040f : 0.012f);

                // Check for clearly wrong octave BEFORE the semitone-distance guard so
                // octave corrections (12 semitones) are not silently blocked.
                const float lockHz = getTargetPitchHz (lockedTargetMidi);
                const float lockCentsError = std::abs (1200.0f * std::log2 (
                    juce::jmax (1.0f, detectedHz) / juce::jmax (1.0f, lockHz)));
                const float candidateHz = getTargetPitchHz (candidateMidiNote);
                const float candidateCentsError = std::abs (1200.0f * std::log2 (
                    juce::jmax (1.0f, detectedHz) / juce::jmax (1.0f, candidateHz)));
                const bool lockAlreadyGood = lockCentsError < 35.0f;
                const float betterMarginCents = hardTuneMode ? 32.0f : 8.0f;
                const bool candidateClearlyBetter = candidateCentsError + betterMarginCents < lockCentsError;
                // Only force a relock when the error is clearly a full octave+ off.
                // Lower threshold was firing on notes that were just far in range, causing
                // frequent lock switches (lockSwitchRateHz=0.5-1.5) and audible wobble.
                const bool lockClearlyWrongOctave = lockCentsError > (hardTuneMode ? 1120.0f : 900.0f)
                    && strongInputForSwitch
                    && pendingTargetStreak >= juce::jmax (2, requiredStableHits / 2);

                if (lockClearlyWrongOctave && targetSwitchCooldownBlocks == 0)
                {
                    lockedTargetMidi = candidateMidiNote;
                    lockedTargetAge = 0;
                    pendingTargetMidi = -1;
                    pendingTargetStreak = 0;
                    const float cooldownSeconds = hardTuneMode ? 0.70f : 0.18f;
                    targetSwitchCooldownBlocks = static_cast<int> (std::round (
                        juce::jmax (2.0f, detectRateHz * cooldownSeconds)));
                    diagWindowLockSwitches++;
                }
                // Ignore implausibly large non-octave jumps — usually detector glitches.
                else if (std::abs (candidateMidiNote - lockedTargetMidi) > (hardTuneMode ? 9 : 6))
                {
                    // Keep existing lock until detector stabilizes.
                }
                else
                {
                const float minHoldSeconds = hardTuneMode
                    ? juce::jlimit (0.80f, 1.60f, detectSeconds * 6.0f)
                    : juce::jlimit (0.040f, 0.220f, detectSeconds * 0.85f);
                const int minHoldBlocks = static_cast<int> (std::round (
                    juce::jmax (3.0f, detectRateHz * minHoldSeconds)));
                const bool switchUp = candidateMidiNote > lockedTargetMidi
                                   && detectedMidi > static_cast<float> (lockedTargetMidi) + switchHysteresis;
                const bool switchDown = candidateMidiNote < lockedTargetMidi
                                     && detectedMidi < static_cast<float> (lockedTargetMidi) - switchHysteresis;
                const float minConfidence = 0.74f - 0.08f * std::pow (k, 1.2f);
                const bool confidentSwitch = pitchConfidence.load() > minConfidence;

                const bool hardSwitchDistanceOk = ! hardTuneMode
                    || std::abs (candidateMidiNote - lockedTargetMidi) >= 3;
                const bool hardSwitchErrorOk = ! hardTuneMode
                    || lockCentsError >= 120.0f;

                if ((switchUp || switchDown)
                    && confidentSwitch
                    && strongInputForSwitch
                    && ! lockAlreadyGood
                    && candidateClearlyBetter
                    && hardSwitchDistanceOk
                    && hardSwitchErrorOk
                    && lockedTargetAge >= minHoldBlocks
                    && pendingTargetStreak >= (hardTuneMode ? requiredStableHits + 14 : requiredStableHits)
                    && targetSwitchCooldownBlocks == 0)
                {
                    lockedTargetMidi = candidateMidiNote;
                    lockedTargetAge = 0;
                    pendingTargetMidi = -1;
                    pendingTargetStreak = 0;
                    const float cooldownSeconds = hardTuneMode
                        ? juce::jlimit (1.00f, 1.90f, detectSeconds * 4.0f)
                        : juce::jlimit (0.050f, 0.220f, detectSeconds * 0.80f);
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

            // Recompute target ratio every detection block so correction tracks continuous intonation
            // drift while the lock is held. Smooth updates on stable lock to avoid jitter-driven wobble.
            const bool lockChanged = (targetMidiNote != previousLockedTargetMidi);

            // Align detector octave to the selected target note before ratio computation.
            float octaveAlignedDetectedHz = detectedHz;
            if (targetHz > 1.0f)
            {
                while (octaveAlignedDetectedHz < targetHz * 0.70710678f)
                    octaveAlignedDetectedHz *= 2.0f;
                while (octaveAlignedDetectedHz > targetHz * 1.41421356f)
                    octaveAlignedDetectedHz *= 0.5f;
            }

            float pitchRatio = computeRetuneRatio (octaveAlignedDetectedHz, targetHz, inputRms, voicedHoldBlocks, lowLatencyMode);
            diagWindowRatioComputedBlocks++;
            if (std::abs (pitchRatio - 1.0f) < 0.001f)
                diagWindowUnityReturnBlocks++;

            const float minRatio = 0.72f;
            const float maxRatio = 1.38f;
            const float computedTargetRatio = juce::jlimit (minRatio, maxRatio, pitchRatio);

            if (lockChanged)
            {
                targetPitchRatio = computedTargetRatio;
                previousLockedTargetMidi = targetMidiNote;
            }
            else
            {
                // Stable-lock retarget smoothing suppresses detector flutter while still allowing
                // real note drift to drive stronger/autotune-like correction.
                // Very small alpha so individual bad detector frames can't spike targetPitchRatio.
                const float stableRetargetAlpha = hardTuneMode
                    ? (lowLatencyMode ? 0.10f : 0.08f)
                    : (lowLatencyMode ? 0.08f : 0.06f);
                targetPitchRatio += (computedTargetRatio - targetPitchRatio) * stableRetargetAlpha;
            }

            const float correctedHz = octaveAlignedDetectedHz * targetPitchRatio;
            correctedPitch.store (correctedHz);

            if (debugCounter % 100 == 2)
            {
                DBG("Nova Pitch: detectedHz=" << detectedHz
                    << " targetHz=" << targetHz
                    << " pitchRatio=" << pitchRatio
                    << " targetPitchRatio=" << targetPitchRatio
                    << " lockChanged=" << (lockChanged ? 1 : 0));
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
                previousLockedTargetMidi = -1;
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
    const bool hardTuneMode = retuneControlActive >= 0.90f;
    const float trackingConfidence = juce::jlimit (0.0f, 1.0f, pitchConfidence.load());
    inputRmsSmoothed += (inputRms - inputRmsSmoothed) * 0.08f;
    const float gateOnThreshold = hardTuneMode ? 0.0010f : 0.0030f;
    const float gateOffThreshold = hardTuneMode ? 0.00018f : 0.0012f;
    if (inputRmsSmoothed >= gateOnThreshold)
    {
        voicedHoldBlocks = hardTuneMode ? 120 : 24;
    }
    else if (voicedHoldBlocks > 0)
    {
        --voicedHoldBlocks;
    }

    const bool signalTooLow = inputRmsSmoothed < gateOffThreshold && voicedHoldBlocks == 0;
    // Go to dry bypass immediately when confidence is zero — no grace period.
    // A 24-block grace window caused the shifter to run 24 blocks without a valid pitch
    // then abruptly switch to dry, producing an audible skip/comb artifact every playback start.
    const bool lowConfidence = trackingConfidence < (hardTuneMode ? 0.0002f : (retuneControlActive >= 0.85f ? 0.002f : 0.01f));
    const bool trackingLost = signalTooLow || (! hardTuneMode && lowConfidence && voicedHoldBlocks == 0);
    if (trackingLost)
        diagWindowTrackingLostBlocks++;

    // Keep the processed path fully wet while signal is present to avoid
    // dry/wet comb filtering (phasey sound). Only fade to dry in true silence.
    if (signalTooLow)
        wetMixSmoothed += (0.0f - wetMixSmoothed) * 0.020f;
    else
        wetMixSmoothed += (1.0f - wetMixSmoothed) * 0.30f;
    const float wetMix = juce::jlimit (0.0f, 1.0f, wetMixSmoothed);

    // Retune stays active across the full knob range; knob controls speed only.
    // Single smooth LP filter toward target — no per-block clamping (eliminates double-limiting oscillations).
    if (! signalTooLow)
    {
        // Smooth target-ratio motion first, then apply retune-speed glide.
        // CRITICAL: Higher smoothing at fast speeds means snappier Auto-Tune effect.
        // At k=1.0 (hard-tune), we use aggressive smoothing for immediate pitch lock.
        const float targetSmoothing = hardTuneMode
            ? 0.96f
            : juce::jmap (retuneControlActive, 0.0f, 1.0f, 0.22f, 0.72f);
        targetRatioSmoothed += (targetPitchRatio - targetRatioSmoothed) * targetSmoothing;

        // Clamp accumulated lag: never let targetRatioSmoothed be more than ~300 cents
        // from activePitchRatio. Without this, turning the speed knob from slow to fast
        // causes a rush-to-catch-up lurch that sounds like wobble/pitch jump.
        {
            const float maxLagRatio = std::pow (2.0f, 180.0f / 1200.0f); // ~180 cents
            const float lagRatio = targetRatioSmoothed / juce::jmax (0.001f, activePitchRatio);
            if (lagRatio > maxLagRatio)
                targetRatioSmoothed = activePitchRatio * maxLagRatio;
            else if (lagRatio < 1.0f / maxLagRatio)
                targetRatioSmoothed = activePitchRatio / maxLagRatio;
        }

        const float desiredRatio = targetRatioSmoothed;
        const float ratioNext = activePitchRatio + (desiredRatio - activePitchRatio) * correctionAlpha;

        // Velocity limit in semitones/sec gives musical slow settings and snapping fast settings.
        const float confidenceRateScale = juce::jmap (trackingConfidence, 0.0f, 1.0f, 0.55f, 1.00f);
        const float maxStepSemitones = maxSemitonesPerSecond * confidenceRateScale * dtSeconds;
        const float deltaSemitones = 12.0f * std::log2 (juce::jmax (0.001f, ratioNext) / juce::jmax (0.001f, activePitchRatio));
        const float limitedDeltaSemitones = juce::jlimit (-maxStepSemitones, maxStepSemitones, deltaSemitones);
        activePitchRatio *= std::pow (2.0f, limitedDeltaSemitones / 12.0f);

        const float minRatio = 0.72f;
        const float maxRatio = 1.38f;
        activePitchRatio = juce::jlimit (minRatio, maxRatio, activePitchRatio);

        const float ratioStep = std::abs (activePitchRatio - diagPrevActivePitchRatio);
        if (ratioStep > 0.008f)
            diagWindowLargeRatioStepBlocks++;
        diagPrevActivePitchRatio = activePitchRatio;

        const float appliedCents = std::abs (1200.0f * std::log2 (juce::jmax (0.001f, activePitchRatio)));
        const float targetCents = std::abs (1200.0f * std::log2 (juce::jmax (0.001f, targetPitchRatio)));
        diagWindowAppliedCentsAbsSum += static_cast<double> (appliedCents);
        diagWindowTargetCentsAbsSum += static_cast<double> (targetCents);

        // Apply vibrato as a continuous effect every block to create smooth sinusoidal modulation.
        // This ensures vibrato is always present and continuous, not just when the lock changes.
        // Vibrato modulates the final activePitchRatio before pitch shifting.
        float appliedRatio = activePitchRatio;
        if (! hardTuneMode && vibratoValue > 0.001f)
        {
            applyVibrato (appliedRatio, static_cast<float> (currentSampleRate), numSamples, vibratoValue);
        }

        // Apply shift only when tracking is valid.
        processCircularBufferPitchShift (channelL, numSamples, appliedRatio, 0, lowLatencyMode, retuneControlActive);
        if (channelR != nullptr)
            processCircularBufferPitchShift (channelR, numSamples, appliedRatio, 1, lowLatencyMode, retuneControlActive);
    }
    else
    {
        // Signal is absent — glide ratio to unity and bypass shifter to avoid idle artifacts.
        activePitchRatio += (1.0f - activePitchRatio) * 0.06f;
        targetRatioSmoothed += (1.0f - targetRatioSmoothed) * 0.08f;
        auto* dryL = dryScratchL.data();
        std::copy (dryL, dryL + numSamples, channelL);
        if (channelR != nullptr)
        {
            auto* dryR = dryScratchR.data();
            std::copy (dryR, dryR + numSamples, channelR);
        }
    }

    if (! signalTooLow)
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

    if (signalTooLow && wetMix < 0.999f)
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
            + " speedNorm=" + juce::String (retuneControlActive, 3)
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
        // Cold-start: fold the first raw reading toward the center of the vocal range
        // so a YIN harmonic flip on the very first frame doesn't anchor everything an
        // octave high. Pick the octave closest to a natural vocal center (~300 Hz).
        const float vocalCenter = 300.0f;
        float initHz = rawDetectedHz;
        while (initHz > vocalCenter * 1.41421356f && initHz * 0.5f >= minPitchHz - 10.0f)
            initHz *= 0.5f;
        while (initHz < vocalCenter * 0.70710678f && initHz * 2.0f <= maxPitchHz + 10.0f)
            initHz *= 2.0f;
        smoothedDetectedHz = initHz;
        return smoothedDetectedHz;
    }

    // Use lockedTargetMidi as the octave-fold reference to anchor the smoother to the
    // correct octave and suppress subharmonics. The circular-lock problem (wrong-octave
    // lock reinforcing itself) is now broken at the processBlock level: candidateMidiNote
    // is derived from a lock-free raw pitch estimate, NOT from smoothedDetectedHz.
    float referenceHz = 0.0f;
    if (lockedTargetMidi >= 0)
        referenceHz = getTargetPitchHz (lockedTargetMidi);
    else if (lastValidDetectedHz > minPitchHz - 10.0f && lastValidDetectedHz < maxPitchHz + 10.0f)
        referenceHz = lastValidDetectedHz;
    else
        referenceHz = smoothedDetectedHz;

    if (referenceHz > 1.0f)
    {
        auto centsDistance = [] (float a, float b)
        {
            return std::abs (1200.0f * std::log2 (juce::jmax (1.0f, a) / juce::jmax (1.0f, b)));
        };

        float bestHz = rawDetectedHz;
        float bestCents = centsDistance (rawDetectedHz, referenceHz);

        float up = rawDetectedHz;
        while (up * 2.0f <= maxPitchHz + 10.0f)
        {
            up *= 2.0f;
            const float cents = centsDistance (up, referenceHz);
            if (cents < bestCents)
            {
                bestCents = cents;
                bestHz = up;
            }
        }

        float down = rawDetectedHz;
        while (down * 0.5f >= minPitchHz - 10.0f)
        {
            down *= 0.5f;
            const float cents = centsDistance (down, referenceHz);
            if (cents < bestCents)
            {
                bestCents = cents;
                bestHz = down;
            }
        }

        rawDetectedHz = bestHz;
    }

    // Octave-error detection and correction.
    // If rawDetectedHz is ~1.5-2.5x or ~0.4-0.67x the smoothed pitch, likely octave error.
    const float ratio = rawDetectedHz / juce::jmax (1.0f, smoothedDetectedHz);
    float correctedHz = rawDetectedHz;

    if (ratio > 1.4f && ratio < 2.6f)
    {
        // Try correcting down one octave
        correctedHz = rawDetectedHz * 0.5f;
    }
    else if (ratio > 0.38f && ratio < 0.72f)
    {
        // Try correcting up one octave
        correctedHz = rawDetectedHz * 2.0f;
    }

    // If octave correction brings us much closer to smoothed, use it
    const float deltaRaw = std::abs (rawDetectedHz - smoothedDetectedHz);
    const float deltaCorrected = std::abs (correctedHz - smoothedDetectedHz);
    if (deltaCorrected < deltaRaw * 0.6f)
        rawDetectedHz = correctedHz;

    // Reject extreme jumps (e.g., detector glitches or false onsets).
    // On a steady note, 20 Hz max jump per block is reasonable; much more suggests error.
    const float delta = std::abs (rawDetectedHz - smoothedDetectedHz);
    if (delta > 50.0f)
    {
        // Extreme jump: blend much more slowly, almost ignoring the raw value.
        smoothedDetectedHz += (rawDetectedHz - smoothedDetectedHz) * 0.02f;
        return smoothedDetectedHz;
    }

    // ── Median filter: collect last detMedianSize readings and use the median ──
    // This kills brief subharmonic hits (e.g. 155 Hz one frame while singing 310 Hz)
    // before they contaminate the smoother and produce ratio wobble.
    detMedianBuf[static_cast<size_t> (detMedianIdx)] = rawDetectedHz;
    detMedianIdx = (detMedianIdx + 1) % detMedianSize;
    if (detMedianIdx == 0) detMedianFull = true;

    const int filledCount = detMedianFull ? detMedianSize : detMedianIdx;
    if (filledCount >= 3)
    {
        // Sort a local copy and pick the middle value.
        std::array<float, 7> sortBuf {};
        for (int k = 0; k < filledCount; ++k)
            sortBuf[static_cast<size_t> (k)] = detMedianBuf[static_cast<size_t> (k)];
        std::sort (sortBuf.begin(), sortBuf.begin() + filledCount);
        rawDetectedHz = sortBuf[static_cast<size_t> (filledCount / 2)];
    }

    // Normal blending — extremely conservative so the smoother cannot be dragged by
    // one or two outlier frames.  At alpha=0.04 a 100 Hz step takes ~17 blocks to close.
    juce::ignoreUnused (signalRms, lowLatencyMode);
    const float alpha = lowLatencyMode ? 0.022f : 0.04f;

    smoothedDetectedHz += (rawDetectedHz - smoothedDetectedHz) * alpha;
    return smoothedDetectedHz;
}

float NovaPitchAudioProcessor::computeRetuneRatio (float detectedHz, float targetHz, float signalRms, int voicedHoldBlocks, bool /*lowLatencyMode*/)
{
    const float toleranceNorm = apvts.getRawParameterValue ("tolerance")->load() / 100.0f;
    const float amountNorm = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("amount")->load() / 100.0f);
    const bool hardTuneMode = amountNorm >= 0.90f;

    const float fullRatio = targetHz / juce::jmax (1.0f, detectedHz);

    // Tolerance window: if singer is already close enough, keep ratio at unity.
    const float centsError = std::abs (1200.0f * std::log2 (juce::jmax (0.001f, std::abs (fullRatio))));
    // Keep tolerance independent of retune speed so turning speed knob doesn't
    // change pitch target behavior, only convergence rate.
    const float toleranceCents = hardTuneMode ? 0.0f : toleranceNorm * 3.5f;
    
    // Voiced-hold gate: only suppress correction if signal is truly gone AND hold window is closed.
    // During hold window, keep computing correction ratio to maintain effect continuity.
    const bool inHoldWindow = voicedHoldBlocks > 0;
    if (signalRms < (hardTuneMode ? 0.00035f : 0.0012f) && !inHoldWindow)
        return 1.0f;
    if (centsError < toleranceCents)
        return 1.0f;

    if (hardTuneMode && centsError > 0.35f)
    {
        // In hard mode, force a minimum correction amount so the effect stays clearly audible.
        const float minAudibleCents = juce::jmap (amountNorm, 0.90f, 1.00f, 38.0f, 85.0f);
        const float sign = (fullRatio >= 1.0f) ? 1.0f : -1.0f;
        const float minAudibleRatio = std::pow (2.0f, (sign * minAudibleCents) / 1200.0f);
        const float boostedRatio = (centsError < minAudibleCents) ? minAudibleRatio : fullRatio;
        return juce::jlimit (0.50f, 2.00f, boostedRatio);
    }

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
    // Keep 50% as neutral so default UI state has no phase coloration.
    const float centered = juce::jlimit (-1.0f, 1.0f, (formantParam - 50.0f) / 50.0f);
    const float amount = std::abs (centered);
    if (amount < 0.02f)
        return;

    const float sign = centered >= 0.0f ? 1.0f : -1.0f;
    const float a = juce::jlimit (-0.14f, 0.14f, sign * amount * 0.14f);
    const float mix = juce::jlimit (0.0f, 0.16f, amount * 0.16f);

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

    auto getReferencePitchHz = [&]() -> float
    {
        if (lockedTargetMidi >= 0)
        {
            const float lockHz = getTargetPitchHz (lockedTargetMidi);
            if (lockHz > minPitchHz - 10.0f && lockHz < maxPitchHz + 10.0f)
                return lockHz;
        }

        if (lastValidDetectedHz > minPitchHz - 10.0f && lastValidDetectedHz < maxPitchHz + 10.0f)
            return lastValidDetectedHz;

        if (smoothedDetectedHz > minPitchHz - 10.0f && smoothedDetectedHz < maxPitchHz + 10.0f)
            return smoothedDetectedHz;

        if (detectedPitch.load() > minPitchHz - 10.0f && detectedPitch.load() < maxPitchHz + 10.0f)
            return detectedPitch.load();

        return 0.0f;
    };

    auto alignToReferenceOctave = [&] (float hz) -> float
    {
        hz = foldIntoRange (hz);
        if (hz <= 0.0f)
            return 0.0f;

        const float referenceHz = getReferencePitchHz();

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

        const float referenceHz = getReferencePitchHz();
        int bestTau = tauMin;
        float bestScore = -1.0e9f;
        float bestCorr = -1.0f;
        constexpr int sampleStep = 2;
        constexpr int tauStep = 2;

        auto octaveDistance = [] (float a, float b)
        {
            return std::abs (std::log2 (juce::jmax (1.0f, a) / juce::jmax (1.0f, b)));
        };

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
            float score = corr;
            if (referenceHz > 0.0f)
            {
                const float rawHz = static_cast<float> (currentSampleRate) / static_cast<float> (juce::jmax (1, tau));
                score -= 0.22f * octaveDistance (rawHz, referenceHz);
            }

            if (std::isfinite (corr) && score > bestScore)
            {
                bestScore = score;
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
            const float referenceHz = getReferencePitchHz();
            if (referenceHz > 0.0f)
            {
                const float centsError = std::abs (1200.0f * std::log2 (
                    juce::jmax (1.0f, hz) / juce::jmax (1.0f, referenceHz)));
                // Zero-crossing is only allowed as a fallback when it agrees reasonably well
                // with the current reference; otherwise keep the reference alive.
                if (centsError > 350.0f)
                    return finalizeDetectedHz (referenceHz, 0.08f);
            }

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
    const float minRatio = 0.72f;
    const float maxRatio = 1.38f;
    const float clampedRatio = juce::jlimit (minRatio, maxRatio, pitchRatio);
    const float ratioDelta = std::abs (clampedRatio - ratioSmoothed);
    // Keep ratio changes smooth enough to avoid time-domain read-head jumps.
    const float ratioSmoothing = juce::jlimit (0.10f, 0.24f, 0.10f + ratioDelta * 0.26f);
    ratioSmoothed += (clampedRatio - ratioSmoothed) * ratioSmoothing;
    const float effectiveRatio = juce::jlimit (minRatio, maxRatio, ratioSmoothed);

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

    // Single-head continuous resampling shifter.
    // Keep one stable read head and gently re-anchor it toward the desired delay target.
    // The previous wrapped phase window reintroduced periodic jumps, heard as wobble/artifacts.
    float readHead = wrapPos (readPos);

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputSample = channelData[i];
        channelDelay[static_cast<size_t> (writeIdx)] = inputSample;

        const float unityDelta = std::abs (effectiveRatio - 1.0f);
        const bool hardTuneMode = retuneSpeedNorm >= 0.90f;
        const float desiredAnchor = wrapPos (static_cast<float> (writeIdx - baseDelaySamples));

        // Near-unity passthrough: keep this very narrow so subtle correction remains audible.
        // Full dry only inside ~3 cents, and fade out by ~7 cents.
        float nearUnityBlend = juce::jlimit (0.0f, 1.0f,
            juce::jmap (unityDelta, 0.0010f, 0.0024f, 1.0f, 0.0f)); // 1=dry, 0=shifted
        if (hardTuneMode)
            nearUnityBlend = 0.0f;

        auto shortestWrappedDelta = [bufferSize] (float from, float to)
        {
            float delta = to - from;
            const float sizeF = static_cast<float> (bufferSize);
            while (delta > sizeF * 0.5f)
                delta -= sizeF;
            while (delta < -sizeF * 0.5f)
                delta += sizeF;
            return delta;
        };

        const float anchorError = shortestWrappedDelta (readHead, desiredAnchor);
        const float anchorPull = hardTuneMode ? 0.10f : 0.035f;
        readHead = wrapPos (readHead + anchorError * anchorPull);

        const float shifted = sampleAt (readHead);

        // Light smoothing to avoid zippering.
        const float baseAlpha = juce::jmap (retuneSpeedNorm, 0.0f, 1.0f, 0.88f, 0.98f);
        const float alphaBoost = juce::jlimit (0.0f, 0.02f, unityDelta * 0.30f);
        const float outputAlpha = juce::jlimit (0.86f, 0.995f, baseAlpha + alphaBoost);
        outputSmoother += (shifted - outputSmoother) * outputAlpha;

        // Blend dry (near-unity) vs shifted.
        dryBlendSmoothed += (nearUnityBlend - dryBlendSmoothed) * 0.035f;
        channelData[i] = outputSmoother * (1.0f - dryBlendSmoothed) + inputSample * dryBlendSmoothed;

        readHead = wrapPos (readHead + effectiveRatio);
        readPos = readHead;

        writeIdx = (writeIdx + 1) % bufferSize;
    }

    crossfadePhase = 0.0f;
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
