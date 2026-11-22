// Headless validation of PluginProcessor's state save/recall: drives the
// REAL NovaPitchAudioProcessor class through getStateInformation()/
// setStateInformation() and confirms all params survive, with particular
// attention to the formant param's bool->float type migration.

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
    // NovaPitchAudioProcessor pulls in APVTS and WebBrowserComponent-
    // capable machinery expecting a running JUCE message-manager lifecycle
    // (matches Nova Doubler's own StateRoundTripValidation harness).
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;

    std::cout << "\n[StateRoundTrip] All params survive a save/restore round trip\n";
    {
        NovaPitchAudioProcessor writer;
        writer.prepareToPlay (48000.0, 512);

        auto setNorm = [&] (const char* id, float normalized)
        { writer.apvts.getParameter (id)->setValueNotifyingHost (normalized); };

        setNorm (ParameterIDs::key,   6.0f / 11.0f);
        setNorm (ParameterIDs::scale, 3.0f / 4.0f);
        setNorm (ParameterIDs::tolerance, 0.22f);
        setNorm (ParameterIDs::amount, 0.77f);
        setNorm (ParameterIDs::confidenceThreshold, 0.55f);
        setNorm (ParameterIDs::vibrato, 0.33f);
        setNorm (ParameterIDs::formant, 0.81f);
        setNorm (ParameterIDs::lowLatency, 1.0f);
        setNorm (ParameterIDs::formantWarp, 0.9f);
        setNorm (ParameterIDs::mix, 0.65f);
        setNorm (ParameterIDs::outputGain, 0.7f);
        setNorm (ParameterIDs::range, 0.4f);
        setNorm (ParameterIDs::vibratoRate, 0.6f);
        setNorm (ParameterIDs::transients, 0.15f);

        juce::MemoryBlock state;
        writer.getStateInformation (state);
        check (state.getSize() > 0, "getStateInformation() produced a non-empty state block");

        NovaPitchAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        auto matches = [&] (const char* id, float expectedNorm)
        {
            const float actual = reader.apvts.getParameter (id)->getValue();
            return std::abs (actual - expectedNorm) < 1.0e-3f;
        };

        check (matches (ParameterIDs::key, 6.0f / 11.0f), "key survives round trip");
        check (matches (ParameterIDs::scale, 3.0f / 4.0f), "scale survives round trip");
        check (matches (ParameterIDs::tolerance, 0.22f), "tolerance (Retune Speed) survives round trip");
        check (matches (ParameterIDs::amount, 0.77f), "amount (Humanize) survives round trip");
        check (matches (ParameterIDs::confidenceThreshold, 0.55f), "confidenceThreshold survives round trip");
        check (matches (ParameterIDs::vibrato, 0.33f), "vibrato (Natural Vibrato) survives round trip");
        check (matches (ParameterIDs::formant, 0.81f), "formant survives round trip");
        check (reader.apvts.getParameter (ParameterIDs::lowLatency)->getValue() >= 0.5f, "lowLatency (true) survives round trip");
        check (matches (ParameterIDs::formantWarp, 0.9f), "formantWarp survives round trip");
        check (matches (ParameterIDs::mix, 0.65f), "mix survives round trip");
        check (matches (ParameterIDs::outputGain, 0.7f), "outputGain survives round trip");
        check (matches (ParameterIDs::range, 0.4f), "range survives round trip");
        check (matches (ParameterIDs::vibratoRate, 0.6f), "vibratoRate survives round trip");
        check (matches (ParameterIDs::transients, 0.15f), "transients survives round trip");
    }

    std::cout << "\n[StateRoundTrip] formant param migration: an old saved BOOL state (0/1) lands on a sane float value\n";
    {
        // Simulate a session saved by the pre-rebuild plugin, where
        // "formant" was an AudioParameterBool. APVTS matches parameters by
        // ID and both bool and float params share the same underlying
        // 0..1 normalized representation, so an old "true" (normalized
        // 1.0) should land at normalized 1.0 on the new float param too -
        // this confirms that migration is sane (doesn't silently zero out
        // or clamp to a nonsensical value), not a bit-for-bit type check.
        juce::ValueTree fakeOldState ("Parameters");
        auto addParam = [&] (const char* id, float value)
        {
            juce::ValueTree v ("PARAM");
            v.setProperty ("id", id, nullptr);
            v.setProperty ("value", value, nullptr);
            fakeOldState.addChild (v, -1, nullptr);
        };
        addParam (ParameterIDs::key, 0.0f);
        addParam (ParameterIDs::scale, 0.0f);
        addParam (ParameterIDs::tolerance, 0.5f);
        addParam (ParameterIDs::amount, 1.0f);
        addParam (ParameterIDs::confidenceThreshold, 0.4f);
        addParam (ParameterIDs::vibrato, 0.0f);
        addParam (ParameterIDs::formant, 1.0f); // old bool "on"
        addParam (ParameterIDs::lowLatency, 0.0f);

        std::unique_ptr<juce::XmlElement> xml (fakeOldState.createXml());
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary (*xml, state);

        NovaPitchAudioProcessor reader;
        reader.prepareToPlay (48000.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        const float formantVal = reader.apvts.getParameter (ParameterIDs::formant)->getValue();
        check (formantVal >= 0.5f, "old bool-'true' formant state lands at a high (not zeroed) float value, got " + juce::String (formantVal, 3));
    }

    std::cout << "\n[StateRoundTrip] Bus layout: mono-in/mono-out and stereo-in/stereo-out accepted, mismatched rejected\n";
    {
        NovaPitchAudioProcessor proc;

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
        check (! proc.checkBusesLayoutSupported (monoInStereoOut), "mismatched mono-in/stereo-out is rejected");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL STATE ROUND-TRIP VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
