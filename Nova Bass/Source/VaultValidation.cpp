// Headless validation harness for NovaBassVault. Uses an isolated temp
// config file and temp vault directory throughout - NEVER touches the
// real user's Sound Vault settings or the real
// ~/Library/Application Support/Nova Audio/Nova Bass Vault location.
// Confirms missing-vault handling never crashes, relocating persists
// across a fresh instance, folder-structure creation covers every
// subfolder Nova Bass's spec calls for, sample-ref resolution, and
// manifest round-trip (both External and NovaGenerated provenance).

#include "../../_shared/NovaBassCore/Vault/NovaBassVault.h"
#include <juce_core/juce_core.h>
#include <cstdio>

using namespace nova::bass;

static int failures = 0;

static void check (bool condition, const char* description)
{
    if (condition) std::printf ("  PASS: %s\n", description);
    else { std::printf ("  FAIL: %s\n", description); ++failures; }
}

static void testMissingVaultIsNonFatal()
{
    std::printf ("-- Missing vault handling --\n");

    auto tempConfig = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("NovaBassVaultValidation_missing_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".json");
    tempConfig.deleteFile();

    NovaBassVault vault;
    vault.setConfigFileLocation (tempConfig);
    vault.initialize(); // no config file exists yet -> falls back to default location, which won't exist in CI either

    check (! vault.isVaultFound() || vault.getVaultPath().isDirectory(),
           "initialize() with no config file does not crash (vaultFound accurately reflects reality)");

    auto resolved = vault.resolveSampleRef ("SomePreset/kick.wav");
    check (! vault.isVaultFound() ? resolved == juce::File() : true,
           "resolveSampleRef returns an empty File (not a crash) when the vault isn't found");

    tempConfig.deleteFile();
}

static void testRelocatePersistsAcrossInstances()
{
    std::printf ("-- Relocate persists across a fresh instance --\n");

    auto tempConfig = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("NovaBassVaultValidation_config_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".json");
    auto tempVaultDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("NovaBassVaultValidation_vault_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
    tempVaultDir.createDirectory();

    {
        NovaBassVault vault;
        vault.setConfigFileLocation (tempConfig);
        vault.initialize();
        vault.setVaultPath (tempVaultDir);
        check (vault.isVaultFound(), "setVaultPath finds the newly-set directory immediately");
    }

    {
        NovaBassVault freshInstance;
        freshInstance.setConfigFileLocation (tempConfig);
        freshInstance.initialize();
        check (freshInstance.getVaultPath() == tempVaultDir, "a fresh NovaBassVault instance recovers the relocated path from the config file");
        check (freshInstance.isVaultFound(), "the relocated path is correctly found by the fresh instance");
    }

    tempConfig.deleteFile();
    tempVaultDir.deleteRecursively();
}

static void testEnsureVaultStructure()
{
    std::printf ("-- ensureVaultStructure creates every required subfolder --\n");

    auto tempVaultDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("NovaBassVaultValidation_structure_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));

    NovaBassVault vault;
    vault.setConfigFileLocation (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                      .getChildFile ("NovaBassVaultValidation_structure_config_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".json"));
    vault.setVaultPath (tempVaultDir);

    const bool ok = vault.ensureVaultStructure();
    check (ok, "ensureVaultStructure reports success");

    check (vault.getSynthPresetsFolder().isDirectory(), "SynthPresets/ was created");
    check (vault.getNovaGeneratedSamplesFolder().isDirectory(), "Samples/NovaGenerated/ was created");
    check (vault.getApprovedExternalSamplesFolder().isDirectory(), "Samples/ApprovedExternal/ was created");
    check (vault.getFactoryPresetsFolder().isDirectory(), "Presets/Factory/ was created");
    check (vault.getExpansionPresetsFolder().isDirectory(), "Presets/Expansions/ was created");
    check (vault.getMetadataFolder().isDirectory(), "Metadata/ was created");
    check (vault.getLicenseArchiveFolder().isDirectory(), "LicenseArchive/ was created");
    check (vault.getManifestFile().getParentDirectory().isDirectory(), "Manifests/ was created");

    tempVaultDir.deleteRecursively();
}

static void testSampleRefResolution()
{
    std::printf ("-- resolveSampleRef checks NovaGenerated then ApprovedExternal --\n");

    auto tempVaultDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("NovaBassVaultValidation_resolve_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));

    NovaBassVault vault;
    vault.setConfigFileLocation (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                      .getChildFile ("NovaBassVaultValidation_resolve_config_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".json"));
    vault.setVaultPath (tempVaultDir);
    vault.ensureVaultStructure();

    // Nothing exists yet: resolution should default to the NovaGenerated location.
    auto beforeAnyFile = vault.resolveSampleRef ("808/dark.wav");
    check (beforeAnyFile.getFullPathName().contains ("NovaGenerated"),
           "with no matching file anywhere, resolution defaults to the NovaGenerated location");

    // Place the file only in ApprovedExternal: resolution should find it there.
    auto externalFile = vault.getApprovedExternalSamplesFolder().getChildFile ("808/dark.wav");
    externalFile.getParentDirectory().createDirectory();
    externalFile.create();
    auto afterExternalOnly = vault.resolveSampleRef ("808/dark.wav");
    check (afterExternalOnly.getFullPathName().contains ("ApprovedExternal"),
           "when only an ApprovedExternal copy exists, resolution finds it there");

    // Now also place it in NovaGenerated: NovaGenerated takes priority.
    auto generatedFile = vault.getNovaGeneratedSamplesFolder().getChildFile ("808/dark.wav");
    generatedFile.getParentDirectory().createDirectory();
    generatedFile.create();
    auto afterBoth = vault.resolveSampleRef ("808/dark.wav");
    check (afterBoth.getFullPathName().contains ("NovaGenerated") && ! afterBoth.getFullPathName().contains ("ApprovedExternal"),
           "when both exist, NovaGenerated takes priority over ApprovedExternal");

    tempVaultDir.deleteRecursively();
}

static void testManifestRoundTrip()
{
    std::printf ("-- Manifest round-trip (External + NovaGenerated provenance) --\n");

    auto tempVaultDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("NovaBassVaultValidation_manifest_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));

    NovaBassVault vault;
    vault.setConfigFileLocation (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                      .getChildFile ("NovaBassVaultValidation_manifest_config_" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)) + ".json"));
    vault.setVaultPath (tempVaultDir);
    vault.ensureVaultStructure();

    NovaBassManifestEntry externalEntry;
    externalEntry.source = NovaBassAssetSource::External;
    externalEntry.sourceLibrary = "VCSL";
    externalEntry.creator = "University of Iowa";
    externalEntry.license = "CC0";
    externalEntry.dateAcquired = "2026-08-17";
    externalEntry.novaProcessedAsset = "NVB_Reese_External_C1_v001.wav";

    NovaBassManifestEntry generatedEntry;
    generatedEntry.source = NovaBassAssetSource::NovaGenerated;
    generatedEntry.engineVersion = "0.1.0";
    generatedEntry.generatorId = "Hard808Generator";
    generatedEntry.dateAcquired = "2026-08-17";
    generatedEntry.novaProcessedAsset = "NVB_808_Dark_C1_v001.wav";
    generatedEntry.novaPresetsUsing.add ("Dark 808");

    check (vault.appendManifestEntry (externalEntry), "appendManifestEntry succeeds for an External entry");
    check (vault.appendManifestEntry (generatedEntry), "appendManifestEntry succeeds for a NovaGenerated entry");

    auto loaded = vault.loadManifest();
    check (loaded.size() == 2, "loadManifest returns both appended entries");

    bool foundExternal = false, foundGenerated = false;
    for (auto& e : loaded)
    {
        if (e.source == NovaBassAssetSource::External && e.sourceLibrary == "VCSL" && e.license == "CC0")
            foundExternal = true;
        if (e.source == NovaBassAssetSource::NovaGenerated && e.generatorId == "Hard808Generator" && e.novaPresetsUsing.contains ("Dark 808"))
            foundGenerated = true;
    }
    check (foundExternal, "the External entry's provenance fields round-trip correctly");
    check (foundGenerated, "the NovaGenerated entry's provenance fields round-trip correctly");

    tempVaultDir.deleteRecursively();
}

int main()
{
    std::printf ("=== NovaBassVault Validation ===\n");

    testMissingVaultIsNonFatal();
    testRelocatePersistsAcrossInstances();
    testEnsureVaultStructure();
    testSampleRefResolution();
    testManifestRoundTrip();

    std::printf ("=== %s (%d failure%s) ===\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}
