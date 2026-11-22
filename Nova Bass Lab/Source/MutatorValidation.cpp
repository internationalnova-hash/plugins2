// Headless validation harness for NovaBassMutator. Confirms perturbation
// magnitude scales Subtle < Medium < Wild (the actual point of having
// three named intensities, not just three synonyms), and that output
// values stay within sane bounds / macros are cleared / nothing crashes.

#include "Mutate/NovaBassMutator.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>

using namespace nova::bass;
using namespace nova::basslab;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

static NovaBassPreset makeBaseline()
{
    NovaBassPreset p;
    p.meta.name = "Baseline";
    p.meta.category = "808";
    p.voiceParams.pitchEnvAmountSemitones = 12.0f;
    p.voiceParams.ampDecaySeconds = 0.6f;
    p.voiceParams.transientAmount = 0.3f;
    p.colorDrive = 0.4f;
    p.stereoParams.widthAmount = 0.5f;
    p.macros["DECAY"] = { { "ampDecaySeconds", 0.05f, 1.5f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal } };
    return p;
}

static float averageAbsoluteDelta (const NovaBassPreset& baseline, NovaMutateIntensity intensity, int trials)
{
    juce::Random rng (42);
    double sum = 0.0;
    for (int i = 0; i < trials; ++i)
    {
        auto mutated = NovaBassMutator::mutate (baseline, intensity, rng);
        sum += std::abs (mutated.voiceParams.ampDecaySeconds - baseline.voiceParams.ampDecaySeconds);
    }
    return (float) (sum / trials);
}

static void testMagnitudeScaling()
{
    std::printf ("-- Perturbation magnitude scales Subtle < Medium < Wild --\n");

    const auto baseline = makeBaseline();
    const int trials = 500;

    const float subtleAvg = averageAbsoluteDelta (baseline, NovaMutateIntensity::Subtle, trials);
    const float mediumAvg = averageAbsoluteDelta (baseline, NovaMutateIntensity::Medium, trials);
    const float wildAvg = averageAbsoluteDelta (baseline, NovaMutateIntensity::Wild, trials);

    std::printf ("     avg |delta| ampDecaySeconds - Subtle: %.5f | Medium: %.5f | Wild: %.5f\n", subtleAvg, mediumAvg, wildAvg);

    check (subtleAvg < mediumAvg, "Subtle produces smaller average perturbation than Medium");
    check (mediumAvg < wildAvg, "Medium produces smaller average perturbation than Wild");
}

static void testBoundsAndCleanliness()
{
    std::printf ("-- Output bounds, macro clearing, no crashes across many Wild trials --\n");

    const auto baseline = makeBaseline();
    juce::Random rng (7);

    bool allFinite = true;
    bool allInBounds = true;
    bool allMacrosEmpty = true;

    for (int i = 0; i < 1000; ++i)
    {
        auto m = NovaBassMutator::mutate (baseline, NovaMutateIntensity::Wild, rng);
        auto& v = m.voiceParams;

        if (! std::isfinite (v.ampDecaySeconds) || ! std::isfinite (v.pitchEnvAmountSemitones) || ! std::isfinite (m.colorDrive))
            allFinite = false;

        if (v.ampSustain < 0.0f || v.ampSustain > 1.0f) allInBounds = false;
        if (m.colorDrive < 0.0f || m.colorDrive > 1.0f) allInBounds = false;
        if (m.stereoParams.widthAmount < 0.0f || m.stereoParams.widthAmount > 1.0f) allInBounds = false;

        if (! m.macros.empty()) allMacrosEmpty = false;
    }

    check (allFinite, "all mutated values are finite across 1000 Wild trials");
    check (allInBounds, "all mutated 0-1-range fields stay within [0,1] across 1000 Wild trials");
    check (allMacrosEmpty, "mutated presets always have empty macros (see NovaBassMutator.cpp comment)");
}

static void testNamingAndOriginalUnmodified()
{
    std::printf ("-- Naming convention and original-preset immutability --\n");

    const auto baseline = makeBaseline();
    juce::Random rng (3);
    auto mutated = NovaBassMutator::mutate (baseline, NovaMutateIntensity::Medium, rng);

    check (mutated.meta.name == "Baseline (mutated)", "mutated preset's name is derived from the original with a suffix");
    check (baseline.voiceParams.ampDecaySeconds == 0.6f, "the original baseline preset is unmodified after mutate() (passed by const&)");
}

int main()
{
    std::printf ("=== NovaBassMutator Validation ===\n");

    testMagnitudeScaling();
    testBoundsAndCleanliness();
    testNamingAndOriginalUnmodified();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
