#pragma once

namespace ParameterIDs
{
    inline constexpr char environmentId[] = "environment_id";
    inline constexpr char environmentAmount[] = "environment_amount";
    inline constexpr char position[] = "position";
    inline constexpr char roomAmount[] = "room_amount";
    inline constexpr char characterAmount[] = "character_amount";
    inline constexpr char noiseAmount[] = "noise_amount";
    inline constexpr char noiseEnabled[] = "noise_enabled";
    inline constexpr char levelMatchEnabled[] = "level_match_enabled";
    inline constexpr char outputTrim[] = "output_trim";
    inline constexpr char monoEnabled[] = "mono_enabled";
    inline constexpr char leftOnly[] = "left_only";
    inline constexpr char rightOnly[] = "right_only";
    inline constexpr char midOnly[] = "mid_only";
    inline constexpr char sideOnly[] = "side_only";
    inline constexpr char dimEnabled[] = "dim_enabled";
    inline constexpr char phaseInvert[] = "phase_invert";
    inline constexpr char bypass[] = "bypass";
    inline constexpr char safeRenderEnabled[] = "safe_render_enabled";

    // Full V1 environment roster (12) - stable choice indices from the start
    // per the spec, even though only the first 4 have real profiles in this
    // phase. See EnvironmentProfiles.hpp.
    inline constexpr const char* environmentNames[] = {
        "Nova Main Room", "Nearfield Studio", "Mastering Suite", "Bedroom Studio",
        "Sedan", "Luxury SUV",
        "Smartphone", "Laptop", "Bluetooth Speaker", "Boombox",
        "Club PA", "Mono Cube"
    };
    inline constexpr int numEnvironments = 12;

    // 4 generic slots - JUCE parameters can't have context-dependent choice
    // lists, so these are the base/automation-lane names. The UI layers
    // category-specific display labels on top of these same 4 indices
    // (e.g. Sedan shows Driver/Passenger/Rear Left/Rear Right) - see
    // POSITION_LABELS in index.html. The DSP's positionBias mapping for each
    // index is in EnvironmentChain::process (PluginProcessorCore.cpp).
    inline constexpr const char* positionNames[] = {
        "Center", "Left", "Right", "Rear"
    };
    inline constexpr int numPositions = 4;
}
