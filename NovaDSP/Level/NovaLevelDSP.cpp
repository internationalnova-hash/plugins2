#include "NovaLevelDSP.h"
#include <cmath>

void NovaLevelDSP::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = juce::jmax (1.0, spec.sampleRate);
    reset();
}

void NovaLevelDSP::reset()
{
    grEnvelopeDb = 0.0f;
    gainReductionDbAtomic.store (0.0f, std::memory_order_relaxed);
    outputPeak.store   (0.0f,  std::memory_order_relaxed);
    outputIsHot.store  (false, std::memory_order_relaxed);
}

void NovaLevelDSP::setParameters (const NovaLevelParameters& parameters) noexcept
{
    params = parameters;
}

void NovaLevelDSP::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int totalChannels = buffer.getNumChannels();
    const int numSamples    = buffer.getNumSamples();

    if (totalChannels == 0 || numSamples == 0)
        return;

    // -----------------------------------------------------------------------
    // Mode presets — identical to original Nova Level behaviour
    // -----------------------------------------------------------------------
    float thresholdDb = -18.0f;
    float ratioMax    =   4.0f;
    float attackMs    =  20.0f;
    float releaseMs   = 140.0f;

    if (params.mode == 1) // PUNCH
    {
        thresholdDb = -16.0f;
        ratioMax    =   6.0f;
        attackMs    =   8.0f;
        releaseMs   =  90.0f;
    }
    else if (params.mode == 2) // LIMIT
    {
        thresholdDb = -12.0f;
        ratioMax    =  10.0f;
        attackMs    =   2.0f;
        releaseMs   =  55.0f;
    }

    // Scale compression amount exactly as original: compression / 10
    const float amount  = juce::jlimit (0.0f, 10.0f, params.compression) / 10.0f;
    const float ratio   = 1.0f + (ratioMax - 1.0f) * amount;

    const float sr           = static_cast<float> (sampleRate);
    const float attackCoeff  = std::exp (-1.0f / (0.001f * attackMs  * sr));
    const float releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * sr));

    const float outputGain  = juce::Decibels::decibelsToGain (params.outputDb);
    const bool  magicActive = params.magic > 0.5f;

    float blockPeak  = 0.0f;
    float blockMaxGr = 0.0f;

    // -----------------------------------------------------------------------
    // Per-sample loop — exact algorithm from original Nova Level processBlock
    // -----------------------------------------------------------------------
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Linked peak detector across all channels (matches original)
        float detector = 0.0f;
        for (int ch = 0; ch < totalChannels; ++ch)
            detector = juce::jmax (detector, std::abs (buffer.getSample (ch, sample)));

        const float inputDb    = juce::Decibels::gainToDecibels (juce::jmax (detector, 1.0e-6f));
        const float overDb     = juce::jmax (0.0f, inputDb - thresholdDb);
        const float targetGrDb = overDb * (1.0f - 1.0f / ratio);

        const float coeff = targetGrDb > grEnvelopeDb ? attackCoeff : releaseCoeff;
        grEnvelopeDb = coeff * grEnvelopeDb + (1.0f - coeff) * targetGrDb;
        blockMaxGr   = juce::jmax (blockMaxGr, grEnvelopeDb);

        const float sampleGain = juce::Decibels::decibelsToGain (-grEnvelopeDb) * outputGain;

        for (int ch = 0; ch < totalChannels; ++ch)
        {
            float y = buffer.getSample (ch, sample) * sampleGain;
            if (magicActive)
                y = std::tanh (1.25f * y);  // exact original magic
            buffer.setSample (ch, sample, y);
            blockPeak = juce::jmax (blockPeak, std::abs (y));
        }
    }

    gainReductionDbAtomic.store (blockMaxGr, std::memory_order_relaxed);
    outputPeak.store   (blockPeak,          std::memory_order_relaxed);
    outputIsHot.store  (blockPeak > 0.98f,  std::memory_order_relaxed);
}

float NovaLevelDSP::getGainReductionDb() const noexcept
{
    return gainReductionDbAtomic.load (std::memory_order_relaxed);
}
