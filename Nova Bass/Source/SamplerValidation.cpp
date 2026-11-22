// Headless validation harness for NovaBassSamplerEngine / NovaBassSamplePlayback.
// Drives them directly against synthetic sine-tone zones (no real sample
// library needed) to prove zone lookup, round-robin rotation, velocity-
// layer crossfade, and pitch-shifted playback work correctly - and, per
// the project plan's explicit decision to upgrade from Nova Keys' linear
// interpolation to Catmull-Rom for Nova Bass's sub-bass content, includes
// a direct regression test proving Catmull-Rom's interpolation error is
// measurably lower than linear's on a pitched-down sub-bass sine.

#include "../../_shared/NovaBassCore/Sampler/NovaBassSamplerEngine.h"
#include "../../_shared/NovaBassCore/Sampler/NovaBassSamplePlayback.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>
#include <memory>

using namespace nova::bass;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

static std::shared_ptr<juce::AudioBuffer<float>> makeSineBuffer (float freqHz, double sampleRate, int numSamples)
{
    auto buf = std::make_shared<juce::AudioBuffer<float>> (1, numSamples);
    auto* d = buf->getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
        d[i] = std::sin (juce::MathConstants<float>::twoPi * freqHz * (float) i / (float) sampleRate);
    return buf;
}

static void testZoneSelectionBasic()
{
    std::printf ("-- Zone selection: basic matching --\n");
    NovaBassSamplerEngine engine;
    engine.prepare (48000.0);

    NovaBassSampleZone zoneA;
    zoneA.name = "Low"; zoneA.lowKey = 0; zoneA.highKey = 47; zoneA.rootNote = 24;
    NovaBassSampleZone zoneB;
    zoneB.name = "High"; zoneB.lowKey = 48; zoneB.highKey = 127; zoneB.rootNote = 72;

    engine.addZone (zoneA);
    engine.addZone (zoneB);

    auto selLow = engine.selectZones (30, 100);
    check (selLow.primary != nullptr && selLow.primary->name == "Low", "note 30 selects the Low zone");

    auto selHigh = engine.selectZones (60, 100);
    check (selHigh.primary != nullptr && selHigh.primary->name == "High", "note 60 selects the High zone");
}

static void testRoundRobinRotation()
{
    std::printf ("-- Round-robin rotation --\n");
    NovaBassSamplerEngine engine;
    engine.prepare (48000.0);

    NovaBassSampleZone z1; z1.name = "RR1"; z1.roundRobinGroup = 1;
    NovaBassSampleZone z2; z2.name = "RR2"; z2.roundRobinGroup = 1;
    NovaBassSampleZone z3; z3.name = "RR3"; z3.roundRobinGroup = 1;
    engine.addZone (z1); engine.addZone (z2); engine.addZone (z3);

    juce::StringArray order;
    for (int i = 0; i < 6; ++i)
    {
        auto sel = engine.selectZones (36, 100);
        order.add (sel.primary != nullptr ? sel.primary->name : "null");
    }

    check (order[0] != order[1] || order[1] != order[2],
           "consecutive triggers rotate through different round-robin siblings");
    check (order[0] == order[3] && order[1] == order[4] && order[2] == order[5],
           "rotation cycles back to the same order after a full lap (3 zones, 6 triggers)");
}

static void testVelocityCrossfade()
{
    std::printf ("-- Velocity-layer crossfade --\n");
    NovaBassSamplerEngine engine;
    engine.prepare (48000.0);

    NovaBassSampleZone soft; soft.name = "Soft"; soft.roundRobinGroup = 1; soft.lowVelocity = 1; soft.highVelocity = 80;
    NovaBassSampleZone hard; hard.name = "Hard"; hard.roundRobinGroup = 2; hard.lowVelocity = 60; hard.highVelocity = 127;
    engine.addZone (soft); engine.addZone (hard);

    auto selMid = engine.selectZones (36, 70); // inside the overlap 60-80
    check (selMid.primary != nullptr && selMid.secondary != nullptr,
           "a velocity inside the overlap region selects both layers");
    check (selMid.secondaryBlend > 0.0f && selMid.secondaryBlend < 1.0f,
           "blend factor is a genuine crossfade (strictly between 0 and 1) inside the overlap");

    auto selLow = engine.selectZones (36, 10); // only Soft matches
    check (selLow.primary != nullptr && selLow.secondary == nullptr,
           "a velocity outside the overlap selects only the matching single layer");
}

static void testPlaybackRendersAudio()
{
    std::printf ("-- Playback: renders finite, non-silent audio --\n");

    NovaBassSampleZone zone;
    zone.name = "TestTone";
    zone.rootNote = 60;
    zone.sourceSampleRate = 48000.0;
    zone.buffer = makeSineBuffer (220.0f, 48000.0, 48000); // 1 second at A3

    NovaBassSamplerEngine::ZoneSelection sel;
    sel.primary = &zone;

    NovaBassSamplePlayback playback;
    playback.start (sel, 60, 48000.0);
    check (playback.isActive(), "playback becomes active on start()");

    std::vector<float> l (1024, 0.0f), r (1024, 0.0f);
    const bool stillActive = playback.renderAdding (l.data(), r.data(), 1024);
    check (stillActive, "playback still active after rendering less than the full buffer");

    bool allFinite = true, anyNonZero = false;
    for (float s : l) { if (! std::isfinite (s)) allFinite = false; if (std::abs (s) > 0.001f) anyNonZero = true; }
    check (allFinite, "rendered output is finite");
    check (anyNonZero, "rendered output is non-silent for a non-silent input zone");
}

// Regression test: Catmull-Rom interpolation error vs. plain linear
// interpolation, on a pitched-DOWN sub-bass sine (the exact scenario the
// project plan flagged as a fidelity risk with Nova Keys' linear approach).
// Drives NovaBassSamplePlayback's real Catmull-Rom path through the public
// API, and separately computes what plain linear interpolation would have
// produced at the same read positions, both compared against the
// analytically exact sine value at each fractional read position.
static void testCatmullRomVsLinearSubBassTHD()
{
    std::printf ("-- Sub-bass pitch-down: Catmull-Rom vs linear interpolation error --\n");

    const double sourceSR = 48000.0;
    const float sourceFreqHz = 110.0f; // recorded content
    const int sourceLen = 48000;

    NovaBassSampleZone zone;
    zone.rootNote = 48; // trigger at 36 (one octave down) -> pitchRatio 0.5
    zone.sourceSampleRate = sourceSR;
    zone.buffer = makeSineBuffer (sourceFreqHz, sourceSR, sourceLen);

    NovaBassSamplerEngine::ZoneSelection sel;
    sel.primary = &zone;

    NovaBassSamplePlayback playback;
    playback.start (sel, 36, sourceSR); // ratio 0.5: plays the 110Hz content back an octave down

    const int numOut = 20000;
    std::vector<float> l (numOut, 0.0f), r (numOut, 0.0f);
    playback.renderAdding (l.data(), r.data(), numOut);

    // Track the same read position sequence the engine used (increment =
    // pitchRatio * sourceSR/outputSR = 0.5 here) to compute the ideal
    // (exact analytic sine) value and the linear-interpolation reference
    // at each step, independent of the engine's internals.
    const double increment = 0.5;
    double pos = 0.0;
    const auto* src = zone.buffer->getReadPointer (0);

    double catmullSumSquaredError = 0.0;
    double linearSumSquaredError = 0.0;

    for (int i = 0; i < numOut; ++i)
    {
        const double idealValue = std::sin (juce::MathConstants<double>::twoPi * sourceFreqHz * pos / sourceSR);

        // Linear reference computed independently here (NOT the engine -
        // the engine now always uses Catmull-Rom; this exists purely as
        // the "what would linear have produced" comparison baseline).
        const int i0 = juce::jlimit (0, sourceLen - 1, (int) pos);
        const int i1 = juce::jmin (i0 + 1, sourceLen - 1);
        const float frac = (float) (pos - (double) i0);
        const double linearValue = src[i0] + frac * (src[i1] - src[i0]);

        const double catmullValue = (double) l[(size_t) i];

        const double catmullErr = catmullValue - idealValue;
        const double linearErr = linearValue - idealValue;
        catmullSumSquaredError += catmullErr * catmullErr;
        linearSumSquaredError += linearErr * linearErr;

        pos += increment;
    }

    const double catmullRmsError = std::sqrt (catmullSumSquaredError / numOut);
    const double linearRmsError = std::sqrt (linearSumSquaredError / numOut);

    std::printf ("     Catmull-Rom RMS error: %.6f | Linear RMS error: %.6f\n", catmullRmsError, linearRmsError);

    check (catmullRmsError < linearRmsError,
           "Catmull-Rom interpolation error is lower than linear's on a pitched-down sub-bass sine");
}

int main()
{
    std::printf ("=== NovaBassSamplerEngine / NovaBassSamplePlayback Validation ===\n");

    testZoneSelectionBasic();
    testRoundRobinRotation();
    testVelocityCrossfade();
    testPlaybackRendersAudio();
    testCatmullRomVsLinearSubBassTHD();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
