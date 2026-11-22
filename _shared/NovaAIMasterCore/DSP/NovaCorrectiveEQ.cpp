#include "NovaCorrectiveEQ.h"
#include <cmath>

namespace
{
    void designLowShelf (double fs, double f0, double gainDb, double q,
                         float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw0 = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double sqrtA = std::sqrt (A);

        const double bb0 =    A * ( (A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha );
        const double bb1 =  2 * A * ( (A - 1) - (A + 1) * cosw0 );
        const double bb2 =    A * ( (A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha );
        const double aa0 =        (A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha;
        const double aa1 =   -2 * ( (A - 1) + (A + 1) * cosw0 );
        const double aa2 =        (A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha;

        b0 = (float) (bb0 / aa0); b1 = (float) (bb1 / aa0); b2 = (float) (bb2 / aa0);
        a1 = (float) (aa1 / aa0); a2 = (float) (aa2 / aa0);
    }

    void designHighShelf (double fs, double f0, double gainDb, double q,
                          float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw0 = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double sqrtA = std::sqrt (A);

        const double bb0 =    A * ( (A + 1) + (A - 1) * cosw0 + 2 * sqrtA * alpha );
        const double bb1 = -2 * A * ( (A - 1) + (A + 1) * cosw0 );
        const double bb2 =    A * ( (A + 1) + (A - 1) * cosw0 - 2 * sqrtA * alpha );
        const double aa0 =        (A + 1) - (A - 1) * cosw0 + 2 * sqrtA * alpha;
        const double aa1 =    2 * ( (A - 1) - (A + 1) * cosw0 );
        const double aa2 =        (A + 1) - (A - 1) * cosw0 - 2 * sqrtA * alpha;

        b0 = (float) (bb0 / aa0); b1 = (float) (bb1 / aa0); b2 = (float) (bb2 / aa0);
        a1 = (float) (aa1 / aa0); a2 = (float) (aa2 / aa0);
    }

    void designPeak (double fs, double f0, double gainDb, double q,
                     float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw0 = std::cos (w0);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double alpha = std::sin (w0) / (2.0 * q);

        const double bb0 = 1.0 + alpha * A;
        const double bb1 = -2.0 * cosw0;
        const double bb2 = 1.0 - alpha * A;
        const double aa0 = 1.0 + alpha / A;
        const double aa1 = -2.0 * cosw0;
        const double aa2 = 1.0 - alpha / A;

        b0 = (float) (bb0 / aa0); b1 = (float) (bb1 / aa0); b2 = (float) (bb2 / aa0);
        a1 = (float) (aa1 / aa0); a2 = (float) (aa2 / aa0);
    }

    constexpr double kShelfQ = 0.707;
    constexpr double kBellQ  = 1.0;
}

void NovaCorrectiveEQ::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reset();
    setPlan (plan); // recompute coefficients for the new sample rate
}

void NovaCorrectiveEQ::reset() noexcept
{
    for (auto& f : filtersL) f.reset();
    for (auto& f : filtersR) f.reset();
}

void NovaCorrectiveEQ::setPlan (const NovaMasteringPlan::CorrectiveEQ& p) noexcept
{
    plan = p;
    if (sampleRate <= 0.0) return;

    float b0, b1, b2, a1, a2;

    designLowShelf (sampleRate, plan.lowShelfFreq, plan.lowShelfDb, kShelfQ, b0, b1, b2, a1, a2);
    filtersL[LowShelf] = { b0, b1, b2, a1, a2, filtersL[LowShelf].z1, filtersL[LowShelf].z2 };
    filtersR[LowShelf] = { b0, b1, b2, a1, a2, filtersR[LowShelf].z1, filtersR[LowShelf].z2 };

    designPeak (sampleRate, plan.lowMidFreq, plan.lowMidDb, kBellQ, b0, b1, b2, a1, a2);
    filtersL[LowMidBell] = { b0, b1, b2, a1, a2, filtersL[LowMidBell].z1, filtersL[LowMidBell].z2 };
    filtersR[LowMidBell] = { b0, b1, b2, a1, a2, filtersR[LowMidBell].z1, filtersR[LowMidBell].z2 };

    designPeak (sampleRate, plan.highMidFreq, plan.highMidDb, kBellQ, b0, b1, b2, a1, a2);
    filtersL[HighMidBell] = { b0, b1, b2, a1, a2, filtersL[HighMidBell].z1, filtersL[HighMidBell].z2 };
    filtersR[HighMidBell] = { b0, b1, b2, a1, a2, filtersR[HighMidBell].z1, filtersR[HighMidBell].z2 };

    designHighShelf (sampleRate, plan.highShelfFreq, plan.highShelfDb, kShelfQ, b0, b1, b2, a1, a2);
    filtersL[HighShelf] = { b0, b1, b2, a1, a2, filtersL[HighShelf].z1, filtersL[HighShelf].z2 };
    filtersR[HighShelf] = { b0, b1, b2, a1, a2, filtersR[HighShelf].z1, filtersR[HighShelf].z2 };
}

void NovaCorrectiveEQ::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! plan.active) return;

    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& filters = (ch == 0) ? filtersL : filtersR;
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < N; ++i)
        {
            float x = d[i];
            for (auto& f : filters)
                x = f.processSample (x);
            d[i] = x;
        }
    }
}
