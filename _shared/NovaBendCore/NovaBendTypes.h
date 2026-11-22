#pragma once

#include <cmath>

// Shared hz<->midi<->ratio helpers and the NOTE+OCTAVE -> Hz mapping used by
// Flatten. A4=440 is centralized here (spec's own requirement) so an
// alternate reference tuning can be added in one place later if ever needed.
namespace nova::bend
{
    inline constexpr float kA4Hz = 440.0f;

    inline float hzToMidiF (float hz) noexcept { return 69.0f + 12.0f * std::log2 (hz / kA4Hz); }
    inline float midiToHzF (float m)  noexcept { return kA4Hz * std::pow (2.0f, (m - 69.0f) / 12.0f); }

    inline float semitoneToRatio (float semitones) noexcept { return std::pow (2.0f, semitones / 12.0f); }
    inline float ratioToSemitone (float ratio) noexcept { return 12.0f * std::log2 (ratio); }

    // noteIndex: 0=C, 1=C#, ... 11=B (matches the UI's NOTE dropdown order).
    // octave: the standard scientific-pitch-notation octave (octave 4
    // contains A4=440Hz). NOTE+OCTAVE -> MIDI: midi = (octave+1)*12 + noteIndex.
    inline float noteOctaveToHz (int noteIndex, int octave) noexcept
    {
        const int midi = (octave + 1) * 12 + noteIndex;
        return midiToHzF ((float) midi);
    }
}
