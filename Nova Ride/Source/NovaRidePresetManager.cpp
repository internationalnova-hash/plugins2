#include "NovaRidePresetManager.h"
#include "ParameterIDs.hpp"

NovaRidePresetManager::NovaRidePresetManager()
{
    presets.push_back ({ "Default",        0.0f, 1, 50.0f, 50.0f, 0.0f, 0, false });
    presets.push_back ({ "Broadcast Smooth", 0.0f, 1, 30.0f, 40.0f, 0.0f, 0, false });
    presets.push_back ({ "Modern Tight",    0.0f, 1, 60.0f, 60.0f, 0.0f, 1, false });
    presets.push_back ({ "Live Safety",     -2.0f, 2, 80.0f, 65.0f, 0.0f, 2, false });
}

const NovaRidePresetManager::Preset& NovaRidePresetManager::getPreset (int index) const noexcept
{
    static const Preset fallback {};
    if (index < 0 || index >= getNumPresets())
        return fallback;
    return presets[static_cast<size_t> (index)];
}

juce::StringArray NovaRidePresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : presets)
        names.add (p.name);
    return names;
}

void NovaRidePresetManager::setCurrentIndex (int index) noexcept
{
    currentIndex = juce::jlimit (0, juce::jmax (0, getNumPresets() - 1), index);
}

void NovaRidePresetManager::applyTo (juce::AudioProcessorValueTreeState& apvts) const
{
    const auto& preset = getPreset (currentIndex);

    auto setNorm = [&apvts] (const char* paramId, float value)
    {
        if (auto* p = apvts.getParameter (paramId))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    setNorm (ParameterIDs::target, preset.targetDb);
    setNorm (ParameterIDs::speed, preset.speedPercent);
    setNorm (ParameterIDs::sensitivity, preset.sensitivityPercent);
    setNorm (ParameterIDs::output, preset.outputDb);

    if (auto* rangeParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParameterIDs::range)))
        *rangeParam = preset.rangeIndex;
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParameterIDs::mode)))
        *modeParam = preset.modeIndex;
    if (auto* musicParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParameterIDs::music)))
        *musicParam = preset.music;
}
