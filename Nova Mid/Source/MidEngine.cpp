#include "MidEngine.h"

namespace nm
{
void Bell::setGainDb (double sampleRate, float gainDb) noexcept
{
    const double A     = std::pow (10.0, gainDb / 40.0);
    const double w0    = juce::MathConstants<double>::twoPi * kBellFreqHz / sampleRate;
    const double alpha = std::sin (w0) / (2.0 * kBellQ);
    const double cw    = std::cos (w0);
    const double a0    = 1.0 + alpha / A;

    b0 = (float) ((1.0 + alpha * A) / a0);
    b1 = (float) ((-2.0 * cw) / a0);
    b2 = (float) ((1.0 - alpha * A) / a0);
    a1 = (float) ((-2.0 * cw) / a0);
    a2 = (float) ((1.0 - alpha / A) / a0);
}

void MidEngine::prepare (double sr, int maxBlockSize, int numChannels)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    channels   = juce::jlimit (1, 2, numChannels);
    maxBlock   = juce::jmax (32, maxBlockSize);

    // 4x, linear-phase FIR half-band stages, integer latency so the host can compensate exactly.
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) channels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    latency = (int) std::lround (oversampler->getLatencyInSamples());

    pushTarget.reset (sampleRate, 0.05);
    ceilingGain.reset (sampleRate * 4.0, 0.02);
    outGain.reset (sampleRate, 0.02);
    pushTarget.setCurrentAndTargetValue (pushTarget.getTargetValue());
    ceilingGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (ceilingDbTarget));
    outGain.setCurrentAndTargetValue (outGain.getTargetValue() > 0.0f ? outGain.getTargetValue() : 1.0f);

    bellGainDb = -1000.0f; // force a coefficient update
    reset();
}

void MidEngine::reset()
{
    bell.reset();
    if (oversampler != nullptr)
        oversampler->reset();
    lastOverDb = 0.0f;
}

void MidEngine::process (float* const* data, int numChannels, int numSamples) noexcept
{
    if (oversampler == nullptr || numSamples <= 0)
        return;

    numChannels = juce::jmin (numChannels, channels);
    lastOverDb = 0.0f;

    for (int start = 0; start < numSamples; start += maxBlock)
        processChunk (data, numChannels, start, juce::jmin (maxBlock, numSamples - start));
}

void MidEngine::processChunk (float* const* data, int numChannels, int start, int n) noexcept
{
    // 1) Push: the midrange bell. Coefficients follow the smoothed gain every 32 samples.
    constexpr int sub = 32;
    for (int pos = 0; pos < n; pos += sub)
    {
        const int len = juce::jmin (sub, n - pos);
        const float pushDb = pushTarget.getCurrentValue();
        pushTarget.skip (len);

        if (std::abs (pushDb - bellGainDb) > 1.0e-3f)
        {
            bellGainDb = pushDb;
            bell.setGainDb (sampleRate, pushDb);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* x = data[ch] + start + pos;
            for (int i = 0; i < len; ++i)
                x[i] = bell.process (ch, std::isfinite (x[i]) ? x[i] : 0.0f); // a NaN would stick in the filter state
        }
    }

    // 2) Soft clip against the ceiling, at 4x so the clipping stays clean.
    juce::dsp::AudioBlock<float> block (data, (size_t) numChannels, (size_t) start, (size_t) n);
    auto up = oversampler->processSamplesUp (block);

    ceilingGain.setTargetValue (juce::Decibels::decibelsToGain (ceilingDbTarget));
    const float cStart = ceilingGain.getCurrentValue();
    float maxRatio = 0.0f;

    const auto upN = (int) up.getNumSamples();
    for (int i = 0; i < upN; ++i)
    {
        const float c = ceilingGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* s = up.getChannelPointer ((size_t) ch) + i;
            const float x = std::isfinite (*s) ? *s : 0.0f;
            maxRatio = juce::jmax (maxRatio, std::abs (x) / c);
            *s = softClip (x, c);
        }
    }

    oversampler->processSamplesDown (block);

    if (maxRatio > 1.0f)
        lastOverDb = juce::jmax (lastOverDb, juce::Decibels::gainToDecibels (maxRatio));

    // 3) The down-sampling filter can overshoot by a hair; hold the ceiling exactly,
    //    then output trim (<= 0 dB) and a NaN-safe final limit.
    const float cHold = juce::jmax (cStart, ceilingGain.getCurrentValue());
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float& s = data[ch][start + i];
            const float y = juce::jlimit (-cHold, cHold, s) * g;
            s = std::isfinite (y) ? juce::jlimit (-1.0f, 1.0f, y) : 0.0f;
        }
    }
}
} // namespace nm
