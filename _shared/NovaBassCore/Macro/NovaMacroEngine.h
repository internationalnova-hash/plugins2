#pragma once

#include "NovaMacroMap.h"
#include "../Voice/NovaBassVoiceParams.h"
#include <map>

namespace nova::bass
{

// The full set of concrete parameters a macro mapping can target - voice
// params plus the two fields (color drive, stereo width) that live outside
// NovaBassVoiceParams in the top-level engine.
struct NovaBassMacroTargets
{
    NovaBassVoiceParams voice;
    float colorDrive = 0.3f;
    float stereoWidth = 0.5f;
};

// Resolves a preset's NovaMacroTable against the six customer-facing knob
// positions (SHAPE/DECAY/DRIVE/WIDTH/GLIDE/BASS, each 0-1) into a concrete
// NovaBassMacroTargets. Any field with no active mapping in the table
// keeps its baseline (preset-authored) value untouched - only fields the
// preset's designer chose to wire up move as the user turns a knob.
//
// The master BASS macro is not special-cased: it simply may have a longer
// mapping list than the other five (spec sections 59-60) - same resolve()
// call, same NovaMacroMapping shape.
class NovaMacroEngine
{
public:
    static NovaBassMacroTargets resolve (const NovaBassMacroTargets& baseline,
                                          const NovaMacroTable& table,
                                          const std::map<juce::String, float>& macroKnobValues01) noexcept;

private:
    // Writes `value` into whichever field of `targets` the given
    // targetParam string names. Unknown target strings are ignored (no
    // crash) so a Lab-authored preset referencing a since-renamed target
    // degrades gracefully rather than taking down the plugin.
    static void applyToTarget (NovaBassMacroTargets& targets, const juce::String& targetParam, float value) noexcept;
};

} // namespace nova::bass
