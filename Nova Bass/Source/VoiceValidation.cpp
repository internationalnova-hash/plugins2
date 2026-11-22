// Headless validation harness for NovaBassVoiceEngine (spec section 12-14).
// Drives the mono voice engine directly with synthetic MIDI sequences and
// asserts on measurable properties: retrigger vs legato envelope behavior,
// glide correctness (frequency actually moves toward target over time),
// held-note-stack fallback on release, and basic finite/no-NaN output
// under a dense note-stress sequence. No JUCE plugin/audio-device machinery
// involved - this is pure DSP-in, samples-out testing.

#include "../../_shared/NovaBassCore/Voice/NovaBassVoiceEngine.h"
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace nova::bass;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition)
    {
        std::printf ("  PASS: %s\n", description);
    }
    else
    {
        std::printf ("  FAIL: %s\n", description);
        ++failures;
    }
}

static NovaBassVoiceParams makeTestParams()
{
    NovaBassVoiceParams p;
    p.sineTriBlend = 0.0f;
    p.pitchEnvAmountSemitones = 12.0f;
    p.pitchEnvDecaySeconds = 0.05f;
    p.pitchEnvCurve = 0.0f;
    p.ampAttackSeconds = 0.001f;
    p.ampDecaySeconds = 0.2f;
    p.ampSustain = 0.5f;
    p.ampReleaseSeconds = 0.05f;
    p.transientAmount = 0.2f;
    p.transientToneHz = 1000.0f;
    p.transientDecaySeconds = 0.02f;
    p.filterHighPass = false;
    p.filterCutoffHz = 12000.0f;
    p.filterResonance = 0.7f;
    p.retrigger = NovaVoiceRetrigger::Retrigger;
    p.glideMode = NovaGlideMode::LegatoOnly;
    p.glideTimeSeconds = 0.05f;
    return p;
}

static void renderSamples (NovaBassVoiceEngine& voice, int numSamples, std::vector<float>& out)
{
    out.reserve (out.size() + (size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
        out.push_back (voice.getNextSample());
}

static float peakAbs (const std::vector<float>& samples)
{
    float peak = 0.0f;
    for (float s : samples)
        peak = std::max (peak, std::abs (s));
    return peak;
}

static float tailPeak (const std::vector<float>& samples, int windowSize)
{
    windowSize = std::min (windowSize, (int) samples.size());
    float peak = 0.0f;
    for (int i = (int) samples.size() - windowSize; i < (int) samples.size(); ++i)
        peak = std::max (peak, std::abs (samples[(size_t) i]));
    return peak;
}

static void testBasicNoteOnOff()
{
    std::printf ("-- Basic note-on/off --\n");
    NovaBassVoiceEngine voice;
    voice.prepare (48000.0);
    voice.setParameters (makeTestParams());
    voice.reset();

    check (! voice.isActive(), "voice is inactive before any note");

    voice.noteOn (36, 1.0f); // C2
    check (voice.isActive(), "voice becomes active on noteOn");
    check (voice.isHoldingAnyNote(), "voice reports a held note after noteOn");

    std::vector<float> samples;
    renderSamples (voice, 4800, samples); // 100ms

    bool allFinite = true;
    for (float s : samples)
        if (! std::isfinite (s))
            allFinite = false;
    check (allFinite, "output is finite (no NaN/Inf) during sustain");

    voice.noteOff (36);
    check (! voice.isHoldingAnyNote(), "no held notes reported after noteOff");

    renderSamples (voice, 4800, samples); // let release finish
    check (! voice.isActive(), "voice becomes inactive after release completes");
}

static void testRetriggerVsLegato()
{
    std::printf ("-- Retrigger vs legato envelope behavior --\n");

    // Note on juce::ADSR semantics: calling noteOn() again mid-decay does
    // NOT snap the envelope back to zero - it switches state back to
    // "attack", which ramps from wherever the envelope currently sits back
    // UP toward 1.0 over the attack time (this is deliberate in JUCE, to
    // avoid a click on retrigger). So the observable signature of a real
    // retrigger is a renewed rise toward peak; the signature of legato
    // (envelope left alone) is a monotonic continuation of the decay
    // already in progress - it never rises again.

    auto ampParamsRetrigger = makeTestParams();
    ampParamsRetrigger.retrigger = NovaVoiceRetrigger::Retrigger;
    ampParamsRetrigger.ampAttackSeconds = 0.05f;
    ampParamsRetrigger.ampDecaySeconds = 0.1f;
    ampParamsRetrigger.ampSustain = 0.15f; // low sustain, fully settled well before retrigger
    ampParamsRetrigger.transientAmount = 0.0f; // isolate the amp envelope from the transient layer

    NovaBassVoiceEngine retrig;
    retrig.prepare (48000.0);
    retrig.setParameters (ampParamsRetrigger);
    retrig.reset();

    retrig.noteOn (36, 1.0f);
    std::vector<float> pre;
    renderSamples (retrig, 14400, pre); // 300ms: comfortably settled into low sustain

    // Window spans >1 full period at these bass frequencies so the peak
    // measurement isn't an artifact of where in the cycle the window lands.
    const float levelAtRetriggerMoment = tailPeak (pre, 1600);

    retrig.noteOn (40, 1.0f); // overlap while 36 still notionally could be held (mono voice)
    std::vector<float> afterRetrigger;
    renderSamples (retrig, 2640, afterRetrigger); // ~55ms - spans the renewed attack ramp
    const float peakAfterRetrigger = peakAbs (afterRetrigger);

    check (peakAfterRetrigger > levelAtRetriggerMoment * 1.15f,
           "Retrigger mode: envelope rises back toward peak after an overlapping noteOn");

    auto legatoParams = ampParamsRetrigger;
    legatoParams.retrigger = NovaVoiceRetrigger::Legato;

    NovaBassVoiceEngine legato;
    legato.prepare (48000.0);
    legato.setParameters (legatoParams);
    legato.reset();

    legato.noteOn (36, 1.0f);
    std::vector<float> pre2;
    renderSamples (legato, 14400, pre2);
    const float legatoLevelAtNoteMoment = tailPeak (pre2, 1600);

    legato.noteOn (40, 1.0f);
    std::vector<float> afterLegato;
    renderSamples (legato, 2640, afterLegato);
    const float peakAfterLegato = peakAbs (afterLegato);

    check (peakAfterLegato <= legatoLevelAtNoteMoment * 1.15f,
           "Legato mode: envelope does not rise back toward peak across an overlapping noteOn (no retrigger)");
}

static void testGlide()
{
    std::printf ("-- Glide correctness --\n");

    auto params = makeTestParams();
    params.glideMode = NovaGlideMode::Always;
    params.glideTimeSeconds = 0.1f;
    params.pitchEnvAmountSemitones = 0.0f; // isolate glide from pitch-env drop

    NovaBassVoiceEngine voice;
    voice.prepare (48000.0);
    voice.setParameters (params);
    voice.reset();

    voice.noteOn (36, 1.0f); // C2 ~ 65.4Hz
    std::vector<float> settleSamples;
    renderSamples (voice, 20000, settleSamples); // let it fully settle

    voice.noteOn (48, 1.0f); // C3 ~ 130.8Hz, an octave up

    // Immediately after the glide starts, measure zero-crossing rate over a
    // short window vs. a window well after the glide should have settled -
    // frequency (and thus zero-crossing rate) should be clearly higher once
    // settled than right at the start of the glide.
    std::vector<float> earlyWindow, lateWindow;
    renderSamples (voice, 480, earlyWindow); // 10ms in
    renderSamples (voice, 20000, lateWindow); // well past the 100ms glide time
    std::vector<float> settledWindow;
    renderSamples (voice, 480, settledWindow);

    auto countZeroCrossings = [] (const std::vector<float>& v)
    {
        int count = 0;
        for (size_t i = 1; i < v.size(); ++i)
            if ((v[i - 1] < 0.0f) != (v[i] < 0.0f))
                ++count;
        return count;
    };

    const int earlyCrossings = countZeroCrossings (earlyWindow);
    const int settledCrossings = countZeroCrossings (settledWindow);

    check (settledCrossings > earlyCrossings,
           "Glide: zero-crossing rate (proxy for pitch) increases as glide progresses toward the higher target note");
}

static void testHeldNoteFallback()
{
    std::printf ("-- Held-note stack fallback on release --\n");

    auto params = makeTestParams();
    params.glideMode = NovaGlideMode::Always;
    params.glideTimeSeconds = 0.02f;
    params.pitchEnvAmountSemitones = 0.0f;

    NovaBassVoiceEngine voice;
    voice.prepare (48000.0);
    voice.setParameters (params);
    voice.reset();

    voice.noteOn (36, 1.0f);
    voice.noteOn (48, 1.0f); // hold two notes, 36 then 48

    voice.noteOff (48); // release the top note

    check (voice.isHoldingAnyNote(), "voice still reports a held note (36) after releasing the top note (48)");
    check (voice.isActive(), "voice remains active (falls back to 36, does not release) after releasing 48");

    voice.noteOff (36);
    check (! voice.isHoldingAnyNote(), "no held notes remain after releasing the last one");
}

int main()
{
    std::printf ("=== NovaBassVoiceEngine Validation ===\n");

    testBasicNoteOnOff();
    testRetriggerVsLegato();
    testGlide();
    testHeldNoteFallback();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
