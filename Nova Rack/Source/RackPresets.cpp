#include "RackPresets.h"

namespace nr
{

//==============================================================================
juce::var SlotData::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("plugin", plugin);
    if (state.getSize() > 0)
        o->setProperty ("state", state.toBase64Encoding());
    if (params.size() > 0)
    {
        auto* po = new juce::DynamicObject();
        for (const auto& k : params.getAllKeys())
            po->setProperty (k, params[k]);
        o->setProperty ("params", juce::var (po));
    }
    o->setProperty ("bypass", bypass);
    o->setProperty ("mix", mix);
    juce::Array<juce::var> f;
    for (auto i : face) f.add (i);
    o->setProperty ("face", f);
    if (preset.isNotEmpty())
        o->setProperty ("preset", preset);
    return juce::var (o);
}

SlotData SlotData::fromVar (const juce::var& v)
{
    SlotData s;
    s.plugin = v.getProperty ("plugin", "").toString();
    const auto st = v.getProperty ("state", "").toString();
    if (st.isNotEmpty())
        s.state.fromBase64Encoding (st);
    if (auto* po = v.getProperty ("params", {}).getDynamicObject())
        for (const auto& p : po->getProperties())
            s.params.set (p.name.toString(), p.value.toString());
    s.bypass = (bool) v.getProperty ("bypass", false);
    s.mix = juce::jlimit (0.0f, 1.0f, (float) (double) v.getProperty ("mix", 1.0));
    if (auto* f = v.getProperty ("face", {}).getArray())
        for (const auto& i : *f)
            s.face.add ((int) i);
    s.preset = v.getProperty ("preset", "").toString();
    return s;
}

juce::var RackData::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "nova-rack");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
    if (group.isNotEmpty()) o->setProperty ("group", group);
    if (note.isNotEmpty())  o->setProperty ("note", note);
    o->setProperty ("inGain", inGainDb);
    o->setProperty ("outGain", outGainDb);
    juce::Array<juce::var> arr;
    for (const auto& s : slots)
        arr.add (s.toVar());
    o->setProperty ("slots", arr);
    return juce::var (o);
}

RackData RackData::fromVar (const juce::var& v)
{
    RackData r;
    r.name = v.getProperty ("name", "").toString();
    r.group = v.getProperty ("group", "").toString();
    r.note = v.getProperty ("note", "").toString();
    r.inGainDb = juce::jlimit (-24.0f, 24.0f, (float) (double) v.getProperty ("inGain", 0.0));
    r.outGainDb = juce::jlimit (-24.0f, 24.0f, (float) (double) v.getProperty ("outGain", 0.0));
    if (auto* arr = v.getProperty ("slots", {}).getArray())
        for (const auto& s : *arr)
        {
            auto sd = SlotData::fromVar (s);
            if (sd.plugin.isNotEmpty())
                r.slots.add (sd);
        }
    return r;
}

//==============================================================================
static juce::String safeName (const juce::String& s)
{
    return juce::File::createLegalFileName (s.trim()).substring (0, 120);
}

static juce::var readJson (const juce::File& f)
{
    return f.existsAsFile() ? juce::JSON::parse (f.loadFileAsString()) : juce::var();
}

static bool writeJson (const juce::File& f, const juce::var& v)
{
    if (! f.getParentDirectory().createDirectory())
        return false;
    // Write to a temp file and swap it in so a crash never leaves half a preset.
    juce::TemporaryFile tmp (f);
    if (! tmp.getFile().replaceWithText (juce::JSON::toString (v, false)))
        return false;
    return tmp.overwriteTargetFileWithTemporary();
}

PresetStore::PresetStore (juce::File r) : root (std::move (r)) {}

juce::File PresetStore::defaultRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
       #if JUCE_MAC
            .getChildFile ("Application Support")
       #endif
            .getChildFile ("Nova Audio").getChildFile ("Nova Rack");
}

juce::File PresetStore::rackFile (const juce::String& name) const
{
    return root.getChildFile ("Racks").getChildFile (safeName (name) + ".novarack");
}

juce::File PresetStore::moduleFile (const juce::String& plugin, const juce::String& name) const
{
    return root.getChildFile ("Modules").getChildFile (safeName (plugin)).getChildFile (safeName (name) + ".novamodule");
}

bool PresetStore::isFactoryRack (const juce::String& name) const
{
    for (const auto& r : factoryRacks())
        if (r.name.equalsIgnoreCase (name))
            return true;
    return false;
}

juce::Array<PresetRef> PresetStore::listRacks() const
{
    juce::Array<PresetRef> out;
    for (const auto& r : factoryRacks())
        out.add ({ r.name, r.group, r.note, {}, true });

    juce::Array<PresetRef> user;
    for (const auto& f : root.getChildFile ("Racks").findChildFiles (juce::File::findFiles, false, "*.novarack"))
    {
        const auto v = readJson (f);
        auto name = v.getProperty ("name", "").toString();
        if (name.isEmpty()) name = f.getFileNameWithoutExtension();
        if (! isFactoryRack (name))
            user.add ({ name, "MY RACKS", v.getProperty ("note", "").toString(), {}, false });
    }
    std::sort (user.begin(), user.end(), [] (const auto& a, const auto& b) { return a.name.compareNatural (b.name) < 0; });
    out.addArray (user);
    return out;
}

bool PresetStore::loadRack (const juce::String& name, RackData& out) const
{
    for (const auto& r : factoryRacks())
        if (r.name.equalsIgnoreCase (name))
        {
            out = r;
            return true;
        }

    const auto v = readJson (rackFile (name));
    if (! v.isObject())
        return false;
    out = RackData::fromVar (v);
    if (out.name.isEmpty()) out.name = name;
    out.factory = false;
    return true;
}

bool PresetStore::saveRack (const RackData& r)
{
    if (r.name.trim().isEmpty() || isFactoryRack (r.name))
        return false;
    auto copy = r;
    copy.factory = false;
    copy.group.clear();
    return writeJson (rackFile (r.name), copy.toVar());
}

bool PresetStore::deleteRack (const juce::String& name)
{
    return ! isFactoryRack (name) && rackFile (name).deleteFile();
}

juce::Array<PresetRef> PresetStore::listModules (const juce::String& pluginOrAll) const
{
    juce::Array<PresetRef> out;
    for (const auto& dir : root.getChildFile ("Modules").findChildFiles (juce::File::findDirectories, false))
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.novamodule"))
        {
            const auto v = readJson (f);
            auto plugin = v.getProperty ("plugin", "").toString();
            if (plugin.isEmpty()) plugin = dir.getFileName();
            if (pluginOrAll.isNotEmpty() && ! plugin.equalsIgnoreCase (pluginOrAll))
                continue;
            auto name = v.getProperty ("name", "").toString();
            if (name.isEmpty()) name = f.getFileNameWithoutExtension();
            out.add ({ name, plugin, {}, plugin, false });
        }

    std::sort (out.begin(), out.end(), [] (const auto& a, const auto& b)
    {
        const int c = a.plugin.compareNatural (b.plugin);
        return c != 0 ? c < 0 : a.name.compareNatural (b.name) < 0;
    });
    return out;
}

bool PresetStore::loadModule (const juce::String& plugin, const juce::String& name, SlotData& out) const
{
    const auto v = readJson (moduleFile (plugin, name));
    if (! v.isObject())
        return false;
    out = SlotData::fromVar (v);
    out.plugin = plugin;
    out.preset = name;
    return true;
}

bool PresetStore::saveModule (const juce::String& plugin, const juce::String& name, const SlotData& s)
{
    if (plugin.isEmpty() || name.trim().isEmpty())
        return false;
    auto v = s.toVar();
    if (auto* o = v.getDynamicObject())
    {
        o->setProperty ("plugin", plugin);
        o->setProperty ("name", name.trim());
        o->removeProperty ("bypass");
        o->removeProperty ("preset");
    }
    return writeJson (moduleFile (plugin, name), v);
}

bool PresetStore::deleteModule (const juce::String& plugin, const juce::String& name)
{
    return moduleFile (plugin, name).deleteFile();
}

} // namespace nr
