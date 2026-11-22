// Headless validation harness for NovaBassGenerator. Confirms constrained
// generation actually stays within each profile's declared ranges across
// many seeds (spec section 34: "do not simply randomize every parameter
// from minimum to maximum" - this is the assertion that proves it).

#include "Generate/NovaBassGenerator.h"
#include <juce_core/juce_core.h>
#include <cstdio>
#include <algorithm>

using namespace nova::bass;
using namespace nova::basslab;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

static bool inRange (float v, NovaBassParamRange r) { return v >= r.lo - 0.0001f && v <= r.hi + 0.0001f; }

static void testProfile (const NovaBassGeneratorProfile& profile, int numSamples)
{
    std::printf ("-- Profile: %s --\n", profile.id.toRawUTF8());

    juce::Random rng (12345);
    bool allInRange = true;
    bool allColorValid = true;

    for (int i = 0; i < numSamples; ++i)
    {
        auto preset = NovaBassGenerator::generate (profile.id, rng);
        auto& v = preset.voiceParams;

        if (! inRange (v.sineTriBlend, profile.sineTriBlend)) allInRange = false;
        if (! inRange (v.pitchEnvAmountSemitones, profile.pitchEnvAmount)) allInRange = false;
        if (! inRange (v.pitchEnvDecaySeconds, profile.pitchEnvDecay)) allInRange = false;
        if (! inRange (v.ampAttackSeconds, profile.ampAttack)) allInRange = false;
        if (! inRange (v.ampDecaySeconds, profile.ampDecay)) allInRange = false;
        if (! inRange (v.ampSustain, profile.ampSustain)) allInRange = false;
        if (! inRange (v.transientAmount, profile.transientAmount)) allInRange = false;
        if (! inRange (v.filterCutoffHz, profile.filterCutoff)) allInRange = false;
        if (! inRange (v.filterResonance, profile.filterResonance)) allInRange = false;
        if (! inRange (preset.colorDrive, profile.colorDrive)) allInRange = false;

        const bool colorOk = std::find (profile.allowedColorModes.begin(), profile.allowedColorModes.end(), preset.colorMode) != profile.allowedColorModes.end();
        if (! colorOk) allColorValid = false;

        if (preset.meta.category != profile.category) allInRange = false;
        if (preset.sourceType != NovaBassSourceType::Synth) allInRange = false;
        if (! preset.macros.empty()) allInRange = false;
    }

    check (allInRange, "every generated sample's fields stay within the profile's declared ranges");
    check (allColorValid, "every generated sample's colorMode is one of the profile's allowed modes");
}

static void testVariety()
{
    std::printf ("-- Generation produces real variety, not a constant value --\n");

    juce::Random rng (999);
    float first = -1.0f;
    bool sawDifferentValue = false;

    for (int i = 0; i < 20; ++i)
    {
        auto preset = NovaBassGenerator::generate ("hard_808", rng);
        if (i == 0) first = preset.voiceParams.pitchEnvAmountSemitones;
        else if (std::abs (preset.voiceParams.pitchEnvAmountSemitones - first) > 0.01f) sawDifferentValue = true;
    }

    check (sawDifferentValue, "repeated generation calls produce varying values (not a stuck/constant RNG)");
}

int main()
{
    std::printf ("=== NovaBassGenerator Validation ===\n");

    check (NovaBassGenerator::findProfile ("clean_sub") != nullptr, "clean_sub profile exists");
    check (NovaBassGenerator::findProfile ("hard_808") != nullptr, "hard_808 profile exists");
    check (NovaBassGenerator::findProfile ("reese") != nullptr, "reese profile exists");
    check (NovaBassGenerator::findProfile ("not_a_real_profile") == nullptr, "unknown profile id returns nullptr, not a crash");

    for (auto& profile : NovaBassGenerator::getProfiles())
        testProfile (profile, 300);

    testVariety();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
