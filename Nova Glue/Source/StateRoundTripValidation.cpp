// Headless validation of PluginProcessor's state save/recall: drives the
// REAL NovaGlueAudioProcessor class through a getStateInformation()/
// setStateInformation() round trip and confirms every parameter survives
// exactly, including the choice-type ones. Also exercises the mono-bus
// adaptation path (duplicate-then-downmix) that only the real processor
// implements - the Core engine itself is stereo-only.

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

    bool floatsMatch (float a, float b) { return std::abs (a - b) < 1.0e-4f; }
}

int main()
{
    // See Nova Doubler's StateRoundTripValidation.cpp for why this guard
    // is required (APVTS + WebBrowserComponent machinery expects a live
    // MessageManager).
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;

    std::cout << "\n[StateRoundTrip] All parameters survive a save/restore round trip\n";
    {
        NovaGlueAudioProcessor writer;
        writer.prepareToPlay (48000.0, 512);

        // Deliberately non-default values on every param. Choice/bool
        // params are set to exact boundary/index values (not arbitrary
        // continuous floats) - a raw continuous value fed into a discrete
        // parameter's setValueNotifyingHost() doesn't reflect how any real
        // host or our own UI ever drives it.
        writer.apvts.getParameter (ParameterIDs::threshold)->setValueNotifyingHost (0.65f);
        writer.apvts.getParameter (ParameterIDs::attack)->setValueNotifyingHost (1.0f);   // 30ms (last of 6)
        writer.apvts.getParameter (ParameterIDs::release)->setValueNotifyingHost (0.5f);  // 0.6s (middle of 5: 0/.25/.5/.75/1)
        writer.apvts.getParameter (ParameterIDs::makeup)->setValueNotifyingHost (0.8f);
        writer.apvts.getParameter (ParameterIDs::ratio)->setValueNotifyingHost (1.0f);    // 10:1 (last of 3)
        writer.apvts.getParameter (ParameterIDs::scHpf)->setValueNotifyingHost (0.7f);
        writer.apvts.getParameter (ParameterIDs::mix)->setValueNotifyingHost (0.6f);
        writer.apvts.getParameter (ParameterIDs::glue)->setValueNotifyingHost (0.85f);
        writer.apvts.getParameter (ParameterIDs::drive)->setValueNotifyingHost (0.3f);
        writer.apvts.getParameter (ParameterIDs::autoGain)->setValueNotifyingHost (0.0f); // false
        writer.apvts.getParameter (ParameterIDs::stereoLink)->setValueNotifyingHost (0.0f); // false
        writer.apvts.getParameter (ParameterIDs::oversampling)->setValueNotifyingHost (1.0f); // 4x (last of 3)
        writer.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (1.0f);   // true

        juce::MemoryBlock state;
        writer.getStateInformation (state);
        check (state.getSize() > 0, "getStateInformation() produced a non-empty state block");

        NovaGlueAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        for (auto id : { ParameterIDs::threshold, ParameterIDs::makeup, ParameterIDs::scHpf,
                          ParameterIDs::mix, ParameterIDs::glue, ParameterIDs::drive })
        {
            const float writerVal = writer.apvts.getParameter (id)->getValue();
            const float readerVal = reader.apvts.getParameter (id)->getValue();
            check (floatsMatch (writerVal, readerVal), juce::String (id) + " survives round trip, got " + juce::String (readerVal, 4));
        }

        check (reader.apvts.getParameter (ParameterIDs::attack)->getValue() >= 0.99f, "attack (30ms, last index) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::release)->getValue() > 0.45f && reader.apvts.getParameter (ParameterIDs::release)->getValue() < 0.55f, "release (0.6s, middle index) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::ratio)->getValue() >= 0.99f, "ratio (10:1, last index) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::oversampling)->getValue() >= 0.99f, "oversampling (4x, last index) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::autoGain)->getValue() < 0.5f, "autoGain (false) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::stereoLink)->getValue() < 0.5f, "stereoLink (false) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::bypass)->getValue() >= 0.5f, "bypass (true) survives round trip");
    }

    std::cout << "\n[StateRoundTrip] Restoring a false boolean doesn't stick at a prior true\n";
    {
        NovaGlueAudioProcessor writer;
        writer.prepareToPlay (48000.0, 512);
        writer.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (0.0f);

        juce::MemoryBlock state;
        writer.getStateInformation (state);

        NovaGlueAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (1.0f);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        check (reader.apvts.getParameter (ParameterIDs::bypass)->getValue() < 0.5f, "bypass (false) correctly overwrites a prior true value");
    }

    std::cout << "\n[StateRoundTrip] Bus layout: mono-in/mono-out and stereo-in/stereo-out accepted, mixed rejected\n";
    {
        NovaGlueAudioProcessor proc;

        juce::AudioProcessor::BusesLayout monoInMonoOut;
        monoInMonoOut.inputBuses.add (juce::AudioChannelSet::mono());
        monoInMonoOut.outputBuses.add (juce::AudioChannelSet::mono());
        check (proc.checkBusesLayoutSupported (monoInMonoOut), "mono-in/mono-out is accepted");

        juce::AudioProcessor::BusesLayout stereoInStereoOut;
        stereoInStereoOut.inputBuses.add (juce::AudioChannelSet::stereo());
        stereoInStereoOut.outputBuses.add (juce::AudioChannelSet::stereo());
        check (proc.checkBusesLayoutSupported (stereoInStereoOut), "stereo-in/stereo-out is accepted");

        juce::AudioProcessor::BusesLayout monoInStereoOut;
        monoInStereoOut.inputBuses.add (juce::AudioChannelSet::mono());
        monoInStereoOut.outputBuses.add (juce::AudioChannelSet::stereo());
        check (! proc.checkBusesLayoutSupported (monoInStereoOut), "mixed mono-in/stereo-out is rejected");
    }

    std::cout << "\n[StateRoundTrip] Mono bus: real processBlock produces finite output via the duplicate-then-downmix adaptation\n";
    {
        NovaGlueAudioProcessor proc;
        juce::AudioProcessor::BusesLayout monoLayout;
        monoLayout.inputBuses.add (juce::AudioChannelSet::mono());
        monoLayout.outputBuses.add (juce::AudioChannelSet::mono());
        proc.setBusesLayout (monoLayout);
        proc.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buf (1, 512);
        juce::MidiBuffer midi;
        bool allFinite = true;
        for (int block = 0; block < 20; ++block)
        {
            for (int i = 0; i < 512; ++i)
                buf.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / 48000.0));
            proc.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i)
                allFinite = allFinite && std::isfinite (buf.getSample (0, i));
        }
        check (allFinite, "Mono bus processBlock: all outputs finite");
    }

    std::cout << "\n[StateRoundTrip] Malformed state does not crash\n";
    {
        NovaGlueAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        const char garbage[] = { 0x00, 0x01, 0x02, 0x03, (char) 0xff, (char) 0xfe, 0x00 };
        proc.setStateInformation (garbage, sizeof (garbage));
        proc.setStateInformation (nullptr, 0);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
        check (true, "setStateInformation with garbage/null data did not crash, processor still processes audio");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL STATE ROUND-TRIP VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
