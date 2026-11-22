// Headless validation of NovaSampleFolderLoader against the REAL CC0 test
// library downloaded into the actual Sound Vault (task #97) - proves the
// loader works on genuine content, not just synthetic zones.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Sampler/NovaSampleFolderLoader.h"
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
}

int main()
{
    // Matches NovaSoundVault::getDefaultVaultLocation() exactly (this test
    // intentionally doesn't link NovaSoundVault.cpp, so it's duplicated
    // here - keep in sync if that ever changes).
   #if JUCE_MAC
    auto vaultSamples = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Library/Application Support/Nova Audio/Nova Sound Vault/Samples");
   #else
    auto vaultSamples = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Nova Audio").getChildFile ("Nova Sound Vault").getChildFile ("Samples");
   #endif

    std::cout << "\n[FolderLoader] Note-name parsing on real filenames\n";
    {
        check (NovaSampleFolderLoader::parseNoteNameFromFilename ("JHPiano_Sus_Close_C4_vl3_rr1") == 60, "'C4' parses to MIDI 60");
        check (NovaSampleFolderLoader::parseNoteNameFromFilename ("BKCtbss_SusNV_G#1_v1_rr1") == 32, "'G#1' parses to MIDI 32");
        check (NovaSampleFolderLoader::parseNoteNameFromFilename ("LLVln_ArcoVib_C5_f") == 72, "'C5' parses to MIDI 72");
        check (NovaSampleFolderLoader::parseNoteNameFromFilename ("Snare2_HitNS_v4_rr1_Mid") == -1, "unpitched percussion filename correctly has no parseable note");
    }

    if (! vaultSamples.isDirectory())
    {
        std::cout << "\n[FolderLoader] SKIPPED - vault samples not found at " << vaultSamples.getFullPathName()
                   << " (expected on a machine that hasn't run task #97's download)\n";
        return failures == 0 ? 0 : 1;
    }

    std::cout << "\n[FolderLoader] Grand Piano: pitched, mixed single/dual velocity-tier notes\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (44100.0);
        const int n = NovaSampleFolderLoader::loadFolderIntoEngine (vaultSamples.getChildFile ("Pianos/Grand"), engine);
        check (n == 8, "loads all 8 piano files as zones (expected 8, got " + juce::String (n) + ")");
        check (engine.getNumZones() == 8, "engine reports 8 zones");

        // C4 has both vl3+vl4 -> should crossfade; C2 has only vl3 -> single zone, full velocity range.
        auto midVel = engine.selectZones (60, 70); // C4, moderate velocity - likely inside the crossfade overlap
        check (midVel.primary != nullptr, "C4 at moderate velocity resolves to a zone");

        auto c2Sel = engine.selectZones (36, 100); // C2, high velocity - only one tier exists for C2
        check (c2Sel.primary != nullptr && c2Sel.secondary == nullptr, "C2 (single-tier note) resolves without a crossfade partner");

        auto offRange = engine.selectZones (48, 100); // C3 is a loaded root note, but check a note between mapped roots still resolves via key-range spread
        check (offRange.primary != nullptr, "a note between explicit samples still resolves via key-range spreading (C3 covers a range)");
    }

    std::cout << "\n[FolderLoader] Solo Violin / Trumpet / Contrabass: pitched, single velocity tier\n";
    {
        NovaSamplerEngine violinEngine;
        violinEngine.prepare (44100.0);
        const int nViolin = NovaSampleFolderLoader::loadFolderIntoEngine (vaultSamples.getChildFile ("Strings/SoloViolin"), violinEngine);
        check (nViolin == 4, "violin: loads all 4 files");

        NovaSamplerEngine trumpetEngine;
        trumpetEngine.prepare (44100.0);
        const int nTrumpet = NovaSampleFolderLoader::loadFolderIntoEngine (vaultSamples.getChildFile ("Brass/Trumpet"), trumpetEngine);
        check (nTrumpet == 4, "trumpet: loads all 4 files");

        NovaSamplerEngine bassEngine;
        bassEngine.prepare (44100.0);
        const int nBass = NovaSampleFolderLoader::loadFolderIntoEngine (vaultSamples.getChildFile ("Bass/SoloContrabass"), bassEngine);
        check (nBass == 4, "contrabass: loads all 4 files");

        // Every note across the full keyboard should resolve to SOME zone (key-range spreading covers 0-127).
        bool violinCoversFullRange = true;
        for (int note : { 0, 40, 60, 90, 127 })
            if (violinEngine.selectZones (note, 100).primary == nullptr) violinCoversFullRange = false;
        check (violinCoversFullRange, "violin's key-range spreading covers the entire keyboard (0-127), not just the 4 sampled notes");
    }

    std::cout << "\n[FolderLoader] Glockenspiel: pitched, all-same-tier (no crossfade expected)\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (44100.0);
        const int n = NovaSampleFolderLoader::loadFolderIntoEngine (vaultSamples.getChildFile ("Mallets/Glockenspiel"), engine);
        check (n == 5, "glockenspiel: loads all 5 files");

        auto sel = engine.selectZones (84, 100); // C6
        check (sel.primary != nullptr && sel.secondary == nullptr, "single-tier glockenspiel note has no crossfade partner");
    }

    std::cout << "\n[FolderLoader] Snare Drum: unpitched percussion with real velocity layers + round robin\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (44100.0);
        const int n = NovaSampleFolderLoader::loadFolderIntoEngine (vaultSamples.getChildFile ("Percussion/SnareDrum"), engine);
        check (n == 6, "snare: loads all 6 files (3 velocity tiers x 2 round-robins)");

        // Trigger the same note+velocity repeatedly - round robin should
        // rotate between the 2 files in that velocity tier's group.
        juce::StringArray hits;
        for (int i = 0; i < 4; ++i)
        {
            auto sel = engine.selectZones (60, 30); // any note works - unpitched spans full range; low velocity -> should land in the lowest tier
            hits.add (sel.primary != nullptr ? sel.primary->name : "null");
        }
        check (! hits.contains ("null"), "snare triggers resolve to a real zone regardless of MIDI note (unpitched, full key range)");
        check (hits[0] != hits[1] || hits[1] != hits[2], "repeated snare hits at the same velocity rotate between round-robin samples, not always the same one");

        // Different velocities should land in different tiers (different zone names).
        auto soft = engine.selectZones (60, 10);
        auto hard = engine.selectZones (60, 120);
        check (soft.primary != nullptr && hard.primary != nullptr && soft.primary->name != hard.primary->name,
               "soft vs hard snare hits resolve to different velocity-tier samples");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL FOLDER LOADER VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
