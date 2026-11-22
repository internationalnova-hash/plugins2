#include "NovaMacroEngine.h"

namespace nova::bass
{

void NovaMacroEngine::applyToTarget (NovaBassMacroTargets& targets, const juce::String& targetParam, float value) noexcept
{
    auto& v = targets.voice;

    if (targetParam == "sineTriBlend")               v.sineTriBlend = value;
    else if (targetParam == "fineTuneCents")          v.fineTuneCents = value;
    else if (targetParam == "pitchEnvAmountSemitones") v.pitchEnvAmountSemitones = value;
    else if (targetParam == "pitchEnvDecaySeconds")   v.pitchEnvDecaySeconds = value;
    else if (targetParam == "pitchEnvCurve")          v.pitchEnvCurve = value;
    else if (targetParam == "ampAttackSeconds")       v.ampAttackSeconds = value;
    else if (targetParam == "ampDecaySeconds")        v.ampDecaySeconds = value;
    else if (targetParam == "ampSustain")             v.ampSustain = value;
    else if (targetParam == "ampReleaseSeconds")      v.ampReleaseSeconds = value;
    else if (targetParam == "transientAmount")        v.transientAmount = value;
    else if (targetParam == "transientToneHz")        v.transientToneHz = value;
    else if (targetParam == "transientDecaySeconds")  v.transientDecaySeconds = value;
    else if (targetParam == "filterCutoffHz")         v.filterCutoffHz = value;
    else if (targetParam == "filterResonance")        v.filterResonance = value;
    else if (targetParam == "glideTimeSeconds")       v.glideTimeSeconds = value;
    else if (targetParam == "colorDrive")             targets.colorDrive = value;
    else if (targetParam == "stereoWidth")            targets.stereoWidth = value;
    // Unknown target: intentionally ignored, not asserted - see header comment.
}

NovaBassMacroTargets NovaMacroEngine::resolve (const NovaBassMacroTargets& baseline,
                                                const NovaMacroTable& table,
                                                const std::map<juce::String, float>& macroKnobValues01) noexcept
{
    NovaBassMacroTargets result = baseline;

    for (const auto& [macroName, knobValue] : macroKnobValues01)
    {
        auto it = table.find (macroName);
        if (it == table.end()) continue;

        for (const auto& mapping : it->second)
        {
            const float resolvedValue = resolveMacroMapping (knobValue, mapping);
            applyToTarget (result, mapping.targetParam, resolvedValue);
        }
    }

    return result;
}

} // namespace nova::bass
