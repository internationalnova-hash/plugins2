#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace nova::seven
{

// Stepped HPF/Low/Mid + fixed 12k high shelf (NOVA_7EVEN_PLAN.md section
// 5.4/5.5). Follows Nova Tone's exact pattern (plan section 3.5):
// AudioParameterChoice indices feed a static constexpr lookup table of
// real Hz values, resolved via getChoiceValue() before hitting the IIR
// coefficient functions.
class NovaSevenEQ
{
public:
    NovaSevenEQ() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    // 0 = Off, 1-5 = hpfFreqChoices[0..4].
    void setHpfFreqIndex (int index) noexcept { hpfIndex = juce::jlimit (0, 5, index); }
    void setLowFreqIndex (int index) noexcept { lowFreqIndex = juce::jlimit (0, 5, index); }
    void setLowGainDb (float db) noexcept { lowGainDb = db; }
    void setMidFreqIndex (int index) noexcept { midFreqIndex = juce::jlimit (0, 5, index); }
    void setMidGainDb (float db) noexcept { midGainDb = db; }
    void setHighShelfGainDb (float db) noexcept { highShelfGainDb = db; }

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    // index 0 is "Off" (no cutoff value - handled specially); indices 1-5
    // map to hpfFreqChoices[0..4].
    static constexpr std::array<float, 5> hpfFreqChoices { 20.0f, 50.0f, 80.0f, 160.0f, 300.0f };
    static constexpr std::array<float, 6> lowFreqChoices { 35.0f, 60.0f, 110.0f, 220.0f, 360.0f, 700.0f };
    static constexpr std::array<float, 6> midFreqChoices { 110.0f, 220.0f, 360.0f, 700.0f, 1600.0f, 3200.0f };
    static constexpr float highShelfFreqHz = 12000.0f;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, juce::dsp::IIR::Coefficients<float>>;

    void updateFilters() noexcept;

    juce::dsp::StateVariableTPTFilter<float> hpf;
    StereoFilter lowShelf;
    StereoFilter midBell;
    StereoFilter highShelf;

    double sampleRate = 44100.0;
    int hpfIndex = 0;
    int lowFreqIndex = 2;
    float lowGainDb = 0.0f;
    int midFreqIndex = 3;
    float midGainDb = 0.0f;
    float highShelfGainDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaSevenEQ)
};

} // namespace nova::seven
