#pragma once

// Locked per NOVA_7EVEN_PLAN.md section 5.10 - stable once shipped
// (spec section 60).
namespace ParameterIDs
{
    inline constexpr char inputTrim[] = "input_trim";
    inline constexpr char preampGain[] = "preamp_gain";
    inline constexpr char characterMode[] = "character_mode";
    inline constexpr char hpfFreq[] = "hpf_freq";
    inline constexpr char lowFreq[] = "low_freq";
    inline constexpr char lowGain[] = "low_gain";
    inline constexpr char midFreq[] = "mid_freq";
    inline constexpr char midGain[] = "mid_gain";
    inline constexpr char highShelfGain[] = "high_shelf_gain";
    inline constexpr char mix[] = "mix";
    inline constexpr char outputTrim[] = "output_trim";
    inline constexpr char phaseInvert[] = "phase_invert";
    inline constexpr char bypass[] = "bypass";
    inline constexpr char oversamplingMode[] = "oversampling_mode"; // choice: Off=0, 2x=1, 4x=2
}
