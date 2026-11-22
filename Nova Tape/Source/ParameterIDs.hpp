#pragma once

// Single source of truth for Nova Tape's APVTS parameter-id strings, used
// by PluginProcessor, PluginEditor, and every validation harness - same
// convention as Nova Pitch/Nova Doubler's ParameterIDs.hpp.
namespace ParameterIDs
{
    static constexpr const char* input        = "input";        // dB, drives the tape model
    static constexpr const char* tapeAmount    = "tapeAmount";   // 0-100, primary tape intensity
    static constexpr const char* tapeType      = "tapeType";     // 0=Studio, 1=Vintage, 2=Hot
    static constexpr const char* ips           = "ips";          // 0=7.5, 1=15, 2=30
    static constexpr const char* bias          = "bias";         // -100..100, normalized around 0
    static constexpr const char* wowFlutter    = "wowFlutter";   // 0-10
    static constexpr const char* noise         = "noise";        // 0-10, 0=off
    static constexpr const char* lowCut        = "lowCut";       // Hz, 20-200
    static constexpr const char* highCut       = "highCut";      // Hz, 5000-20000
    static constexpr const char* mix           = "mix";          // 0-100, dry/wet
    static constexpr const char* output        = "output";       // dB
    static constexpr const char* trim          = "trim";         // dB, +-6, final monitor trim
    static constexpr const char* oversampling  = "oversampling"; // 0=Off, 1=2x, 2=4x
    static constexpr const char* bypass        = "bypass";
}
