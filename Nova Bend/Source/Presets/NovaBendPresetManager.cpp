#include "NovaBendPresetManager.h"
#include "NovaBendFactoryPresets.h"
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

    juce::var presetToVar (const NovaBendPreset& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("formatVersion", 1);
        o->setProperty ("id", p.id);
        o->setProperty ("displayName", p.displayName);
        o->setProperty ("category", p.category);
        o->setProperty ("pitchSemitones", p.pitchSemitones);
        o->setProperty ("formantSemitones", p.formantSemitones);
        o->setProperty ("flatten", p.flatten);
        o->setProperty ("flattenNoteIndex", p.flattenNoteIndex);
        o->setProperty ("flattenOctaveIndex", p.flattenOctaveIndex);
        o->setProperty ("link", p.link);
        o->setProperty ("mixPercent", p.mixPercent);
        o->setProperty ("qualityIndex", p.qualityIndex);
        o->setProperty ("bypass", p.bypass);
        return juce::var (o);
    }

    bool presetFromVar (const juce::var& v, NovaBendPreset& out)
    {
        auto* o = v.getDynamicObject();
        if (o == nullptr) return false;

        out.id = v.getProperty ("id", "").toString();
        out.displayName = v.getProperty ("displayName", "").toString();
        out.category = v.getProperty ("category", "User").toString();
        if (out.id.isEmpty() || out.displayName.isEmpty()) return false;

        out.pitchSemitones = (float) v.getProperty ("pitchSemitones", 0.0);
        out.formantSemitones = (float) v.getProperty ("formantSemitones", 0.0);
        out.flatten = (bool) v.getProperty ("flatten", false);
        out.flattenNoteIndex = (int) v.getProperty ("flattenNoteIndex", 0);
        out.flattenOctaveIndex = (int) v.getProperty ("flattenOctaveIndex", 2);
        out.link = (bool) v.getProperty ("link", false);
        out.mixPercent = (float) v.getProperty ("mixPercent", 100.0);
        out.qualityIndex = (int) v.getProperty ("qualityIndex", 1);
        out.bypass = (bool) v.getProperty ("bypass", false);
        return true;
    }
}

NovaBendPresetManager::NovaBendPresetManager()
{
    scanUserPresets();
}

juce::File NovaBendPresetManager::getUserPresetsFolder()
{
    // userApplicationDataDirectory resolves to ~/Library on macOS, not
    // ~/Library/Application Support - "Application Support" must be added
    // explicitly to land in the conventional location (matches Nova Desk
    // EQ's own preset folder pattern).
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Bend")
        .getChildFile ("User Presets");
}

void NovaBendPresetManager::scanUserPresets()
{
    userPresets.clear();
    const auto folder = getUserPresetsFolder();
    if (! folder.isDirectory()) return;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.novabendpreset"))
    {
        const auto json = juce::JSON::parse (entry.getFile().loadFileAsString());
        NovaBendPreset preset;
        if (presetFromVar (json, preset))
            userPresets.push_back (preset);
    }

    std::sort (userPresets.begin(), userPresets.end(), [] (const NovaBendPreset& a, const NovaBendPreset& b)
    {
        return a.displayName.compareIgnoreCase (b.displayName) < 0;
    });
}

int NovaBendPresetManager::getNumPresets() const noexcept
{
    return (int) getNovaBendFactoryPresets().size() + (int) userPresets.size();
}

bool NovaBendPresetManager::isUserPreset (int index) const noexcept
{
    return index >= (int) getNovaBendFactoryPresets().size();
}

const NovaBendPreset& NovaBendPresetManager::getPreset (int index) const noexcept
{
    const auto& factory = getNovaBendFactoryPresets();
    const auto clamped = juce::jlimit (0, getNumPresets() - 1, index);
    if (clamped < (int) factory.size())
        return factory[(size_t) clamped];
    return userPresets[(size_t) (clamped - (int) factory.size())];
}

void NovaBendPresetManager::applyToApvts (int index, juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;

    currentIndex = juce::jlimit (0, numPresets - 1, index);
    const auto& preset = getPreset (currentIndex);

    setFloatParam (apvts, ParameterIDs::pitch, preset.pitchSemitones);
    setFloatParam (apvts, ParameterIDs::formant, preset.formantSemitones);
    setBoolParam (apvts, ParameterIDs::flatten, preset.flatten);
    setChoiceParam (apvts, ParameterIDs::flattenNote, preset.flattenNoteIndex);
    setChoiceParam (apvts, ParameterIDs::flattenOctave, preset.flattenOctaveIndex);
    setBoolParam (apvts, ParameterIDs::link, preset.link);
    setFloatParam (apvts, ParameterIDs::mix, preset.mixPercent);
    setChoiceParam (apvts, ParameterIDs::quality, preset.qualityIndex);
    setBoolParam (apvts, ParameterIDs::bypass, preset.bypass);
}

void NovaBendPresetManager::selectNext (juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;
    applyToApvts ((currentIndex + 1) % numPresets, apvts);
}

void NovaBendPresetManager::selectPrevious (juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;
    applyToApvts ((currentIndex - 1 + numPresets) % numPresets, apvts);
}

int NovaBendPresetManager::saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    const auto trimmedName = name.trim();
    if (trimmedName.isEmpty())
        return -1;

    NovaBendPreset preset;
    preset.displayName = trimmedName;
    preset.category = "User";
    preset.pitchSemitones = getFloatParam (apvts, ParameterIDs::pitch);
    preset.formantSemitones = getFloatParam (apvts, ParameterIDs::formant);
    preset.flatten = getBoolParam (apvts, ParameterIDs::flatten);
    preset.flattenNoteIndex = getChoiceParam (apvts, ParameterIDs::flattenNote);
    preset.flattenOctaveIndex = getChoiceParam (apvts, ParameterIDs::flattenOctave);
    preset.link = getBoolParam (apvts, ParameterIDs::link);
    preset.mixPercent = getFloatParam (apvts, ParameterIDs::mix);
    preset.qualityIndex = getChoiceParam (apvts, ParameterIDs::quality);
    preset.bypass = getBoolParam (apvts, ParameterIDs::bypass);

    const auto folder = getUserPresetsFolder();
    if (! folder.isDirectory() && ! folder.createDirectory().wasOk())
        return -1;

    auto baseSlug = slugify (trimmedName);
    auto slug = baseSlug;
    auto file = folder.getChildFile (slug + ".novabendpreset");
    int suffix = 2;
    while (file.existsAsFile())
    {
        slug = baseSlug + "_" + juce::String (suffix++);
        file = folder.getChildFile (slug + ".novabendpreset");
    }
    preset.id = slug;

    if (! file.replaceWithText (juce::JSON::toString (presetToVar (preset))))
        return -1;

    userPresets.push_back (preset);
    currentIndex = getNumPresets() - 1;
    return currentIndex;
}
