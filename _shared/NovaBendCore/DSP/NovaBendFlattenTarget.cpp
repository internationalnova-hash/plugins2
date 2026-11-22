#include "NovaBendFlattenTarget.h"
#include <cmath>

namespace nova::bend
{
    void NovaBendFlattenTarget::prepare (double sampleRate) noexcept
    {
        dt = 1.0 / juce::jmax (1.0, sampleRate);
        reset();
    }

    void NovaBendFlattenTarget::reset() noexcept
    {
        primed = false;
        smoothedMidi = (double) hzToMidiF (targetHz);
    }

    void NovaBendFlattenTarget::setTargetHz (float hz) noexcept
    {
        targetHz = juce::jmax (1.0f, hz);
    }

    float NovaBendFlattenTarget::process (float inputHz) noexcept
    {
        juce::ignoreUnused (inputHz); // the target is fixed regardless of input pitch - only voiced-gating (upstream) matters here

        if (! primed)
        {
            smoothedMidi = (double) hzToMidiF (targetHz);
            primed = true;
        }

        const double targetMidi = (double) hzToMidiF (targetHz);
        const double glideAlpha = 1.0 - std::exp (-dt / (double) kGlideTauSeconds);
        smoothedMidi += glideAlpha * (targetMidi - smoothedMidi);

        return midiToHzF ((float) smoothedMidi);
    }
}
