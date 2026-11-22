// Headless validation of PluginProcessor's state save/recall: drives the
// REAL NovaTapeAudioProcessor class (not a reimplementation) through a
// getStateInformation()/setStateInformation() round trip.

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

    std::cout << "\n[StateRoundTrip] All params survive a save/restore round trip\n";
    {
        NovaTapeAudioProcessor writer;
        writer.prepareToPlay (48000.0, 512);

        writer.apvts.getParameter (ParameterIDs::input)->setValueNotifyingHost (0.75f);
        writer.apvts.getParameter (ParameterIDs::tapeAmount)->setValueNotifyingHost (0.9f);
        writer.apvts.getParameter (ParameterIDs::tapeType)->setValueNotifyingHost (0.5f); // Vintage = index 1 of 3 -> normalized 0.5
        writer.apvts.getParameter (ParameterIDs::ips)->setValueNotifyingHost (0.0f); // 7.5
        writer.apvts.getParameter (ParameterIDs::bias)->setValueNotifyingHost (0.8f);
        writer.apvts.getParameter (ParameterIDs::wowFlutter)->setValueNotifyingHost (0.6f);
        writer.apvts.getParameter (ParameterIDs::noise)->setValueNotifyingHost (0.4f);
        writer.apvts.getParameter (ParameterIDs::mix)->setValueNotifyingHost (0.3f);
        writer.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (1.0f);

        juce::MemoryBlock state;
        writer.getStateInformation (state);
        check (state.getSize() > 0, "getStateInformation() produced a non-empty state block");

        NovaTapeAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        auto floatsMatch = [] (float a, float b) { return std::abs (a - b) < 1.0e-3f; };

        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::input)->getValue(), 0.75f), "input survives round trip");
        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::tapeAmount)->getValue(), 0.9f), "tapeAmount survives round trip");
        check (reader.apvts.getRawParameterValue (ParameterIDs::tapeType)->load() == 1, "tapeType (Vintage) survives round trip");
        check (reader.apvts.getRawParameterValue (ParameterIDs::ips)->load() == 0, "ips (7.5) survives round trip");
        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::bias)->getValue(), 0.8f), "bias survives round trip");
        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::wowFlutter)->getValue(), 0.6f), "wowFlutter survives round trip");
        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::noise)->getValue(), 0.4f), "noise survives round trip");
        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::mix)->getValue(), 0.3f), "mix survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::bypass)->getValue() >= 0.5f, "bypass (true) survives round trip");
    }

    std::cout << "\n[StateRoundTrip] Preset system applies and A/B snapshot round-trips through state\n";
    {
        NovaTapeAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        check (proc.presetManager.getNumPresets() >= 18, "at least 18 factory presets are registered, got "
               + juce::String (proc.presetManager.getNumPresets()));

        proc.presetManager.applyPreset (0, proc.apvts);
        proc.abSnapshot.captureActive (proc.apvts);
        proc.presetManager.applyPreset (1, proc.apvts);

        juce::MemoryBlock state;
        proc.getStateInformation (state);

        NovaTapeAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());
        check (reader.abSnapshot.hasSnapshot (ABSnapshot::Slot::A), "A/B snapshot A survives state round trip");
    }

    std::cout << "\n[StateRoundTrip] Bus layout: mono-in/mono-out and stereo-in/stereo-out accepted, mismatched rejected\n";
    {
        NovaTapeAudioProcessor proc;

        juce::AudioProcessor::BusesLayout monoInMonoOut;
        monoInMonoOut.inputBuses.add (juce::AudioChannelSet::mono());
        monoInMonoOut.outputBuses.add (juce::AudioChannelSet::mono());
        check (proc.checkBusesLayoutSupported (monoInMonoOut), "mono-in/mono-out is accepted");

        juce::AudioProcessor::BusesLayout stereoInStereoOut;
        stereoInStereoOut.inputBuses.add (juce::AudioChannelSet::stereo());
        stereoInStereoOut.outputBuses.add (juce::AudioChannelSet::stereo());
        check (proc.checkBusesLayoutSupported (stereoInStereoOut), "stereo-in/stereo-out is accepted");

        juce::AudioProcessor::BusesLayout mismatched;
        mismatched.inputBuses.add (juce::AudioChannelSet::mono());
        mismatched.outputBuses.add (juce::AudioChannelSet::stereo());
        check (! proc.checkBusesLayoutSupported (mismatched), "mismatched mono-in/stereo-out is rejected");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL STATE ROUND-TRIP VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
