#include "PluginProcessor.h"
#include "PluginEditor.h"

// Determinism (spec 37-38): a reopened session must reproduce the identical
// master without re-analyzing, so the resolved MasteringPlan is persisted
// alongside APVTS state, not just the Style/Intensity/Loudness inputs that
// produced it - re-running decide() on reload would need the original
// AudioProfile (never saved, would mean re-analyzing) to be bit-identical.
namespace
{
    std::unique_ptr<juce::XmlElement> planToXml (const NovaMasteringPlan& plan)
    {
        auto xml = std::make_unique<juce::XmlElement> ("MasteringPlan");

        auto* eq = xml->createNewChildElement ("CorrectiveEQ");
        eq->setAttribute ("active", plan.correctiveEQ.active);
        eq->setAttribute ("lowShelfFreq", (double) plan.correctiveEQ.lowShelfFreq);
        eq->setAttribute ("lowShelfDb", (double) plan.correctiveEQ.lowShelfDb);
        eq->setAttribute ("lowMidFreq", (double) plan.correctiveEQ.lowMidFreq);
        eq->setAttribute ("lowMidDb", (double) plan.correctiveEQ.lowMidDb);
        eq->setAttribute ("highMidFreq", (double) plan.correctiveEQ.highMidFreq);
        eq->setAttribute ("highMidDb", (double) plan.correctiveEQ.highMidDb);
        eq->setAttribute ("highShelfFreq", (double) plan.correctiveEQ.highShelfFreq);
        eq->setAttribute ("highShelfDb", (double) plan.correctiveEQ.highShelfDb);

        auto* res = xml->createNewChildElement ("ResonanceControl");
        res->setAttribute ("active", plan.resonanceControl.active);
        res->setAttribute ("amount", (double) plan.resonanceControl.amount);

        auto* comp = xml->createNewChildElement ("Compression");
        comp->setAttribute ("active", plan.compression.active);
        comp->setAttribute ("thresholdDb", (double) plan.compression.thresholdDb);
        comp->setAttribute ("ratio", (double) plan.compression.ratio);
        comp->setAttribute ("attackMs", (double) plan.compression.attackMs);
        comp->setAttribute ("releaseMs", (double) plan.compression.releaseMs);
        comp->setAttribute ("makeupDb", (double) plan.compression.makeupDb);

        auto* sat = xml->createNewChildElement ("Saturation");
        sat->setAttribute ("active", plan.saturation.active);
        sat->setAttribute ("drive", (double) plan.saturation.drive);
        sat->setAttribute ("mix", (double) plan.saturation.mix);

        auto* stereo = xml->createNewChildElement ("Stereo");
        stereo->setAttribute ("active", plan.stereo.active);
        stereo->setAttribute ("widthAmount", (double) plan.stereo.widthAmount);
        stereo->setAttribute ("bassMonoHz", (double) plan.stereo.bassMonoHz);

        auto* clip = xml->createNewChildElement ("Clipping");
        clip->setAttribute ("active", plan.clipping.active);
        clip->setAttribute ("ceilingDb", (double) plan.clipping.ceilingDb);

        auto* lim = xml->createNewChildElement ("Limiting");
        lim->setAttribute ("active", plan.limiting.active);
        lim->setAttribute ("thresholdDb", (double) plan.limiting.thresholdDb);
        lim->setAttribute ("truePeakCeilDb", (double) plan.limiting.truePeakCeilDb);
        lim->setAttribute ("releaseMs", (double) plan.limiting.releaseMs);

        auto* out = xml->createNewChildElement ("Output");
        out->setAttribute ("targetLufs", (double) plan.output.targetLufs);
        out->setAttribute ("gainDb", (double) plan.output.gainDb);

        return xml;
    }

    NovaMasteringPlan planFromXml (const juce::XmlElement& xml)
    {
        NovaMasteringPlan plan;

        if (auto* eq = xml.getChildByName ("CorrectiveEQ"))
        {
            plan.correctiveEQ.active        = eq->getBoolAttribute ("active", plan.correctiveEQ.active);
            plan.correctiveEQ.lowShelfFreq  = (float) eq->getDoubleAttribute ("lowShelfFreq", plan.correctiveEQ.lowShelfFreq);
            plan.correctiveEQ.lowShelfDb    = (float) eq->getDoubleAttribute ("lowShelfDb", plan.correctiveEQ.lowShelfDb);
            plan.correctiveEQ.lowMidFreq    = (float) eq->getDoubleAttribute ("lowMidFreq", plan.correctiveEQ.lowMidFreq);
            plan.correctiveEQ.lowMidDb      = (float) eq->getDoubleAttribute ("lowMidDb", plan.correctiveEQ.lowMidDb);
            plan.correctiveEQ.highMidFreq   = (float) eq->getDoubleAttribute ("highMidFreq", plan.correctiveEQ.highMidFreq);
            plan.correctiveEQ.highMidDb     = (float) eq->getDoubleAttribute ("highMidDb", plan.correctiveEQ.highMidDb);
            plan.correctiveEQ.highShelfFreq = (float) eq->getDoubleAttribute ("highShelfFreq", plan.correctiveEQ.highShelfFreq);
            plan.correctiveEQ.highShelfDb   = (float) eq->getDoubleAttribute ("highShelfDb", plan.correctiveEQ.highShelfDb);
        }

        if (auto* res = xml.getChildByName ("ResonanceControl"))
        {
            plan.resonanceControl.active = res->getBoolAttribute ("active", plan.resonanceControl.active);
            plan.resonanceControl.amount = (float) res->getDoubleAttribute ("amount", plan.resonanceControl.amount);
        }

        if (auto* comp = xml.getChildByName ("Compression"))
        {
            plan.compression.active      = comp->getBoolAttribute ("active", plan.compression.active);
            plan.compression.thresholdDb = (float) comp->getDoubleAttribute ("thresholdDb", plan.compression.thresholdDb);
            plan.compression.ratio       = (float) comp->getDoubleAttribute ("ratio", plan.compression.ratio);
            plan.compression.attackMs    = (float) comp->getDoubleAttribute ("attackMs", plan.compression.attackMs);
            plan.compression.releaseMs   = (float) comp->getDoubleAttribute ("releaseMs", plan.compression.releaseMs);
            plan.compression.makeupDb    = (float) comp->getDoubleAttribute ("makeupDb", plan.compression.makeupDb);
        }

        if (auto* sat = xml.getChildByName ("Saturation"))
        {
            plan.saturation.active = sat->getBoolAttribute ("active", plan.saturation.active);
            plan.saturation.drive  = (float) sat->getDoubleAttribute ("drive", plan.saturation.drive);
            plan.saturation.mix    = (float) sat->getDoubleAttribute ("mix", plan.saturation.mix);
        }

        if (auto* stereo = xml.getChildByName ("Stereo"))
        {
            plan.stereo.active      = stereo->getBoolAttribute ("active", plan.stereo.active);
            plan.stereo.widthAmount = (float) stereo->getDoubleAttribute ("widthAmount", plan.stereo.widthAmount);
            plan.stereo.bassMonoHz  = (float) stereo->getDoubleAttribute ("bassMonoHz", plan.stereo.bassMonoHz);
        }

        if (auto* clip = xml.getChildByName ("Clipping"))
        {
            plan.clipping.active    = clip->getBoolAttribute ("active", plan.clipping.active);
            plan.clipping.ceilingDb = (float) clip->getDoubleAttribute ("ceilingDb", plan.clipping.ceilingDb);
        }

        if (auto* lim = xml.getChildByName ("Limiting"))
        {
            plan.limiting.active         = lim->getBoolAttribute ("active", plan.limiting.active);
            plan.limiting.thresholdDb    = (float) lim->getDoubleAttribute ("thresholdDb", plan.limiting.thresholdDb);
            plan.limiting.truePeakCeilDb = (float) lim->getDoubleAttribute ("truePeakCeilDb", plan.limiting.truePeakCeilDb);
            plan.limiting.releaseMs      = (float) lim->getDoubleAttribute ("releaseMs", plan.limiting.releaseMs);
        }

        if (auto* out = xml.getChildByName ("Output"))
        {
            plan.output.targetLufs = (float) out->getDoubleAttribute ("targetLufs", plan.output.targetLufs);
            plan.output.gainDb     = (float) out->getDoubleAttribute ("gainDb", plan.output.gainDb);
        }

        return plan;
    }
}

NovaAIMasterProcessor::NovaAIMasterProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{}

NovaAIMasterProcessor::~NovaAIMasterProcessor()
{
    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout NovaAIMasterProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "style", "Style",
        juce::StringArray { "Clean", "Warm", "Punchy", "Loud", "Open" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "intensity", "Intensity", 0.f, 1.f, 0.5f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "loudness", "Loudness",
        juce::StringArray { "Streaming", "Balanced", "Loud" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "bypass", "Bypass", false));

    return layout;
}

void NovaAIMasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numChannels = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels(), 2);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numChannels };

    analysisEngine.prepare (spec);
    realtimeEngine.prepare (spec);

    captureCapacitySamples = (int) (sampleRate * kMaxCaptureSeconds);
    captureBuffer.setSize (numChannels, captureCapacitySamples, false, true, true);
    captureWritePos.store (0);
    isCapturing.store (false);
    analysisJobRunning.store (false);
}

void NovaAIMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (licenseGate.blocks (buffer)) return;   // not activated: pass audio through
    juce::ScopedNoDenormals noDenormals;

    if (*apvts.getRawParameterValue ("bypass") > 0.5f) return;

    // Capture (spec section 31: no allocation - captureBuffer is fixed-size
    // from prepareToPlay). If the buffer fills mid-capture, stop and hand
    // off to the background thread via triggerAsyncUpdate() - launching a
    // thread directly here would not be realtime-safe.
    if (isCapturing.load (std::memory_order_relaxed))
    {
        const int writePos = captureWritePos.load (std::memory_order_relaxed);
        const int spaceLeft = juce::jmax (0, captureCapacitySamples - writePos);
        const int numToCopy = juce::jmin (buffer.getNumSamples(), spaceLeft);

        if (numToCopy > 0)
        {
            const int nCh = juce::jmin (buffer.getNumChannels(), captureBuffer.getNumChannels());
            for (int ch = 0; ch < nCh; ++ch)
                captureBuffer.copyFrom (ch, writePos, buffer, ch, 0, numToCopy);
            captureWritePos.store (writePos + numToCopy, std::memory_order_relaxed);
        }

        if (numToCopy < buffer.getNumSamples())
        {
            isCapturing.store (false, std::memory_order_relaxed);
            triggerAsyncUpdate();
        }
    }

    // Plan handoff from the message thread (spinlock over a small POD
    // struct copy - bounded, not a real-time hazard).
    if (planPending.exchange (false, std::memory_order_acquire))
    {
        NovaMasteringPlan localPlan;
        {
            const juce::SpinLock::ScopedLockType sl (planLock);
            localPlan = pendingPlan;
        }
        realtimeEngine.setPlan (localPlan);
    }

    realtimeEngine.process (buffer);
}

juce::AudioProcessorEditor* NovaAIMasterProcessor::createEditor()
{
    return new NovaAIMasterEditor (*this);
}

void NovaAIMasterProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement root ("NovaAIMasterState");

    auto state = apvts.copyState();
    if (auto apvtsXml = state.createXml())
        root.addChildElement (apvtsXml.release());

    // Only worth persisting once a plan has actually been produced - an
    // Idle/Analyzing/Ready session has nothing deterministic to recall yet.
    const auto state_ = buttonState.load();
    if (state_ == NovaAIMasterButtonState::Complete)
    {
        root.setAttribute ("buttonState", (int) state_);
        root.addChildElement (planToXml (lastPlan).release());
    }

    copyXmlToBinary (root, dest);
}

void NovaAIMasterProcessor::setStateInformation (const void* data, int size)
{
    auto root = getXmlFromBinary (data, size);
    if (root == nullptr) return;

    if (auto* apvtsXml = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));

    if (auto* planXml = root->getChildByName ("MasteringPlan"))
    {
        lastPlan = planFromXml (*planXml);

        {
            const juce::SpinLock::ScopedLockType sl (planLock);
            pendingPlan = lastPlan;
        }
        planPending.store (true, std::memory_order_release);

        buttonState.store (NovaAIMasterButtonState::Complete);
    }
}

void NovaAIMasterProcessor::beginAnalyze()
{
    const auto state = buttonState.load();

    if (state == NovaAIMasterButtonState::Analyzing)
    {
        // Already capturing (or already crunching, in which case this is a
        // no-op guarded by analysisJobRunning below) - stop capture now.
        isCapturing.store (false);
        if (! analysisJobRunning.exchange (true))
            triggerAsyncUpdate();
        return;
    }

    // Idle/Ready/Complete -> start a fresh capture.
    captureWritePos.store (0);
    isCapturing.store (true);
    buttonState.store (NovaAIMasterButtonState::Analyzing);
}

void NovaAIMasterProcessor::handleAsyncUpdate()
{
    const int captured = captureWritePos.load();
    juce::Thread::launch ([this, captured] { runAnalysisJob (captured); });
}

void NovaAIMasterProcessor::runAnalysisJob (int numCapturedSamples)
{
    if (numCapturedSamples <= 0)
    {
        juce::MessageManager::callAsync ([this] {
            analysisJobRunning.store (false);
            buttonState.store (NovaAIMasterButtonState::Idle);
        });
        return;
    }

    juce::AudioBuffer<float> capturedView (captureBuffer.getArrayOfWritePointers(),
                                           captureBuffer.getNumChannels(), 0, numCapturedSamples);
    analysisEngine.analyze (capturedView);
    analysisEngine.finishAnalysis();
    lastProfile = analysisEngine.getProfile();

    juce::MessageManager::callAsync ([this] {
        analysisJobRunning.store (false);
        buttonState.store (NovaAIMasterButtonState::Ready);
        if (onAnalysisComplete) onAnalysisComplete();
    });
}

void NovaAIMasterProcessor::beginMaster (const NovaMasterUserPrefs& prefs)
{
    const auto state = buttonState.load();
    if (state != NovaAIMasterButtonState::Ready && state != NovaAIMasterButtonState::Complete)
        return; // no profile to decide from yet

    buttonState.store (NovaAIMasterButtonState::Processing);

    const NovaMasteringPlan plan = analysisEngine.decide (prefs);
    lastPlan = plan;

    {
        const juce::SpinLock::ScopedLockType sl (planLock);
        pendingPlan = plan;
    }
    planPending.store (true, std::memory_order_release);

    buttonState.store (NovaAIMasterButtonState::Complete);
    if (onMasterApplied) onMasterApplied();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaAIMasterProcessor(); }
