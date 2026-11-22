/*
  Nova Clean — VST3 host-level load diagnostic.
  Uses JUCE's own VST3PluginFormat (the same code path a real host like FL
  Studio uses) to scan and instantiate the INSTALLED Nova Clean.vst3 bundle,
  surfacing the real error message a DAW's generic dialog hides.
*/

#include <JuceHeader.h>
#include <iostream>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String pathToScan = argc > 1 ? juce::String (argv[1])
                                        : "/Library/Audio/Plug-Ins/VST3/Nova Clean.vst3";

    std::cout << "=== VST3 HOST-LEVEL LOAD DIAGNOSTIC ===\n";
    std::cout << "Target: " << pathToScan << "\n\n";

    juce::VST3PluginFormat format;

    std::cout << "[1] fileMightContainThisPluginType...\n" << std::flush;
    const bool mightContain = format.fileMightContainThisPluginType (pathToScan);
    std::cout << "    -> " << (mightContain ? "true" : "FALSE") << "\n\n" << std::flush;

    std::cout << "[2] findAllTypesForFile (scanning for plugin descriptions)...\n" << std::flush;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, pathToScan);
    std::cout << "    -> found " << found.size() << " plugin description(s)\n";
    for (auto* d : found)
        std::cout << "       - name=\"" << d->name << "\" uid=" << d->uniqueId
                   << " manufacturer=\"" << d->manufacturerName << "\"\n";
    std::cout << "\n" << std::flush;

    if (found.isEmpty())
    {
        std::cout << "!!! No plugin descriptions found - scan itself failed, stopping here.\n";
        return 1;
    }

    std::cout << "[3] createPluginInstance (the actual load a host performs)...\n" << std::flush;
    juce::String errorMessage;
    auto instance = format.createInstanceFromDescription (*found[0], 48000.0, 512, errorMessage);

    if (instance == nullptr)
    {
        std::cout << "!!! FAILED TO CREATE INSTANCE.\n";
        std::cout << "!!! Error message: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "    -> instance created OK: " << instance->getName() << "\n\n" << std::flush;

    std::cout << "[4] prepareToPlay + processBlock...\n" << std::flush;
    instance->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    buffer.clear();
    instance->processBlock (buffer, midi);
    std::cout << "    -> OK\n\n" << std::flush;

    std::cout << "=== HOST-LEVEL LOAD SUCCEEDED ===\n";
    return 0;
}
