#pragma once

#include <cmath>

// Shared scale/note-space types and hz<->midi helpers used across the
// pitch-detection, scale-quantization, and diagnostic layers. Degree tables
// are the same five scales Nova Pitch has always shipped - moved here
// verbatim so both NovaPitchDetector (search-range biasing) and
// NovaScaleQuantizer (snap targets) can share one definition instead of
// duplicating them.
namespace nova::pitch
{
    enum class Scale : int { Chromatic = 0, Major = 1, Minor = 2, Pentatonic = 3, Blues = 4 };

    // Returns the scale's degree table (pitch classes within an octave, 0=root)
    // and writes its length to numDegrees. Chromatic has no explicit table -
    // callers should treat it as "every semitone is in scale" rather than
    // call this.
    inline const int* getScaleDegrees (Scale scale, int& numDegrees) noexcept
    {
        static const int major[]      = { 0, 2, 4, 5, 7, 9, 11 };
        static const int minor[]      = { 0, 2, 3, 5, 7, 8, 10 };
        static const int pentatonic[] = { 0, 2, 4, 7, 9 };
        static const int blues[]      = { 0, 3, 5, 6, 7, 10 };

        switch (scale)
        {
            case Scale::Major:      numDegrees = 7; return major;
            case Scale::Minor:      numDegrees = 7; return minor;
            case Scale::Pentatonic: numDegrees = 5; return pentatonic;
            case Scale::Blues:      numDegrees = 6; return blues;
            case Scale::Chromatic:
            default:                numDegrees = 0; return nullptr;
        }
    }

    inline float hzToMidiF (float hz) noexcept { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
    inline float midiToHzF (float m)  noexcept { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }
}
