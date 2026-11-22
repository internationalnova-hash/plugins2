#pragma once

#include "../PitchShift/NovaMicroShiftLine.h"
#include "../Modulation/NovaOrganicModulator.h"
#include <juce_core/juce_core.h>
#include <cstdint>
#include <cmath>

// One "virtual double" - a single voice's contribution to the doubling
// effect. Wraps one NovaMicroShiftLine (does both the micro-delay and
// micro-pitch) plus pan, driven by three INDEPENDENT NovaOrganicModulators
// (delay-drift, pitch-drift, pan-drift) so each voice's movement is its
// own, never a duplicate-and-pan copy of another voice (spec section 9:
// voice decorrelation).
struct NovaDoublerVoiceShape
{
    float panSign = 1.0f;     // -1 or +1
    float delayUnit = 1.0f;   // 0..1, this voice's position within the ensemble's delay spread
    float pitchSign = 1.0f;   // -1 or +1
    float pitchUnit = 1.0f;   // 0..1
    uint32_t seedBase = 1;    // distinct per voice - feeds the 3 modulators with 3 distinct seeds
};

class NovaDoublerVoice
{
public:
    NovaDoublerVoice() = default;

    void prepare (double sampleRate, float maxDelayMs) noexcept
    {
        sr = sampleRate;
        shiftLine.prepare (sampleRate, maxDelayMs);
        // Time constants: delay/pitch drift deliberately slow (spec:
        // "extremely slow, decorrelated, organic modulation"); pan drifts
        // slower still since a wandering stereo position reads as
        // unstable/seasick faster than a wandering delay or pitch does.
        delayMod.prepare (sampleRate, 4.0f, 0);
        pitchMod.prepare (sampleRate, 3.0f, 0);
        panMod.prepare (sampleRate, 6.0f, 0);
        primed = false;
        everConfigured = false;
    }

    // Called once at startup (from prepare()'s first configure) AND again
    // whenever the ensemble reassigns this voice slot's shape (e.g. a live
    // 2<->4 VOICES switch while audio is flowing). Only the FIRST call
    // resets modulator state and re-primes the shift line with a hard
    // jump - both are safe only when silence is flowing (true at startup).
    // On a later, live reconfigure, resetting modulator state (a bounded
    // [-1,1] random walk) or hard-jumping the delay line would itself be
    // an audible discontinuity; instead the new shape's targets reach
    // this voice through the normal setTargets()/shiftLine gradual
    // jump-correction path, which was built exactly for smooth in-flight
    // drift and glides instead of clicking.
    void configure (const NovaDoublerVoiceShape& newShape) noexcept
    {
        shape = newShape;
        if (! everConfigured)
        {
            delayMod.prepare (sr, 4.0f, shape.seedBase + 1);
            pitchMod.prepare (sr, 3.0f, shape.seedBase + 2);
            panMod.prepare (sr, 6.0f, shape.seedBase + 3);
            primed = false;
            everConfigured = true;
        }
    }

    // Called whenever the ensemble's Width/Tightness-derived targets
    // change for this voice (spec: WIDTH sets inter-voice spread,
    // TIGHTNESS sets offset-from-original magnitude and modulation depth).
    void setTargets (float baseDelayMs, float delayModDepthMs,
                      float basePitchCents, float pitchModDepthCents,
                      float basePan, float panModDepth) noexcept
    {
        targetBaseDelayMs = baseDelayMs;
        targetDelayModDepthMs = delayModDepthMs;
        targetBasePitchCents = basePitchCents;
        targetPitchModDepthCents = pitchModDepthCents;
        targetBasePan = basePan;
        targetPanModDepth = panModDepth;

        if (! primed)
        {
            shiftLine.primeDelay ((float) (baseDelayMs / 1000.0 * sr));
            primed = true;
        }
    }

    void reset() noexcept
    {
        shiftLine.reset();
        delayMod.reset();
        pitchMod.reset();
        panMod.reset();
        primed = false;
    }

    // input: mono dry source shared by all voices. outL/outR: ADDED to
    // (not overwritten) by the caller's convention - see
    // NovaVoiceEnsemble, which owns the mixing/summing.
    void processSample (float input, float& outL, float& outR) noexcept
    {
        const float delayMs = targetBaseDelayMs + delayMod.getNextSample() * targetDelayModDepthMs;
        const float pitchCents = targetBasePitchCents + pitchMod.getNextSample() * targetPitchModDepthCents;
        const float pan = juce::jlimit (-1.0f, 1.0f, targetBasePan + panMod.getNextSample() * targetPanModDepth);

        const float targetSamples = (float) (delayMs / 1000.0 * sr);
        const float ratio = std::pow (2.0f, pitchCents / 1200.0f);

        const float shifted = shiftLine.processSample (input, targetSamples, ratio);

        // Equal-power pan.
        const float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // 0..pi/2
        const float gainL = std::cos (panAngle);
        const float gainR = std::sin (panAngle);

        outL += shifted * gainL;
        outR += shifted * gainR;
    }

    const NovaDoublerVoiceShape& getShape() const noexcept { return shape; }

private:
    NovaMicroShiftLine shiftLine;
    NovaOrganicModulator delayMod, pitchMod, panMod;
    NovaDoublerVoiceShape shape;

    double sr = 44100.0;
    bool primed = false;
    bool everConfigured = false;

    float targetBaseDelayMs = 15.0f, targetDelayModDepthMs = 0.0f;
    float targetBasePitchCents = 0.0f, targetPitchModDepthCents = 0.0f;
    float targetBasePan = 0.0f, targetPanModDepth = 0.0f;
};
