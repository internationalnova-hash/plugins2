#include "NovaBassVault.h"

namespace nova::bass
{

namespace
{
    juce::String sourceToString (NovaBassAssetSource s) { return s == NovaBassAssetSource::External ? "external" : "nova_generated"; }
    NovaBassAssetSource sourceFromString (const juce::String& s) { return s == "external" ? NovaBassAssetSource::External : NovaBassAssetSource::NovaGenerated; }
}

NovaBassVault::NovaBassVault()
    : configFile (getDefaultConfigFileLocation())
{
}

juce::File NovaBassVault::getDefaultVaultLocation()
{
    // Same userApplicationDataDirectory + explicit "Application Support"
    // handling as NovaSoundVault (JUCE's special-location maps to
    // "~/Library" directly on macOS, not "~/Library/Application Support").
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Bass Vault");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Bass Vault");
   #endif
}

juce::File NovaBassVault::getDefaultConfigFileLocation()
{
    return getDefaultVaultLocation().getParentDirectory().getChildFile ("NovaBassVaultLocation.json");
}

void NovaBassVault::initialize()
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

void NovaBassVault::setVaultPath (const juce::File& newPath)
{
    vaultPath = newPath;
    vaultFound = vaultPath.isDirectory();

    auto* o = new juce::DynamicObject();
    o->setProperty ("vaultPath", vaultPath.getFullPathName());

    configFile.getParentDirectory().createDirectory();
    configFile.replaceWithText (juce::JSON::toString (juce::var (o), false));
}

bool NovaBassVault::ensureVaultStructure()
{
    if (vaultPath == juce::File()) return false;

    bool ok = vaultPath.createDirectory().wasOk();
    ok = getSynthPresetsFolder().createDirectory().wasOk() && ok;
    ok = getNovaGeneratedSamplesFolder().createDirectory().wasOk() && ok;
    ok = getApprovedExternalSamplesFolder().createDirectory().wasOk() && ok;
    ok = getFactoryPresetsFolder().createDirectory().wasOk() && ok;
    ok = getExpansionPresetsFolder().createDirectory().wasOk() && ok;
    ok = getMetadataFolder().createDirectory().wasOk() && ok;
    ok = getLicenseArchiveFolder().createDirectory().wasOk() && ok;
    ok = getManifestFile().getParentDirectory().createDirectory().wasOk() && ok;

    vaultFound = vaultPath.isDirectory();
    return ok;
}

juce::File NovaBassVault::resolveSampleRef (const juce::String& sampleRef) const
{
    if (! vaultFound || sampleRef.isEmpty()) return {};

    auto generated = getNovaGeneratedSamplesFolder().getChildFile (sampleRef);
    if (generated.existsAsFile()) return generated;

    auto external = getApprovedExternalSamplesFolder().getChildFile (sampleRef);
    if (external.existsAsFile()) return external;

    // Neither exists yet (e.g. resolving ahead of a render) - default to
    // the NovaGenerated location, matching this catalog's convention of
    // originally-Nova-synthesized content being the primary source.
    return generated;
}

std::vector<NovaBassManifestEntry> NovaBassVault::loadManifest() const
{
    std::vector<NovaBassManifestEntry> entries;

    auto file = getManifestFile();
    if (! file.existsAsFile()) return entries;

    auto parsed = juce::JSON::parse (file);
    if (auto* arr = parsed.getArray())
    {
        for (auto& v : *arr)
        {
            NovaBassManifestEntry e;
            e.source = sourceFromString (v.getProperty ("source", "nova_generated").toString());
            e.sourceLibrary = v.getProperty ("sourceLibrary", "").toString();
            e.creator = v.getProperty ("creator", "").toString();
            e.sourceUrl = v.getProperty ("sourceUrl", "").toString();
            e.license = v.getProperty ("license", "").toString();
            e.engineVersion = v.getProperty ("engineVersion", "").toString();
            e.generatorId = v.getProperty ("generatorId", "").toString();
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

bool NovaBassVault::saveManifest (const std::vector<NovaBassManifestEntry>& entries) const
{
    auto file = getManifestFile();
    file.getParentDirectory().createDirectory();

    juce::Array<juce::var> arr;
    for (auto& e : entries)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("source", sourceToString (e.source));
        o->setProperty ("sourceLibrary", e.sourceLibrary);
        o->setProperty ("creator", e.creator);
        o->setProperty ("sourceUrl", e.sourceUrl);
        o->setProperty ("license", e.license);
        o->setProperty ("engineVersion", e.engineVersion);
        o->setProperty ("generatorId", e.generatorId);
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

bool NovaBassVault::appendManifestEntry (const NovaBassManifestEntry& entry)
{
    auto entries = loadManifest();
    entries.push_back (entry);
    return saveManifest (entries);
}

} // namespace nova::bass
