// Headless validation harness for NovaBassPresetManager (spec sections
// 23-26). Confirms JSON round-trip fidelity (including the macro table,
// which none of Nova Keys' preset fields needed to cover), directory
// scanning, search/category/favorites filtering, and favorite-state
// persistence to disk. Uses an isolated temp directory - never touches
// any real user data.

#include "../../_shared/NovaBassCore/Preset/NovaBassPresetManager.h"
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

static bool approx (float a, float b, float eps = 0.0001f) { return std::abs (a - b) < eps; }

static NovaBassPreset makeTestPreset (const juce::String& name, const juce::String& category)
{
    NovaBassPreset p;
    p.meta.name = name;
    p.meta.category = category;
    p.meta.tags = { "test", "808" };
    p.meta.description = "A test preset for validation.";
    p.sourceType = NovaBassSourceType::Synth;
    p.voiceParams.pitchEnvAmountSemitones = 15.5f;
    p.voiceParams.retrigger = NovaVoiceRetrigger::Legato;
    p.voiceParams.glideMode = NovaGlideMode::Always;
    p.colorMode = NovaColorMode::Gritty;
    p.colorDrive = 0.65f;
    p.stereoParams.widthAmount = 0.8f;
    p.macros["DECAY"] = { { "ampDecaySeconds", 0.05f, 2.0f, NovaMacroCurve::Exponential, NovaMacroPolarity::Inverted } };
    p.macros["BASS"] = {
        { "pitchEnvAmountSemitones", 3.0f, 24.0f, NovaMacroCurve::Linear, NovaMacroPolarity::Normal },
        { "colorDrive", 0.1f, 0.9f, NovaMacroCurve::SCurve, NovaMacroPolarity::Normal },
    };
    return p;
}

static void testJsonRoundTrip()
{
    std::printf ("-- JSON round-trip fidelity --\n");

    auto original = makeTestPreset ("Test 808", "808");
    const auto asVar = NovaBassPresetManager::presetToVar (original);
    const auto jsonText = juce::JSON::toString (asVar, false);
    const auto reparsed = juce::JSON::parse (jsonText);

    NovaBassPreset roundTripped;
    const bool parsedOk = NovaBassPresetManager::presetFromVar (reparsed, roundTripped);
    check (parsedOk, "presetFromVar succeeds on a round-tripped JSON string");

    check (roundTripped.meta.name == original.meta.name, "meta.name round-trips");
    check (roundTripped.meta.category == original.meta.category, "meta.category round-trips");
    check (roundTripped.meta.tags.size() == original.meta.tags.size(), "meta.tags round-trips");
    check (approx (roundTripped.voiceParams.pitchEnvAmountSemitones, original.voiceParams.pitchEnvAmountSemitones),
           "voiceParams.pitchEnvAmountSemitones round-trips");
    check (roundTripped.voiceParams.retrigger == original.voiceParams.retrigger, "voiceParams.retrigger enum round-trips");
    check (roundTripped.voiceParams.glideMode == original.voiceParams.glideMode, "voiceParams.glideMode enum round-trips");
    check (roundTripped.colorMode == original.colorMode, "colorMode enum round-trips");
    check (approx (roundTripped.colorDrive, original.colorDrive), "colorDrive round-trips");
    check (approx (roundTripped.stereoParams.widthAmount, original.stereoParams.widthAmount), "stereoParams.widthAmount round-trips");

    check (roundTripped.macros.count ("DECAY") == 1, "DECAY macro entry exists after round-trip");
    check (roundTripped.macros.at ("DECAY").size() == 1, "DECAY macro has the expected number of mappings");
    check (roundTripped.macros.at ("DECAY")[0].curve == NovaMacroCurve::Exponential, "DECAY mapping curve round-trips");
    check (roundTripped.macros.at ("DECAY")[0].polarity == NovaMacroPolarity::Inverted, "DECAY mapping polarity round-trips");

    check (roundTripped.macros.count ("BASS") == 1, "BASS macro entry exists after round-trip");
    check (roundTripped.macros.at ("BASS").size() == 2, "BASS macro's multi-target fan-out round-trips (2 mappings)");
}

static void testScanSearchFilterFavorites()
{
    std::printf ("-- Directory scan, search, category filter, favorites --\n");

    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("NovaBassPresetValidation_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
    tempDir.createDirectory();

    auto p1 = makeTestPreset ("Dark 808", "808");
    auto p2 = makeTestPreset ("Clean Sub", "Sub");
    auto p3 = makeTestPreset ("Reese Motion", "Reese");
    p2.meta.favorite = true;

    NovaBassPresetManager::savePresetFile (p1, tempDir.getChildFile ("dark808.novabasspreset"));
    NovaBassPresetManager::savePresetFile (p2, tempDir.getChildFile ("cleansub.novabasspreset"));
    NovaBassPresetManager::savePresetFile (p3, tempDir.getChildFile ("reesemotion.novabasspreset"));

    NovaBassPresetManager manager;
    manager.scanDirectory (tempDir);

    check (manager.getNumPresets() == 3, "scanDirectory finds all 3 saved preset files");

    auto categories = manager.getCategories();
    check (categories[0] == "All", "categories list starts with \"All\"");
    check (categories.size() == 4, "categories list contains All + 3 distinct categories");

    auto subOnly = manager.getFilteredIndices ("", "Sub", false);
    check (subOnly.size() == 1, "category filter narrows to exactly the Sub-category preset");

    auto searchDark = manager.getFilteredIndices ("dark", "", false);
    check (searchDark.size() == 1, "text search matches the preset with \"Dark\" in its name");

    auto favoritesOnly = manager.getFilteredIndices ("", "", true);
    check (favoritesOnly.size() == 1, "favorites-only filter matches exactly the one preset marked favorite on save");

    // -- Favorite persistence: toggling and re-scanning survives to disk --
    int darkIndex = -1;
    for (int i = 0; i < manager.getNumPresets(); ++i)
        if (manager.getPreset (i)->meta.name == "Dark 808") darkIndex = i;

    check (darkIndex >= 0, "found Dark 808's index for the favorite-persistence check");
    const bool setOk = manager.setFavorite (darkIndex, true);
    check (setOk, "setFavorite writes successfully");

    NovaBassPresetManager rescanned;
    rescanned.scanDirectory (tempDir);
    auto favoritesAfterRescan = rescanned.getFilteredIndices ("", "", true);
    check (favoritesAfterRescan.size() == 2,
           "favorite state persisted to disk: a fresh scan sees 2 favorites after setFavorite + rescan");

    tempDir.deleteRecursively();
}

int main()
{
    std::printf ("=== NovaBassPresetManager Validation ===\n");

    testJsonRoundTrip();
    testScanSearchFilterFavorites();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
