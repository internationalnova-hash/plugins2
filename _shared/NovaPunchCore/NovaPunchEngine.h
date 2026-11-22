#pragma once

#include "NovaPunchTypes.h"
#include "Analysis/NovaPunchEnvelopeFollower.h"
#include "Analysis/NovaPunchCrossover.h"
#include "Analysis/NovaPunchTransientDetector.h"
#include "Analysis/NovaPunchEventSegmenter.h"
#include "DSP/NovaPunchGainShaper.h"
#include "DSP/NovaPunchClipSafe.h"

#include <juce_audio_basics/juce_audio_basics.h>

// Top-level facade tying PUNCH/BODY/FOCUS/SENSITIVITY/CLIP SAFE/MIX
// together - the only class PluginProcessor talks to for audio.
//
// Detection runs on the mono-summed signal (one shared crossover +
// detector + segmenter + gain shaper produce ONE gain decision per
// sample), matching this catalog's established pattern for keeping a
// stereo image stable (see e.g. Nova Bend's mono-summed pitch detection) -
// there is no failure mode here where L and R could independently trigger
// different regions and pull the image apart. The SAME computed gain is
// then applied to L and R independently through their own crossover
// instances (kept running unconditionally regardless of Focus, so
// switching Focus mid-stream never hits cold filter state).
//
// Zero added latency by design: every stage here is a continuous IIR/
// one-pole process, nothing buffers a block or delays output relative to
// input, so Mix needs no dry-delay-alignment line at all (unlike Nova
// Bend's shifter) - getLatencySamples() is a true, permanent 0.
namespace nova::punch
{
    class NovaPunchEngine
    {
    public:
        NovaPunchEngine() = default;

        void prepare (double sampleRate, int maxBlockSize) noexcept;
        void reset() noexcept;

        enum class Focus { Full = 0, Low = 1, Mid = 2, High = 3 };

        struct Params
        {
            float punchPercent = 50.0f;
            float bodyPercent = 50.0f;
            Focus focus = Focus::Full;
            float sensitivityPercent = 65.0f;
            float mixPercent = 100.0f;
            bool  clipSafe = true;
            bool  bypassed = false;
        };

        void setParams (const Params& p) noexcept;

        void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

        int getLatencySamples() const noexcept { return 0; }

        // --- telemetry (block-level snapshots, safe to read from the
        // message thread - see PluginProcessor for how these get pushed
        // into atomics for the UI) ---
        float getTransientProbability() const noexcept { return lastTransientProbability; }
        float getTransientStrength() const noexcept { return lastTransientStrength; }
        float getAttackGainDb() const noexcept { return lastAttackGainDb; }
        float getDecayGainDb() const noexcept { return lastDecayGainDb; }
        float getSustainGainDb() const noexcept { return lastSustainGainDb; }
        float getPunchActivity() const noexcept { return lastPunchActivity; }
        float getBodyActivity() const noexcept { return lastBodyActivity; }
        float getLowActivity() const noexcept { return lastLowActivity; }
        float getMidActivity() const noexcept { return lastMidActivity; }
        float getHighActivity() const noexcept { return lastHighActivity; }
        float getInputPeakL() const noexcept { return lastInputPeakL; }
        float getInputPeakR() const noexcept { return lastInputPeakR; }
        float getOutputPeakL() const noexcept { return lastOutputPeakL; }
        float getOutputPeakR() const noexcept { return lastOutputPeakR; }
        int   getCurrentRegion() const noexcept { return (int) lastRegion; }

    private:
        NovaPunchCrossover crossoverMono, crossoverL, crossoverR;
        NovaPunchTransientDetector detector;
        NovaPunchEventSegmenter segmenter;
        NovaPunchGainShaper gainShaper;
        NovaPunchClipSafe clipSafeL, clipSafeR;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed, bypassSmoothed;

        Params params;

        float lastTransientProbability = 0.0f, lastTransientStrength = 0.0f;
        float lastAttackGainDb = 0.0f, lastDecayGainDb = 0.0f, lastSustainGainDb = 0.0f;
        float lastPunchActivity = 0.0f, lastBodyActivity = 0.0f;
        float lastLowActivity = 0.0f, lastMidActivity = 0.0f, lastHighActivity = 0.0f;
        float lastInputPeakL = 0.0f, lastInputPeakR = 0.0f;
        float lastOutputPeakL = 0.0f, lastOutputPeakR = 0.0f;
        NovaPunchEventSegmenter::Region lastRegion = NovaPunchEventSegmenter::Region::Idle;
    };
}
