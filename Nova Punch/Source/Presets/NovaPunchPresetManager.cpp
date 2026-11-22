#include "NovaPunchPresetManager.h"
#include "NovaPunchFactoryPresets.h"
#include "../ParameterIDs.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    void setFloatParam (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void setChoiceParam (juce::AudioProcessorValueTreeState& apvts, const char* id, int index)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    }

    void setBoolParam (juce::AudioProcessorValueTreeState& apvts, const char* id, bool value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (value ? 1.0f : 0.0f);
    }

    float getFloatParam (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* p = apvts.getParameter (id))
            return p->convertFrom0to1 (p->getValue());
        return 0.0f;
    }

    int getChoiceParam (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        return (int) std::round (getFloatParam (apvts, id));
    }

    bool getBoolParam (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* p = apvts.getParameter (id))
            return p->getValue() > 0.5f;
        return false;
    }

    juce::String slugify (const juce::String& name)
    {
        juce::String out;
        bool lastWasUnderscore = false;
        for (auto c : name.trim().toLowerCase())
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c))
            {
                out << c;
                lastWasUnderscore = false;
            }
            else if (! lastWasUnderscore && out.isNotEmpty())
            {
                out << '_';
                lastWasUnderscore = true;
            }
        }
        while (out.endsWithChar ('_'))
            out = out.dropLastCharacters (1);
        return out.isEmpty() ? juce::String ("user_preset") : out;
    }

    juce::var presetToVar (const NovaPunchPreset& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("formatVersion", 1);
        o->setProperty ("id", p.id);
        o->setProperty ("displayName", p.displayName);
        o->setProperty ("category", p.category);
        o->setProperty ("punchPercent", p.punchPercent);
        o->setProperty ("bodyPercent", p.bodyPercent);
        o->setProperty ("focusIndex", p.focusIndex);
        o->setProperty ("sensitivityPercent", p.sensitivityPercent);
        o->setProperty ("mixPercent", p.mixPercent);
        o->setProperty ("clipSafe", p.clipSafe);
        o->setProperty ("bypass", p.bypass);
        return juce::var (o);
    }

    bool presetFromVar (const juce::var& v, NovaPunchPreset& out)
    {
        auto* o = v.getDynamicObject();
        if (o == nullptr) return false;

        out.id = v.getProperty ("id", "").toString();
        out.displayName = v.getProperty ("displayName", "").toString();
        out.category = v.getProperty ("category", "User").toString();
        if (out.id.isEmpty() || out.displayName.isEmpty()) return false;

        out.punchPercent = (float) v.getProperty ("punchPercent", 50.0);
        out.bodyPercent = (float) v.getProperty ("bodyPercent", 50.0);
        out.focusIndex = (int) v.getProperty ("focusIndex", 0);
        out.sensitivityPercent = (float) v.getProperty ("sensitivityPercent", 65.0);
        out.mixPercent = (float) v.getProperty ("mixPercent", 100.0);
        out.clipSafe = (bool) v.getProperty ("clipSafe", true);
        out.bypass = (bool) v.getProperty ("bypass", false);
        return true;
    }
}

NovaPunchPresetManager::NovaPunchPresetManager()
{
    scanUserPresets();
}

juce::File NovaPunchPresetManager::getUserPresetsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Punch")
        .getChildFile ("User Presets");
}

void NovaPunchPresetManager::scanUserPresets()
{
    userPresets.clear();
    const auto folder = getUserPresetsFolder();
    if (! folder.isDirectory()) return;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.novapunchpreset"))
    {
        const auto json = juce::JSON::parse (entry.getFile().loadFileAsString());
        NovaPunchPreset preset;
        if (presetFromVar (json, preset))
            userPresets.push_back (preset);
    }

    std::sort (userPresets.begin(), userPresets.end(), [] (const NovaPunchPreset& a, const NovaPunchPreset& b)
    {
        return a.displayName.compareIgnoreCase (b.displayName) < 0;
    });
}

int NovaPunchPresetManager::getNumPresets() const noexcept
{
    return (int) getNovaPunchFactoryPresets().size() + (int) userPresets.size();
}

bool NovaPunchPresetManager::isUserPreset (int index) const noexcept
{
    return index >= (int) getNovaPunchFactoryPresets().size();
}

const NovaPunchPreset& NovaPunchPresetManager::getPreset (int index) const noexcept
{
    const auto& factory = getNovaPunchFactoryPresets();
    const auto clamped = juce::jlimit (0, getNumPresets() - 1, index);
    if (clamped < (int) factory.size())
        return factory[(size_t) clamped];
    return userPresets[(size_t) (clamped - (int) factory.size())];
}

void NovaPunchPresetManager::applyToApvts (int index, juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;

    currentIndex = juce::jlimit (0, numPresets - 1, index);
    const auto& preset = getPreset (currentIndex);

    setFloatParam (apvts, ParameterIDs::punch, preset.punchPercent);
    setFloatParam (apvts, ParameterIDs::body, preset.bodyPercent);
    setChoiceParam (apvts, ParameterIDs::focus, preset.focusIndex);
    setFloatParam (apvts, ParameterIDs::sensitivity, preset.sensitivityPercent);
    setFloatParam (apvts, ParameterIDs::mix, preset.mixPercent);
    setBoolParam (apvts, ParameterIDs::clipSafe, preset.clipSafe);
    setBoolParam (apvts, ParameterIDs::bypass, preset.bypass);
}

void NovaPunchPresetManager::selectNext (juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;
    applyToApvts ((currentIndex + 1) % numPresets, apvts);
}

void NovaPunchPresetManager::selectPrevious (juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;
    applyToApvts ((currentIndex - 1 + numPresets) % numPresets, apvts);
}

int NovaPunchPresetManager::saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    const auto trimmedName = name.trim();
    if (trimmedName.isEmpty())
        return -1;

    NovaPunchPreset preset;
    preset.displayName = trimmedName;
    preset.category = "User";
    preset.punchPercent = getFloatParam (apvts, ParameterIDs::punch);
    preset.bodyPercent = getFloatParam (apvts, ParameterIDs::body);
    preset.focusIndex = getChoiceParam (apvts, ParameterIDs::focus);
    preset.sensitivityPercent = getFloatParam (apvts, ParameterIDs::sensitivity);
    preset.mixPercent = getFloatParam (apvts, ParameterIDs::mix);
    preset.clipSafe = getBoolParam (apvts, ParameterIDs::clipSafe);
    preset.bypass = getBoolParam (apvts, ParameterIDs::bypass);

    const auto folder = getUserPresetsFolder();
    if (! folder.isDirectory() && ! folder.createDirectory().wasOk())
        return -1;

    auto baseSlug = slugify (trimmedName);
    auto slug = baseSlug;
    auto file = folder.getChildFile (slug + ".novapunchpreset");
    int suffix = 2;
    while (file.existsAsFile())
    {
        slug = baseSlug + "_" + juce::String (suffix++);
        file = folder.getChildFile (slug + ".novapunchpreset");
    }
    preset.id = slug;

    if (! file.replaceWithText (juce::JSON::toString (presetToVar (preset))))
        return -1;

    userPresets.push_back (preset);
    currentIndex = getNumPresets() - 1;
    return currentIndex;
}
