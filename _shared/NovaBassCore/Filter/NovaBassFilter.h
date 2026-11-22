#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nova::bass
{

// Thin wrapper around juce::dsp::StateVariableTPTFilter - the catalog's
// standard filter (used identically in Nova Console, Space By Nova, and
// Nova Keys' own NovaSynthVoice). No need to reinvent this; the wrapper
// exists only to give the voice engine a single mono-sample call shape and
// a low-pass/high-pass toggle matching NovaSynthVoiceParams' convention.
class NovaBassFilter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept
    {
        filter.prepare (spec);
    }

    void reset() noexcept
    {
        filter.reset();
    }

    void setHighPass (bool shouldBeHighPass) noexcept
    {
        filter.setType (shouldBeHighPass ? juce::dsp::StateVariableTPTFilterType::highpass
                                          : juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    void setCutoffHz (float cutoffHz) noexcept
    {
        filter.setCutoffFrequency (juce::jlimit (20.0f, 20000.0f, cutoffHz));
    }

    void setResonance (float q) noexcept
    {
        filter.setResonance (juce::jlimit (0.1f, 10.0f, q));
    }

    float processSample (float input) noexcept
    {
        return filter.processSample (0, input);
    }

private:
    juce::dsp::StateVariableTPTFilter<float> filter;
};

} // namespace nova::bass
