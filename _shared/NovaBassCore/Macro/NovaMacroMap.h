#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

namespace nova::bass
{

// Generic, data-driven macro-mapping structures - confirmed net-new for
// this catalog (Nova Keys' 4-macro system is hardcoded semantic C++
// dispatch, not a per-preset table). One mechanism serves BOTH the five
// simple knobs (SHAPE/DECAY/DRIVE/WIDTH/GLIDE, which typically map to one
// internal param each) and the master BASS orb (which uses a longer
// mapping list) - there is no special-cased "intensity" logic anywhere;
// BASS is just a macro with more entries in its table.
enum class NovaMacroCurve { Linear, Exponential, Logarithmic, SCurve };
enum class NovaMacroPolarity { Normal, Inverted };

struct NovaMacroMapping
{
    // Identifies which concrete field in NovaBassVoiceParams/color/stereo
    // params this entry drives. Resolved by NovaMacroEngine's dispatch
    // table (see NovaMacroEngine.cpp) - kept as a string rather than an
    // enum so Nova Bass Lab's Macro Mapping Editor (Milestone B) can list
    // available targets data-driven from the same table, without a shared
    // header of magic numbers between the engine and the editor UI.
    juce::String targetParam;
    float min = 0.0f;
    float max = 1.0f;
    NovaMacroCurve curve = NovaMacroCurve::Linear;
    NovaMacroPolarity polarity = NovaMacroPolarity::Normal;
};

// key: "SHAPE" | "DECAY" | "DRIVE" | "WIDTH" | "GLIDE" | "BASS"
using NovaMacroTable = std::map<juce::String, std::vector<NovaMacroMapping>>;

// Applies curve + polarity + min/max to a single macro's raw 0-1 knob
// position, producing the concrete value that mapping entry should write
// into its target param. Pulled out as a free function so both
// NovaMacroEngine and (eventually) Lab's editor preview can share it.
inline float resolveMacroMapping (float macroValue01, const NovaMacroMapping& mapping) noexcept
{
    float t = juce::jlimit (0.0f, 1.0f, macroValue01);

    if (mapping.polarity == NovaMacroPolarity::Inverted)
        t = 1.0f - t;

    switch (mapping.curve)
    {
        case NovaMacroCurve::Exponential: t = t * t; break;
        case NovaMacroCurve::Logarithmic: t = std::sqrt (t); break;
        case NovaMacroCurve::SCurve:      t = t * t * (3.0f - 2.0f * t); break;
        case NovaMacroCurve::Linear:
        default: break;
    }

    return mapping.min + (mapping.max - mapping.min) * t;
}

} // namespace nova::bass
