#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include <cmath>

NovaPitchAudioProcessor::NovaPitchAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

NovaPitchAudioProcessor::~NovaPitchAudioProcessor()
{
    // Safety net in case the host never calls releaseResources() before
    // destroying the plugin instance - see maybeEnableDebugCaptureLog().
    flushDebugCaptureLog();
}

juce::AudioProcessorValueTreeState::ParameterLayout
NovaPitchAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Choice params so hosts (and Nova Rack) show note/scale names and snap to them.
    // Index values match the old 0-11 / 0-4 ints, so saved sessions load unchanged.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::key, "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::scale, "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor", "Pentatonic", "Blues" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::tolerance, "Retune Speed", 0.0f, 100.0f, 50.0f));
    // Default 0 (was 100): under the new Flex-Tune-style behavior (see
    // NovaScaleQuantizer::process()), 0 means "no humanization - always
    // fully correct", which is the safe/expected default for a hard-tune
    // plugin. Turning it up deliberately opts into letting near-target
    // natural closeness through; it can never let a genuinely off-key
    // note pass uncorrected, at any value.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::amount,    "Humanize",     0.0f, 100.0f, 0.0f));
    // Default nudged from the pre-rebuild 40 toward ~70, matching the
    // proven reference confidence-gate threshold - a deliberate behavior
    // change under the same param ID, not a silent one (see rebuild plan).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::confidenceThreshold, "Confidence Thresh", 0.0f, 100.0f, 70.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::vibrato, "Natural Vibrato", 0.0f, 100.0f, 0.0f));
    // Was AudioParameterBool pre-rebuild; now a real, continuous, load-
    // bearing amount (see NovaFormantCorrector). Same ID.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::formant, "Formant Preserve", 0.0f, 100.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (ParameterIDs::lowLatency, "Low Latency", false));

    // New in the ground-up rebuild.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::formantWarp, "Formant Warp", -100.0f, 100.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::mix, "Mix", 0.0f, 100.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::outputGain, "Output", -24.0f, 24.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::range, "Range", 0.0f, 100.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::vibratoRate, "Vibrato Rate", 2.0f, 8.0f, 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::transients, "Transients", 0.0f, 100.0f, 30.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (ParameterIDs::bypass, "Bypass", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (ParameterIDs::tracking, "Tracking Mode", false));

    // Harmony (off by default - Nova Pitch is a plain tuner until it's switched on)
    const juce::StringArray intervals { "Unison", "2nd Above", "3rd Above", "4th Above", "5th Above", "6th Above",
                                        "Octave Above", "3rd Below", "4th Below", "5th Below", "6th Below", "Octave Below" };
    params.push_back (std::make_unique<juce::AudioParameterBool>  (ParameterIDs::harmOn, "Harmony", false));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::harmMode, "Harmony Mode", juce::StringArray { "Scale", "MIDI" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::harmStyle, "Harmony Style",
        juce::StringArray { "Pop", "R&B", "Trap", "Gospel", "Choir", "Ambient", "Doubles" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>   (ParameterIDs::harmVoices, "Voices", 1, 32, 8));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::harmWidth, "Harmony Width", 0.0f, 200.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::harmHumanize, "Harmony Humanize", 0.0f, 100.0f, 30.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::harmLead, "Lead Level", -24.0f, 6.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::harmMix, "Harmony Mix", 0.0f, 100.0f, 70.0f));
    const bool  partOnDefault[4]  = { true, true, false, false };
    const int   intervalDefault[4] = { 2, 4, 11, 0 };
    const float levelDefault[4]   = { -4.0f, -6.0f, -8.0f, -6.0f };
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = "Part " + juce::String (i + 1);
        params.push_back (std::make_unique<juce::AudioParameterBool>  (ParameterIDs::partOn[i], n + " On", partOnDefault[i]));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::partInterval[i], n + " Interval", intervals, intervalDefault[i]));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterIDs::partLevel[i], n + " Level", -24.0f, 6.0f, levelDefault[i]));
    }

    return { params.begin(), params.end() };
}

void NovaPitchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    lastAppliedLowLatencyMode = apvts.getRawParameterValue (ParameterIDs::lowLatency)->load() >= 0.5f;
    lastAppliedTracking = apvts.getRawParameterValue (ParameterIDs::tracking)->load() >= 0.5f;

    engine.prepare (sampleRate, samplesPerBlock, lastAppliedLowLatencyMode, lastAppliedTracking);
    preparedBlockSize = juce::jmax (64, samplesPerBlock);
    harmony.prepare (sampleRate, preparedBlockSize, lastAppliedLowLatencyMode, lastAppliedTracking);
    harmDryL.assign ((size_t) preparedBlockSize, 0.0f);
    harmDryR.assign ((size_t) preparedBlockSize, 0.0f);
    setLatencySamples (engine.getLatencySamples());

    // Synchronous (not deferred to the timer) - see maybeEnableDebugCaptureLog()
    // for why. Deliberately kept as cheap as possible (no large allocation,
    // no file I/O here) after the first version of this crashed Pro Tools
    // on load by doing a ~60MB reservation + real file creation right here.
    maybeEnableDebugCaptureLog();

    startTimer (250);
}

void NovaPitchAudioProcessor::releaseResources()
{
    flushDebugCaptureLog();
    stopTimer();
}

// See the header comment on setGrainCaptureDebugCallback for why this
// exists: every purely-offline hypothesis this session failed to explain
// why a real DAW bounce sounds different from a guaranteed-clean offline
// batch render of the identical settings and source. This gets the actual
// grain-capture ground truth from inside a real host session instead of
// guessing further. Sentinel-file gated (not an env var) because GUI apps
// launched from the Dock/Finder on macOS don't reliably inherit shell
// environment variables, so an env-var gate would silently never trigger
// for a real DAW.
//
// Called SYNCHRONOUSLY from prepareToPlay() - a real bounce/render pass in
// a host can use a genuinely short-lived plugin instance (verified: a
// version of this that deferred setup to the next 250ms timer tick logged
// nothing but an idle, never-fed "live" mixer instance for 166 real
// seconds, while the actual export - confirmed independently to be
// correctly pitch-corrected - produced no log data at all, because its own
// instance never survived long enough for a 250ms-deferred callback
// registration to ever run). Deliberately kept minimal to stay safe for
// AAX's stricter load-time constraints: a cheap file-exists check, a
// MODEST (not the original ~60MB) reservation only large enough for a
// realistic single bounce, and a lambda assignment - no other file I/O
// happens here. The actual CSV file itself is created lazily, on first
// flush (from the timer, releaseResources(), or the destructor - never
// from inside this method), so a render-pass instance that lives only a
// few hundred milliseconds still gets its in-memory data safely flushed
// out before destruction via the destructor's safety-net call.
void NovaPitchAudioProcessor::maybeEnableDebugCaptureLog()
{
    if (debugCaptureActive)
        return; // already armed by an earlier prepareToPlay() call on this same instance - don't wipe accumulated data

    const juce::File flagFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                     .getChildFile ("nova_pitch_debug.flag");
    if (! flagFile.existsAsFile())
        return;

    // ~90 seconds of continuous voiced content at a plausible worst-case
    // (highest) pitch - generous enough for any realistic single bounce,
    // small enough (a few MB) to be a safe synchronous allocation even
    // during a host's load/validation pass.
    debugCaptureLog.reserve (300'000);
    debugCaptureFlushedCount = 0;
    debugCaptureActive = true;

    engine.setGrainCaptureDebugCallback ([this] (const nova::pitch::NovaPitchSynchronousShifter::CaptureDebugInfo& info)
    {
        if (debugCaptureActive && debugCaptureLog.size() < debugCaptureLog.capacity())
            debugCaptureLog.push_back (info);
    });
}

void NovaPitchAudioProcessor::flushDebugCaptureLog()
{
    if (! debugCaptureActive)
        return;

    const size_t total = debugCaptureLog.size();
    if (debugCaptureFlushedCount >= total)
        return;

    // Lazy file creation - deliberately NOT done in maybeEnableDebugCaptureLog()
    // (which runs synchronously inside prepareToPlay()) to keep that path
    // free of any file I/O at all.
    if (debugCaptureOutputFile == juce::File())
    {
        const juce::String uniqueSuffix = juce::String::toHexString ((juce::pointer_sized_int) this)
                                           + "_" + juce::String ((juce::int64) juce::Time::getCurrentTime().toMilliseconds());
        debugCaptureOutputFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                      .getChildFile ("nova_pitch_debug_captures_" + uniqueSuffix + ".csv");
        debugCaptureOutputFile.deleteFile();
        debugCaptureOutputFile.create();
        debugCaptureOutputFile.appendText ("wallClockMs,sampleIndex,periodInSamples,wsolaOffset,wsolaBestScore,wsolaBaselineScore,measuredGrainHz,assumedHz,centsError\n");
    }

    juce::String chunk;
    chunk.preallocateBytes (128 * (size_t) (total - debugCaptureFlushedCount));
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    for (size_t i = debugCaptureFlushedCount; i < total; ++i)
    {
        const auto& info = debugCaptureLog[i];
        const float centsError = (info.measuredGrainHz > 0.0f && info.assumedHz > 0.0f)
            ? 1200.0f * std::log2 (info.measuredGrainHz / info.assumedHz) : 0.0f;
        chunk << nowMs << "," << (juce::int64) info.sampleIndex << "," << info.periodInSamples << ","
              << info.wsolaOffset << "," << info.wsolaBestScore << "," << info.wsolaBaselineScore << ","
              << info.measuredGrainHz << "," << info.assumedHz << "," << centsError << "\n";
    }
    debugCaptureOutputFile.appendText (chunk);
    debugCaptureFlushedCount = total;
}

void NovaPitchAudioProcessor::timerCallback()
{
    // Message-thread-only reconfigure for lowLatency mode changes (resizes
    // the pitch detector's analysis window, which heap-allocates) - mirrors
    // Nova Seven's proven oversampling-reconfigure pattern
    // (NovaSevenAudioProcessor::reconfigureOversamplingIfNeeded).
    const bool desired = apvts.getRawParameterValue (ParameterIDs::lowLatency)->load() >= 0.5f;
    const bool desiredTracking = apvts.getRawParameterValue (ParameterIDs::tracking)->load() >= 0.5f;
    if (desired != lastAppliedLowLatencyMode || desiredTracking != lastAppliedTracking)
    {
        // Reallocates - keep the audio thread out while it happens.
        suspendProcessing (true);
        engine.reconfigureIfNeeded (currentSampleRate, desired, desiredTracking);
        harmony.prepare (currentSampleRate, preparedBlockSize, desired, desiredTracking);
        suspendProcessing (false);
        lastAppliedLowLatencyMode = desired;
        lastAppliedTracking = desiredTracking;
        setLatencySamples (engine.getLatencySamples());
    }

    if (debugCaptureActive)
        flushDebugCaptureLog();
}

void NovaPitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const float* chL = buffer.getReadPointer (0);
    const float* chR = (numChannels > 1) ? buffer.getReadPointer (1) : chL;

    NovaPitchTunedEngine::Params p;
    p.keyPitchClass = (int) apvts.getRawParameterValue (ParameterIDs::key)->load();
    p.scale = (nova::pitch::Scale) (int) apvts.getRawParameterValue (ParameterIDs::scale)->load();
    p.retuneSpeedPercent = apvts.getRawParameterValue (ParameterIDs::tolerance)->load();
    p.amountPercent = apvts.getRawParameterValue (ParameterIDs::amount)->load();
    p.confidenceThresholdPercent = apvts.getRawParameterValue (ParameterIDs::confidenceThreshold)->load();
    p.vibratoPreservePercent = apvts.getRawParameterValue (ParameterIDs::vibrato)->load();
    p.vibratoRateHz = apvts.getRawParameterValue (ParameterIDs::vibratoRate)->load();
    p.formantAmountPercent = apvts.getRawParameterValue (ParameterIDs::formant)->load();
    p.formantWarpPercent = apvts.getRawParameterValue (ParameterIDs::formantWarp)->load();
    p.rangePercent = apvts.getRawParameterValue (ParameterIDs::range)->load();
    p.mixPercent = apvts.getRawParameterValue (ParameterIDs::mix)->load();
    p.outputGainDb = apvts.getRawParameterValue (ParameterIDs::outputGain)->load();
    p.transientsPercent = apvts.getRawParameterValue (ParameterIDs::transients)->load();
    p.bypassed = apvts.getRawParameterValue (ParameterIDs::bypass)->load() >= 0.5f;

    engine.setParams (p);

    // Harmony settings (the harmony engine runs beside the tuner, never inside it)
    NovaPitchHarmony::Params hp;
    hp.on       = apvts.getRawParameterValue (ParameterIDs::harmOn)->load() >= 0.5f;
    hp.midiMode = (int) apvts.getRawParameterValue (ParameterIDs::harmMode)->load() == 1;
    hp.lead     = engine.getTunerParams();
    hp.keyPc    = p.keyPitchClass;
    // Intervals step through the key's major or minor scale (chromatic and
    // pentatonic use major, blues uses minor) so a "3rd" is a real 3rd.
    hp.stepMask = (p.scale == nova::pitch::Scale::Minor || p.scale == nova::pitch::Scale::Blues) ? 0x5AD : 0xAB5;
    for (int i = 0; i < NovaPitchHarmony::kParts; ++i)
    {
        hp.partOn[(size_t) i]     = apvts.getRawParameterValue (ParameterIDs::partOn[i])->load() >= 0.5f;
        hp.interval[(size_t) i]   = (int) apvts.getRawParameterValue (ParameterIDs::partInterval[i])->load();
        hp.partGainDb[(size_t) i] = apvts.getRawParameterValue (ParameterIDs::partLevel[i])->load();
    }
    hp.voices     = (int) apvts.getRawParameterValue (ParameterIDs::harmVoices)->load();
    hp.width      = apvts.getRawParameterValue (ParameterIDs::harmWidth)->load() / 100.0f;
    hp.humanize   = apvts.getRawParameterValue (ParameterIDs::harmHumanize)->load() / 100.0f;
    hp.leadGainDb = apvts.getRawParameterValue (ParameterIDs::harmLead)->load();
    hp.mix        = apvts.getRawParameterValue (ParameterIDs::harmMix)->load() / 100.0f;
    hp.outGainDb  = p.outputGainDb;
    harmony.setParams (hp);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())                               harmony.noteOn (m.getNoteNumber());
        else if (m.isNoteOff())                         harmony.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) harmony.allNotesOff();
    }

    float* outL = buffer.getWritePointer (0);
    if (hp.on || harmony.isActive())
    {
        float* outR = numChannels > 1 ? buffer.getWritePointer (1) : outL;
        for (int start = 0; start < numSamples; start += preparedBlockSize)
        {
            const int n = juce::jmin (preparedBlockSize, numSamples - start);
            std::copy (chL + start, chL + start + n, harmDryL.begin());
            std::copy (chR + start, chR + start + n, harmDryR.begin());
            engine.process (harmDryL.data(), harmDryR.data(), outL + start,
                            outR == outL ? outL + start : outR + start, n);
            harmony.process (harmDryL.data(), harmDryR.data(), outL + start,
                             outR == outL ? outL + start : outR + start, n, engine.getTargetMidi());
        }
    }
    else if (numChannels > 1)
    {
        float* outR = buffer.getWritePointer (1);
        engine.process (chL, chR, outL, outR, numSamples);
    }
    else
    {
        // Mono bus: the engine always operates in stereo internally: L and
        // R chains are fed identical input here (chR==chL above), so they
        // produce identical output deterministically - write both engine
        // outputs into the same mono buffer, R after L, which simply
        // leaves the (identical) R result in place.
        engine.process (chL, chR, outL, outL, numSamples);
    }

    detectedPitch.store (engine.getDetectedHz());
    inputMidi.store (engine.getInputMidi());
    targetMidi.store (engine.getTargetMidi());
    correctedPitch.store (engine.getCorrectedHz());
    pitchConfidence.store (engine.getConfidence());
    voicedDebug.store (engine.isVoicedDebug());
    shifterRatioDebug.store (engine.getShifterRatioDebug());

    // Real level history for the hero display's waveform - RMS of the
    // ACTUAL output audio for this block, not a decorative animation.
    float sumSq = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        sumSq += outL[i] * outL[i];
    const float blockRms = numSamples > 0 ? std::sqrt (sumSq / (float) numSamples) : 0.0f;
    const int writeIdx = levelHistoryWrite.load();
    levelHistory[(size_t) writeIdx].store (blockRms);
    levelHistoryWrite.store ((writeIdx + 1) % kLevelHistorySize);
}

float NovaPitchAudioProcessor::getLevelHistoryAt (int indexFromOldest) const noexcept
{
    const int oldest = levelHistoryWrite.load(); // next write slot == oldest sample
    const int idx = (oldest + juce::jlimit (0, kLevelHistorySize - 1, indexFromOldest)) % kLevelHistorySize;
    return levelHistory[(size_t) idx].load();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NovaPitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}
#endif

juce::AudioProcessorEditor* NovaPitchAudioProcessor::createEditor()
{
    return new NovaPitchAudioProcessorEditor (*this);
}

bool NovaPitchAudioProcessor::hasEditor() const { return true; }
const juce::String NovaPitchAudioProcessor::getName() const { return JucePlugin_Name; }
bool NovaPitchAudioProcessor::acceptsMidi()  const { return true; }  // Harmony MIDI mode
bool NovaPitchAudioProcessor::producesMidi() const { return false; }
bool NovaPitchAudioProcessor::isMidiEffect() const { return false; }
double NovaPitchAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int NovaPitchAudioProcessor::getNumPrograms()    { return 1; }
int NovaPitchAudioProcessor::getCurrentProgram() { return 0; }
void NovaPitchAudioProcessor::setCurrentProgram (int) {}
const juce::String NovaPitchAudioProcessor::getProgramName (int) { return {}; }
void NovaPitchAudioProcessor::changeProgramName (int, const juce::String&) {}

void NovaPitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    abSnapshot.addToXml (*xml);
    xml->setAttribute ("compactView", compactView.load() ? 1 : 0);
    copyXmlToBinary (*xml, destData);
}

void NovaPitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    // One-time migration: the pre-rebuild "formant" parameter was a bool
    // (real value 0.0 or 1.0, same ID kept for automation stability). It's
    // now a continuous 0-100 amount, and APVTS restores a param's raw
    // *real* value, not a normalized one - so a session saved by that old
    // bool version would otherwise land at 1/100 (~"off") instead of "on".
    // Detect a bool-shaped raw value before restoring, remap it to the new
    // range's equivalent afterward.
    float legacyFormantBoolValue = -1.0f;
    for (auto* child : xmlState->getChildIterator())
    {
        if (child->getStringAttribute ("id") == juce::String (ParameterIDs::formant))
        {
            const float raw = (float) child->getDoubleAttribute ("value", -1.0);
            if (raw == 0.0f || raw == 1.0f)
                legacyFormantBoolValue = raw;
        }
    }

    compactView.store (xmlState->getIntAttribute ("compactView", 0) != 0);
    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    abSnapshot.restoreFromXml (*xmlState);

    if (legacyFormantBoolValue >= 0.0f)
        if (auto* param = apvts.getParameter (ParameterIDs::formant))
            param->setValueNotifyingHost (legacyFormantBoolValue); // 0.0/1.0 normalized == 0/100 real value
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NovaPitchAudioProcessor();
}
