#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/NovaFactoryPresets.h"

const juce::StringArray& NovaAIVocalProcessor::macroParamIds()
{
    static const juce::StringArray ids { "clean", "deess", "punch", "air", "warmth", "space", "tune" };
    return ids;
}

NovaAIVocalProcessor::NovaAIVocalProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    apvts.addParameterListener ("vibe", this);
    (void) getNovaFactoryPresets(); // build the table here, never first on the audio thread
}

NovaAIVocalProcessor::~NovaAIVocalProcessor()
{
    apvts.removeParameterListener ("vibe", this);
    aliveToken.reset();
    analysisPool.removeAllJobs (true, 10000);
    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaAIVocalProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray vibeNames;
    for (const auto& v : getNovaVibes()) vibeNames.add (v.name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "vibe", 1 }, "Vibe", vibeNames, 0));

    const auto& defaults = getNovaVibe (0).macros;
    const juce::StringArray names { "Clean", "De-Ess", "Punch", "Air", "Warmth", "Space", "Tune" };
    for (int i = 0; i < kNumMacros; ++i)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { macroParamIds()[i], 1 }, names[i],
            juce::NormalisableRange<float> (0.f, 1.f), defaults[(size_t) i],
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return juce::String (juce::roundToInt (v * 100.f)) + "%"; })));

    juce::StringArray keyChoices { "Auto (Listen)", "Chromatic" };
    for (const char* k : { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" })
    {
        keyChoices.add (juce::String (k) + " Major");
        keyChoices.add (juce::String (k) + " Minor");
    }
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "key", 1 }, "Key", keyChoices, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.f, 1.f), 1.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v * 100.f)) + "%"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // Tempo for the delay and reverb. Sync follows the host (falls back to
    // BPM when the host has none, e.g. the standalone app); off = always BPM.
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "tempoSync", 1 }, "Tempo Sync", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "bpm", 1 }, "BPM", juce::NormalisableRange<float> (40.f, 240.f, 0.1f), 120.f,
        juce::AudioParameterFloatAttributes().withLabel ("BPM")));

    return layout;
}

bool NovaAIVocalProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk && in.size() <= out.size();
}

int NovaAIVocalProcessor::currentVibe() const
{
    return juce::roundToInt (apvts.getRawParameterValue ("vibe")->load());
}

void NovaAIVocalProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Never reallocate the capture buffer under a running analysis job.
    analysisPool.removeAllJobs (true, 10000);
    isCapturing.store (false);
    captureFull.store (false);
    if (listenState.load() == NovaVocalListenState::Listening || listenState.load() == NovaVocalListenState::Analyzing)
        listenState.store (profile.valid ? NovaVocalListenState::Mixed : NovaVocalListenState::Idle);

    currentSampleRate = sampleRate;
    captureCapacity = (int) (sampleRate * kMaxCaptureSeconds);
    captureBuffer.setSize (2, captureCapacity, false, true, false);
    captureWritePos.store (0);

    monoScratch.setSize (1, juce::jmax (samplesPerBlock, 4096), false, true, false);

    // Load current settings before prepare() so the smoothers start at the
    // right values instead of ramping up from zero.
    {
        const juce::SpinLock::ScopedLockType sl (planLock);
        chain.setPlan (pendingPlan);
        planPending.store (false);
    }
    NovaVocalMacros m;
    for (int i = 0; i < kNumMacros; ++i)
        m.v[(size_t) i] = apvts.getRawParameterValue (macroParamIds()[i])->load();
    chain.setMacros (m);
    chain.setVibe (currentVibe());
    {
        const int fpi = activeVoicing.load (std::memory_order_relaxed);
        chain.setVoicing (fpi >= 0 ? &getNovaFactoryPresets()[(size_t) fpi].voicing : nullptr);
    }
    chain.setMix (apvts.getRawParameterValue ("mix")->load());
    chain.setOutputDb (apvts.getRawParameterValue ("output")->load());
    chain.prepare (sampleRate, samplesPerBlock);

    tuner.prepare (sampleRate, samplesPerBlock);
    updateTunerParams (false);
    setLatencySamples (tuner.getLatencySamples()); // constant, tuning on or off
}

void NovaAIVocalProcessor::updateTunerParams (bool bypassed) noexcept
{
    const float t = apvts.getRawParameterValue ("tune")->load();
    int keyPc = 0, scaleMask = 0xFFF; // chromatic
    const int k = juce::roundToInt (apvts.getRawParameterValue ("key")->load());
    if (k >= 2)
    {
        keyPc = (k - 2) / 2;
        scaleMask = NovaVocalTuner::scaleMaskFor (((k - 2) % 2) == 1);
    }
    else if (k == 0 && autoKeyPc.load (std::memory_order_relaxed) >= 0)
    {
        keyPc = autoKeyPc.load (std::memory_order_relaxed);
        scaleMask = NovaVocalTuner::scaleMaskFor (autoKeyMinor.load (std::memory_order_relaxed));
    }

    auto tp = NovaVocalTuner::fromTuneKnob (bypassed ? 0.f : t, keyPc, scaleMask);
    const auto& V = getActiveVoicing();
    tp.retuneMs        *= V.retuneScale;
    tp.humanize         = V.tuneHumanize;
    tp.vibratoPreserve  = V.tuneVibrato;
    tuner.setParams (tp);
}

void NovaAIVocalProcessor::publishAutoKey() noexcept
{
    autoKeyMinor.store (profile.keyMinor);
    autoKeyPc.store (profile.valid && profile.keyValid ? profile.keyPc : -1);
}

juce::String NovaAIVocalProcessor::getActiveKeyName() const
{
    const int k = juce::roundToInt (apvts.getRawParameterValue ("key")->load());
    if (k == 1) return "Chromatic";
    if (k >= 2) return novaKeyName ((k - 2) / 2, ((k - 2) % 2) == 1);
    const int a = autoKeyPc.load();
    return a >= 0 ? novaKeyName (a, autoKeyMinor.load()) + " (auto)" : juce::String ("Chromatic (auto)");
}

void NovaAIVocalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();
    if (n == 0 || numIn == 0) return;

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, n); // mono in -> stereo out

    const float inPeak = buffer.getMagnitude (0, n);
    inLevelDb.store (juce::Decibels::gainToDecibels (inPeak, -100.f), std::memory_order_relaxed);

    // Input scope for the UI spectrum
    {
        int w = scopeWrite.load (std::memory_order_relaxed);
        const float* in0 = buffer.getReadPointer (0);
        const float* in1 = buffer.getReadPointer (numOut >= 2 ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            scopeRing[(size_t) w] = 0.5f * (in0[i] + in1[i]);
            if (++w >= kScopeSize) w = 0;
        }
        scopeWrite.store (w, std::memory_order_relaxed);
    }

    // Capture the dry input for Listen (pre-allocated, copy only)
    if (isCapturing.load (std::memory_order_relaxed))
    {
        const int pos = captureWritePos.load (std::memory_order_relaxed);
        const int toCopy = juce::jmin (n, juce::jmax (0, captureCapacity - pos));
        if (toCopy > 0)
        {
            for (int ch = 0; ch < 2; ++ch)
                captureBuffer.copyFrom (ch, pos, buffer, juce::jmin (ch, numOut - 1), 0, toCopy);
            captureWritePos.store (pos + toCopy, std::memory_order_relaxed);
        }
        if (toCopy < n)
        {
            isCapturing.store (false, std::memory_order_relaxed);
            captureFull.store (true);
            triggerAsyncUpdate();
        }
    }

    const bool bypassed = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    updateTunerParams (bypassed);

    if (bypassed)
    {
        // Still run the tuner as a plain delay so the reported latency holds
        if (numOut >= 2)
            tuner.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        else
        {
            float* L = buffer.getWritePointer (0);
            float* R = monoScratch.getWritePointer (0);
            const int cap = monoScratch.getNumSamples();
            for (int start = 0; start < n; start += cap)
            {
                const int len = juce::jmin (cap, n - start);
                std::copy_n (L + start, len, R);
                tuner.process (L + start, R, len);
            }
        }
        outLevelDb.store (inLevelDb.load (std::memory_order_relaxed), std::memory_order_relaxed);
        return;
    }

    if (planPending.exchange (false, std::memory_order_acquire))
    {
        NovaVocalPlan local;
        {
            const juce::SpinLock::ScopedLockType sl (planLock);
            local = pendingPlan;
        }
        chain.setPlan (local);
    }

    NovaVocalMacros m;
    for (int i = 0; i < kNumMacros; ++i)
        m.v[(size_t) i] = apvts.getRawParameterValue (macroParamIds()[i])->load();
    chain.setMacros (m);
    chain.setVibe (currentVibe());
    {
        const int fpi = activeVoicing.load (std::memory_order_relaxed);
        chain.setVoicing (fpi >= 0 ? &getNovaFactoryPresets()[(size_t) fpi].voicing : nullptr);
    }
    chain.setMix (apvts.getRawParameterValue ("mix")->load());
    chain.setOutputDb (apvts.getRawParameterValue ("output")->load());

    {
        double hostBpm = 0.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto bpm = pos->getBpm())
                    hostBpm = *bpm;
        hostTempo.store ((float) hostBpm, std::memory_order_relaxed);

        const bool sync = apvts.getRawParameterValue ("tempoSync")->load() > 0.5f;
        const double manual = apvts.getRawParameterValue ("bpm")->load();
        const double use = (sync && hostBpm > 20.0) ? hostBpm : manual;
        chain.setTempo (use);
        activeTempo.store ((float) use, std::memory_order_relaxed);
    }

    if (numOut >= 2)
    {
        tuner.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        chain.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    }
    else
    {
        float* L = buffer.getWritePointer (0);
        float* R = monoScratch.getWritePointer (0);
        const int cap = monoScratch.getNumSamples();
        for (int start = 0; start < n; start += cap)
        {
            const int len = juce::jmin (cap, n - start);
            std::copy_n (L + start, len, R);
            tuner.process (L + start, R, len);
            chain.process (L + start, R, len);
            for (int i = 0; i < len; ++i)
                L[start + i] = 0.5f * (L[start + i] + R[i]);
        }
    }

    outLevelDb.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, n), -100.f), std::memory_order_relaxed);
}

// ── Listen workflow (message thread) ─────────────────────────────────────

float NovaAIVocalProcessor::getCapturedSeconds() const noexcept
{
    return currentSampleRate > 0 ? (float) (captureWritePos.load() / currentSampleRate) : 0.f;
}

void NovaAIVocalProcessor::toggleListen()
{
    const auto state = listenState.load();

    if (state == NovaVocalListenState::Analyzing)
        return;

    if (state == NovaVocalListenState::Listening)
    {
        isCapturing.store (false);
        const int captured = captureWritePos.load();
        if (captured < (int) (kMinCaptureSeconds * currentSampleRate))
        {
            summary = "Too short - play at least a few seconds of vocal, then stop.";
            listenState.store (profile.valid ? NovaVocalListenState::Mixed : NovaVocalListenState::Idle);
            return;
        }
        launchAnalysis (captured);
        return;
    }

    captureFull.store (false);
    captureWritePos.store (0);
    isCapturing.store (true);
    summary = {};
    listenState.store (NovaVocalListenState::Listening);
}

void NovaAIVocalProcessor::launchAnalysis (int numSamples)
{
    listenState.store (NovaVocalListenState::Analyzing);

    std::weak_ptr<bool> alive = aliveToken;
    const double sr = currentSampleRate;

    analysisPool.addJob ([this, numSamples, sr, alive]
    {
        auto result = analyzeNovaVocal (captureBuffer.getArrayOfReadPointers(), 2, numSamples, sr);

        juce::MessageManager::callAsync ([this, alive, result]
        {
            if (alive.expired()) return;

            if (! result.valid)
            {
                summary = "Couldn't hear enough singing - try a longer take.";
                listenState.store (profile.valid ? NovaVocalListenState::Mixed : NovaVocalListenState::Idle);
                return;
            }

            profile = result;
            publishAutoKey();
            applyDecisionForCurrentVibe();
            listenState.store (NovaVocalListenState::Mixed);
        });
    });
}

void NovaAIVocalProcessor::applyDecisionForCurrentVibe()
{
    const int v = currentVibe();
    lastAppliedVibe = v;

    const int fpi = activeVoicing.load();
    if (profile.valid)
    {
        const auto d = decideNovaVocalMix (profile, v);
        currentPlan = d.plan;
        pushPlan (currentPlan);
        summary = d.summary;
        if (fpi < 0) setMacroParams (d.macros);
    }
    else if (fpi < 0)
    {
        setMacroParams (novaVibeDefaultMacros (v));
    }

    if (fpi >= 0) // artist preset: its own knob positions
    {
        NovaVocalMacros m;
        const auto& km = getNovaFactoryPresets()[(size_t) fpi].voicing.macros;
        for (int i = 0; i < kNumMacros; ++i) m.v[(size_t) i] = km[(size_t) i];
        setMacroParams (m);
    }
}

void NovaAIVocalProcessor::setMacroParams (const NovaVocalMacros& m)
{
    for (int i = 0; i < kNumMacros; ++i)
        if (auto* p = apvts.getParameter (macroParamIds()[i]))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (m.v[(size_t) i]));
            p->endChangeGesture();
        }
}

void NovaAIVocalProcessor::pushPlan (const NovaVocalPlan& plan)
{
    {
        const juce::SpinLock::ScopedLockType sl (planLock);
        pendingPlan = plan;
    }
    planPending.store (true, std::memory_order_release);
}

void NovaAIVocalProcessor::parameterChanged (const juce::String&, float)
{
    // May arrive on the audio thread (automation) - defer to the message thread.
    vibeChanged.store (true);
    triggerAsyncUpdate();
}

void NovaAIVocalProcessor::handleAsyncUpdate()
{
    if (captureFull.exchange (false) && listenState.load() == NovaVocalListenState::Listening)
        launchAnalysis (captureWritePos.load());

    if (vibeChanged.exchange (false) && currentVibe() != lastAppliedVibe)
    {
        activeVoicing.store (-1); // a different vibe card was picked: back to plain vibes
        applyDecisionForCurrentVibe();
    }
}

// ── State ────────────────────────────────────────────────────────────────

std::unique_ptr<juce::XmlElement> NovaAIVocalProcessor::createStateXml()
{
    auto root = std::make_unique<juce::XmlElement> ("NovaAIVocalState");

    if (auto xml = apvts.copyState().createXml())
        root->addChildElement (xml.release());

    root->addChildElement (profile.toXml().release());
    root->addChildElement (novaVocalPlanToXml (currentPlan).release());
    root->setAttribute ("summary", summary);
    root->setAttribute ("presetName", currentPresetName);
    if (const int fpi = activeVoicing.load(); fpi >= 0)
        root->setAttribute ("voicing", getNovaFactoryPresets()[(size_t) fpi].name);
    return root;
}

void NovaAIVocalProcessor::restoreStateXml (const juce::XmlElement& root)
{
    if (auto* s = root.getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*s));

    // Restored knobs are the user's mix - don't let the vibe listener
    // overwrite them with fresh decisions.
    lastAppliedVibe = currentVibe();

    profile = {};
    if (auto* p = root.getChildByName ("VocalProfile"))
        profile = NovaVocalProfile::fromXml (*p);
    publishAutoKey();

    currentPlan = {};
    if (auto* p = root.getChildByName ("VocalPlan"))
        currentPlan = novaVocalPlanFromXml (*p);
    pushPlan (currentPlan);

    summary = root.getStringAttribute ("summary");
    currentPresetName = root.getStringAttribute ("presetName");
    activeVoicing.store (findFactoryPreset (root.getStringAttribute ("voicing")));
    listenState.store (profile.valid ? NovaVocalListenState::Mixed : NovaVocalListenState::Idle);
}

void NovaAIVocalProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto root = createStateXml())
        copyXmlToBinary (*root, dest);
}

void NovaAIVocalProcessor::setStateInformation (const void* data, int size)
{
    if (auto root = getXmlFromBinary (data, size))
        restoreStateXml (*root);
}

// ── Presets ──────────────────────────────────────────────────────────────

juce::File NovaAIVocalProcessor::getUserPresetsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
       #if JUCE_MAC
        .getChildFile ("Application Support")
       #endif
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova AI Vocal")
        .getChildFile ("User Presets");
}

static bool isFactoryPresetName (const juce::String& n)
{
    for (const auto& fp : getNovaFactoryPresets())
        if (n.equalsIgnoreCase (fp.name)) return true;
    return false;
}

static juce::String sanitisePresetName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).substring (0, 64).trim();
}

juce::StringArray NovaAIVocalProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : getUserPresetsFolder().findChildFiles (juce::File::findFiles, false, "*.novavocalpreset"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool NovaAIVocalProcessor::saveUserPreset (const juce::String& rawName)
{
    auto name = sanitisePresetName (rawName);
    if (isFactoryPresetName (name)) name << " (mine)";
    if (name.isEmpty() || name.equalsIgnoreCase ("Default")) return false;

    auto folder = getUserPresetsFolder();
    if (! folder.createDirectory()) return false;

    currentPresetName = name;
    auto root = createStateXml();
    if (! root->writeTo (folder.getChildFile (name + ".novavocalpreset")))
        return false;
    return true;
}

bool NovaAIVocalProcessor::loadUserPreset (const juce::String& name)
{
    const auto file = getUserPresetsFolder().getChildFile (sanitisePresetName (name) + ".novavocalpreset");
    auto root = juce::XmlDocument::parse (file);
    if (root == nullptr || ! root->hasTagName ("NovaAIVocalState")) return false;

    // Tempo belongs to the song, not the preset: keep the current settings.
    const float keepSync = apvts.getRawParameterValue ("tempoSync")->load();
    const float keepBpm  = apvts.getRawParameterValue ("bpm")->load();
    restoreStateXml (*root);
    for (auto [id, v] : { std::pair<const char*, float> { "tempoSync", keepSync }, { "bpm", keepBpm } })
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

bool NovaAIVocalProcessor::deleteUserPreset (const juce::String& name)
{
    const auto file = getUserPresetsFolder().getChildFile (sanitisePresetName (name) + ".novavocalpreset");
    if (! file.existsAsFile()) return false;
    if (currentPresetName == file.getFileNameWithoutExtension()) currentPresetName = {};
    return file.moveToTrash() || file.deleteFile();
}

bool NovaAIVocalProcessor::loadFactoryPreset (const juce::String& name)
{
    for (const auto& fp : getNovaFactoryPresets())
    {
        if (name != fp.name) continue;

        auto setParam = [this] (const char* id, float plainValue)
        {
            if (auto* p = apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
                p->endChangeGesture();
            }
        };

        // Vibe first, marked as applied so the vibe listener doesn't
        // replace the preset's knobs with the vibe's own defaults.
        activeVoicing.store (findFactoryPreset (fp.name));
        setParam ("vibe", (float) fp.vibe);
        lastAppliedVibe = fp.vibe;

        // Keep Listen's corrections for this voice, re-planned for the vibe.
        if (profile.valid)
        {
            const auto d = decideNovaVocalMix (profile, fp.vibe);
            currentPlan = d.plan;
            pushPlan (currentPlan);
            summary = d.summary;
        }

        NovaVocalMacros m;
        for (int i = 0; i < kNumMacros; ++i) m.v[(size_t) i] = fp.voicing.macros[(size_t) i];
        setMacroParams (m);
        setParam ("mix", fp.mix);
        setParam ("output", fp.outputDb);
        currentPresetName = fp.name;
        return true;
    }
    return false;
}

int NovaAIVocalProcessor::findFactoryPreset (const juce::String& name) const
{
    if (name.isEmpty()) return -1;
    const auto& all = getNovaFactoryPresets();
    for (size_t i = 0; i < all.size(); ++i)
        if (name == all[i].name) return (int) i;
    return -1;
}

const NovaVibe& NovaAIVocalProcessor::getActiveVoicing() const noexcept
{
    const int fpi = activeVoicing.load (std::memory_order_relaxed);
    return fpi >= 0 ? getNovaFactoryPresets()[(size_t) fpi].voicing : getNovaVibe (currentVibe());
}

void NovaAIVocalProcessor::loadDefaultPreset()
{
    activeVoicing.store (-1);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    forgetVoice();
    currentPresetName = {};
}

void NovaAIVocalProcessor::reapplyVibe()
{
    applyDecisionForCurrentVibe();
}

void NovaAIVocalProcessor::forgetVoice()
{
    if (listenState.load() == NovaVocalListenState::Analyzing) return;
    isCapturing.store (false);
    profile = {};
    currentPlan = {};
    pushPlan (currentPlan);
    summary = {};
    publishAutoKey();
    listenState.store (NovaVocalListenState::Idle);
    applyDecisionForCurrentVibe();
}

void NovaAIVocalProcessor::copyScope (float* dest, int num) const noexcept
{
    num = juce::jmin (num, kScopeSize);
    int r = scopeWrite.load (std::memory_order_relaxed) - num;
    if (r < 0) r += kScopeSize;
    for (int i = 0; i < num; ++i)
    {
        dest[i] = scopeRing[(size_t) r];
        if (++r >= kScopeSize) r = 0;
    }
}

juce::AudioProcessorEditor* NovaAIVocalProcessor::createEditor()
{
    return new NovaAIVocalEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaAIVocalProcessor(); }
