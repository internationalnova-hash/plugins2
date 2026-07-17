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
    gainReductionDb.store (0.0f);
    outputPeak.store (0.0f);
    outputIsHot.store (false);
}

void NovaLevelDSP::setParameters (const NovaLevelParameters& p)
{
    params = p;
}

void NovaLevelDSP::process (juce::AudioBuffer<float>& buffer)
{
    const int totalChannels = buffer.getNumChannels();
    const int numSamples    = buffer.getNumSamples();

    if (totalChannels == 0 || numSamples == 0)
        return;

    // Mode presets — identical to original Nova Level behaviour
    float thresholdDb = -18.0f;
    float ratioMax    =   4.0f;
    float attackMs    =  20.0f;
    float releaseMs   = 140.0f;

    if (params.mode == 1) // Punch
    {
        thresholdDb = -16.0f;
        ratioMax    =   6.0f;
        attackMs    =   8.0f;
        releaseMs   =  90.0f;
    }
    else if (params.mode == 2) // Limit
    {
        thresholdDb = -12.0f;
        ratioMax    =  10.0f;
        attackMs    =   2.0f;
        releaseMs   =  55.0f;
    }

    const float amount  = juce::jlimit (0.0f, 1.0f, params.compressionAmount);
    const float ratio   = 1.0f + (ratioMax - 1.0f) * amount;

    const float sr = static_cast<float> (sampleRate);
    const float attackCoeff  = std::exp (-1.0f / (0.001f * attackMs  * sr));
    const float releaseCoeff = std::exp (-1.0f / (0.001f * releaseMs * sr));

    const float outputGain = juce::Decibels::decibelsToGain (params.outputDb);
    const float wet        = juce::jlimit (0.0f, 1.0f, params.mix);
    const float dry        = 1.0f - wet;

    float blockPeak  = 0.0f;
    float blockMaxGr = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Linked peak detector across all channels
        float detector = 0.0f;
        for (int ch = 0; ch < totalChannels; ++ch)
            detector = juce::jmax (detector, std::abs (buffer.getSample (ch, sample)));

        const float inputDb    = juce::Decibels::gainToDecibels (juce::jmax (detector, 1.0e-6f));
        const float overDb     = juce::jmax (0.0f, inputDb - thresholdDb);
        const float targetGrDb = overDb * (1.0f - 1.0f / ratio);

        const float coeff = targetGrDb > grEnvelopeDb ? attackCoeff : releaseCoeff;
        grEnvelopeDb = coeff * grEnvelopeDb + (1.0f - coeff) * targetGrDb;
        blockMaxGr = juce::jmax (blockMaxGr, grEnvelopeDb);

        const float sampleGain = juce::Decibels::decibelsToGain (-grEnvelopeDb) * outputGain;

        for (int ch = 0; ch < totalChannels; ++ch)
        {
            const float in = buffer.getSample (ch, sample);
            float y = in * sampleGain;

            if (params.magic)
                y = std::tanh (1.25f * y);

            // Parallel compression blend
            const float out = wet * y + dry * in * outputGain;
            buffer.setSample (ch, sample, out);
            blockPeak = juce::jmax (blockPeak, std::abs (out));
        }
    }

    gainReductionDb.store (blockMaxGr);
    outputPeak.store (blockPeak);
    outputIsHot.store (blockPeak > 0.98f);
}
