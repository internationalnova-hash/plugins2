// Headless validation of NovaBendAudioProcessor's real APVTS state
// round-trip: getStateInformation()/setStateInformation() into a FRESH
// processor instance, confirming every parameter survives. Choice
// parameters are set to exact legal step values, not arbitrary floats.

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
    std::cout << "\n[StateRoundTrip] All parameters survive save/restore into a fresh instance\n";
    {
        NovaBendAudioProcessor source;
        source.prepareToPlay (44100.0, 512);

        auto setF = [&] (const char* id, float v) { source.apvts.getParameter (id)->setValueNotifyingHost (source.apvts.getParameter (id)->convertTo0to1 (v)); };
        auto setChoice = [&] (const char* id, int idx)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (source.apvts.getParameter (id))) *p = idx;
        };
        auto setBool = [&] (const char* id, bool v)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterBool*> (source.apvts.getParameter (id))) *p = v;
        };

        setF (ParameterIDs::pitch, 5.5f);
        setF (ParameterIDs::formant, -3.25f);
        setBool (ParameterIDs::flatten, true);
        setChoice (ParameterIDs::flattenNote, 7); // G
        setChoice (ParameterIDs::flattenOctave, 4); // octave 5 (index 4)
        setBool (ParameterIDs::link, true);
        setF (ParameterIDs::mix, 62.0f);
        setChoice (ParameterIDs::quality, 0); // Live
        setBool (ParameterIDs::bypass, true);

        juce::MemoryBlock state;
        source.getStateInformation (state);

        NovaBendAudioProcessor dest;
        dest.prepareToPlay (44100.0, 512);
        dest.setStateInformation (state.getData(), (int) state.getSize());

        auto getF = [&] (const char* id) { return dest.apvts.getRawParameterValue (id)->load(); };
        auto getChoiceIdx = [&] (const char* id) { return (int) dest.apvts.getRawParameterValue (id)->load(); };
        auto getBool = [&] (const char* id) { return dest.apvts.getRawParameterValue (id)->load() >= 0.5f; };

        check (std::abs (getF (ParameterIDs::pitch) - 5.5f) < 0.01f, "pitch survives round trip, got " + juce::String (getF (ParameterIDs::pitch), 2));
        check (std::abs (getF (ParameterIDs::formant) - (-3.25f)) < 0.01f, "formant survives round trip, got " + juce::String (getF (ParameterIDs::formant), 2));
        check (getBool (ParameterIDs::flatten) == true, "flatten survives round trip");
        check (getChoiceIdx (ParameterIDs::flattenNote) == 7, "flattenNote survives round trip (G), got index " + juce::String (getChoiceIdx (ParameterIDs::flattenNote)));
        check (getChoiceIdx (ParameterIDs::flattenOctave) == 4, "flattenOctave survives round trip (index 4), got " + juce::String (getChoiceIdx (ParameterIDs::flattenOctave)));
        check (getBool (ParameterIDs::link) == true, "link survives round trip");
        check (std::abs (getF (ParameterIDs::mix) - 62.0f) < 0.1f, "mix survives round trip, got " + juce::String (getF (ParameterIDs::mix), 2));
        check (getChoiceIdx (ParameterIDs::quality) == 0, "quality survives round trip (Live), got " + juce::String (getChoiceIdx (ParameterIDs::quality)));
        check (getBool (ParameterIDs::bypass) == true, "bypass survives round trip");
    }

    std::cout << "\n[StateRoundTrip] Restoring a false boolean doesn't stick at a prior true\n";
    {
        NovaBendAudioProcessor source;
        source.prepareToPlay (44100.0, 512);
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (source.apvts.getParameter (ParameterIDs::bypass))) *p = false;

        juce::MemoryBlock state;
        source.getStateInformation (state);

        NovaBendAudioProcessor dest;
        dest.prepareToPlay (44100.0, 512);
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (dest.apvts.getParameter (ParameterIDs::bypass))) *p = true; // pre-existing true
        dest.setStateInformation (state.getData(), (int) state.getSize());

        check (dest.apvts.getRawParameterValue (ParameterIDs::bypass)->load() < 0.5f, "bypass (false) correctly overwrites a prior true value");
    }

    std::cout << "\n[StateRoundTrip] Bus layout: mono-in/mono-out and stereo-in/stereo-out accepted, mixed rejected\n";
    {
        NovaBendAudioProcessor proc;
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        check (proc.checkBusesLayoutSupported (mono), "mono-in/mono-out is accepted");

        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        check (proc.checkBusesLayoutSupported (stereo), "stereo-in/stereo-out is accepted");

        juce::AudioProcessor::BusesLayout mixed;
        mixed.inputBuses.add (juce::AudioChannelSet::mono());
        mixed.outputBuses.add (juce::AudioChannelSet::stereo());
        check (! proc.checkBusesLayoutSupported (mixed), "mixed mono-in/stereo-out is rejected");
    }

    std::cout << "\n[StateRoundTrip] Mono bus: real processBlock produces finite output via the duplicate-then-downmix adaptation\n";
    {
        NovaBendAudioProcessor proc;
        proc.setPlayConfigDetails (1, 1, 44100.0, 512);
        proc.prepareToPlay (44100.0, 512);

        juce::AudioBuffer<float> buffer (1, 512);
        for (int i = 0; i < 512; ++i)
            buffer.setSample (0, i, 0.3f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / 44100.0));

        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);

        bool finite = true;
        for (int i = 0; i < 512; ++i) if (! std::isfinite (buffer.getSample (0, i))) { finite = false; break; }
        check (finite, "Mono bus processBlock: all outputs finite");
    }

    std::cout << "\n[StateRoundTrip] Malformed state does not crash\n";
    {
        NovaBendAudioProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        const char garbage[] = { 1, 2, 3, 4, 5 };
        proc.setStateInformation (garbage, (int) sizeof (garbage));
        proc.setStateInformation (nullptr, 0);

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);
        check (true, "setStateInformation with garbage/null data did not crash, processor still processes audio");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL STATE ROUND-TRIP VALIDATION TESTS PASSED\n" : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";
    return failures == 0 ? 0 : 1;
}
