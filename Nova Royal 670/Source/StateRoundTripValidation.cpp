// Headless validation of PluginProcessor's state save/recall: drives the
// REAL NovaRoyal670Processor class (not a reimplementation) through a
// getStateInformation()/setStateInformation() round trip and confirms
// every parameter survives exactly, including the choice-type ones
// (Time Constant, Stereo Mode, Oversampling). Also exercises the mono-bus
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
        NovaRoyal670Processor writer;
        writer.prepareToPlay (48000.0, 512);

        // Deliberately non-default values on every param.
        writer.apvts.getParameter (ParameterIDs::input)->setValueNotifyingHost (0.83f);
        writer.apvts.getParameter (ParameterIDs::hrmDrive)->setValueNotifyingHost (0.71f);
        writer.apvts.getParameter (ParameterIDs::acThreshold)->setValueNotifyingHost (0.15f);
        writer.apvts.getParameter (ParameterIDs::dcThreshold)->setValueNotifyingHost (0.9f);
        writer.apvts.getParameter (ParameterIDs::timeConstant)->setValueNotifyingHost (1.0f); // last choice (mode 6)
        writer.apvts.getParameter (ParameterIDs::output)->setValueNotifyingHost (0.3f);
        writer.apvts.getParameter (ParameterIDs::mix)->setValueNotifyingHost (0.55f);
        writer.apvts.getParameter (ParameterIDs::stereoMode)->setValueNotifyingHost (1.0f); // M/S
        writer.apvts.getParameter (ParameterIDs::link)->setValueNotifyingHost (0.2f);
        writer.apvts.getParameter (ParameterIDs::sidechainHpf)->setValueNotifyingHost (0.6f);
        writer.apvts.getParameter (ParameterIDs::sidechainListen)->setValueNotifyingHost (1.0f);
        writer.apvts.getParameter (ParameterIDs::weight)->setValueNotifyingHost (0.77f);
        writer.apvts.getParameter (ParameterIDs::tubeSaturation)->setValueNotifyingHost (0.44f);
        writer.apvts.getParameter (ParameterIDs::transformer)->setValueNotifyingHost (0.66f);
        writer.apvts.getParameter (ParameterIDs::outputTrim)->setValueNotifyingHost (0.9f);
        writer.apvts.getParameter (ParameterIDs::autoGain)->setValueNotifyingHost (0.0f); // false (default is true)
        writer.apvts.getParameter (ParameterIDs::oversampling)->setValueNotifyingHost (1.0f); // last choice (8x)
        writer.apvts.getParameter (ParameterIDs::analogNoise)->setValueNotifyingHost (0.88f);
        writer.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (1.0f); // true

        juce::MemoryBlock state;
        writer.getStateInformation (state);
        check (state.getSize() > 0, "getStateInformation() produced a non-empty state block");

        NovaRoyal670Processor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        for (auto id : { ParameterIDs::input, ParameterIDs::hrmDrive, ParameterIDs::acThreshold,
                          ParameterIDs::dcThreshold, ParameterIDs::output, ParameterIDs::mix,
                          ParameterIDs::link, ParameterIDs::sidechainHpf, ParameterIDs::weight,
                          ParameterIDs::tubeSaturation, ParameterIDs::transformer,
                          ParameterIDs::outputTrim, ParameterIDs::analogNoise })
        {
            const float writerVal = writer.apvts.getParameter (id)->getValue();
            const float readerVal = reader.apvts.getParameter (id)->getValue();
            check (floatsMatch (writerVal, readerVal), juce::String (id) + " survives round trip, got " + juce::String (readerVal, 4));
        }

        check (reader.apvts.getParameter (ParameterIDs::timeConstant)->getValue() >= 0.99f, "timeConstant (choice, last index) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::stereoMode)->getValue() >= 0.99f, "stereoMode (M/S) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::oversampling)->getValue() >= 0.99f, "oversampling (choice, last index) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::sidechainListen)->getValue() >= 0.5f, "sidechainListen (true) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::autoGain)->getValue() < 0.5f, "autoGain (false, non-default) survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::bypass)->getValue() >= 0.5f, "bypass (true) survives round trip");
    }

    std::cout << "\n[StateRoundTrip] Restoring a false boolean doesn't stick at a prior true\n";
    {
        NovaRoyal670Processor writer;
        writer.prepareToPlay (48000.0, 512);
        writer.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (0.0f);
        writer.apvts.getParameter (ParameterIDs::autoGain)->setValueNotifyingHost (0.0f);

        juce::MemoryBlock state;
        writer.getStateInformation (state);

        NovaRoyal670Processor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.apvts.getParameter (ParameterIDs::bypass)->setValueNotifyingHost (1.0f);
        reader.apvts.getParameter (ParameterIDs::autoGain)->setValueNotifyingHost (1.0f);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        check (reader.apvts.getParameter (ParameterIDs::bypass)->getValue() < 0.5f, "bypass (false) correctly overwrites a prior true value");
        check (reader.apvts.getParameter (ParameterIDs::autoGain)->getValue() < 0.5f, "autoGain (false) correctly overwrites a prior true value");
    }

    std::cout << "\n[StateRoundTrip] Bus layout: mono-in/mono-out and stereo-in/stereo-out accepted, mixed rejected\n";
    {
        NovaRoyal670Processor proc;

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
        NovaRoyal670Processor proc;
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

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL STATE ROUND-TRIP VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
