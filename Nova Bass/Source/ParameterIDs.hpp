#pragma once

namespace ParameterIDs
{
    // The six customer-facing macro knobs - the ONLY sound-shaping controls
    // exposed to the customer (spec section 18). Each is a plain 0-1 float;
    // what it actually does is entirely determined by the loaded preset's
    // NovaMacroTable (see NovaMacroEngine) - the plugin never hardcodes
    // what "DECAY" or "BASS" mean in DSP terms.
    inline constexpr const char* shape = "shape";
    inline constexpr const char* decay = "decay";
    inline constexpr const char* drive = "drive";
    inline constexpr const char* width = "width";
    inline constexpr const char* glide = "glide";
    inline constexpr const char* bass = "bass";

    // CLEAN / WARM / GRITTY - a direct selector, not a macro-mapped value.
    inline constexpr const char* colorMode = "colorMode";

    inline constexpr const char* bypass = "bypass";
}
