#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Analysis/NovaAudioProfile.h"
#include "Analysis/NovaLoudnessAnalyzer.h"
#include "Analysis/NovaDynamicsAnalyzer.h"
#include "Analysis/NovaSpectralAnalyzer.h"
#include "Analysis/NovaStereoAnalyzer.h"
#include "Decision/NovaMasteringDecisionEngine.h"
#include "DSP/NovaMasteringChain.h"

// The single facade both the plugin (Nova AI Master) and the standalone
// app (Nova AI Master Standalone) call into. Guarantees the app and plugin
// can never diverge in their mastering decisions (spec section 3, 38) -
// there is exactly one analysis engine, one decision engine, and one DSP
// chain, shared by relative-path source reference (see both targets'
// CMakeLists.txt), not two parallel implementations.
class NovaAIMasterEngine
{
public:
    NovaAIMasterEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    // Offline/faster-than-realtime: feed the whole captured/imported mix,
    // in one call or successive chunks, via analyze(). Once every chunk
    // has been fed, call finishAnalysis() exactly once - loudness gating
    // (BS.1770 integrated LUFS, LRA) needs the complete signal before it
    // can compute anything, so getProfile() is only valid after that call.
    void analyze (const juce::AudioBuffer<float>& buffer) noexcept;
    void finishAnalysis() noexcept;
    const NovaAudioProfile& getProfile() const noexcept { return profile; }

    // AudioProfile + prefs -> MasteringPlan. Safe to call repeatedly if the
    // user changes Style/Intensity/Loudness without re-analyzing.
    NovaMasteringPlan decide (const NovaMasterUserPrefs& prefs) const;

    // Applies a plan to the realtime DSP chain. Call setPlan() once (e.g.
    // after MASTER is pressed), then process() per block/per whole buffer.
    void setPlan (const NovaMasteringPlan& plan) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    NovaLoudnessAnalyzer  loudnessAnalyzer;
    NovaDynamicsAnalyzer  dynamicsAnalyzer;
    NovaSpectralAnalyzer  spectralAnalyzer;
    NovaStereoAnalyzer    stereoAnalyzer;

    NovaMasteringDecisionEngine decisionEngine;
    NovaMasteringChain          masteringChain;

    NovaAudioProfile profile;
    double sampleRate = 44100.0;
    long long totalSamplesAnalyzed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAIMasterEngine)
};
