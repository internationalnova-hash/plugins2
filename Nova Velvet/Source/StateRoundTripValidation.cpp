// Headless validation of PluginProcessor's state save/recall: drives the
// REAL NovaVelvetAudioProcessor class (not a reimplementation) through a
// getStateInformation()/setStateInformation() round trip and confirms all
// 13 parameters survive exactly, plus bus-layout acceptance.

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;

    std::cout << "\n[StateRoundTrip] All 13 params survive a save/restore round trip\n";
    {
        NovaVelvetAudioProcessor writer;
        writer.prepareToPlay (48000.0, 512);

        struct { const char* id; float value; } nonDefaults[] = {
            { ParameterIDs::threshold, 0.71f },
            { ParameterIDs::ratio, 0.34f },
            { ParameterIDs::attack, 0.62f },
            { ParameterIDs::release, 0.18f },
            { ParameterIDs::gain, 0.90f },
            { ParameterIDs::timeSelect, 1.0f },   // Manual (index 2 of 3 -> normalised 1.0)
            { ParameterIDs::scHpf, 0.45f },
            { ParameterIDs::mix, 0.20f },
            { ParameterIDs::output, 0.77f },
            { ParameterIDs::stereoLink, 0.0f },   // false
            { ParameterIDs::analogMode, 0.0f },   // Clean (index 0 of 3)
            { ParameterIDs::tubeDrive, 0.83f },
            { ParameterIDs::bypass, 1.0f },       // true
        };

        for (auto& p : nonDefaults)
            writer.apvts.getParameter (p.id)->setValueNotifyingHost (p.value);

        juce::MemoryBlock state;
        writer.getStateInformation (state);
        check (state.getSize() > 0, "getStateInformation() produced a non-empty state block");

        NovaVelvetAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        auto floatsMatch = [] (float a, float b) { return std::abs (a - b) < 1.0e-3f; };

        for (auto& p : nonDefaults)
        {
            auto got = reader.apvts.getParameter (p.id)->getValue();
            check (floatsMatch (got, p.value),
                   juce::String (p.id) + " survives round trip, got " + juce::String (got, 4)
                       + " expected " + juce::String (p.value, 4));
        }
    }

    std::cout << "\n[StateRoundTrip] Restoring a false boolean state doesn't stick at true\n";
    {
        NovaVelvetAudioProcessor writer;
        writer.prepareToPlay (48000.0, 512);
        writer.apvts.getParameter (ParameterIDs::stereoLink)->setValueNotifyingHost (0.0f);
        writer.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (0.0f);

        juce::MemoryBlock state;
        writer.getStateInformation (state);

        NovaVelvetAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.apvts.getParameter (ParameterIDs::stereoLink)->setValueNotifyingHost (1.0f);
        reader.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (1.0f);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        check (reader.apvts.getParameter (ParameterIDs::stereoLink)->getValue() < 0.5f,
               "stereoLink (false) correctly overwrites a prior true value");
        check (reader.apvts.getParameter (ParameterIDs::bypass)->getValue() < 0.5f,
               "bypass (false) correctly overwrites a prior true value");
    }

    std::cout << "\n[StateRoundTrip] Selected preset index survives a round trip\n";
    {
        NovaVelvetAudioProcessor writer;
        writer.prepareToPlay (48000.0, 512);
        writer.loadPreset (3);

        juce::MemoryBlock state;
        writer.getStateInformation (state);

        NovaVelvetAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        check (reader.getCurrentPresetIndex() == 3,
               "preset index 3 survives round trip, got " + juce::String (reader.getCurrentPresetIndex()));
    }

    std::cout << "\n[StateRoundTrip] Bus layout: mono and stereo accepted symmetrically, mismatched rejected\n";
    {
        NovaVelvetAudioProcessor proc;

        juce::AudioProcessor::BusesLayout stereoInOut;
        stereoInOut.inputBuses.add (juce::AudioChannelSet::stereo());
        stereoInOut.outputBuses.add (juce::AudioChannelSet::stereo());
        check (proc.checkBusesLayoutSupported (stereoInOut), "stereo-in/stereo-out is accepted");

        juce::AudioProcessor::BusesLayout monoInOut;
        monoInOut.inputBuses.add (juce::AudioChannelSet::mono());
        monoInOut.outputBuses.add (juce::AudioChannelSet::mono());
        check (proc.checkBusesLayoutSupported (monoInOut), "mono-in/mono-out is accepted");

        juce::AudioProcessor::BusesLayout mismatched;
        mismatched.inputBuses.add (juce::AudioChannelSet::mono());
        mismatched.outputBuses.add (juce::AudioChannelSet::stereo());
        check (! proc.checkBusesLayoutSupported (mismatched), "mono-in/stereo-out is rejected (must match)");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL STATE ROUND-TRIP VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
