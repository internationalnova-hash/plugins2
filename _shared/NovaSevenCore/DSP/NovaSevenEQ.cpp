#include "NovaSevenEQ.h"

namespace nova::seven
{

namespace
{
    template <size_t N>
    float getChoiceValue (const std::array<float, N>& values, int index) noexcept
    {
        return values[(size_t) juce::jlimit (0, (int) N - 1, index)];
    }
}

void NovaSevenEQ::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    hpf.prepare (spec);
    hpf.setType (juce::dsp::StateVariableTPTFilter<float>::Type::highpass);

    lowShelf.prepare (spec);
    midBell.prepare (spec);
    highShelf.prepare (spec);

    reset();
    updateFilters();
}

void NovaSevenEQ::reset() noexcept
{
    hpf.reset();
    lowShelf.reset();
    midBell.reset();
    highShelf.reset();
}

void NovaSevenEQ::updateFilters() noexcept
{
    if (hpfIndex > 0)
    {
        const float hpfFreq = getChoiceValue (hpfFreqChoices, hpfIndex - 1);
        hpf.setCutoffFrequency (hpfFreq);
        hpf.setResonance (0.7071068f); // Butterworth Q - no resonant bump, plan section 5.5
    }

    const float lowFreq = getChoiceValue (lowFreqChoices, lowFreqIndex);
    *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, lowFreq, 0.6f, juce::Decibels::decibelsToGain (lowGainDb));

    const float midFreq = getChoiceValue (midFreqChoices, midFreqIndex);
    *midBell.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, midFreq, 0.8f, juce::Decibels::decibelsToGain (midGainDb));

    *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, highShelfFreqHz, 0.6f, juce::Decibels::decibelsToGain (highShelfGainDb));
}

void NovaSevenEQ::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumSamples() == 0)
        return;

    updateFilters();

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    if (hpfIndex > 0)
        hpf.process (context);

    lowShelf.process (context);
    midBell.process (context);
    highShelf.process (context);
}

} // namespace nova::seven
