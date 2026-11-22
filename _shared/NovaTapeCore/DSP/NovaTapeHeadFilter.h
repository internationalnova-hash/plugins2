#pragma once

#include "../NovaTapeTypes.h"
#include <juce_dsp/juce_dsp.h>

// Speed- and personality-dependent head/tape frequency response: a low-
// frequency "head bump" resonance plus a gentle high-frequency roll-off
// shelf, modeling the physical reality that a tape machine's frequency
// response isn't flat - it depends on tape speed (IPS) and, in this
// stylized model, on the tape "personality" (Studio/Vintage/Hot) too.
// Independent of Nova Heat, which has no head-bump or speed concept at
// all - see NovaTapeSaturator.h for the nonlinearity-side independence.
namespace nova::tape
{
    class NovaTapeHeadFilter
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate;
            bumpL.reset(); bumpR.reset();
            rolloffL.reset(); rolloffR.reset();
            update();
        }

        void reset() noexcept
        {
            bumpL.reset(); bumpR.reset();
            rolloffL.reset(); rolloffR.reset();
        }

        void setParams (IpsSpeed speed, const TapeTypeProfile& typeProfile) noexcept
        {
            const auto ipsProfile = getIpsProfile (speed);
            const float bumpGainDb = ipsProfile.headBumpGainDb + typeProfile.headBumpGainDb;
            const float rolloffFreq = juce::jlimit (2000.0f, 20000.0f,
                ipsProfile.hfRolloffFreqHz - typeProfile.hfRolloffTiltDb * 300.0f);

            if (! juce::approximatelyEqual (bumpGainDb, lastBumpGainDb)
                || ! juce::approximatelyEqual (ipsProfile.headBumpFreqHz, lastBumpFreq)
                || ! juce::approximatelyEqual (rolloffFreq, lastRolloffFreq))
            {
                lastBumpGainDb = bumpGainDb;
                lastBumpFreq = ipsProfile.headBumpFreqHz;
                lastBumpQ = ipsProfile.headBumpQ;
                lastRolloffFreq = rolloffFreq;
                update();
            }
        }

        float processL (float x) noexcept { return rolloffL.processSample (bumpL.processSample (x)); }
        float processR (float x) noexcept { return rolloffR.processSample (bumpR.processSample (x)); }

    private:
        void update() noexcept
        {
            const float bumpGainLinear = juce::Decibels::decibelsToGain (lastBumpGainDb);
            auto bumpCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sr, juce::jmax (20.0f, lastBumpFreq), juce::jmax (0.1f, lastBumpQ), juce::jmax (0.05f, bumpGainLinear));
            *bumpL.coefficients = *bumpCoeffs;
            *bumpR.coefficients = *bumpCoeffs;

            auto rolloffCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sr, juce::jmax (1000.0f, lastRolloffFreq), 0.707f, juce::Decibels::decibelsToGain (-9.0f));
            *rolloffL.coefficients = *rolloffCoeffs;
            *rolloffR.coefficients = *rolloffCoeffs;
        }

        double sr = 44100.0;
        juce::dsp::IIR::Filter<float> bumpL, bumpR;
        juce::dsp::IIR::Filter<float> rolloffL, rolloffR;

        float lastBumpGainDb = -1000.0f, lastBumpFreq = -1.0f, lastBumpQ = 0.7f, lastRolloffFreq = -1.0f;
    };
}
