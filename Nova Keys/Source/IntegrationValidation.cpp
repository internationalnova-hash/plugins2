// Headless end-to-end validation of the REAL v0.1 signal path used by
// PluginProcessor::loadPresetByIndex()/processBlock() (task #98): the real
// Sound Vault -> real NovaPresetManager -> real NovaSampleFolderLoader ->
// NovaKeysEngine -> NovaEffectsChain, exercised against all 12 real
// presets from the actual downloaded CC0 test library (task #97).
//
// Deliberately does NOT link PluginProcessor.cpp/PluginEditor.cpp directly:
// doing so would pull in the WebView/juce_gui_extra machinery just to
// satisfy NovaKeysProcessor::createEditor()'s vtable entry, for no real
// benefit (createEditor() is never invoked here). Instead this test
// reconstructs PluginProcessor's exact composition (one NovaSamplerEngine
// per layer slot + NovaKeysEngine + NovaEffectsChain, configured the same
// way loadPresetByIndex() does) directly - proving the same real data path
// without the extra linkage. The actual PluginProcessor/PluginEditor
// wiring itself was verified by building + launching the real Standalone
// app and confirming correct real preset/layer/FX data on screen.
//
// Also deliberately never calls NovaSoundVault::setVaultPath() or
// NovaPresetManager::setFavorite() - both persist to the user's REAL,
// shared Sound Vault config/preset files (not an isolated test fixture),
// so mutating them here would corrupt state for every other Nova Keys
// instance (standalone, installed plugin copies) on this machine.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Vault/NovaSoundVault.h"
#include "Preset/NovaPresetManager.h"
#include "Sampler/NovaSampleFolderLoader.h"
#include "NovaKeysEngine.h"
#include "Effects/NovaEffectsChain.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    constexpr double kSampleRate = 44100.0;
    constexpr int kBlockSize = 512;

    // Mirrors PluginProcessor::loadPresetByIndex() exactly, minus the
    // SpinLock (no concurrent audio thread exists in this single-threaded
    // console app) and minus hardResetAllVoices() (each preset gets a
    // freshly-constructed NovaKeysEngine here, so there's no stale voice
    // state to clear).
    void configureEngineForPreset (const NovaPreset& preset, NovaSoundVault& vault,
                                    std::array<NovaSamplerEngine, NovaKeysEngine::kNumLayerSlots>& samplerEngines,
                                    NovaKeysEngine& engine, NovaEffectsChain& fx)
    {
        for (int i = 0; i < NovaKeysEngine::kNumLayerSlots; ++i)
        {
            const auto& layer = preset.layers[(size_t) i];
            samplerEngines[(size_t) i].prepare (kSampleRate);

            if (! layer.used)
            {
                engine.setLayerUnconfigured (i);
                continue;
            }

            if (layer.sourceType == NovaLayerSourceType::Sample)
            {
                auto folder = vault.resolveSampleRef (layer.sampleRef);
                const int n = NovaSampleFolderLoader::loadFolderIntoEngine (folder, samplerEngines[(size_t) i]);
                check (n > 0, "  layer " + juce::String (i) + " (" + layer.displayName + "): loaded " + juce::String (n) + " zone(s) from " + folder.getFullPathName());
                engine.configureLayerAsSample (i, &samplerEngines[(size_t) i], layer.layerParams);
            }
            else
            {
                engine.configureLayerAsSynth (i, layer.synthParams, layer.layerParams);
            }
        }

        fx.setParams (preset.effects);
        fx.setMacros (preset.macros);
    }

    // Renders numBlocks blocks through engine -> fx, returns peak abs sample
    // and whether every sample was finite.
    struct RenderResult { float peak = 0.f; bool allFinite = true; };
    RenderResult renderBlocks (NovaKeysEngine& engine, NovaEffectsChain& fx, int numBlocks)
    {
        RenderResult result;
        std::vector<float> l ((size_t) kBlockSize), r ((size_t) kBlockSize);

        for (int b = 0; b < numBlocks; ++b)
        {
            engine.renderNextBlock (l.data(), r.data(), kBlockSize);

            juce::AudioBuffer<float> buf;
            float* chans[2] = { l.data(), r.data() };
            buf.setDataToReferTo (chans, 2, kBlockSize);
            fx.process (buf);

            for (int s = 0; s < kBlockSize; ++s)
            {
                for (float v : { l[(size_t) s], r[(size_t) s] })
                {
                    if (! std::isfinite (v)) result.allFinite = false;
                    result.peak = juce::jmax (result.peak, std::abs (v));
                }
            }
        }
        return result;
    }
}

int main()
{
    std::cout << "\n[Integration] Real Sound Vault + real preset library\n";

    NovaSoundVault vault;
    vault.initialize();
    check (vault.isVaultFound(), "real Sound Vault found at " + vault.getVaultPath().getFullPathName());
    if (! vault.isVaultFound())
    {
        std::cout << "\nSKIPPED - remaining tests require the real vault from task #97's download\n";
        return failures == 0 ? 0 : 1;
    }

    NovaPresetManager presetManager;
    presetManager.scanDirectory (vault.getPresetsFolder());
    // The library grows across phases (v0.1 test library -> expanded factory
    // library), so this only checks it isn't empty/broken, not an exact count.
    check (presetManager.getNumPresets() > 0, "real preset library scanned successfully (" + juce::String (presetManager.getNumPresets()) + " presets found)");

    std::cout << "\n[Integration] Each real preset: load -> noteOn -> render -> noteOff -> release\n";
    for (int i = 0; i < presetManager.getNumPresets(); ++i)
    {
        auto* preset = presetManager.getPreset (i);
        if (preset == nullptr) { check (false, "preset " + juce::String (i) + " failed to load"); continue; }

        std::cout << " -- " << preset->meta.name << " --\n";

        std::array<NovaSamplerEngine, NovaKeysEngine::kNumLayerSlots> samplerEngines;
        NovaKeysEngine engine;
        NovaEffectsChain fx;
        engine.prepare (kSampleRate, kBlockSize, 32);
        fx.prepare (kSampleRate, kBlockSize);

        configureEngineForPreset (*preset, vault, samplerEngines, engine, fx);

        const bool anyLayerUsed = std::any_of (preset->layers.begin(), preset->layers.end(),
                                                [] (const NovaPresetLayer& l) { return l.used; });

        // Trigger a note in the middle of layer A's range if layer A is
        // used, else fall back to a generally-central note (60).
        const auto& l0 = preset->layers[0];
        const int note = l0.used ? (l0.layerParams.lowKey + l0.layerParams.highKey) / 2 : 60;

        engine.noteOn (note, 100);
        auto sounding = renderBlocks (engine, fx, 20); // ~232ms - enough for attack/decay on any v0.1 preset

        check (sounding.allFinite, "audio stays finite (no NaN/Inf) while sounding");
        if (anyLayerUsed)
            check (sounding.peak > 1.0e-4f, "produces audible (non-silent) output while a note is held (peak=" + juce::String (sounding.peak, 5) + ")");

        engine.noteOff (note);

        // Size the release window to this preset's own longest layer
        // release (some pads run out to 3000ms - a fixed short window here
        // would flag a real 3s release as a false "stuck voice").
        float maxReleaseMs = 300.f;
        for (auto& l : preset->layers)
            if (l.used) maxReleaseMs = juce::jmax (maxReleaseMs, l.layerParams.releaseMs);
        const int releaseBlocks = 1 + (int) ((maxReleaseMs + 500.f) / 1000.f * kSampleRate / kBlockSize); // +500ms margin

        auto released = renderBlocks (engine, fx, releaseBlocks);
        check (released.allFinite, "audio stays finite (no NaN/Inf) through release");
        check (engine.getNumActiveVoices() == 0, "voice fully releases and goes silent after note-off (no stuck notes, within " + juce::String (maxReleaseMs, 0) + "ms release + margin)");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL INTEGRATION VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
