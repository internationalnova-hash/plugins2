#pragma once

#include <algorithm>
#include <cmath>

// Shared conversions for the Nova Punch DSP core. Kept minimal - most
// dB/linear work goes through juce::Decibels directly in the classes that
// need it; this only holds the couple of helpers used in more than one
// file.
namespace nova::punch
{
    inline float dbToGain (float db) noexcept
    {
        return std::pow (10.0f, db / 20.0f);
    }

    inline float gainToDb (float gain) noexcept
    {
        return 20.0f * std::log10 (std::max (gain, 1.0e-9f));
    }
}
