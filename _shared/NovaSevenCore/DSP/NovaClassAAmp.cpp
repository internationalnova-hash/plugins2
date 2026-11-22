#include "NovaClassAAmp.h"
#include <cmath>

namespace nova::seven
{

// Concrete mode table per NOVA_7EVEN_PLAN.md section 5.4. baseCurve isn't
// itself spec'd numerically (the plan locks driveMultiplier/asymmetry/
// sagAmount/transient-rounding-ms; baseCurve is this stage's own dial-in
// so Clean sits close to transparent and Hot reads as clearly, heavily
// colored - the plan's explicit intent for section 6's cross-check).
NovaClassAAmp::ModeProfile NovaClassAAmp::getModeProfile (NovaCharacterMode mode) noexcept
{
    switch (mode)
    {
        case NovaCharacterMode::Clean:   return { 0.55f, 0.90f, 0.05f, 0.10f, 2.0f };
        case NovaCharacterMode::Hot:     return { 1.70f, 2.00f, 0.32f, 0.65f, 18.0f };
        case NovaCharacterMode::Classic:
        default:                        return { 1.00f, 1.40f, 0.18f, 0.35f, 8.0f };
    }
}

void NovaClassAAmp::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    reset();
}

void NovaClassAAmp::reset() noexcept
{
    sagEnv = { 0.0f, 0.0f };
}

void NovaClassAAmp::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumSamples() == 0)
        return;

    const auto mode = getModeProfile (currentMode);

    // driveGain: preamp_gain (0-10, normalized 0-1) scaled by the mode's
    // own drive multiplier, times a fixed headroom-to-drive scale so full
    // preamp + Hot lands well into the tanh's compressive region without
    // needing separate auto-trim (plan section 5.8 covers output-level
    // compensation via output_trim/mix, this stage only shapes harmonics).
    const float driveGain = 1.0f + (preampGain010 / 10.0f) * mode.driveMultiplier * 3.0f;
    const float sagCoeff = 1.0f - std::exp (-1.0f / (mode.sagTimeMs * 0.001f * (float) sampleRate));

    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) sagEnv.size());

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        float& env = sagEnv[(size_t) channel];

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float x = samples[sample];
            const float driven = x * driveGain;

            env += (std::abs (driven) - env) * sagCoeff;

            const float k = mode.baseCurve + env * mode.sagAmount;
            const float cubic = driven * driven * driven;
            const float skewed = driven + mode.asymmetry * 0.3f * cubic;

            samples[sample] = driven >= 0.0f
                ? std::tanh (skewed * k)
                : std::tanh (skewed * juce::jmax (0.2f, k - mode.asymmetry * 0.4f));
        }
    }
}

} // namespace nova::seven
