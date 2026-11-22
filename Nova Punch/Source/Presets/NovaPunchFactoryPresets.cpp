#include "NovaPunchFactoryPresets.h"

// Factory library. Punch/Body starting points for Kick/Snare/808/Drum
// Loop/Vocal follow the spec's own listening-test matrix (section 24)
// directly, so the shipped presets ARE the validated starting points, not
// arbitrary guesses.
const std::vector<NovaPunchPreset>& getNovaPunchFactoryPresets()
{
    static const std::vector<NovaPunchPreset> presets = {
        { .id = "default", .displayName = "Default", .category = "Init",
          .punchPercent = 50.0f, .bodyPercent = 50.0f, .focusIndex = 0,
          .sensitivityPercent = 65.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },

        { .id = "kick_punch", .displayName = "Kick Punch", .category = "Drums",
          .punchPercent = 70.0f, .bodyPercent = 50.0f, .focusIndex = 1,
          .sensitivityPercent = 65.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },

        { .id = "snare_crack", .displayName = "Snare Crack", .category = "Drums",
          .punchPercent = 70.0f, .bodyPercent = 40.0f, .focusIndex = 2,
          .sensitivityPercent = 65.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },

        { .id = "808_weight", .displayName = "808 Weight", .category = "Bass",
          .punchPercent = 60.0f, .bodyPercent = 70.0f, .focusIndex = 1,
          .sensitivityPercent = 60.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },

        { .id = "drum_loop", .displayName = "Drum Loop", .category = "Drums",
          .punchPercent = 60.0f, .bodyPercent = 55.0f, .focusIndex = 0,
          .sensitivityPercent = 65.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },

        { .id = "vocal_presence", .displayName = "Vocal Presence", .category = "Vocals",
          .punchPercent = 40.0f, .bodyPercent = 55.0f, .focusIndex = 2,
          .sensitivityPercent = 55.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },

        { .id = "full_mix_glue", .displayName = "Full Mix Glue", .category = "Bus",
          .punchPercent = 55.0f, .bodyPercent = 52.0f, .focusIndex = 0,
          .sensitivityPercent = 50.0f, .mixPercent = 60.0f, .clipSafe = true, .bypass = false },

        { .id = "soft_transients", .displayName = "Soft Transients", .category = "Utility",
          .punchPercent = 25.0f, .bodyPercent = 45.0f, .focusIndex = 0,
          .sensitivityPercent = 60.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },

        { .id = "extreme_punch", .displayName = "Extreme Punch", .category = "Drums",
          .punchPercent = 90.0f, .bodyPercent = 60.0f, .focusIndex = 0,
          .sensitivityPercent = 70.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },

        { .id = "tight_control", .displayName = "Tight & Controlled", .category = "Utility",
          .punchPercent = 55.0f, .bodyPercent = 20.0f, .focusIndex = 0,
          .sensitivityPercent = 65.0f, .mixPercent = 100.0f, .clipSafe = true, .bypass = false },
    };
    return presets;
}
