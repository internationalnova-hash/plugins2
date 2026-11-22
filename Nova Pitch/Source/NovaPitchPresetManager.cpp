#include "NovaPitchPresetManager.h"
#include "ParameterIDs.hpp"

namespace
{
    juce::var presetToVar (const NovaPitchPreset& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", p.name);
        o->setProperty ("key", p.key);
        o->setProperty ("scale", p.scale);
        o->setProperty ("tolerance", p.tolerance);
        o->setProperty ("amount", p.amount);
        o->setProperty ("confidenceThreshold", p.confidenceThreshold);
        o->setProperty ("vibrato", p.vibrato);
        o->setProperty ("formant", p.formant);
        o->setProperty ("lowLatency", p.lowLatency);
        o->setProperty ("formantWarp", p.formantWarp);
        o->setProperty ("mix", p.mix);
        o->setProperty ("outputGain", p.outputGain);
        o->setProperty ("range", p.range);
        o->setProperty ("vibratoRate", p.vibratoRate);
        o->setProperty ("transients", p.transients);
        return juce::var (o);
    }

    NovaPitchPreset presetFromVar (const juce::var& v)
    {
        NovaPitchPreset p;
        p.name = v.getProperty ("name", "User Preset").toString();
        p.key = (int) v.getProperty ("key", 0);
        p.scale = (int) v.getProperty ("scale", 1);
        p.tolerance = (float) v.getProperty ("tolerance", 30.0);
        p.amount = (float) v.getProperty ("amount", 100.0);
        p.confidenceThreshold = (float) v.getProperty ("confidenceThreshold", 70.0);
        p.vibrato = (float) v.getProperty ("vibrato", 0.0);
        p.formant = (float) v.getProperty ("formant", 100.0);
        p.lowLatency = (bool) v.getProperty ("lowLatency", false);
        p.formantWarp = (float) v.getProperty ("formantWarp", 0.0);
        p.mix = (float) v.getProperty ("mix", 100.0);
        p.outputGain = (float) v.getProperty ("outputGain", 0.0);
        p.range = (float) v.getProperty ("range", 100.0);
        p.vibratoRate = (float) v.getProperty ("vibratoRate", 5.0);
        p.transients = (float) v.getProperty ("transients", 30.0);
        return p;
    }

    juce::String slugify (const juce::String& name)
    {
        juce::String s = name.trim().toLowerCase();
        juce::String out;
        for (auto c : s)
            out += (juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : "-");
        while (out.contains ("--")) out = out.replace ("--", "-");
        return out.trim().removeCharacters ("-").isEmpty() ? "user-preset" : out;
    }

    void setNorm (juce::AudioProcessorValueTreeState& apvts, const char* id, float realValue, float lo, float hi)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (realValue - lo) / (hi - lo)));
    }

    float getReal (juce::AudioProcessorValueTreeState& apvts, const char* id, float lo, float hi)
    {
        const float norm = apvts.getParameter (id) != nullptr ? apvts.getParameter (id)->getValue() : 0.0f;
        return lo + norm * (hi - lo);
    }
}

NovaPitchPresetManager::NovaPitchPresetManager()
{
    // Sensible starting points matching each name's character - key/scale
    // default to C/Major throughout since the actual song key is always
    // user-adjustable regardless of preset.
    factoryPresets = {
        { "Modern Vocal",   0, 1, 30.0f, 100.0f, 70.0f, 20.0f, 100.0f, false, 0.0f,  100.0f, 0.0f, 100.0f, 5.0f, 30.0f },
        { "Natural Vocal",  0, 1, 70.0f,  60.0f, 65.0f, 60.0f, 100.0f, false, 0.0f,  100.0f, 0.0f, 100.0f, 5.0f, 40.0f },
        { "Hard Tune",      0, 1,  3.0f, 100.0f, 75.0f,  0.0f, 100.0f, false, 0.0f,  100.0f, 0.0f, 100.0f, 5.0f, 10.0f },
        { "R&B Smooth",     0, 1, 55.0f,  85.0f, 68.0f, 45.0f, 100.0f, false, 0.0f,  100.0f, 0.0f, 100.0f, 5.0f, 35.0f },
        { "Rap Tight",      0, 1,  8.0f, 100.0f, 72.0f,  5.0f, 100.0f, false, 0.0f,  100.0f, 0.0f, 100.0f, 5.0f, 60.0f },
        { "Pop Clean",      0, 1, 25.0f,  95.0f, 70.0f, 15.0f, 100.0f, false, 0.0f,  100.0f, 0.0f, 100.0f, 5.0f, 25.0f },
        { "Transparent",    0, 1, 50.0f,  35.0f, 65.0f, 30.0f, 100.0f, false, 0.0f,   70.0f, 0.0f, 100.0f, 5.0f, 30.0f },
        { "Creative Tune",  0, 1,  2.0f, 100.0f, 75.0f,  0.0f, 100.0f, false, 40.0f, 100.0f, 0.0f, 100.0f, 5.0f,  0.0f },
    };

    scanUserPresets();
}

juce::File NovaPitchPresetManager::getUserPresetsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Pitch")
        .getChildFile ("User Presets");
}

void NovaPitchPresetManager::scanUserPresets()
{
    userPresets.clear();
    auto folder = getUserPresetsFolder();
    if (! folder.isDirectory())
        return;

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.novapitchpreset"))
    {
        auto parsed = juce::JSON::parse (f);
        if (parsed.isObject())
            userPresets.push_back (presetFromVar (parsed));
    }
}

const NovaPitchPreset& NovaPitchPresetManager::getPreset (int index) const noexcept
{
    if (index < (int) factoryPresets.size())
        return factoryPresets[(size_t) index];
    return userPresets[(size_t) (index - (int) factoryPresets.size())];
}

void NovaPitchPresetManager::applyPreset (int index, juce::AudioProcessorValueTreeState& apvts) const
{
    if (index < 0 || index >= getNumPresets())
        return;
    const auto& p = getPreset (index);

    setNorm (apvts, ParameterIDs::key, (float) p.key, 0.0f, 11.0f);
    setNorm (apvts, ParameterIDs::scale, (float) p.scale, 0.0f, 4.0f);
    setNorm (apvts, ParameterIDs::tolerance, p.tolerance, 0.0f, 100.0f);
    setNorm (apvts, ParameterIDs::amount, p.amount, 0.0f, 100.0f);
    setNorm (apvts, ParameterIDs::confidenceThreshold, p.confidenceThreshold, 0.0f, 100.0f);
    setNorm (apvts, ParameterIDs::vibrato, p.vibrato, 0.0f, 100.0f);
    setNorm (apvts, ParameterIDs::formant, p.formant, 0.0f, 100.0f);
    if (auto* param = apvts.getParameter (ParameterIDs::lowLatency))
        param->setValueNotifyingHost (p.lowLatency ? 1.0f : 0.0f);
    setNorm (apvts, ParameterIDs::formantWarp, p.formantWarp, -100.0f, 100.0f);
    setNorm (apvts, ParameterIDs::mix, p.mix, 0.0f, 100.0f);
    setNorm (apvts, ParameterIDs::outputGain, p.outputGain, -24.0f, 24.0f);
    setNorm (apvts, ParameterIDs::range, p.range, 0.0f, 100.0f);
    setNorm (apvts, ParameterIDs::vibratoRate, p.vibratoRate, 2.0f, 8.0f);
    setNorm (apvts, ParameterIDs::transients, p.transients, 0.0f, 100.0f);
}

int NovaPitchPresetManager::saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    NovaPitchPreset p;
    p.name = name.isNotEmpty() ? name : "User Preset";
    p.key = (int) getReal (apvts, ParameterIDs::key, 0.0f, 11.0f);
    p.scale = (int) getReal (apvts, ParameterIDs::scale, 0.0f, 4.0f);
    p.tolerance = getReal (apvts, ParameterIDs::tolerance, 0.0f, 100.0f);
    p.amount = getReal (apvts, ParameterIDs::amount, 0.0f, 100.0f);
    p.confidenceThreshold = getReal (apvts, ParameterIDs::confidenceThreshold, 0.0f, 100.0f);
    p.vibrato = getReal (apvts, ParameterIDs::vibrato, 0.0f, 100.0f);
    p.formant = getReal (apvts, ParameterIDs::formant, 0.0f, 100.0f);
    p.lowLatency = apvts.getParameter (ParameterIDs::lowLatency) != nullptr
                       && apvts.getParameter (ParameterIDs::lowLatency)->getValue() >= 0.5f;
    p.formantWarp = getReal (apvts, ParameterIDs::formantWarp, -100.0f, 100.0f);
    p.mix = getReal (apvts, ParameterIDs::mix, 0.0f, 100.0f);
    p.outputGain = getReal (apvts, ParameterIDs::outputGain, -24.0f, 24.0f);
    p.range = getReal (apvts, ParameterIDs::range, 0.0f, 100.0f);
    p.vibratoRate = getReal (apvts, ParameterIDs::vibratoRate, 2.0f, 8.0f);
    p.transients = getReal (apvts, ParameterIDs::transients, 0.0f, 100.0f);

    auto folder = getUserPresetsFolder();
    folder.createDirectory();

    juce::String slug = slugify (p.name);
    juce::File file = folder.getChildFile (slug + ".novapitchpreset");
    int suffix = 2;
    while (file.existsAsFile())
    {
        file = folder.getChildFile (slug + "-" + juce::String (suffix) + ".novapitchpreset");
        ++suffix;
    }

    file.replaceWithText (juce::JSON::toString (presetToVar (p)));

    userPresets.push_back (p);
    return (int) factoryPresets.size() + (int) userPresets.size() - 1;
}
