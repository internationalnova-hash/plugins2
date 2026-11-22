#pragma once
// Single source of truth for every parameter ID string, used by
// PluginProcessor, PluginEditor, and every validation harness.
namespace ParameterIDs
{
    static constexpr const char* input = "input";
    static constexpr const char* hrmDrive = "hrmDrive";
    static constexpr const char* acThreshold = "acThreshold";
    static constexpr const char* dcThreshold = "dcThreshold";
    static constexpr const char* timeConstant = "timeConstant"; // choice, 1-6
    static constexpr const char* output = "output";
    static constexpr const char* mix = "mix";
    static constexpr const char* stereoMode = "stereoMode"; // choice: Stereo, M/S
    static constexpr const char* link = "link";
    static constexpr const char* sidechainHpf = "sidechainHpf";
    static constexpr const char* sidechainListen = "sidechainListen";
    static constexpr const char* weight = "weight";
    static constexpr const char* tubeSaturation = "tubeSaturation";
    static constexpr const char* transformer = "transformer";
    static constexpr const char* outputTrim = "outputTrim";
    static constexpr const char* autoGain = "autoGain";
    static constexpr const char* oversampling = "oversampling"; // choice: Off, 2x, 4x, 8x
    static constexpr const char* analogNoise = "analogNoise";
    static constexpr const char* bypass = "bypass";
}
