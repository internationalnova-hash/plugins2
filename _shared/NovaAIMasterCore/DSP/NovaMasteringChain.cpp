#include "NovaMasteringChain.h"

// ---- ChainStages ----

void NovaMasteringChain::ChainStages::prepare (const juce::dsp::ProcessSpec& spec)
{
    eq.prepare (spec);
    resonance.prepare (spec);
    glue.prepare (spec);
    saturation.prepare (spec);
    stereo.prepare (spec);
    clipper.prepare (spec);
    limiter.prepare (spec);
}

void NovaMasteringChain::ChainStages::reset() noexcept
{
    eq.reset();
    resonance.reset();
    glue.reset();
    saturation.reset();
    stereo.reset();
    clipper.reset();
    limiter.reset();
}

void NovaMasteringChain::ChainStages::setPlan (const NovaMasteringPlan& plan) noexcept
{
    eq.setPlan (plan.correctiveEQ);
    resonance.setPlan (plan.resonanceControl);
    glue.setPlan (plan.compression);
    saturation.setPlan (plan.saturation);
    stereo.setPlan (plan.stereo);
    clipper.setPlan (plan.clipping);
    limiter.setPlan (plan.limiting);
    drive = plan.output.applyGain ? juce::Decibels::decibelsToGain (juce::jlimit (-24.f, 24.f, plan.output.gainDb)) : 1.f;
}

void NovaMasteringChain::ChainStages::process (juce::AudioBuffer<float>& buffer) noexcept
{
    // Order per spec section 11: EQ -> Resonance -> Glue -> Saturation ->
    // Stereo -> Soft Clip -> Limiter.
    eq.process (buffer);
    resonance.process (buffer);
    glue.process (buffer);
    saturation.process (buffer);
    stereo.process (buffer);
    if (drive != 1.f) buffer.applyGain (drive);
    clipper.process (buffer);
    limiter.process (buffer);
}

// ---- NovaMasteringChain ----

void NovaMasteringChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    stagesA.prepare (spec);
    stagesB.prepare (spec);
    crossfadeTotalSamples = (int) (spec.sampleRate * 0.030); // 30ms transition
    crossfadeScratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize, false, true, true);
    reset();
}

void NovaMasteringChain::reset() noexcept
{
    stagesA.reset();
    stagesB.reset();
    usingA = true;
    hasAppliedFirstPlan = false;
    crossfadeSamplesRemaining = 0;
}

void NovaMasteringChain::setPlan (const NovaMasteringPlan& plan) noexcept
{
    auto& incoming = usingA ? stagesB : stagesA;
    incoming.setPlan (plan);

    if (! hasAppliedFirstPlan)
    {
        // No prior plan to crossfade from - apply immediately to both
        // slots so future transitions start from consistent state.
        auto& other = usingA ? stagesA : stagesB;
        other.setPlan (plan);
        hasAppliedFirstPlan = true;
        crossfadeSamplesRemaining = 0;
        return;
    }

    // the incoming chain's limiter has been idle: give it the current look-ahead audio
    incoming.limiter.continueFrom ((usingA ? stagesA : stagesB).limiter);
    crossfadeSamplesRemaining = crossfadeTotalSamples;
}

void NovaMasteringChain::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (crossfadeSamplesRemaining <= 0)
    {
        (usingA ? stagesA : stagesB).process (buffer);
        return;
    }

    const int nCh = buffer.getNumChannels();
    const int N   = buffer.getNumSamples();

    crossfadeScratch.setSize (nCh, N, false, false, true);
    crossfadeScratch.makeCopyOf (buffer, true);

    auto& oldChain      = usingA ? stagesA : stagesB;
    auto& incomingChain = usingA ? stagesB : stagesA;

    oldChain.process (crossfadeScratch);
    incomingChain.process (buffer); // buffer now holds the new chain's output

    for (int i = 0; i < N; ++i)
    {
        const float t = crossfadeTotalSamples > 0
            ? juce::jlimit (0.f, 1.f, 1.f - (float) crossfadeSamplesRemaining / (float) crossfadeTotalSamples)
            : 1.f;

        for (int ch = 0; ch < nCh; ++ch)
        {
            float* newData = buffer.getWritePointer (ch);
            const float* oldData = crossfadeScratch.getReadPointer (ch);
            newData[i] = oldData[i] * (1.f - t) + newData[i] * t;
        }

        if (crossfadeSamplesRemaining > 0) --crossfadeSamplesRemaining;
    }

    if (crossfadeSamplesRemaining <= 0)
        usingA = ! usingA;
}
