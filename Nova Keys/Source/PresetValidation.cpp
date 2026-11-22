// Headless validation of NovaPresetManager (spec sections 15-17): JSON
// round-trip fidelity, directory scanning, search/category/favorites
// filtering, prev/next navigation, and favorite-state persistence to disk.

#include <juce_core/juce_core.h>
#include "Preset/NovaPresetManager.h"
#include <iostream>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    NovaPreset makeTestPreset (const juce::String& name, const juce::String& category, bool favorite = false)
    {
        NovaPreset preset;
        preset.meta.name = name;
        preset.meta.category = category;
        preset.meta.author = "Nova Audio";
        preset.meta.tags = { "PIANO", "PAD" };
        preset.meta.description = "A test preset for " + name;
        preset.meta.favorite = favorite;

        preset.layers[0].used = true;
        preset.layers[0].sourceType = NovaLayerSourceType::Sample;
        preset.layers[0].sampleRef = "Pianos/Grand/NovaGrand";
        preset.layers[0].displayName = "Grand Piano";
        preset.layers[0].layerParams.volumeDb = -3.f;
        preset.layers[0].layerParams.lowKey = 0;
        preset.layers[0].layerParams.highKey = 127;

        preset.layers[1].used = true;
        preset.layers[1].sourceType = NovaLayerSourceType::Synth;
        preset.layers[1].displayName = "Warm Pad";
        preset.layers[1].synthParams.waveform = NovaOscWaveform::Triangle;
        preset.layers[1].synthParams.unisonVoices = 4;
        preset.layers[1].layerParams.attackMs = 400.f;

        preset.effects.eqHighGainDb = 2.5f;
        preset.effects.reverbMix = 0.35f;
        preset.macros.tone = 0.7f;
        preset.macros.space = 0.6f;
        preset.outputVolumeDb = -2.f;
        return preset;
    }
}

int main()
{
    std::cout << "\n[Preset] JSON round-trip preserves all fields\n";
    {
        auto original = makeTestPreset ("Celestial Grand", "Pianos", true);
        auto v = NovaPresetManager::presetToVar (original);
        const auto jsonText = juce::JSON::toString (v, false);
        auto reparsed = juce::JSON::parse (jsonText);

        NovaPreset roundTripped;
        const bool ok = NovaPresetManager::presetFromVar (reparsed, roundTripped);
        check (ok, "presetFromVar succeeds on the re-parsed JSON");

        check (roundTripped.meta.name == original.meta.name, "name round-trips");
        check (roundTripped.meta.category == original.meta.category, "category round-trips");
        check (roundTripped.meta.favorite == original.meta.favorite, "favorite flag round-trips");
        check (roundTripped.meta.tags.size() == 2 && roundTripped.meta.tags[0] == "PIANO", "tags array round-trips");

        check (roundTripped.layers[0].used && roundTripped.layers[0].sourceType == NovaLayerSourceType::Sample, "layer A (sample) round-trips its type");
        check (roundTripped.layers[0].sampleRef == "Pianos/Grand/NovaGrand", "layer A sampleRef round-trips");
        check (std::abs (roundTripped.layers[0].layerParams.volumeDb - (-3.f)) < 0.001f, "layer A volumeDb round-trips");

        check (roundTripped.layers[1].used && roundTripped.layers[1].sourceType == NovaLayerSourceType::Synth, "layer B (synth) round-trips its type");
        check (roundTripped.layers[1].synthParams.waveform == NovaOscWaveform::Triangle, "layer B waveform round-trips");
        check (roundTripped.layers[1].synthParams.unisonVoices == 4, "layer B unisonVoices round-trips");

        check (std::abs (roundTripped.effects.eqHighGainDb - 2.5f) < 0.001f, "effects.eqHighGainDb round-trips");
        check (std::abs (roundTripped.macros.tone - 0.7f) < 0.001f, "macros.tone round-trips");
        check (std::abs (roundTripped.outputVolumeDb - (-2.f)) < 0.001f, "outputVolumeDb round-trips");

        check (! roundTripped.layers[2].used && ! roundTripped.layers[3].used, "unused layers C/D correctly default to unused");
    }

    auto testDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("NovaKeysPresetValidation_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
    testDir.createDirectory();

    std::cout << "\n[Preset] Save to disk / load from disk round-trips correctly\n";
    {
        auto preset = makeTestPreset ("Nova Grand", "Pianos");
        auto file = testDir.getChildFile ("nova_grand.novapreset");
        check (NovaPresetManager::savePresetFile (preset, file), "savePresetFile writes successfully");
        check (file.existsAsFile(), "the .novapreset file exists on disk after saving");

        NovaPreset loaded;
        check (NovaPresetManager::loadPresetFile (file, loaded), "loadPresetFile reads it back successfully");
        check (loaded.meta.name == "Nova Grand", "loaded preset has the correct name");
    }

    std::cout << "\n[Preset] Missing / malformed files are handled safely (no crash)\n";
    {
        NovaPreset dummy;
        check (! NovaPresetManager::loadPresetFile (testDir.getChildFile ("does_not_exist.novapreset"), dummy), "loading a missing file fails gracefully");

        auto badFile = testDir.getChildFile ("malformed.novapreset");
        badFile.replaceWithText ("{ this is not valid json ][");
        check (! NovaPresetManager::loadPresetFile (badFile, dummy), "loading malformed JSON fails gracefully rather than crashing");
    }

    std::cout << "\n[Preset] Directory scan, category list, search/category/favorites filtering\n";
    {
        auto libDir = testDir.getChildFile ("Library");
        libDir.getChildFile ("Pianos").createDirectory();
        libDir.getChildFile ("Pads").createDirectory();

        NovaPresetManager::savePresetFile (makeTestPreset ("Nova Grand", "Pianos", false), libDir.getChildFile ("Pianos/nova_grand.novapreset"));
        NovaPresetManager::savePresetFile (makeTestPreset ("Celestial Grand", "Pianos", true), libDir.getChildFile ("Pianos/celestial_grand.novapreset"));
        NovaPresetManager::savePresetFile (makeTestPreset ("Dream Piano", "Pianos", false), libDir.getChildFile ("Pianos/dream_piano.novapreset"));
        NovaPresetManager::savePresetFile (makeTestPreset ("Warm Cloud", "Pads", true), libDir.getChildFile ("Pads/warm_cloud.novapreset"));

        NovaPresetManager manager;
        manager.scanDirectory (libDir);
        check (manager.getNumPresets() == 4, "scanDirectory (recursive) finds all 4 presets across subfolders");

        auto categories = manager.getCategories();
        check (categories.contains ("All") && categories.contains ("Pianos") && categories.contains ("Pads"), "getCategories reports All + every distinct category found");

        auto pianoIndices = manager.getFilteredIndices ("", "Pianos", false);
        check ((int) pianoIndices.size() == 3, "category filter 'Pianos' returns exactly the 3 piano presets");

        auto favIndices = manager.getFilteredIndices ("", "All", true);
        check ((int) favIndices.size() == 2, "favorites-only filter returns exactly the 2 favorited presets");

        auto searchIndices = manager.getFilteredIndices ("celestial", "All", false);
        check ((int) searchIndices.size() == 1 && manager.getPreset (searchIndices[0])->meta.name == "Celestial Grand", "search 'celestial' finds exactly Celestial Grand");

        check (manager.getSelectedIndex() == 0, "scanning selects the first preset by default");
    }

    std::cout << "\n[Preset] Next/previous navigation wraps correctly\n";
    {
        auto libDir = testDir.getChildFile ("NavLibrary");
        libDir.createDirectory();
        NovaPresetManager::savePresetFile (makeTestPreset ("A", "Test"), libDir.getChildFile ("a.novapreset"));
        NovaPresetManager::savePresetFile (makeTestPreset ("B", "Test"), libDir.getChildFile ("b.novapreset"));
        NovaPresetManager::savePresetFile (makeTestPreset ("C", "Test"), libDir.getChildFile ("c.novapreset"));

        NovaPresetManager manager;
        manager.scanDirectory (libDir);
        check (manager.getNumPresets() == 3, "3 presets loaded for navigation test");

        manager.setSelectedIndex (0);
        manager.selectPrevious();
        check (manager.getSelectedIndex() == 2, "selectPrevious from index 0 wraps to the last preset");

        manager.setSelectedIndex (2);
        manager.selectNext();
        check (manager.getSelectedIndex() == 0, "selectNext from the last index wraps back to 0");
    }

    std::cout << "\n[Preset] Favorite toggling persists to disk across a fresh scan\n";
    {
        auto libDir = testDir.getChildFile ("FavLibrary");
        libDir.createDirectory();
        NovaPresetManager::savePresetFile (makeTestPreset ("Toggle Me", "Test", false), libDir.getChildFile ("toggle.novapreset"));

        NovaPresetManager manager;
        manager.scanDirectory (libDir);
        check (! manager.getPreset (0)->meta.favorite, "preset starts un-favorited");

        check (manager.setFavorite (0, true), "setFavorite writes back to disk successfully");
        check (manager.getPreset (0)->meta.favorite, "in-memory favorite flag updates immediately");

        NovaPresetManager freshManager; // simulates a new session re-scanning the same library
        freshManager.scanDirectory (libDir);
        check (freshManager.getPreset (0)->meta.favorite, "favorite state persisted to disk survives a fresh directory scan");
    }

    testDir.deleteRecursively();

    std::cout << "\n[Preset] Real v0.1 test library presets (task #98) load correctly from the real vault\n";
    {
       #if JUCE_MAC
        auto realPresetsDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile ("Library/Application Support/Nova Audio/Nova Sound Vault/Presets");
       #else
        auto realPresetsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Nova Audio/Nova Sound Vault/Presets");
       #endif

        if (! realPresetsDir.isDirectory())
        {
            std::cout << "  SKIPPED - real presets folder not found (expected on a machine that hasn't run task #97/#98's content setup)\n";
        }
        else
        {
            NovaPresetManager manager;
            manager.scanDirectory (realPresetsDir);
            check (manager.getNumPresets() == 12, "all 12 v0.1 test presets load without error (got " + juce::String (manager.getNumPresets()) + ")");

            bool foundCelestial = false;
            int usedLayerCounts = 0;
            for (int i = 0; i < manager.getNumPresets(); ++i)
            {
                auto* p = manager.getPreset (i);
                if (p == nullptr) continue;
                if (p->meta.name == "Celestial Grand")
                {
                    foundCelestial = true;
                    int used = 0;
                    for (auto& l : p->layers) if (l.used) ++used;
                    check (used == 4, "Celestial Grand has all 4 layers used, matching the approved mockup exactly");
                }
                for (auto& l : p->layers) if (l.used) ++usedLayerCounts;
            }
            check (foundCelestial, "Celestial Grand preset is present");
            check (usedLayerCounts >= 12, "the 12 presets collectively use a realistic number of layers (>=12), not all empty");

            auto categories = manager.getCategories();
            check (categories.size() >= 8, "real presets span at least 8 distinct categories (got " + juce::String (categories.size()) + ")");
        }
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL PRESET VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
