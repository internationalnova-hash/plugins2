// Nova Rack tests: latency-compensated dry path, wet/dry mix, preset round-trips and factory racks.
#include <juce_core/juce_core.h>
#include "RackPresets.h"
#include "RackSlot.h"

static int failures = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    (ok ? passes : failures)++;
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << "\n";
}

static void testDryDelay()
{
    std::cout << "DryDelay\n";
    for (int delay : { 0, 1, 37, 512, 4096 })
    {
        nr::DryDelay d;
        d.prepare (2, 256);
        d.setDelay (delay);
        const int total = 9000;
        std::vector<float> inL (total), inR (total), outL (total), outR (total);
        for (int i = 0; i < total; ++i) { inL[(size_t) i] = (float) (i + 1); inR[(size_t) i] = -(float) (i + 1); }

        for (int pos = 0; pos < total;)
        {
            const int n = std::min (total - pos, 100 + (pos % 157)); // uneven blocks
            const float* in[] { inL.data() + pos, inR.data() + pos };
            float* out[] { outL.data() + pos, outR.data() + pos };
            d.process (in, out, 2, n);
            pos += n;
        }

        bool ok = true;
        for (int i = 0; i < total; ++i)
        {
            const float want = i < delay ? 0.0f : (float) (i - delay + 1);
            ok = ok && outL[(size_t) i] == want && outR[(size_t) i] == -want;
        }
        check (ok, "delay " + juce::String (delay) + " samples is exact across uneven blocks");
    }
}

static void testMix()
{
    std::cout << "Wet/dry mix\n";
    juce::AudioBuffer<float> wet (2, 64), dry (2, 64);
    wet.clear(); dry.clear();
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 64; ++i) { wet.setSample (c, i, 1.0f); dry.setSample (c, i, 0.0f); }

    juce::LinearSmoothedValue<float> g;
    g.reset (48000.0, 0.0);
    g.setCurrentAndTargetValue (0.25f);
    nr::mixWetDry (wet, dry, 2, 64, g);
    check (std::abs (wet.getSample (1, 10) - 0.25f) < 1.0e-6f, "25% mix gives 0.25 of the wet signal");

    for (int c = 0; c < 2; ++c) for (int i = 0; i < 64; ++i) { wet.setSample (c, i, 1.0f); dry.setSample (c, i, 0.5f); }
    g.setCurrentAndTargetValue (0.0f);
    nr::mixWetDry (wet, dry, 2, 64, g);
    check (wet.getSample (0, 63) == 0.5f, "0% mix is exactly the dry signal");

    juce::LinearSmoothedValue<float> ramp;
    ramp.reset (64.0, 1.0); // 64-sample ramp
    ramp.setCurrentAndTargetValue (1.0f);
    ramp.setTargetValue (0.0f);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 64; ++i) { wet.setSample (c, i, 1.0f); dry.setSample (c, i, 0.0f); }
    nr::mixWetDry (wet, dry, 2, 64, ramp);
    bool falling = true;
    for (int i = 1; i < 64; ++i) falling = falling && wet.getSample (0, i) <= wet.getSample (0, i - 1);
    check (falling && wet.getSample (0, 63) < 0.05f, "bypass ramps smoothly instead of clicking");
}

static void testPresets()
{
    std::cout << "Presets\n";
    juce::TemporaryFile tmpDir;
    const auto root = tmpDir.getFile();
    root.createDirectory();
    nr::PresetStore store (root);

    nr::RackData r;
    r.name = "My Vocal / Chain";   // slash must not escape the folder
    r.inGainDb = -3.5f;
    r.outGainDb = 1.0f;
    nr::SlotData a;
    a.plugin = "Nova Tape";
    const char blob[] = { 0, 1, 2, 3, 127, -1, 42 };
    a.state.append (blob, sizeof (blob));
    a.bypass = true;
    a.mix = 0.4f;
    a.face = { 3, 1, 7 };
    a.preset = "Warm";
    nr::SlotData b;
    b.plugin = "Nova Silk";
    b.params.set ("Threshold", "-20");
    r.slots.add (a);
    r.slots.add (b);

    check (store.saveRack (r), "rack preset saves");
    nr::RackData back;
    check (store.loadRack (r.name, back), "rack preset loads by name");
    check (back.slots.size() == 2 && back.slots[0].plugin == "Nova Tape" && back.slots[1].plugin == "Nova Silk", "chain order kept");
    check (back.slots[0].state == a.state, "plugin state blob round-trips byte for byte");
    check (back.slots[0].bypass && std::abs (back.slots[0].mix - 0.4f) < 1.0e-6f, "bypass and mix kept");
    check (back.slots[0].face == a.face && back.slots[0].preset == "Warm", "knob layout and preset name kept");
    check (back.slots[1].params["Threshold"] == "-20", "parameter texts kept");
    check (std::abs (back.inGainDb + 3.5f) < 1.0e-6f && std::abs (back.outGainDb - 1.0f) < 1.0e-6f, "rack gains kept");

    bool listed = false;
    for (const auto& p : store.listRacks()) listed = listed || (p.name == r.name && ! p.factory);
    check (listed, "user rack appears in the list");

    check (store.saveModule ("Nova Tape", "Warm", a), "module preset saves");
    nr::SlotData m;
    check (store.loadModule ("Nova Tape", "Warm", m) && m.state == a.state, "module preset loads with its state");
    check (store.listModules ("Nova Tape").size() == 1 && store.listModules().size() == 1, "module preset listed under its plugin");
    check (store.deleteModule ("Nova Tape", "Warm") && store.listModules().isEmpty(), "module preset deletes");
    check (store.deleteRack (r.name), "rack preset deletes");
    root.deleteRecursively();
}

static void testFactoryRacks()
{
    std::cout << "Factory racks\n";
    const auto& racks = nr::factoryRacks();
    check (racks.size() > 0, "factory racks exist (" + juce::String (racks.size()) + ")");

    juce::StringArray names;
    bool unique = true, sized = true, named = true, rangesOk = true;
    for (const auto& r : racks)
    {
        unique = unique && ! names.contains (r.name, true);
        names.add (r.name);
        sized = sized && r.slots.size() > 0 && r.slots.size() <= 8;
        named = named && r.group.isNotEmpty() && r.note.isNotEmpty();
        for (const auto& s : r.slots)
            rangesOk = rangesOk && s.plugin.startsWith ("Nova") && s.mix >= 0.0f && s.mix <= 1.0f;
    }
    check (unique, "factory rack names are unique");
    bool utf8 = false;
    for (const auto& r : racks) utf8 = utf8 || r.group.containsChar ((juce::juce_wchar) 0x2022);
    check (utf8, "group names keep their UTF-8 bullet (no mojibake)");
    check (sized, "every factory rack has 1-8 units");
    check (named, "every factory rack has a group and a note");
    check (rangesOk, "every unit names a Nova plugin and a valid mix");

    nr::PresetStore store (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("nr-none"));
    nr::RackData f;
    check (store.loadRack (racks.getReference (0).name, f) && f.factory, "factory racks load through the store");
    check (! store.saveRack (f), "factory racks can't be overwritten");
}

int main()
{
    testDryDelay();
    testMix();
    testPresets();
    testFactoryRacks();
    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
