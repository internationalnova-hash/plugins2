#pragma once

#include <juce_core/juce_core.h>

namespace nr
{

/** One rack unit as saved in a session or preset. */
struct SlotData
{
    juce::String plugin;              // product name, e.g. "Nova Tape"
    juce::MemoryBlock state;          // the plugin's own state blob (empty = start from defaults)
    juce::StringPairArray params;     // factory presets: parameter name -> value text, applied on load
    bool bypass = false;
    float mix = 1.0f;
    juce::Array<int> face;            // knob layout on the module (empty = default)
    juce::String preset;              // module preset name shown on the unit

    juce::var toVar() const;
    static SlotData fromVar (const juce::var&);
};

/** A whole chain. */
struct RackData
{
    juce::String name;
    juce::String group;               // factory presets: browser group ("VOCAL • LATIN", …)
    juce::String note;                // one-line description shown in the browser
    bool factory = false;
    float inGainDb = 0.0f, outGainDb = 0.0f;
    juce::Array<SlotData> slots;

    juce::var toVar() const;
    static RackData fromVar (const juce::var&);
};

struct PresetRef
{
    juce::String name, group, note, plugin;
    bool factory = false;
};

/** Rack presets (whole chains) and module presets (one plugin's settings),
    stored as JSON under the user's application-data folder:
      Nova Audio/Nova Rack/Racks/<name>.novarack
      Nova Audio/Nova Rack/Modules/<plugin>/<name>.novamodule
    Shared by every Nova Rack instance. */
class PresetStore
{
public:
    explicit PresetStore (juce::File root = defaultRoot());

    static juce::File defaultRoot();
    juce::File getRoot() const { return root; }

    // Rack presets: factory first, then the user's (alphabetical).
    juce::Array<PresetRef> listRacks() const;
    bool loadRack (const juce::String& name, RackData& out) const;
    bool saveRack (const RackData&);
    bool deleteRack (const juce::String& name);
    bool isFactoryRack (const juce::String& name) const;

    // Module presets for every plugin in the library.
    juce::Array<PresetRef> listModules (const juce::String& pluginOrAll = {}) const;
    bool loadModule (const juce::String& plugin, const juce::String& name, SlotData& out) const;
    bool saveModule (const juce::String& plugin, const juce::String& name, const SlotData&);
    bool deleteModule (const juce::String& plugin, const juce::String& name);

private:
    juce::File root;
    juce::File rackFile (const juce::String& name) const;
    juce::File moduleFile (const juce::String& plugin, const juce::String& name) const;
};

/** Built-in chains, including the vocal presets (see FactoryRacks in RackPresets.cpp). */
const juce::Array<RackData>& factoryRacks();

} // namespace nr
