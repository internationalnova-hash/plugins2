#include "DSP/NovaTapeHeadFilter.h"
#include "NovaTapeTypes.h"
#include <juce_dsp/juce_dsp.h>
#include <iostream>
#include <cmath>

using namespace nova::tape;

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    // Measures the filter's steady-state gain (in dB) at a given frequency
    // by feeding a settled sine and comparing input/output RMS.
    float measureGainDb (NovaTapeHeadFilter& filt, double sr, float freqHz)
    {
        const int settle = (int) (sr * 0.05);
        const int measure = (int) (sr * 0.05);
        double phase = 0.0;
        double sumInSq = 0.0, sumOutSq = 0.0;

        for (int i = 0; i < settle; ++i)
        {
            const float x = std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * freqHz / sr;
            filt.processL (x);
        }
        for (int i = 0; i < measure; ++i)
        {
            const float x = std::sin (phase);
            phase += juce::MathConstants<double>::twoPi * freqHz / sr;
            const float y = filt.processL (x);
            sumInSq += (double) x * x;
            sumOutSq += (double) y * y;
        }
        const double rmsIn = std::sqrt (sumInSq / measure);
        const double rmsOut = std::sqrt (sumOutSq / measure);
        return (float) (20.0 * std::log10 (juce::jmax (1.0e-9, rmsOut / juce::jmax (1.0e-9, rmsIn))));
    }
}

int main()
{
    constexpr double sr = 48000.0;

    std::cout << "\n[HeadFilter] Head bump strength decreases as tape speed increases (7.5 > 15 > 30 IPS)\n";
    {
        auto bumpGainFor = [&] (IpsSpeed speed) -> float
        {
            NovaTapeHeadFilter filt;
            filt.prepare (sr);
            const auto typeProfile = getTapeTypeProfile (TapeType::Studio);
            filt.setParams (speed, typeProfile);
            return measureGainDb (filt, sr, getIpsProfile (speed).headBumpFreqHz);
        };

        const float g75 = bumpGainFor (IpsSpeed::Speed7_5);
        const float g15 = bumpGainFor (IpsSpeed::Speed15);
        const float g30 = bumpGainFor (IpsSpeed::Speed30);
        std::cout << "  (info) bump gain dB: 7.5=" << g75 << " 15=" << g15 << " 30=" << g30 << "\n";

        check (g75 > g15, "7.5 IPS head bump is stronger than 15 IPS");
        check (g15 > g30, "15 IPS head bump is stronger than 30 IPS");
    }

    std::cout << "\n[HeadFilter] HF rolloff is darkest at 7.5 IPS, most open at 30 IPS\n";
    {
        auto gainAt16k = [&] (IpsSpeed speed) -> float
        {
            NovaTapeHeadFilter filt;
            filt.prepare (sr);
            const auto typeProfile = getTapeTypeProfile (TapeType::Studio);
            filt.setParams (speed, typeProfile);
            return measureGainDb (filt, sr, 16000.0f);
        };

        const float g75 = gainAt16k (IpsSpeed::Speed7_5);
        const float g15 = gainAt16k (IpsSpeed::Speed15);
        const float g30 = gainAt16k (IpsSpeed::Speed30);
        std::cout << "  (info) gain @16kHz dB: 7.5=" << g75 << " 15=" << g15 << " 30=" << g30 << "\n";

        check (g75 < g15, "7.5 IPS rolls off more at 16kHz than 15 IPS");
        check (g15 < g30, "15 IPS rolls off more at 16kHz than 30 IPS");
    }

    std::cout << "\n[HeadFilter] TapeType adds further shaping on top of the IPS baseline (Vintage darker/bumpier than Studio at the same speed)\n";
    {
        NovaTapeHeadFilter studioFilt, vintageFilt;
        studioFilt.prepare (sr);
        vintageFilt.prepare (sr);
        studioFilt.setParams (IpsSpeed::Speed15, getTapeTypeProfile (TapeType::Studio));
        vintageFilt.setParams (IpsSpeed::Speed15, getTapeTypeProfile (TapeType::Vintage));

        const float studioBump = measureGainDb (studioFilt, sr, getIpsProfile (IpsSpeed::Speed15).headBumpFreqHz);
        const float vintageBump = measureGainDb (vintageFilt, sr, getIpsProfile (IpsSpeed::Speed15).headBumpFreqHz);
        std::cout << "  (info) bump gain dB @15ips: Studio=" << studioBump << " Vintage=" << vintageBump << "\n";
        check (vintageBump > studioBump, "Vintage's head bump is stronger than Studio's at the same IPS");

        NovaTapeHeadFilter studioFilt2, vintageFilt2;
        studioFilt2.prepare (sr); vintageFilt2.prepare (sr);
        studioFilt2.setParams (IpsSpeed::Speed15, getTapeTypeProfile (TapeType::Studio));
        vintageFilt2.setParams (IpsSpeed::Speed15, getTapeTypeProfile (TapeType::Vintage));
        const float studioHf = measureGainDb (studioFilt2, sr, 16000.0f);
        const float vintageHf = measureGainDb (vintageFilt2, sr, 16000.0f);
        std::cout << "  (info) gain @16kHz @15ips: Studio=" << studioHf << " Vintage=" << vintageHf << "\n";
        check (vintageHf < studioHf, "Vintage rolls off more HF than Studio at the same IPS");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL HEAD FILTER VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
