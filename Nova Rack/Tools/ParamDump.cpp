// Dev tool: prints every installed Nova plugin's parameters (name, default, text at
// several positions) so factory rack presets can be written against real names.
//   NovaRackParamDump [plugin-name-filter]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "RackPresets.h"
#include "RackSlot.h"

// --verify-factory: load every factory rack unit, apply its values and report anything that
// didn't land (unknown parameter name, or a value the plugin parsed differently).
static int verifyFactory (juce::AudioPluginFormatManager& fm, juce::VST3PluginFormat& vst3)
{
    std::map<juce::String, juce::PluginDescription> byName;
    for (const auto& f : vst3.searchPathsForPlugins (vst3.getDefaultLocationsToSearch(), true, false))
    {
        juce::OwnedArray<juce::PluginDescription> types;
        if (! juce::File (f).getFileName().containsIgnoreCase ("Nova")) continue;
        vst3.findAllTypesForFile (types, f);
        for (auto* d : types) byName.emplace (d->name, *d);
    }

    int problems = 0, checked = 0;
    for (const auto& r : nr::factoryRacks())
        for (const auto& s : r.slots)
        {
            auto it = byName.find (s.plugin);
            if (it == byName.end()) { std::cout << r.name << ": " << s.plugin << " NOT INSTALLED\n"; ++problems; continue; }
            juce::String err;
            auto inst = fm.createPluginInstance (it->second, 48000.0, 512, err);
            if (! inst) { std::cout << r.name << ": " << s.plugin << " failed: " << err << "\n"; ++problems; continue; }

            for (const auto& k : nr::applyParamTexts (*inst, s.params))
            {
                std::cout << r.name << " / " << s.plugin << ": no parameter named \"" << k << "\"\n";
                ++problems;
            }
            for (const auto& k : s.params.getAllKeys())
                for (auto* p : inst->getParameters())
                    if (p->getName (128).trim().equalsIgnoreCase (k))
                    {
                        ++checked;
                        const auto want = s.params[k].trim();
                        const auto got = p->getText (p->getValue(), 32).trim();
                        if (want.startsWithChar ('='))
                        {
                            const bool ok = std::abs (p->getValue() - want.substring (1).getFloatValue()) < 0.01f;
                            if (! ok) { std::cout << r.name << " / " << s.plugin << " / " << k << ": wanted " << want << " got " << p->getValue() << "\n"; ++problems; }
                            break;
                        }
                        auto gotNum = got.retainCharacters ("-+.0123456789").getFloatValue();
                        if (got.containsIgnoreCase ("kHz")) gotNum *= 1000.0f;
                        const bool numeric = want.containsOnly ("-+.0123456789");
                        bool ok = numeric ? std::abs (gotNum - want.getFloatValue()) <= std::max (0.051f, std::abs (want.getFloatValue()) * 0.02f)
                                          : got.equalsIgnoreCase (want) || got.startsWithIgnoreCase (want);
                        if (! ok)
                        {
                            std::cout << r.name << " / " << s.plugin << " / " << k << ": wanted \"" << want << "\" got \"" << got << "\"\n";
                            ++problems;
                        }
                        break;
                    }
        }
    std::cout << checked << " values checked, " << problems << " problems\n";
    return problems == 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioPluginFormatManager fm;
    auto* vst3 = new juce::VST3PluginFormat();
    fm.addFormat (vst3);
    const juce::String filter = argc > 1 ? juce::String (argv[1]) : juce::String();
    if (filter == "--verify-factory")
        return verifyFactory (fm, *vst3);
    if (filter == "--rack-json" && argc > 2) // print a factory rack as saved session JSON
    {
        for (const auto& r : nr::factoryRacks())
            if (r.name.equalsIgnoreCase (argv[2]))
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("version", 1);
                o->setProperty ("rack", r.toVar());
                const auto json = juce::JSON::toString (juce::var (o), true);
                if (argc > 3) // --rack-json NAME state: encoded like a host/Standalone saves it
                {
                    juce::MemoryBlock mb;
                    { juce::MemoryOutputStream out (mb, false); out.writeString (json); }
                    std::cout << mb.toBase64Encoding() << std::endl;
                }
                else
                    std::cout << json << std::endl;
                return 0;
            }
        return 1;
    }

    for (const auto& f : vst3->searchPathsForPlugins (vst3->getDefaultLocationsToSearch(), true, false))
    {
        const auto fname = juce::File (f).getFileNameWithoutExtension();
        if (! (fname.startsWithIgnoreCase ("Nova") || fname.containsIgnoreCase ("By Nova"))) continue;
        if (filter.isNotEmpty() && ! fname.containsIgnoreCase (filter)) continue;

        juce::OwnedArray<juce::PluginDescription> types;
        vst3->findAllTypesForFile (types, f);
        for (auto* d : types)
        {
            juce::String err;
            auto inst = fm.createPluginInstance (*d, 48000.0, 512, err);
            std::cout << "=== " << d->name << " | maker=" << d->manufacturerName << " | instr=" << d->isInstrument
                      << " | latency=" << (inst ? inst->getLatencySamples() : -1) << " " << err << "\n";
            if (! inst) continue;
            const auto& ps = inst->getParameters();
            for (int i = 0; i < ps.size(); ++i)
            {
                auto* p = ps[i];
                juce::String id;
                if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (p)) id = h->getParameterID();
                std::cout << "  [" << i << "] " << p->getName (64) << " | def=" << p->getText (p->getDefaultValue(), 16)
                          << " | 0=" << p->getText (0.0f, 16) << " | .5=" << p->getText (0.5f, 16)
                          << " | 1=" << p->getText (1.0f, 16) << " | steps=" << (p->isDiscrete() ? p->getNumSteps() : 0)
                          << " | lbl=" << p->getLabel() << "\n";
            }
        }
    }
    return 0;
}
