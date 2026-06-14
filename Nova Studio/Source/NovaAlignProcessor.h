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

        static bool alignTargetClip(const Clip& guideClip,
                                    const Clip& targetClip,
                                    const juce::File& destinationFile,
                                    const AlignSettings& settings,
                                    int64_t& outShiftSamples);
    };
}
