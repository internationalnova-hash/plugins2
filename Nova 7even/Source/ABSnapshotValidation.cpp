// Headless validation of the A/B snapshot system (NOVA_7EVEN_PLAN.md
// section 5.9 / 8.1 step 7). Drives the real NovaSevenAudioProcessor
// class directly.

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include <juce_core/juce_core.h>
#include <iostream>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    void setFloat (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void setChoice (juce::AudioProcessorValueTreeState& apvts, const char* id, int index)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    }

    void applyConfigA (juce::AudioProcessorValueTreeState& apvts)
    {
        setFloat (apvts, ParameterIDs::inputTrim, 2.0f);
        setFloat (apvts, ParameterIDs::preampGain, 3.0f);
        setChoice (apvts, ParameterIDs::characterMode, 0); // Clean
        setChoice (apvts, ParameterIDs::hpfFreq, 1); // 20Hz
        setFloat (apvts, ParameterIDs::lowGain, 2.0f);
        setFloat (apvts, ParameterIDs::midGain, -1.5f);
        setFloat (apvts, ParameterIDs::mix, 80.0f);
        setFloat (apvts, ParameterIDs::outputTrim, -1.0f);
    }

    void applyConfigB (juce::AudioProcessorValueTreeState& apvts)
    {
        setFloat (apvts, ParameterIDs::inputTrim, -10.0f);
        setFloat (apvts, ParameterIDs::preampGain, 8.5f);
        setChoice (apvts, ParameterIDs::characterMode, 2); // Hot
        setChoice (apvts, ParameterIDs::hpfFreq, 4); // 160Hz
        setFloat (apvts, ParameterIDs::lowGain, -6.0f);
        setFloat (apvts, ParameterIDs::midGain, 9.0f);
        setFloat (apvts, ParameterIDs::mix, 45.0f);
        setFloat (apvts, ParameterIDs::outputTrim, 5.5f);
    }

    const char* const kParamIds[] = {
        ParameterIDs::inputTrim, ParameterIDs::preampGain, ParameterIDs::characterMode,
        ParameterIDs::hpfFreq, ParameterIDs::lowGain, ParameterIDs::midGain,
        ParameterIDs::mix, ParameterIDs::outputTrim
    };

    bool paramsMatch (juce::AudioProcessorValueTreeState& a, juce::AudioProcessorValueTreeState& b)
    {
        for (auto* id : kParamIds)
        {
            const auto va = a.getRawParameterValue (id)->load();
            const auto vb = b.getRawParameterValue (id)->load();
            if (std::abs (va - vb) > 1.0e-4f)
            {
                std::cout << "    mismatch on " << id << ": " << va << " vs " << vb << "\n";
                return false;
            }
        }
        return true;
    }
}

int main()
{
    const double sampleRate = 48000.0;
    const int blockSize = 512;

    std::cout << "\n[ABSnapshot] Capture A, edit to B, recall A restores exact original values\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        applyConfigA (proc.apvts);
        proc.abSnapshot.captureActive (proc.apvts); // captures into slot A (default active)
        check (proc.abSnapshot.getActiveSlot() == ABSnapshot::Slot::A, "starts on slot A");
        check (proc.abSnapshot.hasSnapshot (ABSnapshot::Slot::A), "slot A has a snapshot after capture");
        check (! proc.abSnapshot.hasSnapshot (ABSnapshot::Slot::B), "slot B has no snapshot yet");

        // Snapshot the exact A values via a second, independent processor
        // instance loaded from A's captured state, for later comparison
        // (rather than re-reading proc's own live params, which are about
        // to be edited).
        NovaSevenAudioProcessor referenceA;
        referenceA.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        referenceA.prepareToPlay (sampleRate, blockSize);
        applyConfigA (referenceA.apvts);

        proc.abSnapshot.toggleAndRecall (proc.apvts); // -> slot B, empty, no-op recall
        check (proc.abSnapshot.getActiveSlot() == ABSnapshot::Slot::B, "toggled to slot B");

        applyConfigB (proc.apvts); // user edits while "on" slot B
        proc.abSnapshot.captureActive (proc.apvts); // captures into slot B
        check (proc.abSnapshot.hasSnapshot (ABSnapshot::Slot::B), "slot B has a snapshot after capture");

        NovaSevenAudioProcessor referenceB;
        referenceB.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        referenceB.prepareToPlay (sampleRate, blockSize);
        applyConfigB (referenceB.apvts);

        proc.abSnapshot.toggleAndRecall (proc.apvts); // -> back to slot A, recalls it
        check (proc.abSnapshot.getActiveSlot() == ABSnapshot::Slot::A, "toggled back to slot A");
        check (paramsMatch (proc.apvts, referenceA.apvts), "recalling slot A restores exactly config A's values, not left over from config B edits");

        proc.abSnapshot.toggleAndRecall (proc.apvts); // -> slot B again
        check (paramsMatch (proc.apvts, referenceB.apvts), "recalling slot B restores exactly config B's values");
    }

    std::cout << "\n[ABSnapshot] Both snapshots survive a save/reload cycle\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        applyConfigA (proc.apvts);
        proc.abSnapshot.captureActive (proc.apvts);
        proc.abSnapshot.toggleAndRecall (proc.apvts);
        applyConfigB (proc.apvts);
        proc.abSnapshot.captureActive (proc.apvts);
        proc.abSnapshot.toggleAndRecall (proc.apvts); // back to A, active=A again

        juce::MemoryBlock stateBlock;
        proc.getStateInformation (stateBlock);

        NovaSevenAudioProcessor freshProc;
        freshProc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        freshProc.prepareToPlay (sampleRate, blockSize);
        freshProc.setStateInformation (stateBlock.getData(), (int) stateBlock.getSize());

        check (freshProc.abSnapshot.hasSnapshot (ABSnapshot::Slot::A), "slot A snapshot survives save/reload");
        check (freshProc.abSnapshot.hasSnapshot (ABSnapshot::Slot::B), "slot B snapshot survives save/reload");

        NovaSevenAudioProcessor referenceA;
        referenceA.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        referenceA.prepareToPlay (sampleRate, blockSize);
        applyConfigA (referenceA.apvts);

        NovaSevenAudioProcessor referenceB;
        referenceB.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        referenceB.prepareToPlay (sampleRate, blockSize);
        applyConfigB (referenceB.apvts);

        freshProc.abSnapshot.recallActive (freshProc.apvts); // active slot was A at save time
        check (paramsMatch (freshProc.apvts, referenceA.apvts), "reloaded slot A recalls exactly config A's values");

        freshProc.abSnapshot.toggleAndRecall (freshProc.apvts);
        check (paramsMatch (freshProc.apvts, referenceB.apvts), "reloaded slot B recalls exactly config B's values");
    }

    std::cout << "\n[ABSnapshot] copyActiveToOther overwrites the other slot with the active slot's state\n";
    {
        NovaSevenAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);

        applyConfigA (proc.apvts);
        proc.abSnapshot.captureActive (proc.apvts); // slot A = config A, active = A
        proc.abSnapshot.copyActiveToOther(); // slot B := slot A

        proc.abSnapshot.toggleAndRecall (proc.apvts); // -> B, recalls the just-copied A values
        NovaSevenAudioProcessor referenceA;
        referenceA.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        referenceA.prepareToPlay (sampleRate, blockSize);
        applyConfigA (referenceA.apvts);

        check (paramsMatch (proc.apvts, referenceA.apvts), "slot B, after copyActiveToOther from A, recalls the same values as config A");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL A/B SNAPSHOT VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
