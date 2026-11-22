// Headless validation of NovaRideAudioProcessor::getStateInformation() /
// setStateInformation() - the real processor, no host. See CMakeLists.txt's
// NovaRideStateRoundTripValidation target.
#include <cmath>
#include <cstdio>
#include <random>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& label)
    {
        if (condition)
            std::printf ("PASS: %s\n", label.toRawUTF8());
        else
        {
            std::printf ("FAIL: %s\n", label.toRawUTF8());
            ++failures;
        }
    }

    void section (const char* title)
    {
        std::printf ("\n=== %s ===\n", title);
    }

    void testDefaultStateRoundTrips()
    {
        section ("Default state round-trips");
        NovaRideAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);

        juce::MemoryBlock state;
        proc.getStateInformation (state);
        check (state.getSize() > 0, "getStateInformation produces non-empty data");

        NovaRideAudioProcessor proc2;
        proc2.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc2.prepareToPlay (48000.0, 512);
        proc2.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        for (auto* param : proc.getParameters())
        {
            const auto id = dynamic_cast<juce::AudioProcessorParameterWithID*> (param)->paramID;
            auto* other = proc2.apvts.getParameter (id);
            check (other != nullptr && std::abs (other->getValue() - param->getValue()) < 1.0e-4f,
                   "parameter '" + id + "' round-trips at default values");
        }
    }

    void testRandomizedParameterStateRoundTrips()
    {
        section ("Randomized parameter state round-trips");
        NovaRideAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);

        std::mt19937 rng (99);
        std::uniform_real_distribution<float> norm01 (0.0f, 1.0f);
        for (auto* param : proc.getParameters())
        {
            float v = norm01 (rng);
            // Discrete parameters (bool/choice) only ever receive an exact
            // legal step value from a real host or from our own code - a
            // raw continuous float bypasses AudioParameterBool/Choice's own
            // snapping (their setValue() stores the raw float as-is) in a
            // way state save/restore's own snapping then doesn't reproduce.
            // Round to the nearest legal step so this test reflects how the
            // parameter is actually ever driven.
            if (param->isDiscrete())
            {
                const int numSteps = param->getNumSteps();
                if (numSteps > 1)
                    v = std::round (v * static_cast<float> (numSteps - 1)) / static_cast<float> (numSteps - 1);
            }
            param->setValueNotifyingHost (v);
        }

        juce::MemoryBlock state;
        proc.getStateInformation (state);

        NovaRideAudioProcessor proc2;
        proc2.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc2.prepareToPlay (48000.0, 512);
        proc2.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        bool allMatch = true;
        for (auto* param : proc.getParameters())
        {
            const auto id = dynamic_cast<juce::AudioProcessorParameterWithID*> (param)->paramID;
            auto* other = proc2.apvts.getParameter (id);
            if (other == nullptr || std::abs (other->getValue() - param->getValue()) > 1.0e-3f)
                allMatch = false;
        }
        check (allMatch, "every randomized parameter value round-trips through save/restore");
    }

    void testStateSurvivesSampleRateChange()
    {
        section ("State survives a sample-rate change between save and restore");
        NovaRideAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, 44100.0, 512);
        proc.prepareToPlay (44100.0, 512);
        proc.apvts.getParameter (ParameterIDs::target)->setValueNotifyingHost (0.75f);

        juce::MemoryBlock state;
        proc.getStateInformation (state);

        NovaRideAudioProcessor proc2;
        proc2.setPlayConfigDetails (2, 2, 96000.0, 1024);
        proc2.prepareToPlay (96000.0, 1024);
        proc2.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        auto* original = proc.apvts.getParameter (ParameterIDs::target);
        auto* restored = proc2.apvts.getParameter (ParameterIDs::target);
        check (restored != nullptr && std::abs (restored->getValue() - original->getValue()) < 1.0e-4f,
               "Target parameter matches after restoring into a processor prepared at a different sample rate");
    }

    void testMalformedStateDoesNotCrash()
    {
        section ("Malformed state does not crash");
        NovaRideAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);

        const char garbage[] = { 0x00, 0x01, 0x02, 0x03, (char) 0xff, (char) 0xfe, 0x00 };
        proc.setStateInformation (garbage, sizeof (garbage));
        check (true, "setStateInformation with garbage data did not crash");

        proc.setStateInformation (nullptr, 0);
        check (true, "setStateInformation with null/zero-length data did not crash");

        // Processor should still be usable afterward.
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);
        check (true, "processor still processes audio after malformed state input");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    testDefaultStateRoundTrips();
    testRandomizedParameterStateRoundTrips();
    testStateSurvivesSampleRateChange();
    testMalformedStateDoesNotCrash();

    std::printf ("\n  NovaRideStateRoundTripValidation: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
