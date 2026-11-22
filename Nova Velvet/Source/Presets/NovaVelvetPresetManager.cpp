#include "NovaVelvetPresetManager.h"
#include "NovaVelvetFactoryPresets.h"
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

    juce::var presetToVar (const NovaVelvetPreset& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("formatVersion", 1);
        o->setProperty ("id", p.id);
        o->setProperty ("displayName", p.displayName);
        o->setProperty ("category", p.category);
        o->setProperty ("thresholdDbu", p.thresholdDbu);
        o->setProperty ("ratio", p.ratio);
        o->setProperty ("attackMs", p.attackMs);
        o->setProperty ("releaseMs", p.releaseMs);
        o->setProperty ("gainDb", p.gainDb);
        o->setProperty ("timeSelectIndex", p.timeSelectIndex);
        o->setProperty ("scHpfHz", p.scHpfHz);
        o->setProperty ("mixPercent", p.mixPercent);
        o->setProperty ("outputDb", p.outputDb);
        o->setProperty ("stereoLink", p.stereoLink);
        o->setProperty ("analogModeIndex", p.analogModeIndex);
        o->setProperty ("tubeDrivePercent", p.tubeDrivePercent);
        o->setProperty ("bypass", p.bypass);
        return juce::var (o);
    }

    bool presetFromVar (const juce::var& v, NovaVelvetPreset& out)
    {
        auto* o = v.getDynamicObject();
        if (o == nullptr) return false;

        out.id = v.getProperty ("id", "").toString();
        out.displayName = v.getProperty ("displayName", "").toString();
        out.category = v.getProperty ("category", "User").toString();
        if (out.id.isEmpty() || out.displayName.isEmpty()) return false;

        out.thresholdDbu = (float) v.getProperty ("thresholdDbu", -20.0);
        out.ratio = (float) v.getProperty ("ratio", 4.0);
        out.attackMs = (float) v.getProperty ("attackMs", 10.0);
        out.releaseMs = (float) v.getProperty ("releaseMs", 300.0);
        out.gainDb = (float) v.getProperty ("gainDb", 6.0);
        out.timeSelectIndex = (int) v.getProperty ("timeSelectIndex", 1);
        out.scHpfHz = (float) v.getProperty ("scHpfHz", 20.0);
        out.mixPercent = (float) v.getProperty ("mixPercent", 100.0);
        out.outputDb = (float) v.getProperty ("outputDb", 0.0);
        out.stereoLink = (bool) v.getProperty ("stereoLink", true);
        out.analogModeIndex = (int) v.getProperty ("analogModeIndex", 1);
        out.tubeDrivePercent = (float) v.getProperty ("tubeDrivePercent", 25.0);
        out.bypass = (bool) v.getProperty ("bypass", false);
        return true;
    }
}

NovaVelvetPresetManager::NovaVelvetPresetManager()
{
    scanUserPresets();
}

juce::File NovaVelvetPresetManager::getUserPresetsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Velvet")
        .getChildFile ("User Presets");
}

void NovaVelvetPresetManager::scanUserPresets()
{
    userPresets.clear();
    const auto folder = getUserPresetsFolder();
    if (! folder.isDirectory()) return;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.novavelvetpreset"))
    {
        const auto json = juce::JSON::parse (entry.getFile().loadFileAsString());
        NovaVelvetPreset preset;
        if (presetFromVar (json, preset))
            userPresets.push_back (preset);
    }

    std::sort (userPresets.begin(), userPresets.end(), [] (const NovaVelvetPreset& a, const NovaVelvetPreset& b)
    {
        return a.displayName.compareIgnoreCase (b.displayName) < 0;
    });
}

int NovaVelvetPresetManager::getNumPresets() const noexcept
{
    return (int) getNovaVelvetFactoryPresets().size() + (int) userPresets.size();
}

bool NovaVelvetPresetManager::isUserPreset (int index) const noexcept
{
    return index >= (int) getNovaVelvetFactoryPresets().size();
}

const NovaVelvetPreset& NovaVelvetPresetManager::getPreset (int index) const noexcept
{
    const auto& factory = getNovaVelvetFactoryPresets();
    const auto clamped = juce::jlimit (0, getNumPresets() - 1, index);
    if (clamped < (int) factory.size())
        return factory[(size_t) clamped];
    return userPresets[(size_t) (clamped - (int) factory.size())];
}

void NovaVelvetPresetManager::applyToApvts (int index, juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;

    currentIndex = juce::jlimit (0, numPresets - 1, index);
    const auto& preset = getPreset (currentIndex);

    setFloatParam (apvts, ParameterIDs::threshold, preset.thresholdDbu);
    setFloatParam (apvts, ParameterIDs::ratio, preset.ratio);
    setFloatParam (apvts, ParameterIDs::attack, preset.attackMs);
    setFloatParam (apvts, ParameterIDs::release, preset.releaseMs);
    setFloatParam (apvts, ParameterIDs::gain, preset.gainDb);
    setChoiceParam (apvts, ParameterIDs::timeSelect, preset.timeSelectIndex);
    setFloatParam (apvts, ParameterIDs::scHpf, preset.scHpfHz);
    setFloatParam (apvts, ParameterIDs::mix, preset.mixPercent);
    setFloatParam (apvts, ParameterIDs::output, preset.outputDb);
    setBoolParam (apvts, ParameterIDs::stereoLink, preset.stereoLink);
    setChoiceParam (apvts, ParameterIDs::analogMode, preset.analogModeIndex);
    setFloatParam (apvts, ParameterIDs::tubeDrive, preset.tubeDrivePercent);
    setBoolParam (apvts, ParameterIDs::bypass, preset.bypass);
}

void NovaVelvetPresetManager::selectNext (juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;
    applyToApvts ((currentIndex + 1) % numPresets, apvts);
}

void NovaVelvetPresetManager::selectPrevious (juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;
    applyToApvts ((currentIndex - 1 + numPresets) % numPresets, apvts);
}

int NovaVelvetPresetManager::saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    const auto trimmedName = name.trim();
    if (trimmedName.isEmpty())
        return -1;

    NovaVelvetPreset preset;
    preset.displayName = trimmedName;
    preset.category = "User";
    preset.thresholdDbu = getFloatParam (apvts, ParameterIDs::threshold);
    preset.ratio = getFloatParam (apvts, ParameterIDs::ratio);
    preset.attackMs = getFloatParam (apvts, ParameterIDs::attack);
    preset.releaseMs = getFloatParam (apvts, ParameterIDs::release);
    preset.gainDb = getFloatParam (apvts, ParameterIDs::gain);
    preset.timeSelectIndex = getChoiceParam (apvts, ParameterIDs::timeSelect);
    preset.scHpfHz = getFloatParam (apvts, ParameterIDs::scHpf);
    preset.mixPercent = getFloatParam (apvts, ParameterIDs::mix);
    preset.outputDb = getFloatParam (apvts, ParameterIDs::output);
    preset.stereoLink = getBoolParam (apvts, ParameterIDs::stereoLink);
    preset.analogModeIndex = getChoiceParam (apvts, ParameterIDs::analogMode);
    preset.tubeDrivePercent = getFloatParam (apvts, ParameterIDs::tubeDrive);
    preset.bypass = getBoolParam (apvts, ParameterIDs::bypass);

    const auto folder = getUserPresetsFolder();
    if (! folder.isDirectory() && ! folder.createDirectory().wasOk())
        return -1;

    auto baseSlug = slugify (trimmedName);
    auto slug = baseSlug;
    auto file = folder.getChildFile (slug + ".novavelvetpreset");
    int suffix = 2;
    while (file.existsAsFile())
    {
        slug = baseSlug + "_" + juce::String (suffix++);
        file = folder.getChildFile (slug + ".novavelvetpreset");
    }
    preset.id = slug;

    if (! file.replaceWithText (juce::JSON::toString (presetToVar (preset))))
        return -1;

    userPresets.push_back (preset);
    currentIndex = getNumPresets() - 1;
    return currentIndex;
}
