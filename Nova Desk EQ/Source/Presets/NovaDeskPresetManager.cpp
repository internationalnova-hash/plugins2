#include "NovaDeskPresetManager.h"
#include "NovaDeskFactoryPresets.h"
#include "../ParameterIDs.hpp"

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

    juce::var presetToVar (const NovaDeskPreset& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("formatVersion", 1);
        o->setProperty ("id", p.id);
        o->setProperty ("displayName", p.displayName);
        o->setProperty ("category", p.category);
        o->setProperty ("egMode", p.egMode);
        o->setProperty ("inputDb", p.inputDb);
        o->setProperty ("hpfFreqIndex", p.hpfFreqIndex);
        o->setProperty ("hpfIn", p.hpfIn);
        o->setProperty ("lowFreqHz", p.lowFreqHz);
        o->setProperty ("lowGainDb", p.lowGainDb);
        o->setProperty ("lowType", p.lowType);
        o->setProperty ("lowMidFreqHz", p.lowMidFreqHz);
        o->setProperty ("lowMidGainDb", p.lowMidGainDb);
        o->setProperty ("lowMidQ", p.lowMidQ);
        o->setProperty ("highMidFreqHz", p.highMidFreqHz);
        o->setProperty ("highMidGainDb", p.highMidGainDb);
        o->setProperty ("highMidQ", p.highMidQ);
        o->setProperty ("highFreqHz", p.highFreqHz);
        o->setProperty ("highGainDb", p.highGainDb);
        o->setProperty ("highType", p.highType);
        o->setProperty ("lpfFreqIndex", p.lpfFreqIndex);
        o->setProperty ("lpfIn", p.lpfIn);
        o->setProperty ("driveAmount", p.driveAmount);
        o->setProperty ("mixPercent", p.mixPercent);
        o->setProperty ("outputDb", p.outputDb);
        return juce::var (o);
    }

    bool presetFromVar (const juce::var& v, NovaDeskPreset& out)
    {
        auto* o = v.getDynamicObject();
        if (o == nullptr) return false;

        out.id = v.getProperty ("id", "").toString();
        out.displayName = v.getProperty ("displayName", "").toString();
        out.category = v.getProperty ("category", "User").toString();
        if (out.id.isEmpty() || out.displayName.isEmpty()) return false;

        out.egMode = (int) v.getProperty ("egMode", 0);
        out.inputDb = (float) v.getProperty ("inputDb", 0.0);
        out.hpfFreqIndex = (int) v.getProperty ("hpfFreqIndex", 0);
        out.hpfIn = (bool) v.getProperty ("hpfIn", true);
        out.lowFreqHz = (float) v.getProperty ("lowFreqHz", 100.0);
        out.lowGainDb = (float) v.getProperty ("lowGainDb", 0.0);
        out.lowType = (int) v.getProperty ("lowType", 1);
        out.lowMidFreqHz = (float) v.getProperty ("lowMidFreqHz", 400.0);
        out.lowMidGainDb = (float) v.getProperty ("lowMidGainDb", 0.0);
        out.lowMidQ = (float) v.getProperty ("lowMidQ", 0.8);
        out.highMidFreqHz = (float) v.getProperty ("highMidFreqHz", 2000.0);
        out.highMidGainDb = (float) v.getProperty ("highMidGainDb", 0.0);
        out.highMidQ = (float) v.getProperty ("highMidQ", 0.8);
        out.highFreqHz = (float) v.getProperty ("highFreqHz", 8000.0);
        out.highGainDb = (float) v.getProperty ("highGainDb", 0.0);
        out.highType = (int) v.getProperty ("highType", 1);
        out.lpfFreqIndex = (int) v.getProperty ("lpfFreqIndex", 0);
        out.lpfIn = (bool) v.getProperty ("lpfIn", true);
        out.driveAmount = (float) v.getProperty ("driveAmount", 0.0);
        out.mixPercent = (float) v.getProperty ("mixPercent", 100.0);
        out.outputDb = (float) v.getProperty ("outputDb", 0.0);
        return true;
    }
}

NovaDeskPresetManager::NovaDeskPresetManager()
{
    scanUserPresets();
}

juce::File NovaDeskPresetManager::getUserPresetsFolder()
{
    // userApplicationDataDirectory resolves to ~/Library on macOS, not
    // ~/Library/Application Support - "Application Support" must be added
    // explicitly to land in the conventional location (matches the
    // pattern already established by Nova Bass Lab's NovaBassVault).
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Desk EQ")
        .getChildFile ("User Presets");
}

void NovaDeskPresetManager::scanUserPresets()
{
    userPresets.clear();
    const auto folder = getUserPresetsFolder();
    if (! folder.isDirectory()) return;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.novadeskpreset"))
    {
        const auto json = juce::JSON::parse (entry.getFile().loadFileAsString());
        NovaDeskPreset preset;
        if (presetFromVar (json, preset))
            userPresets.push_back (preset);
    }

    std::sort (userPresets.begin(), userPresets.end(), [] (const NovaDeskPreset& a, const NovaDeskPreset& b)
    {
        return a.displayName.compareIgnoreCase (b.displayName) < 0;
    });
}

int NovaDeskPresetManager::getNumPresets() const noexcept
{
    return (int) getNovaDeskFactoryPresets().size() + (int) userPresets.size();
}

bool NovaDeskPresetManager::isUserPreset (int index) const noexcept
{
    return index >= (int) getNovaDeskFactoryPresets().size();
}

const NovaDeskPreset& NovaDeskPresetManager::getPreset (int index) const noexcept
{
    const auto& factory = getNovaDeskFactoryPresets();
    const auto clamped = juce::jlimit (0, getNumPresets() - 1, index);
    if (clamped < (int) factory.size())
        return factory[(size_t) clamped];
    return userPresets[(size_t) (clamped - (int) factory.size())];
}

void NovaDeskPresetManager::applyToApvts (int index, juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;

    currentIndex = juce::jlimit (0, numPresets - 1, index);
    const auto& preset = getPreset (currentIndex);

    setChoiceParam (apvts, ParameterIDs::egMode, preset.egMode);
    setFloatParam (apvts, ParameterIDs::input, preset.inputDb);

    setChoiceParam (apvts, ParameterIDs::hpfFreqIndex, preset.hpfFreqIndex);
    setBoolParam (apvts, ParameterIDs::hpfIn, preset.hpfIn);

    setFloatParam (apvts, ParameterIDs::lowFreq, preset.lowFreqHz);
    setFloatParam (apvts, ParameterIDs::lowGain, preset.lowGainDb);
    setChoiceParam (apvts, ParameterIDs::lowType, preset.lowType);

    setFloatParam (apvts, ParameterIDs::lowMidFreq, preset.lowMidFreqHz);
    setFloatParam (apvts, ParameterIDs::lowMidGain, preset.lowMidGainDb);
    setFloatParam (apvts, ParameterIDs::lowMidQ, preset.lowMidQ);

    setFloatParam (apvts, ParameterIDs::highMidFreq, preset.highMidFreqHz);
    setFloatParam (apvts, ParameterIDs::highMidGain, preset.highMidGainDb);
    setFloatParam (apvts, ParameterIDs::highMidQ, preset.highMidQ);

    setFloatParam (apvts, ParameterIDs::highFreq, preset.highFreqHz);
    setFloatParam (apvts, ParameterIDs::highGain, preset.highGainDb);
    setChoiceParam (apvts, ParameterIDs::highType, preset.highType);

    setChoiceParam (apvts, ParameterIDs::lpfFreqIndex, preset.lpfFreqIndex);
    setBoolParam (apvts, ParameterIDs::lpfIn, preset.lpfIn);

    setFloatParam (apvts, ParameterIDs::drive, preset.driveAmount);
    setFloatParam (apvts, ParameterIDs::mix, preset.mixPercent);
    setFloatParam (apvts, ParameterIDs::output, preset.outputDb);
}

void NovaDeskPresetManager::selectNext (juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;
    applyToApvts ((currentIndex + 1) % numPresets, apvts);
}

void NovaDeskPresetManager::selectPrevious (juce::AudioProcessorValueTreeState& apvts)
{
    const auto numPresets = getNumPresets();
    if (numPresets == 0) return;
    applyToApvts ((currentIndex - 1 + numPresets) % numPresets, apvts);
}

int NovaDeskPresetManager::saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    const auto trimmedName = name.trim();
    if (trimmedName.isEmpty())
        return -1;

    NovaDeskPreset preset;
    preset.displayName = trimmedName;
    preset.category = "User";
    preset.egMode = getChoiceParam (apvts, ParameterIDs::egMode);
    preset.inputDb = getFloatParam (apvts, ParameterIDs::input);
    preset.hpfFreqIndex = getChoiceParam (apvts, ParameterIDs::hpfFreqIndex);
    preset.hpfIn = getBoolParam (apvts, ParameterIDs::hpfIn);
    preset.lowFreqHz = getFloatParam (apvts, ParameterIDs::lowFreq);
    preset.lowGainDb = getFloatParam (apvts, ParameterIDs::lowGain);
    preset.lowType = getChoiceParam (apvts, ParameterIDs::lowType);
    preset.lowMidFreqHz = getFloatParam (apvts, ParameterIDs::lowMidFreq);
    preset.lowMidGainDb = getFloatParam (apvts, ParameterIDs::lowMidGain);
    preset.lowMidQ = getFloatParam (apvts, ParameterIDs::lowMidQ);
    preset.highMidFreqHz = getFloatParam (apvts, ParameterIDs::highMidFreq);
    preset.highMidGainDb = getFloatParam (apvts, ParameterIDs::highMidGain);
    preset.highMidQ = getFloatParam (apvts, ParameterIDs::highMidQ);
    preset.highFreqHz = getFloatParam (apvts, ParameterIDs::highFreq);
    preset.highGainDb = getFloatParam (apvts, ParameterIDs::highGain);
    preset.highType = getChoiceParam (apvts, ParameterIDs::highType);
    preset.lpfFreqIndex = getChoiceParam (apvts, ParameterIDs::lpfFreqIndex);
    preset.lpfIn = getBoolParam (apvts, ParameterIDs::lpfIn);
    preset.driveAmount = getFloatParam (apvts, ParameterIDs::drive);
    preset.mixPercent = getFloatParam (apvts, ParameterIDs::mix);
    preset.outputDb = getFloatParam (apvts, ParameterIDs::output);

    const auto folder = getUserPresetsFolder();
    if (! folder.isDirectory() && ! folder.createDirectory().wasOk())
        return -1;

    auto baseSlug = slugify (trimmedName);
    auto slug = baseSlug;
    auto file = folder.getChildFile (slug + ".novadeskpreset");
    int suffix = 2;
    while (file.existsAsFile())
    {
        slug = baseSlug + "_" + juce::String (suffix++);
        file = folder.getChildFile (slug + ".novadeskpreset");
    }
    preset.id = slug;

    if (! file.replaceWithText (juce::JSON::toString (presetToVar (preset))))
        return -1;

    userPresets.push_back (preset);
    currentIndex = getNumPresets() - 1;
    return currentIndex;
}
