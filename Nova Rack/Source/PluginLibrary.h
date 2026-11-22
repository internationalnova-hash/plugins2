#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace nr
{

/** The installed Nova effect plugins (VST3), shared by every Nova Rack instance in the process.

    Only files whose names look like Nova plugins are opened, and only plugins made by
    International Nova / Nova Audio that aren't instruments are offered. Results are cached
    in Nova Audio/Nova Rack/PluginCache.xml, so after the first scan only new or updated
    plugins are opened. Scanning runs on the message thread, one plugin per timer tick, and
    a plugin that crashes during a scan is skipped next time (dead-man's-pedal file). */
class PluginLibrary : private juce::Timer
{
public:
    PluginLibrary();
    ~PluginLibrary() override;

    juce::Array<juce::PluginDescription> getPlugins() const;
    std::optional<juce::PluginDescription> find (const juce::String& pluginName) const;

    bool isScanning() const noexcept { return scanner != nullptr; }
    float getProgress() const noexcept { return progress; }
    juce::String getCurrentlyScanning() const { return currentName; }
    int getGeneration() const noexcept { return generation; }

    /** Look again for added, removed or updated plugins (forceAll = reopen every plugin). */
    void rescan (bool forceAll = false);

    std::unique_ptr<juce::AudioPluginInstance> create (const juce::PluginDescription&, double sampleRate,
                                                       int blockSize, juce::String& error);

    /** Short category shown on the library cards ("Dynamics", "EQ", …). */
    static juce::String categoryFor (const juce::PluginDescription&);

private:
    void timerCallback() override;
    void saveCache();
    static bool looksLikeNovaFile (const juce::String& path);
    bool isOffered (const juce::PluginDescription&) const;

    juce::AudioPluginFormatManager formats;
    juce::VST3PluginFormat* vst3 = nullptr;
    juce::KnownPluginList known;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    juce::File cacheFile, deadMansPedal;
    float progress = 1.0f;
    bool forceRescan = false;
    juce::String currentName;
    int generation = 0;
};

} // namespace nr
