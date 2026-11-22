#pragma once

// Stable parameter ID strings for Nova Desk EQ. Per the build spec (section
// 60): "Assign stable parameter IDs before release. Do not casually change
// parameter IDs later." Locked here before any DSP is written.

namespace ParameterIDs
{
    // Input / Output stage
    static constexpr auto input       = "input";        // float, -24..24 dB
    static constexpr auto inputIn     = "input_in";      // bool, section IN switch
    static constexpr auto output      = "output";        // float, -24..24 dB
    static constexpr auto outputIn    = "output_in";      // bool, section IN switch

    // High-pass / low-pass stepped filters
    static constexpr auto hpfFreqIndex = "hpf_freq_index"; // choice: Off,20,50,100,200,400
    static constexpr auto hpfIn        = "hpf_in";          // bool
    static constexpr auto lpfFreqIndex = "lpf_freq_index"; // choice: Off,5k,8k,12k,16k,20k
    static constexpr auto lpfIn        = "lpf_in";          // bool

    // Low band: continuous freq + gain, Bell/Shelf type
    static constexpr auto lowFreq = "low_freq"; // float, 30-300 Hz
    static constexpr auto lowGain = "low_gain"; // float, -15..15 dB
    static constexpr auto lowType = "low_type"; // choice: Bell=0, Shelf=1

    // Low-Mid band: continuous freq + gain + Q, Bell only
    static constexpr auto lowMidFreq = "low_mid_freq"; // float, 100-2000 Hz
    static constexpr auto lowMidGain = "low_mid_gain"; // float, -15..15 dB
    static constexpr auto lowMidQ    = "low_mid_q";    // float, 0.3-4.0

    // High-Mid band: continuous freq + gain + Q, Bell only
    static constexpr auto highMidFreq = "high_mid_freq"; // float, 600-8000 Hz
    static constexpr auto highMidGain = "high_mid_gain"; // float, -15..15 dB
    static constexpr auto highMidQ    = "high_mid_q";    // float, 0.3-4.0

    // High band: continuous freq + gain, Bell/Shelf type
    static constexpr auto highFreq = "high_freq"; // float, 3000-20000 Hz
    static constexpr auto highGain = "high_gain"; // float, -15..15 dB
    static constexpr auto highType = "high_type"; // choice: Bell=0, Shelf=1

    // EQ personality
    static constexpr auto egMode = "eg_mode"; // choice: E=0 (bolder/faster), G=1 (smoother/broader)

    // Console drive + output blend
    static constexpr auto drive = "drive"; // float, 0-10
    static constexpr auto mix   = "mix";   // float, 0-100 %

    static constexpr auto bypass = "bypass"; // bool

    // Settings-menu only (not a visible front-panel control, per the
    // gap-closure spec). Choice: Off=0, 2x=1, 4x=2, default 2x.
    static constexpr auto oversamplingMode = "oversampling_mode";
}
