// Integration validation: NovaBassRenderer's output + NovaBassVault's
// existing appendManifestEntry/resolveSampleRef (both already proven in
// Milestone A - no engine changes needed here) actually work together
// correctly. Uses an isolated temp vault throughout - never touches the
// real Nova Bass Vault.

#include "Render/NovaBassRenderer.h"
#include "Vault/NovaBassVault.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

using namespace nova::bass;
using namespace nova::basslab;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

int main()
{
    std::printf ("=== Vault Save Integration Validation ===\n");

    auto tempVaultDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("NovaBassLabVaultSaveIntegration_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
    auto tempConfigFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("NovaBassLabVaultSaveIntegration_config_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".json");

    NovaBassVault vault;
    vault.setConfigFileLocation (tempConfigFile);
    vault.setVaultPath (tempVaultDir);
    check (vault.ensureVaultStructure(), "vault structure created in isolated temp location");

    NovaBassPreset preset;
    preset.meta.name = "Integration Dark 808";
    preset.meta.category = "Hard 808";
    preset.sourceType = NovaBassSourceType::Synth;
    preset.voiceParams.ampAttackSeconds = 0.0005f;
    preset.voiceParams.ampDecaySeconds = 0.3f;
    preset.voiceParams.pitchEnvAmountSemitones = 18.0f;
    preset.colorMode = NovaColorMode::Gritty;
    preset.colorDrive = 0.6f;

    NovaBassEngine headlessEngine;
    NovaBassRenderOptions options;
    options.rootNote = 24; // C1
    options.durationSeconds = 0.2f;
    options.tailSeconds = 0.3f;

    auto renderResult = NovaBassRenderer::renderToBuffer (headlessEngine, preset, options);
    check (renderResult.success, "render succeeds");

    const auto fileName = NovaBassRenderer::makeAssetFileName (preset.meta.category, preset.meta.name, options.rootNote, 1);
    const auto outputFile = vault.getNovaGeneratedSamplesFolder().getChildFile (fileName);

    check (NovaBassRenderer::writeToWavFile (renderResult, outputFile, options.bitDepth), "render written to the vault's NovaGenerated samples folder");
    check (outputFile.existsAsFile(), "the rendered file exists on disk inside the vault");

    NovaBassManifestEntry entry;
    entry.source = NovaBassAssetSource::NovaGenerated;
    entry.engineVersion = "0.1.0";
    entry.generatorId = "hard_808";
    entry.dateAcquired = "2026-08-17";
    entry.novaProcessedAsset = fileName;
    entry.novaPresetsUsing.add (preset.meta.name);

    check (vault.appendManifestEntry (entry), "manifest entry appended successfully");

    // Fresh instance, same paths - proves this all persists to disk correctly,
    // not just in the live objects we already have handles to.
    NovaBassVault freshVault;
    freshVault.setConfigFileLocation (tempConfigFile);
    freshVault.initialize();

    check (freshVault.isVaultFound(), "a fresh NovaBassVault instance finds the same vault via the persisted config");

    auto manifest = freshVault.loadManifest();
    bool foundEntry = false;
    for (auto& e : manifest)
        if (e.novaProcessedAsset == fileName && e.source == NovaBassAssetSource::NovaGenerated && e.generatorId == "hard_808")
            foundEntry = true;
    check (foundEntry, "the manifest entry (with correct provenance) is found by a fresh vault instance");

    auto resolved = freshVault.resolveSampleRef (fileName);
    check (resolved.existsAsFile(), "resolveSampleRef finds the rendered asset via its vault-relative name");

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (resolved));
    check (reader != nullptr, "the resolved file is a real, readable WAV");

    tempVaultDir.deleteRecursively();
    tempConfigFile.deleteFile();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
