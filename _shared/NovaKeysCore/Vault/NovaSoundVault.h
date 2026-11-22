#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// External, relocatable sample library location + missing-library recovery
// + license provenance manifest (spec sections 19, 22-23, 26-27). This is
// the FIRST plugin in this codebase to establish an external-resource-
// directory convention (every existing Nova plugin only ever writes a
// single flat .settings properties file, never a resource folder) - see
// getDefaultVaultLocation().
//
// Nothing in here does any DSP or sample loading - it only answers "where
// is the vault, is it there, and what's the provenance record for what's
// in it." Missing-vault handling is deliberately non-fatal everywhere
// (spec section 36: "Do not crash... Allow Locate Library") - callers
// should check isVaultFound()/resolveSampleRef() and degrade gracefully.
struct NovaLicenseManifestEntry
{
    juce::String sourceLibrary;
    juce::String instrument;
    juce::String creator;
    juce::String sourceUrl;
    juce::String license;
    juce::String dateAcquired;       // ISO date string, e.g. "2026-08-14"
    juce::String originalFile;
    juce::String novaProcessedAsset;
    juce::StringArray novaPresetsUsing;
};

class NovaSoundVault
{
public:
    NovaSoundVault();

    static juce::File getDefaultVaultLocation();
    static juce::File getDefaultConfigFileLocation();

    // Points this instance at a different config file than the real
    // per-user one - for validation/testing only, so automated runs never
    // touch the user's actual Sound Vault settings.
    void setConfigFileLocation (const juce::File& file) noexcept { configFile = file; }

    // Loads the persisted vault path (falling back to the default location
    // if none was ever set) and checks whether it currently exists.
    void initialize();

    bool isVaultFound() const noexcept { return vaultFound; }
    juce::File getVaultPath() const noexcept { return vaultPath; }

    // Sets and persists a new vault location (e.g. the user picked one via
    // a "Locate Library" file chooser in the UI), then re-checks it.
    void setVaultPath (const juce::File& newPath);

    // Creates the vault's top-level folder structure at the current path if
    // missing (Samples/, Presets/, Licenses/, Manifest/) - conceptual
    // separation per spec section 19, not a rigidly-enforced deep tree.
    bool ensureVaultStructure() const;

    // Resolves a preset's portable sample reference (e.g.
    // "Pianos/Grand/NovaGrand") into an absolute path under the vault.
    // Returns an empty/non-existent File if the vault isn't found - callers
    // must check exists() themselves, this never throws or asserts.
    juce::File resolveSampleRef (const juce::String& sampleRef) const;

    juce::File getPresetsFolder() const { return vaultPath.getChildFile ("Presets"); }
    juce::File getLicenseArchiveFolder() const { return vaultPath.getChildFile ("Licenses"); }
    juce::File getManifestFile() const { return vaultPath.getChildFile ("Manifest").getChildFile ("manifest.json"); }

    std::vector<NovaLicenseManifestEntry> loadManifest() const;
    bool saveManifest (const std::vector<NovaLicenseManifestEntry>& entries) const;
    bool appendManifestEntry (const NovaLicenseManifestEntry& entry);

private:
    juce::File configFile;
    juce::File vaultPath;
    bool vaultFound = false;
};
