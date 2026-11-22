#pragma once

#include <juce_core/juce_core.h>

namespace ParameterIDs
{
    inline constexpr char voices[] = "voices";
    inline constexpr char width[] = "width";
    inline constexpr char humanize[] = "humanize";
    inline constexpr char mix[] = "mix";
    inline constexpr char style[] = "style";
    inline constexpr char tone[] = "tone";
    inline constexpr char keyMode[] = "key_mode";
    inline constexpr char keyNote[] = "key_note";
    inline constexpr char lowLatency[] = "low_latency";
    inline constexpr char qualityMode[] = "quality_mode";

    // Group solo toggles - matches NovaHarmonyAudioProcessor's Core/Support/
    // Texture voice-index groups. Standard mixing-console solo semantics:
    // if any group is soloed, only soloed groups are audible.
    inline constexpr char soloCore[] = "solo_core";
    inline constexpr char soloSupport[] = "solo_support";
    inline constexpr char soloTexture[] = "solo_texture";

    // Individual level trim, one per internal voice slot (matches
    // NovaHarmonyAudioProcessor::maxVoices). index is 0-based.
    inline constexpr int numVoiceLevels = 32;

    inline juce::String voiceLevelId (int index)
    {
        return "voice" + juce::String (index + 1) + "_level";
    }
}
