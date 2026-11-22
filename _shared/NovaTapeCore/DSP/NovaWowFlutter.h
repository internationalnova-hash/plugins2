#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>

// Tape transport pitch/time instability - a short modulated delay line
// driven by two summed slow ("wow") sines at different, non-harmonically-
// related rates (avoiding a perfectly periodic wobble) plus a faster
// ("flutter") component. Self-contained and tape-specific; not shared
// with or derived from any other plugin's modulation source.
namespace nova::tape
{
    class NovaWowFlutter
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate;
            maxDelaySamples = (int) std::ceil (0.006 * sr) + 4; // 6ms max, generous headroom
            bufL.assign ((size_t) maxDelaySamples, 0.0f);
            bufR.assign ((size_t) maxDelaySamples, 0.0f);
            writePos = 0;
            reset();
        }

        void reset() noexcept
        {
            std::fill (bufL.begin(), bufL.end(), 0.0f);
            std::fill (bufR.begin(), bufR.end(), 0.0f);
            writePos = 0;
            phaseWow1 = phaseWow2 = phaseFlutter = 0.0;
        }

        // amount: 0-10 from the WOW/FLUTTER knob.
        void setAmount (float amount0to10) noexcept
        {
            depthSamples = juce::jlimit (0.0f, 10.0f, amount0to10) / 10.0f * maxDepthSamples;
        }

        void processBlockInPlace (float* dataL, float* dataR, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
            {
                bufL[(size_t) writePos] = dataL[i];
                bufR[(size_t) writePos] = dataR[i];

                phaseWow1 += wow1Hz / sr;
                phaseWow2 += wow2Hz / sr;
                phaseFlutter += flutterHz / sr;
                if (phaseWow1 >= 1.0) phaseWow1 -= 1.0;
                if (phaseWow2 >= 1.0) phaseWow2 -= 1.0;
                if (phaseFlutter >= 1.0) phaseFlutter -= 1.0;

                const float wow = 0.6f * std::sin (juce::MathConstants<float>::twoPi * (float) phaseWow1)
                                 + 0.4f * std::sin (juce::MathConstants<float>::twoPi * (float) phaseWow2);
                const float flutter = 0.35f * std::sin (juce::MathConstants<float>::twoPi * (float) phaseFlutter);
                const float modulation = (wow * 0.7f + flutter * 0.3f) * depthSamples;

                const float readPos = (float) baseDelaySamples + modulation;
                dataL[i] = readInterpolated (bufL, readPos);
                dataR[i] = readInterpolated (bufR, readPos);

                writePos = (writePos + 1) % maxDelaySamples;
            }
        }

    private:
        float readInterpolated (const std::vector<float>& buf, float delaySamples) const noexcept
        {
            const float clamped = juce::jlimit (1.0f, (float) maxDelaySamples - 2.0f, delaySamples);
            const float readPosF = (float) writePos - clamped;
            const int i0 = wrapIndex ((int) std::floor (readPosF));
            const int i1 = wrapIndex (i0 + 1);
            const float frac = readPosF - std::floor (readPosF);
            return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
        }

        int wrapIndex (int idx) const noexcept
        {
            idx %= maxDelaySamples;
            if (idx < 0) idx += maxDelaySamples;
            return idx;
        }

        double sr = 44100.0;
        int maxDelaySamples = 300;
        int baseDelaySamples = 64; // center point the modulation swings around
        static constexpr float maxDepthSamples = 24.0f; // ~0.5ms at 48kHz

        std::vector<float> bufL, bufR;
        int writePos = 0;

        double phaseWow1 = 0.0, phaseWow2 = 0.0, phaseFlutter = 0.0;
        static constexpr double wow1Hz = 0.7;
        static constexpr double wow2Hz = 0.31;
        static constexpr double flutterHz = 7.3;

        float depthSamples = 0.0f;
    };
}
