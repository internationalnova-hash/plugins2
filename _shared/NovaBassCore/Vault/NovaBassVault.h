#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace nova::bass
{

// Every asset needs full provenance (spec sections 45-46): external
// material carries source/creator/URL/license/date, Nova-generated
// material carries engine version + generator ID + date instead. One
// entry type covers both, distinguished by `source`.
enum class NovaBassAssetSource { External, NovaGenerated };

struct NovaBassManifestEntry
{
    NovaBassAssetSource source = NovaBassAssetSource::NovaGenerated;

    // Populated when source == External.
    juce::String sourceLibrary;
    juce::String creator;
    juce::String sourceUrl;
    juce::String license;

    // Populated when source == NovaGenerated.
    juce::String engineVersion;
    juce::String generatorId;

    // Common to both.
    juce::String dateAcquired; // ISO date string, e.g. "2026-08-17"
    juce::String originalFile;
    juce::String novaProcessedAsset;
    juce::StringArray novaPresetsUsing;
};

// External, relocatable sample/preset library location + missing-library
// recovery + provenance manifest for Nova Bass, extending the convention
// _shared/NovaKeysCore/Vault/NovaSoundVault.h established (first plugin in
// this catalog to use an external resource directory instead of a flat
// .settings file). Adds Nova Bass's own subfolder structure on top of the
// same core mechanism (config-file-persisted path, non-fatal missing-vault
// handling, portable vault-relative sample refs).
//
// Nothing in here does DSP or sample loading - it only answers "where is
// the vault, is it there, and what's the provenance record for what's in
// it." Missing-vault handling is deliberately non-fatal throughout.
class NovaBassVault
{
public:
    NovaBassVault();

    static juce::File getDefaultVaultLocation();
    static juce::File getDefaultConfigFileLocation();

    // Points this instance at a different config file than the real
    // per-user one - for validation/testing only.
    void setConfigFileLocation (const juce::File& file) noexcept { configFile = file; }

    void initialize();

    bool isVaultFound() const noexcept { return vaultFound; }
    juce::File getVaultPath() const noexcept { return vaultPath; }

    void setVaultPath (const juce::File& newPath);

    // Creates every subfolder Nova Bass's spec calls for if missing:
    // SynthPresets/, Samples/NovaGenerated/, Samples/ApprovedExternal/,
    // Presets/Factory/, Presets/Expansions/, Metadata/, Manifests/,
    // LicenseArchive/. Refreshes isVaultFound() afterward - a first-time
    // setup flow (setVaultPath on a not-yet-existing directory, then
    // ensureVaultStructure to create it) must leave the vault correctly
    // reporting "found", not permanently latched to whatever it was
    // before creation.
    bool ensureVaultStructure();

    // Resolves a preset's portable sample reference into an absolute path
    // under Samples/ (searches NovaGenerated/ then ApprovedExternal/).
    // Returns an empty/non-existent File if the vault isn't found.
    juce::File resolveSampleRef (const juce::String& sampleRef) const;

    juce::File getSynthPresetsFolder() const { return vaultPath.getChildFile ("SynthPresets"); }
    juce::File getNovaGeneratedSamplesFolder() const { return vaultPath.getChildFile ("Samples").getChildFile ("NovaGenerated"); }
    juce::File getApprovedExternalSamplesFolder() const { return vaultPath.getChildFile ("Samples").getChildFile ("ApprovedExternal"); }
    juce::File getFactoryPresetsFolder() const { return vaultPath.getChildFile ("Presets").getChildFile ("Factory"); }
    juce::File getExpansionPresetsFolder() const { return vaultPath.getChildFile ("Presets").getChildFile ("Expansions"); }
    juce::File getMetadataFolder() const { return vaultPath.getChildFile ("Metadata"); }
    juce::File getLicenseArchiveFolder() const { return vaultPath.getChildFile ("LicenseArchive"); }
    juce::File getManifestFile() const { return vaultPath.getChildFile ("Manifests").getChildFile ("manifest.json"); }

    std::vector<NovaBassManifestEntry> loadManifest() const;
    bool saveManifest (const std::vector<NovaBassManifestEntry>& entries) const;
    bool appendManifestEntry (const NovaBassManifestEntry& entry);

private:
    juce::File configFile;
    juce::File vaultPath;
    bool vaultFound = false;
};

} // namespace nova::bass
