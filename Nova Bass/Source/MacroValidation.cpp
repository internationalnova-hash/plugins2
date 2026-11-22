// Headless validation harness for NovaMacroMap/NovaMacroEngine (spec
// sections 18-20, 52, 59-60). Confirms curve/polarity/min/max resolution
// is correct, a single-target macro writes only its own field, a multi-
// target macro (the shape the master BASS orb uses) fans out to several
// fields from one knob value, and unknown targets/macros are ignored
// gracefully rather than crashing.

#include "../../_shared/NovaBassCore/Macro/NovaMacroEngine.h"
#include <juce_core/juce_core.h>
#include <cstdio>
#include <cmath>

using namespace nova::bass;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

static bool approx (float a, float b, float eps = 0.001f) { return std::abs (a - b) < eps; }

static void testCurveResolution()
{
    std::printf ("-- resolveMacroMapping: curve/polarity/min/max --\n");

    NovaMacroMapping linearMap { "x", 0.0f, 10.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal };
    check (approx (resolveMacroMapping (0.0f, linearMap), 0.0f), "linear curve at t=0 maps to min");
    check (approx (resolveMacroMapping (1.0f, linearMap), 10.0f), "linear curve at t=1 maps to max");
    check (approx (resolveMacroMapping (0.5f, linearMap), 5.0f), "linear curve at t=0.5 maps to the midpoint");

    NovaMacroMapping expMap { "x", 0.0f, 10.0f, NovaMacroCurve::Exponential, NovaMacroPolarity::Normal };
    check (resolveMacroMapping (0.5f, expMap) < 5.0f, "exponential curve at t=0.5 is below the linear midpoint (slow start)");

    NovaMacroMapping logMap { "x", 0.0f, 10.0f, NovaMacroCurve::Logarithmic, NovaMacroPolarity::Normal };
    check (resolveMacroMapping (0.5f, logMap) > 5.0f, "logarithmic curve at t=0.5 is above the linear midpoint (fast start)");

    NovaMacroMapping invertedMap { "x", 0.0f, 10.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Inverted };
    check (approx (resolveMacroMapping (0.0f, invertedMap), 10.0f), "inverted polarity at t=0 maps to max");
    check (approx (resolveMacroMapping (1.0f, invertedMap), 0.0f), "inverted polarity at t=1 maps to min");

    NovaMacroMapping negRangeMap { "x", -5.0f, 5.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal };
    check (approx (resolveMacroMapping (0.5f, negRangeMap), 0.0f), "a negative-to-positive range resolves correctly at the midpoint");
}

static void testSingleTargetMacro()
{
    std::printf ("-- Single-target macro writes only its own field --\n");

    NovaBassMacroTargets baseline;
    baseline.voice.ampDecaySeconds = 0.3f;
    baseline.voice.filterCutoffHz = 8000.0f;

    NovaMacroTable table;
    table["DECAY"] = { { "ampDecaySeconds", 0.05f, 2.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal } };

    std::map<juce::String, float> knobs { { "DECAY", 0.5f } };
    auto resolved = NovaMacroEngine::resolve (baseline, table, knobs);

    check (approx (resolved.voice.ampDecaySeconds, 1.025f), "DECAY macro at 0.5 resolves ampDecaySeconds to the expected interpolated value");
    check (approx (resolved.voice.filterCutoffHz, 8000.0f), "an unrelated field (filterCutoffHz) is untouched by the DECAY macro");
}

static void testMultiTargetFanOut()
{
    std::printf ("-- Multi-target macro (BASS-style fan-out) --\n");

    NovaBassMacroTargets baseline;

    NovaMacroTable table;
    table["BASS"] = {
        { "pitchEnvAmountSemitones", 3.0f, 24.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal },
        { "transientAmount", 0.0f, 1.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal },
        { "colorDrive", 0.1f, 0.9f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal },
    };

    std::map<juce::String, float> knobs { { "BASS", 1.0f } };
    auto resolved = NovaMacroEngine::resolve (baseline, table, knobs);

    check (approx (resolved.voice.pitchEnvAmountSemitones, 24.0f), "BASS at full resolves pitchEnvAmountSemitones to its max");
    check (approx (resolved.voice.transientAmount, 1.0f), "BASS at full resolves transientAmount to its max");
    check (approx (resolved.colorDrive, 0.9f), "BASS at full resolves colorDrive (outside NovaBassVoiceParams) to its max");
}

static void testGracefulUnknowns()
{
    std::printf ("-- Unknown macros/targets are ignored, not fatal --\n");

    NovaBassMacroTargets baseline;
    baseline.voice.ampDecaySeconds = 0.42f;

    NovaMacroTable table;
    table["SHAPE"] = { { "thisParamDoesNotExist", 0.0f, 1.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal } };

    std::map<juce::String, float> knobs { { "SHAPE", 0.7f }, { "UNMAPPED_MACRO", 0.3f } };
    auto resolved = NovaMacroEngine::resolve (baseline, table, knobs);

    check (approx (resolved.voice.ampDecaySeconds, 0.42f),
           "an unknown target string is ignored without corrupting other baseline fields");
}

int main()
{
    std::printf ("=== NovaMacroEngine Validation ===\n");

    testCurveResolution();
    testSingleTargetMacro();
    testMultiTargetFanOut();
    testGracefulUnknowns();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
