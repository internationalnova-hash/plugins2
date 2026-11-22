#include "PluginLibrary.h"
#include "RackPresets.h"

namespace nr
{

static constexpr const char* kSelfName = "Nova Rack";

PluginLibrary::PluginLibrary()
{
    vst3 = new juce::VST3PluginFormat();
    formats.addFormat (vst3);

    const auto dir = PresetStore::defaultRoot();
    dir.createDirectory();
    cacheFile = dir.getChildFile ("PluginCache.xml");
    deadMansPedal = dir.getChildFile ("ScanInProgress.txt");

    if (auto xml = juce::XmlDocument::parse (cacheFile))
        known.recreateFromXml (*xml);

    rescan (false);
}

PluginLibrary::~PluginLibrary()
{
    stopTimer();
    scanner.reset();
}

bool PluginLibrary::looksLikeNovaFile (const juce::String& path)
{
    const auto name = juce::File (path).getFileNameWithoutExtension();
    if (name.equalsIgnoreCase (kSelfName))
        return false;
    return name.startsWithIgnoreCase ("Nova") || name.containsIgnoreCase ("By Nova")
        || name.equalsIgnoreCase ("Juice Gang");
}

bool PluginLibrary::isOffered (const juce::PluginDescription& d) const
{
    const auto maker = d.manufacturerName;
    const bool nova = maker.equalsIgnoreCase ("International Nova") || maker.equalsIgnoreCase ("Nova Audio");
    return nova && ! d.isInstrument && ! d.name.equalsIgnoreCase (kSelfName)
        && d.pluginFormatName == vst3->getName();
}

void PluginLibrary::rescan (bool forceAll)
{
    stopTimer();
    scanner.reset();

    juce::StringArray files;
    for (const auto& f : vst3->searchPathsForPlugins (vst3->getDefaultLocationsToSearch(), true, false))
        if (looksLikeNovaFile (f))
            files.add (f);

    // Forget plugins that were uninstalled.
    for (const auto& t : known.getTypes())
        if (! files.contains (t.fileOrIdentifier))
            known.removeType (t);

    scanner = std::make_unique<juce::PluginDirectoryScanner> (known, *vst3, juce::FileSearchPath(), false,
                                                               deadMansPedal, false);
    scanner->setFilesOrIdentifiersToScan (files);
    forceRescan = forceAll;
    progress = 0.0f;
    ++generation;
    startTimer (1);
}

void PluginLibrary::timerCallback()
{
    if (scanner == nullptr)
    {
        stopTimer();
        return;
    }

    // One plugin per tick keeps the UI alive during a first scan.
    juce::String name;
    const bool more = scanner->scanNextFile (! forceRescan, name);
    currentName = name;
    progress = scanner->getProgress();

    if (! more)
    {
        stopTimer();
        scanner.reset();
        progress = 1.0f;
        currentName.clear();
        saveCache();
        ++generation;
    }
}

void PluginLibrary::saveCache()
{
    if (auto xml = known.createXml())
        xml->writeTo (cacheFile);
}

juce::Array<juce::PluginDescription> PluginLibrary::getPlugins() const
{
    juce::Array<juce::PluginDescription> out;
    for (const auto& t : known.getTypes())
        if (isOffered (t))
        {
            bool dup = false;
            for (const auto& o : out)
                dup = dup || o.name.equalsIgnoreCase (t.name);
            if (! dup)
                out.add (t);
        }
    std::sort (out.begin(), out.end(), [] (const auto& a, const auto& b) { return a.name.compareNatural (b.name) < 0; });
    return out;
}

std::optional<juce::PluginDescription> PluginLibrary::find (const juce::String& pluginName) const
{
    for (const auto& t : known.getTypes())
        if (isOffered (t) && t.name.equalsIgnoreCase (pluginName))
            return t;
    return std::nullopt;
}

std::unique_ptr<juce::AudioPluginInstance> PluginLibrary::create (const juce::PluginDescription& d, double sr,
                                                                  int block, juce::String& error)
{
    return formats.createPluginInstance (d, sr > 0 ? sr : 44100.0, block > 0 ? block : 512, error);
}

juce::String PluginLibrary::categoryFor (const juce::PluginDescription& d)
{
    const auto c = d.category.toLowerCase();
    const auto n = d.name.toLowerCase();
    auto any = [&] (std::initializer_list<const char*> words)
    {
        for (auto w : words) if (n.contains (w) || c.contains (w)) return true;
        return false;
    };

    if (any ({ "tune", "pitch", "harmony", "chord", "bend", "align", "autotune" })) return "Pitch";
    if (any ({ "space", "delay", "reverb", "aura", "motion", "doubler", "glow" }))  return "Space";
    if (any ({ "eq", "curve", "tone", "desk" }))                                     return "EQ";
    if (any ({ "tape", "heat", "velvet", "silk", "console", "7even", "670", "nova mid" })) return "Color";
    if (any ({ "meter", "analyzer" }))                                      return "Utility";
    if (any ({ "master", "apex", "clip", "translate", "2-track" }))                             return "Master";
    if (any ({ "punch", "glue", "ride", "dynamics", "comp", "clean", "level" }))             return "Dynamics";
    if (any ({ "vox", "voice", "vocal" }))                                           return "Vocal";
    return "Effect";
}

} // namespace nr
