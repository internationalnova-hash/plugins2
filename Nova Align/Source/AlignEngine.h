#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace NovaAlign
{
    // Ported from Nova Studio's NovaAlignProcessor (Source/NovaAlignProcessor.h/.cpp)
    // with the DAW-specific Clip/Session dependency removed - this plugin has no
    // timeline concept, just two audio files chosen by the user.
    class AlignEngine
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

        // Loads guideFile and targetFile, computes the alignment, and writes the
        // warped result to destinationFile (32-bit float WAV). Returns false if
        // either file fails to load or the sample rates don't match (no
        // resampling is performed). outShiftSamples receives the average shift
        // applied, for UI/telemetry purposes.
        static bool alignTargetClip (const juce::File& guideFile,
                                     const juce::File& targetFile,
                                     const juce::File& destinationFile,
                                     const AlignSettings& settings,
                                     int64_t& outShiftSamples);

        // Same as alignTargetClip, but returns the rendered result directly in
        // memory (outBuffer/outSampleRate) instead of writing to a file - used
        // by the plugin's live "aligned substitution" playback path, which must
        // not touch the filesystem from the audio thread's perspective (the
        // buffer is built on a background thread and handed off atomically).
        static bool alignTargetClipToBuffer (const juce::File& guideFile,
                                             const juce::File& targetFile,
                                             const AlignSettings& settings,
                                             juce::AudioBuffer<float>& outBuffer,
                                             double& outSampleRate,
                                             int64_t& outShiftSamples);
    };
}
