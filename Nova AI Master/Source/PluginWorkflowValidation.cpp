// Headless validation of NovaAIMasterProcessor's realtime capture ->
// background analyze -> master workflow: drives real processBlock() calls
// (as a host would) plus the real beginAnalyze()/beginMaster() control
// surface, and pumps the JUCE message loop so the background thread's
// triggerAsyncUpdate()/callAsync completions actually fire - proving the
// cross-thread handoff genuinely works, not just compiles.

#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    bool pumpUntil (std::function<bool()> predicate, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (! predicate())
        {
            if (juce::Time::getMillisecondCounter() > deadline) return false;
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }
        return true;
    }

    bool isFinite (const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (! std::isfinite (d[i])) return false;
        }
        return true;
    }

    juce::AudioBuffer<float> makeBlock (int nCh, int N, double sr, int blockIndex, int blockSize)
    {
        juce::AudioBuffer<float> buf (nCh, N);
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < N; ++i)
            {
                const double t = (blockIndex * blockSize + i) / sr;
                d[i] = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * (220.0 + (blockIndex % 5) * 90.0) * t)
                     + 0.1f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 3000.0 * t);
            }
        }
        return buf;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const double sr = 44100.0;
    const int blockSize = 512;

    NovaAIMasterProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, blockSize);
    proc.prepareToPlay (sr, blockSize);

    std::cout << "\n[Workflow] Idle: audio passes through finite and unprocessed-by-mastering (no plan applied yet)\n";
    {
        auto buf = makeBlock (2, blockSize, sr, 0, blockSize);
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
        check (isFinite (buf), "output finite before any analyze/master");
        check (proc.getButtonState() == NovaAIMasterButtonState::Idle, "button state starts Idle");
    }

    std::cout << "\n[Workflow] beginAnalyze() starts capture; processBlock() feeds real audio into it\n";
    proc.beginAnalyze();
    check (proc.getButtonState() == NovaAIMasterButtonState::Analyzing, "button state -> Analyzing after first beginAnalyze()");

    const int captureBlocks = (int) (sr * 3.0) / blockSize; // capture ~3s of material
    bool capturedAudioFinite = true;
    for (int b = 1; b <= captureBlocks; ++b)
    {
        auto buf = makeBlock (2, blockSize, sr, b, blockSize);
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
        if (! isFinite (buf)) capturedAudioFinite = false;
    }
    check (capturedAudioFinite, "audio stays finite throughout capture");

    std::cout << "\n[Workflow] beginAnalyze() again stops capture and triggers background analysis\n";
    proc.beginAnalyze();

    bool reachedReady = pumpUntil ([&] { return proc.getButtonState() == NovaAIMasterButtonState::Ready; }, 15000);
    check (reachedReady, "background analysis completes and button state reaches Ready within 15s");

    const auto& profile = proc.getLastProfile();
    std::cout << "    measured: integratedLufs=" << profile.integratedLufs
              << " crestFactorDb=" << profile.crestFactorDb << "\n";
    check (profile.integratedLufs > -60.f, "captured analysis produced a plausible (non-silence-floor) profile");

    std::cout << "\n[Workflow] beginMaster() hands a real plan to the audio thread; next processBlock() applies it audibly\n";
    NovaMasterUserPrefs prefs;
    prefs.style = NovaMasterStyle::Clean;
    prefs.intensity = 0.6f;
    prefs.loudness = NovaLoudnessMode::Balanced;
    proc.beginMaster (prefs);
    const bool mastered = pumpUntil ([&] { return proc.getButtonState() == NovaAIMasterButtonState::Complete; }, 30000);
    check (mastered, "button state -> Complete after beginMaster() (loudness calibrated in the background)");

    // Feed a steady tone through several blocks after MASTER, across the
    // crossfade window and beyond - confirms the plan handoff + crossfade
    // (task #83) works correctly end-to-end from the plugin's real control
    // surface, not just when driven directly at the DSP-chain level.
    bool postMasterFinite = true;
    float lastSample = 0.f, lastIn = 0.f;
    float worstJump = 0.f, worstInJump = 0.f;
    double inSq = 0.0, outSq = 0.0;
    for (int b = 0; b < 20; ++b)
    {
        auto buf = makeBlock (2, blockSize, sr, 100 + b, blockSize);
        const auto input = buf;
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
        if (! isFinite (buf)) { postMasterFinite = false; break; }

        const float* d = buf.getReadPointer (0);
        const float* x = input.getReadPointer (0);
        for (int i = 0; i < blockSize; ++i)
        {
            if (b > 0) { worstJump = juce::jmax (worstJump, std::abs (d[i] - lastSample)); worstInJump = juce::jmax (worstInJump, std::abs (x[i] - lastIn)); }
            lastSample = d[i]; lastIn = x[i];
            inSq += x[i] * x[i]; outSq += d[i] * d[i];
        }
    }
    // The test tone itself jumps at every block (its pitch changes), and the
    // master now brings it up to the loudness target - so judge the output's
    // jumps against the input's own, scaled by the level change. A crossfade
    // glitch (a cold chain cutting in) would be far above that.
    const float levelRatio = (float) std::sqrt (outSq / juce::jmax (1.0e-12, inSq));
    std::cout << "    input's own worst jump = " << worstInJump << ", level ratio = " << levelRatio << "\n";
    std::cout << "    worst sample-to-sample jump after MASTER (crossfade should keep this bounded) = " << worstJump << "\n";
    check (postMasterFinite, "audio stays finite through and after the plan crossfade");
    check (worstJump < 1.5f * worstInJump * levelRatio + 0.05f, "no gross discontinuity from the plan transition (crossfade is working)");

    std::cout << "\n[Workflow] State recall (spec 37-38): reopening a session must reproduce the identical master WITHOUT re-analyzing\n";
    {
        juce::MemoryBlock savedState;
        proc.getStateInformation (savedState);

        // Fresh processor, as a DAW would create on project reopen - never
        // touches beginAnalyze()/beginMaster(), only setStateInformation().
        NovaAIMasterProcessor reopened;
        reopened.setPlayConfigDetails (2, 2, sr, blockSize);
        reopened.prepareToPlay (sr, blockSize);
        reopened.setStateInformation (savedState.getData(), (int) savedState.getSize());

        check (reopened.getButtonState() == NovaAIMasterButtonState::Complete,
               "reopened session restores button state to Complete without calling beginAnalyze/beginMaster");

        const auto& origPlan = proc.getLastPlan();
        const auto& restoredPlan = reopened.getLastPlan();
        const bool planMatches = juce::exactlyEqual (origPlan.limiting.thresholdDb, restoredPlan.limiting.thresholdDb)
                               && juce::exactlyEqual (origPlan.output.targetLufs, restoredPlan.output.targetLufs)
                               && juce::exactlyEqual (origPlan.output.gainDb, restoredPlan.output.gainDb)
                               && juce::exactlyEqual (origPlan.compression.ratio, restoredPlan.compression.ratio)
                               && juce::exactlyEqual (origPlan.correctiveEQ.lowShelfDb, restoredPlan.correctiveEQ.lowShelfDb)
                               && origPlan.saturation.active == restoredPlan.saturation.active;
        check (planMatches, "restored MasteringPlan fields are bit-identical to the original");

        // Feed the SAME material into both a fresh (never-reopened) engine
        // primed with the original plan and the reopened one, and compare
        // sample-for-sample once each has run past its own crossfade window.
        NovaAIMasterProcessor reference;
        reference.setPlayConfigDetails (2, 2, sr, blockSize);
        reference.prepareToPlay (sr, blockSize);
        reference.beginAnalyze();
        for (int b = 1; b <= captureBlocks; ++b)
        {
            auto buf = makeBlock (2, blockSize, sr, b, blockSize);
            juce::MidiBuffer midi;
            reference.processBlock (buf, midi);
        }
        reference.beginAnalyze();
        pumpUntil ([&] { return reference.getButtonState() == NovaAIMasterButtonState::Ready; }, 15000);
        reference.beginMaster (prefs);
        pumpUntil ([&] { return reference.getButtonState() == NovaAIMasterButtonState::Complete; }, 30000);

        bool primed = true;
        for (int b = 0; b < 10; ++b) // run both past the crossfade window before comparing
        {
            auto bufRef = makeBlock (2, blockSize, sr, 200 + b, blockSize);
            auto bufReopened = makeBlock (2, blockSize, sr, 200 + b, blockSize);
            juce::MidiBuffer midiRef, midiReopened;
            reference.processBlock (bufRef, midiRef);
            reopened.processBlock (bufReopened, midiReopened);
            if (! isFinite (bufRef) || ! isFinite (bufReopened)) primed = false;
        }
        check (primed, "both engines stay finite through their own crossfades");

        auto bufRef = makeBlock (2, blockSize, sr, 300, blockSize);
        auto bufReopened = makeBlock (2, blockSize, sr, 300, blockSize);
        juce::MidiBuffer midiRef, midiReopened;
        reference.processBlock (bufRef, midiRef);
        reopened.processBlock (bufReopened, midiReopened);

        float maxDiff = 0.f;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* a = bufRef.getReadPointer (ch);
            const float* b = bufReopened.getReadPointer (ch);
            for (int i = 0; i < blockSize; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (a[i] - b[i]));
        }
        std::cout << "    max sample diff, reopened vs. freshly-mastered reference = " << maxDiff << "\n";
        check (maxDiff < 1.0e-5f, "reopened session's audio output matches a freshly-mastered reference sample-for-sample");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL PLUGIN WORKFLOW VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
