#include "NovaStereoAnalyzer.h"
#include <cmath>

void NovaStereoAnalyzer::prepare (double sr)
{
    sampleRate = sr;
    // One-pole lowpass, ~150Hz cutoff: coeff = exp(-2*pi*fc/fs).
    lpCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 150.0 / sr);
    reset();
}

void NovaStereoAnalyzer::reset() noexcept
{
    sumLR = sumLL = sumRR = 0.0;
    sumMidSq = sumSideSq = 0.0;
    lpStateL = lpStateR = 0.f;
    sumLowMidSq = sumLowSideSq = 0.0;
    correlation      = 1.f;
    width             = 0.f;
    lowFrequencyWidth = 0.f;
    monoDetected      = false;
}

void NovaStereoAnalyzer::process (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();
    if (N == 0) return;

    if (nCh < 2)
    {
        monoDetected = true;
        correlation  = 1.f;
        width         = 0.f;
        return;
    }

    // Correlation: Pearson formula, ported from Nova Master
    // (PluginProcessor.cpp:538-550) / Nova Translate (identical).
    // Width: mid/side RMS ratio - side energy relative to mid energy,
    // computed both full-band and on a ~150Hz-lowpassed copy for
    // low-frequency width specifically.
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (1);

    for (int i = 0; i < N; ++i)
    {
        const double lv = l[i], rv = r[i];
        sumLR += lv * rv;
        sumLL += lv * lv;
        sumRR += rv * rv;

        const double mid  = (lv + rv) * 0.5;
        const double side = (lv - rv) * 0.5;
        sumMidSq  += mid * mid;
        sumSideSq += side * side;

        lpStateL = lpCoeff * lpStateL + (1.f - lpCoeff) * l[i];
        lpStateR = lpCoeff * lpStateR + (1.f - lpCoeff) * r[i];
        const double lowMid  = (lpStateL + lpStateR) * 0.5;
        const double lowSide = (lpStateL - lpStateR) * 0.5;
        sumLowMidSq  += lowMid * lowMid;
        sumLowSideSq += lowSide * lowSide;
    }

    const double denom = std::sqrt (sumLL * sumRR);
    correlation = denom > 1e-9 ? juce::jlimit (-1.f, 1.f, (float) (sumLR / denom)) : 1.f;

    // side/(mid+side) rather than side/mid: bounded 0 (mono, side=0) to 1
    // (fully out-of-phase, mid=0) - the side/mid ratio alone is undefined
    // right at the "maximally wide" case (mid -> 0), the opposite of what
    // a raw division would suggest.
    const double midRms  = std::sqrt (sumMidSq);
    const double sideRms = std::sqrt (sumSideSq);
    const double widthDenom = midRms + sideRms;
    width = widthDenom > 1e-9 ? juce::jlimit (0.f, 1.f, (float) (sideRms / widthDenom)) : 0.f;

    const double lowMidRms  = std::sqrt (sumLowMidSq);
    const double lowSideRms = std::sqrt (sumLowSideSq);
    const double lowWidthDenom = lowMidRms + lowSideRms;
    lowFrequencyWidth = lowWidthDenom > 1e-9 ? juce::jlimit (0.f, 1.f, (float) (lowSideRms / lowWidthDenom)) : 0.f;
}
