// Headless validation of NovaDeskEQBands (build spec sections 8, 9-14, 15,
// 62, 63). Uses the class's own analytic getMagnitudeResponseDb() methods
// (built on JUCE's exact Coefficients::getMagnitudeForFrequency(), not a
// simulated sine sweep) so these checks are precise rather than
// measurement-noise-limited.

#include "DSP/NovaDeskEQBands.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>

using namespace nova::desk;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    juce::dsp::ProcessSpec makeSpec (double sampleRate, int numChannels = 1)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = 512;
        spec.numChannels = (juce::uint32) numChannels;
        return spec;
    }

    void testHpfLpfCorners (double sampleRate)
    {
        std::cout << "\n[HPF] -3dB corner lands at the labeled frequency (24dB/oct Butterworth)\n";
        for (size_t i = 1; i < NovaDeskEQBands<float>::hpfFreqChoices.size(); ++i)
        {
            NovaDeskEQBands<float> eq;
            eq.prepare (makeSpec (sampleRate));
            eq.setHpfFreqIndex ((int) i);
            eq.setLpfFreqIndex (0);

            const auto hz = NovaDeskEQBands<float>::hpfFreqChoices[i];
            const auto atCorner = eq.getMagnitudeResponseDb (hz);
            check (std::abs (atCorner - (-3.0f)) < 0.75f,
                   juce::String (hz) + "Hz HPF: " + juce::String (atCorner, 2) + " dB at corner (expect ~-3dB)");
        }

        std::cout << "\n[LPF] -3dB corner lands at the labeled frequency (24dB/oct Butterworth)\n";
        for (size_t i = 1; i < NovaDeskEQBands<float>::lpfFreqChoices.size(); ++i)
        {
            NovaDeskEQBands<float> eq;
            eq.prepare (makeSpec (sampleRate));
            eq.setHpfFreqIndex (0);
            eq.setLpfFreqIndex ((int) i);

            const auto hz = NovaDeskEQBands<float>::lpfFreqChoices[i];
            const auto atCorner = eq.getMagnitudeResponseDb (hz);
            check (std::abs (atCorner - (-3.0f)) < 0.75f,
                   juce::String (hz) + "Hz LPF: " + juce::String (atCorner, 2) + " dB at corner (expect ~-3dB)");
        }
    }

    void testBandGainAtCenter (double sampleRate)
    {
        std::cout << "\n[Bands] Gain at the band's own center frequency matches the requested dB\n";

        NovaDeskEQBands<float> eq;
        eq.prepare (makeSpec (sampleRate));
        eq.setHpfFreqIndex (0);
        eq.setLpfFreqIndex (0);

        eq.setLowFreq (100.0f); eq.setLowGainDb (6.0f); eq.setLowType (BandType::Bell);
        check (std::abs (eq.getLowMagnitudeResponseDb (100.0) - 6.0f) < 0.3f, "Low (Bell) +6dB at 100Hz");

        eq.setLowType (BandType::Shelf);
        const auto lowShelfDb = eq.getLowMagnitudeResponseDb (100.0);
        check (std::abs (lowShelfDb - 3.0f) < 1.0f,
               "Low (Shelf) ~+3dB at 100Hz (shelf midpoint, half of +6dB plateau), got " + juce::String (lowShelfDb, 2));

        eq.setLowMidFreq (400.0f); eq.setLowMidGainDb (-8.0f); eq.setLowMidQ (1.0f);
        check (std::abs (eq.getLowMidMagnitudeResponseDb (400.0) - (-8.0f)) < 0.3f, "Low-Mid (Bell) -8dB at 400Hz");

        eq.setHighMidFreq (2000.0f); eq.setHighMidGainDb (4.5f); eq.setHighMidQ (1.2f);
        check (std::abs (eq.getHighMidMagnitudeResponseDb (2000.0) - 4.5f) < 0.3f, "High-Mid (Bell) +4.5dB at 2kHz");

        eq.setHighFreq (8000.0f); eq.setHighGainDb (5.0f); eq.setHighType (BandType::Bell);
        check (std::abs (eq.getHighMagnitudeResponseDb (8000.0) - 5.0f) < 0.3f, "High (Bell) +5dB at 8kHz");

        eq.setHighType (BandType::Shelf);
        const auto highShelfDb = eq.getHighMagnitudeResponseDb (8000.0);
        check (std::abs (highShelfDb - 2.5f) < 1.0f,
               "High (Shelf) ~+2.5dB at 8kHz (shelf midpoint, half of +5dB plateau), got " + juce::String (highShelfDb, 2));
    }

    void testShelfVsBellShape (double sampleRate)
    {
        std::cout << "\n[Bands] Shelf plateaus far from center; Bell returns toward 0dB far from center\n";

        NovaDeskEQBands<float> eq;
        eq.prepare (makeSpec (sampleRate));
        eq.setHpfFreqIndex (0);
        eq.setLpfFreqIndex (0);
        eq.setLowFreq (100.0f);
        eq.setLowGainDb (10.0f);

        eq.setLowType (BandType::Shelf);
        const auto shelfFar = eq.getLowMagnitudeResponseDb (25.0); // well below shelf freq -> full plateau
        check (shelfFar > 8.0f, "Low Shelf at 25Hz (far below 100Hz corner) stays near full +10dB, got " + juce::String (shelfFar, 2));

        eq.setLowType (BandType::Bell);
        const auto bellFar = eq.getLowMagnitudeResponseDb (25.0); // far below bell center -> back near 0dB
        check (bellFar < 3.0f, "Low Bell at 25Hz (far below 100Hz center) returns near 0dB, got " + juce::String (bellFar, 2));
    }

    void testEqPersonalityIsReal (double sampleRate)
    {
        std::cout << "\n[E/G] Personality produces a real, measurable Q difference (not cosmetic)\n";

        NovaDeskEQBands<float> eqE, eqG;
        eqE.prepare (makeSpec (sampleRate));
        eqG.prepare (makeSpec (sampleRate));
        eqE.setHpfFreqIndex (0); eqE.setLpfFreqIndex (0);
        eqG.setHpfFreqIndex (0); eqG.setLpfFreqIndex (0);

        eqE.setPersonality (EQPersonality::E);
        eqG.setPersonality (EQPersonality::G);

        // Large boost on High-Mid at matched freq/Q/gain in both modes.
        // E's proportional-Q tightening should make it drop off FASTER
        // moving away from center than G's constant-Q curve - measure at
        // 2x the center frequency (one octave up) where the difference
        // should already be clearly visible.
        for (auto* eq : { &eqE, &eqG })
        {
            eq->setHighMidFreq (2000.0f);
            eq->setHighMidGainDb (12.0f);
            eq->setHighMidQ (1.0f);
        }

        const auto centerE = eqE.getHighMidMagnitudeResponseDb (2000.0);
        const auto centerG = eqG.getHighMidMagnitudeResponseDb (2000.0);
        check (std::abs (centerE - centerG) < 0.2f, "E and G agree at the exact center frequency (both ~+12dB)");

        const auto offCenterE = eqE.getHighMidMagnitudeResponseDb (4000.0);
        const auto offCenterG = eqG.getHighMidMagnitudeResponseDb (4000.0);
        check (offCenterE < offCenterG - 0.5f,
               "E drops off faster than G one octave above center at +12dB: E=" + juce::String (offCenterE, 2)
               + "dB, G=" + juce::String (offCenterG, 2) + "dB");

        // Same check on a cut, since E's Q-tightening is driven by
        // |gainDb| (spec section 15's "proportional Q... as boost/cut
        // magnitude increases").
        for (auto* eq : { &eqE, &eqG })
        {
            eq->setHighMidFreq (2000.0f);
            eq->setHighMidGainDb (-12.0f);
            eq->setHighMidQ (1.0f);
        }
        const auto offCenterCutE = eqE.getHighMidMagnitudeResponseDb (4000.0);
        const auto offCenterCutG = eqG.getHighMidMagnitudeResponseDb (4000.0);
        check (offCenterCutE > offCenterCutG + 0.5f,
               "E recovers faster than G one octave above center at -12dB cut: E=" + juce::String (offCenterCutE, 2)
               + "dB, G=" + juce::String (offCenterCutG, 2) + "dB");
    }

    void testNoNansAtExtremes (double sampleRate)
    {
        std::cout << "\n[Safety] No NaN/Inf across extreme parameter combinations\n";

        NovaDeskEQBands<float> eq;
        eq.prepare (makeSpec (sampleRate, 2));
        eq.setHpfFreqIndex (5);
        eq.setLpfFreqIndex (5);
        eq.setLowFreq (300.0f); eq.setLowGainDb (15.0f); eq.setLowType (BandType::Bell);
        eq.setLowMidFreq (2000.0f); eq.setLowMidGainDb (-15.0f); eq.setLowMidQ (4.0f);
        eq.setHighMidFreq (600.0f); eq.setHighMidGainDb (15.0f); eq.setHighMidQ (0.3f);
        eq.setHighFreq (20000.0f); eq.setHighGainDb (-15.0f); eq.setHighType (BandType::Shelf);

        juce::AudioBuffer<float> buffer (2, 512);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < 512; ++i)
                w[i] = (i % 7 == 0) ? 1.0f : -0.3f; // harsh, non-sinusoidal stress input
        }

        eq.process (buffer);

        bool finite = true;
        for (int ch = 0; ch < 2 && finite; ++ch)
        {
            const auto* r = buffer.getReadPointer (ch);
            for (int i = 0; i < 512 && finite; ++i)
                if (! std::isfinite (r[i])) finite = false;
        }
        check (finite, "Output stays finite at maximum HPF/LPF + all bands at +-15dB extremes");
    }

    // Rapid HPF-In / LPF-In / E<->G toggling mid-stream must not produce a
    // hard sample-domain discontinuity (gap-closure pass item 2/click-
    // safety). A steady sine's own natural per-sample delta at 200Hz,
    // 0.5 amplitude, 48kHz is ~0.013 - a genuine click (an unsmoothed
    // engage/disengage branch) would jump far above that, so a threshold
    // several times the natural delta cleanly discriminates a real click
    // from ordinary waveform motion.
    void testClickSafety (double sampleRate)
    {
        std::cout << "\n[Click-Safety] Rapid HPF-In / LPF-In / E<->G toggling produces no hard discontinuity\n";

        NovaDeskEQBands<float> eq;
        eq.prepare (makeSpec (sampleRate));
        eq.setHpfFreqIndex (3); // 100Hz
        eq.setHpfIn (false);
        eq.setLpfFreqIndex (3); // 12kHz
        eq.setLpfIn (false);
        eq.setPersonality (EQPersonality::E);
        eq.setHighMidFreq (2000.0f);
        eq.setHighMidGainDb (10.0f); // give E<->G something real to affect

        const double toneFreq = 200.0;
        const int blockSize = 256;
        const int numBlocks = 40;

        std::vector<float> fullOutput;
        fullOutput.reserve ((size_t) (blockSize * numBlocks));

        int sampleCounter = 0;
        for (int b = 0; b < numBlocks; ++b)
        {
            if (b == 10) eq.setHpfIn (true);
            if (b == 20) eq.setLpfIn (true);
            if (b == 30) eq.setPersonality (EQPersonality::G);

            juce::AudioBuffer<float> buffer (1, blockSize);
            auto* w = buffer.getWritePointer (0);
            for (int i = 0; i < blockSize; ++i)
            {
                w[i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * toneFreq * (double) sampleCounter / sampleRate);
                ++sampleCounter;
            }
            eq.process (buffer);
            for (int i = 0; i < blockSize; ++i)
                fullOutput.push_back (buffer.getSample (0, i));
        }

        float maxJump = 0.0f;
        for (size_t i = 1; i < fullOutput.size(); ++i)
            maxJump = std::max (maxJump, std::abs (fullOutput[i] - fullOutput[i - 1]));

        std::cout << "    max inter-sample jump across all three toggles: " << maxJump << "\n";
        check (maxJump < 0.05f, "no hard discontinuity (>0.05 sample-to-sample jump) across HPF-In/LPF-In/E-G toggles, got " + juce::String (maxJump, 4));
    }

    // Proves the double-precision instantiation is genuinely functional
    // (not just compiling) - same shape as testBandGainAtCenter's Bell
    // check, run through NovaDeskEQBands<double> end to end.
    void testDoublePrecisionInstantiation (double sampleRate)
    {
        std::cout << "\n[Double] NovaDeskEQBands<double> processes correctly at native double precision\n";

        NovaDeskEQBands<double> eq;
        eq.prepare (makeSpec (sampleRate, 2));
        eq.setHpfFreqIndex (0);
        eq.setLpfFreqIndex (0);
        eq.setLowFreq (100.0f);
        eq.setLowGainDb (6.0f);
        eq.setLowType (BandType::Bell);
        check (std::abs (eq.getLowMagnitudeResponseDb (100.0) - 6.0f) < 0.3f, "double-precision Low (Bell) +6dB at 100Hz matches the float result");

        juce::AudioBuffer<double> buffer (2, 512);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < 512; ++i)
                w[i] = 0.4 * std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (double) i / sampleRate);
        }
        eq.process (buffer);

        bool finite = true;
        for (int ch = 0; ch < 2 && finite; ++ch)
        {
            const auto* r = buffer.getReadPointer (ch);
            for (int i = 0; i < 512 && finite; ++i)
                if (! std::isfinite (r[i])) finite = false;
        }
        check (finite, "double-precision process() produces finite output on a real buffer");
    }
}

int main()
{
    std::cout << "=== NovaDeskEQBands Validation ===\n";

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::cout << "\n\n----- Sample rate: " << sr << "Hz -----\n";
        testHpfLpfCorners (sr);
        testBandGainAtCenter (sr);
        testShelfVsBellShape (sr);
        testEqPersonalityIsReal (sr);
        testNoNansAtExtremes (sr);
    }

    testClickSafety (48000.0);
    testDoublePrecisionInstantiation (48000.0);

    std::cout << "\n\n=== " << (failures == 0 ? "ALL PASS" : juce::String (failures) + " FAILURE(S)") << " ===\n";
    return failures == 0 ? 0 : 1;
}
