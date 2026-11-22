#include "NovaSoundVault.h"

NovaSoundVault::NovaSoundVault()
    : configFile (getDefaultConfigFileLocation())
{
}

juce::File NovaSoundVault::getDefaultVaultLocation()
{
    // JUCE's userApplicationDataDirectory maps to "~/Library" directly on
    // macOS (NOT "~/Library/Application Support" - confirmed by reading
    // juce_Files_mac.mm), so "Application Support" must be appended
    // explicitly to match this catalog's established per-plugin settings-
    // file convention (e.g. "~/Library/Application Support/Nova AI
    // Master.settings").
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Sound Vault");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Sound Vault");
   #endif
}

juce::File NovaSoundVault::getDefaultConfigFileLocation()
{
    return getDefaultVaultLocation().getParentDirectory().getChildFile ("NovaKeysVaultLocation.json");
}

void NovaSoundVault::initialize()
{
    vaultPath = getDefaultVaultLocation();

    if (configFile.existsAsFile())
    {
        auto parsed = juce::JSON::parse (configFile);
        const auto storedPath = parsed.getProperty ("vaultPath", juce::var()).toString();
        if (storedPath.isNotEmpty())
            vaultPath = juce::File (storedPath);
    }

    vaultFound = vaultPath.isDirectory();
}

void NovaSoundVault::setVaultPath (const juce::File& newPath)
{
    vaultPath = newPath;
    vaultFound = vaultPath.isDirectory();

    auto* o = new juce::DynamicObject();
    o->setProperty ("vaultPath", vaultPath.getFullPathName());

    configFile.getParentDirectory().createDirectory();
    configFile.replaceWithText (juce::JSON::toString (juce::var (o), false));
}

bool NovaSoundVault::ensureVaultStructure() const
{
    if (vaultPath == juce::File()) return false;

    bool ok = vaultPath.createDirectory().wasOk();
    ok = vaultPath.getChildFile ("Samples").createDirectory().wasOk() && ok;
    ok = vaultPath.getChildFile ("Presets").createDirectory().wasOk() && ok;
    ok = vaultPath.getChildFile ("Licenses").createDirectory().wasOk() && ok;
    ok = vaultPath.getChildFile ("Manifest").createDirectory().wasOk() && ok;
    return ok;
}

juce::File NovaSoundVault::resolveSampleRef (const juce::String& sampleRef) const
{
    if (! vaultFound || sampleRef.isEmpty()) return {};
    return vaultPath.getChildFile ("Samples").getChildFile (sampleRef);
}

std::vector<NovaLicenseManifestEntry> NovaSoundVault::loadManifest() const
{
    std::vector<NovaLicenseManifestEntry> entries;

    auto file = getManifestFile();
    if (! file.existsAsFile()) return entries;

    auto parsed = juce::JSON::parse (file);
    if (auto* arr = parsed.getArray())
    {
        for (auto& v : *arr)
        {
            NovaLicenseManifestEntry e;
            e.sourceLibrary = v.getProperty ("sourceLibrary", "").toString();
            e.instrument = v.getProperty ("instrument", "").toString();
            e.creator = v.getProperty ("creator", "").toString();
            e.sourceUrl = v.getProperty ("sourceUrl", "").toString();
            e.license = v.getProperty ("license", "").toString();
            e.dateAcquired = v.getProperty ("dateAcquired", "").toString();
            e.originalFile = v.getProperty ("originalFile", "").toString();
            e.novaProcessedAsset = v.getProperty ("novaProcessedAsset", "").toString();
            if (auto* presetsArr = v.getProperty ("novaPresetsUsing", juce::var()).getArray())
                for (auto& p : *presetsArr) e.novaPresetsUsing.add (p.toString());
            entries.push_back (e);
        }
    }
    return entries;
}

bool NovaSoundVault::saveManifest (const std::vector<NovaLicenseManifestEntry>& entries) const
{
    auto file = getManifestFile();
    file.getParentDirectory().createDirectory();

    juce::Array<juce::var> arr;
    for (auto& e : entries)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("sourceLibrary", e.sourceLibrary);
        o->setProperty ("instrument", e.instrument);
        o->setProperty ("creator", e.creator);
        o->setProperty ("sourceUrl", e.sourceUrl);
        o->setProperty ("license", e.license);
        o->setProperty ("dateAcquired", e.dateAcquired);
        o->setProperty ("originalFile", e.originalFile);
        o->setProperty ("novaProcessedAsset", e.novaProcessedAsset);

        juce::Array<juce::var> presetsArr;
        for (auto& p : e.novaPresetsUsing) presetsArr.add (p);
        o->setProperty ("novaPresetsUsing", juce::var (presetsArr));

        arr.add (juce::var (o));
    }

    return file.replaceWithText (juce::JSON::toString (juce::var (arr), false));
}

bool NovaSoundVault::appendManifestEntry (const NovaLicenseManifestEntry& entry)
{
    auto entries = loadManifest();
    entries.push_back (entry);
    return saveManifest (entries);
}
