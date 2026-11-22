// Headless validation of PluginProcessor's state save/recall: drives the
// REAL NovaDoublerProcessor class (not a reimplementation) through a
// getStateInformation()/setStateInformation() round trip and confirms all
// five parameters survive exactly.

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
    // NovaDoublerProcessor pulls in AudioProcessorValueTreeState and
    // WebBrowserComponent-capable machinery that expects a running JUCE
    // message-manager lifecycle (unlike this session's other console
    // harnesses, which only touch juce_core/juce_audio_basics). Without
    // this RAII guard, APVTS's internal Timer starts with no MessageManager
    // ever initialized, which fires JUCE's own debug assertions on
    // shutdown - harmless to the test result itself, but noisy and worth
    // fixing properly rather than ignoring.
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;

    std::cout << "\n[StateRoundTrip] All 5 params survive a save/restore round trip\n";
    {
        NovaDoublerProcessor writer;
        writer.prepareToPlay (48000.0, 512);

        // Deliberately non-default values on every param, including both
        // booleans in their non-default state.
        writer.apvts.getParameter (ParameterIDs::width)->setValueNotifyingHost (0.83f);
        writer.apvts.getParameter (ParameterIDs::tightness)->setValueNotifyingHost (0.21f);
        writer.apvts.getParameter (ParameterIDs::mix)->setValueNotifyingHost (0.64f);
        writer.apvts.getParameter (ParameterIDs::voicesFourMode)->setValueNotifyingHost (1.0f); // true
        writer.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (1.0f); // true

        juce::MemoryBlock state;
        writer.getStateInformation (state);
        check (state.getSize() > 0, "getStateInformation() produced a non-empty state block");

        NovaDoublerProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        auto floatsMatch = [] (float a, float b) { return std::abs (a - b) < 1.0e-4f; };

        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::width)->getValue(), 0.83f),
               "width survives round trip, got " + juce::String (reader.apvts.getParameter (ParameterIDs::width)->getValue(), 4));
        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::tightness)->getValue(), 0.21f),
               "tightness survives round trip, got " + juce::String (reader.apvts.getParameter (ParameterIDs::tightness)->getValue(), 4));
        check (floatsMatch (reader.apvts.getParameter (ParameterIDs::mix)->getValue(), 0.64f),
               "mix survives round trip, got " + juce::String (reader.apvts.getParameter (ParameterIDs::mix)->getValue(), 4));
        check (reader.apvts.getParameter (ParameterIDs::voicesFourMode)->getValue() >= 0.5f,
               "voicesFourMode (true) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::bypass)->getValue() >= 0.5f,
               "bypass (true) survives round trip");
    }

    std::cout << "\n[StateRoundTrip] Restoring a false/false boolean state doesn't stick at true\n";
    {
        NovaDoublerProcessor writer;
        writer.prepareToPlay (48000.0, 512);
        writer.apvts.getParameter (ParameterIDs::voicesFourMode)->setValueNotifyingHost (0.0f);
        writer.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (0.0f);

        juce::MemoryBlock state;
        writer.getStateInformation (state);

        NovaDoublerProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        // Pre-set to true so a no-op restore would be a false pass.
        reader.apvts.getParameter (ParameterIDs::voicesFourMode)->setValueNotifyingHost (1.0f);
        reader.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (1.0f);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        check (reader.apvts.getParameter (ParameterIDs::voicesFourMode)->getValue() < 0.5f, "voicesFourMode (false) correctly overwrites a prior true value");
        check (reader.apvts.getParameter (ParameterIDs::bypass)->getValue() < 0.5f, "bypass (false) correctly overwrites a prior true value");
    }

    std::cout << "\n[StateRoundTrip] Bus layout: mono-in/stereo-out and stereo-in/stereo-out both accepted, mono-out rejected\n";
    {
        NovaDoublerProcessor proc;

        juce::AudioProcessor::BusesLayout monoInStereoOut;
        monoInStereoOut.inputBuses.add (juce::AudioChannelSet::mono());
        monoInStereoOut.outputBuses.add (juce::AudioChannelSet::stereo());
        check (proc.checkBusesLayoutSupported (monoInStereoOut), "mono-in/stereo-out is accepted");

        juce::AudioProcessor::BusesLayout stereoInStereoOut;
        stereoInStereoOut.inputBuses.add (juce::AudioChannelSet::stereo());
        stereoInStereoOut.outputBuses.add (juce::AudioChannelSet::stereo());
        check (proc.checkBusesLayoutSupported (stereoInStereoOut), "stereo-in/stereo-out is accepted");

        juce::AudioProcessor::BusesLayout monoInMonoOut;
        monoInMonoOut.inputBuses.add (juce::AudioChannelSet::mono());
        monoInMonoOut.outputBuses.add (juce::AudioChannelSet::mono());
        check (! proc.checkBusesLayoutSupported (monoInMonoOut), "mono-out is rejected (output must always be stereo)");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL STATE ROUND-TRIP VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
