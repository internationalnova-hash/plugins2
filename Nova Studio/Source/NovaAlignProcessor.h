#pragma once

#include <JuceHeader.h>
#include "Session.h"

namespace NovaStudio
{
    class NovaAlignProcessor
    {
    public:
        struct AnchorPoint
        {
            int64_t samplePosition = 0;
            float strength = 0.0f;
        };

        struct WarpPoint
        {
            int64_t samplePosition = 0;
            int64_t shiftSamples = 0;
        };

        struct AlignSettings
        {
            float amount = 1.0f;
            float naturalness = 0.75f;
            float tightness = 0.5f;
            float phraseSensitivity = 0.75f;
            float consonantPriority = 0.5f;
            int maxShiftMs = 1000;
        };

        // Aligns exactly the part of `targetClip` that's on the timeline to whatever the
        // guide clip plays over the same stretch of the timeline. Both clips' trims,
        // positions and file sample rates are respected (audio is converted to
        // `sessionSampleRate`). The result covers the target clip's timeline range, so it
        // replaces the target in place: same start, same length, file offset 0.
        static bool alignTargetClip(const Clip& guideClip,
                                    const Clip& targetClip,
                                    double sessionSampleRate,
                                    const juce::File& destinationFile,
                                    const AlignSettings& settings,
                                    int64_t& outShiftSamples,
                                    juce::String* error = nullptr);
    };
}
