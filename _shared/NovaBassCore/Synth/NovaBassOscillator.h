#pragma once

#include <juce_core/juce_core.h>

namespace nova::bass
{

// A single band-limited-enough sine/triangle oscillator for the 808/sub
// synth voice. Deliberately minimal - Nova Bass's spec calls for exactly
// sine, triangle, and a blend between them, nothing more (no wavetables,
// no unison). Phase is tracked in [0, 1) so frequency changes (from the
// pitch envelope or glide) never introduce a phase discontinuity.
class NovaBassOscillator
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
    }

    void reset() noexcept
    {
        phase = 0.0;
    }

    // frequencyHz may change every sample (pitch envelope / glide drive it).
    // sineTriBlend: 0 = pure sine, 1 = pure triangle.
    float getNextSample (float frequencyHz, float sineTriBlend) noexcept
    {
        const float sineValue = std::sin (phase * juce::MathConstants<double>::twoPi);

        // Triangle via a phase-folded ramp: cheap, no branches worse than a
        // couple of comparisons, adequate band-limiting for sub-bass content
        // (triangle's harmonic content rolls off fast enough at these
        // fundamentals that aliasing is not a practical concern here).
        const double t = phase; // 0..1
        const float triangleValue = (float) (t < 0.5 ? (-1.0 + 4.0 * t)
                                                       : (3.0 - 4.0 * t));

        const float sample = sineValue + (triangleValue - sineValue) * juce::jlimit (0.0f, 1.0f, sineTriBlend);

        if (sampleRate > 0.0)
        {
            phase += frequencyHz / sampleRate;
            if (phase >= 1.0)
                phase -= std::floor (phase);
            else if (phase < 0.0)
                phase += std::ceil (-phase);
        }

        return sample;
    }

    double getPhase() const noexcept { return phase; }
    void setPhase (double newPhase) noexcept { phase = newPhase - std::floor (newPhase); }

private:
    double sampleRate = 44100.0;
    double phase = 0.0;
};

} // namespace nova::bass
