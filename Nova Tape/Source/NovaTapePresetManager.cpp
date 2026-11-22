#include "NovaTapePresetManager.h"
#include "ParameterIDs.hpp"

namespace
{
    juce::var presetToVar (const NovaTapePreset& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", p.name);
        o->setProperty ("input", p.input);
        o->setProperty ("tapeAmount", p.tapeAmount);
        o->setProperty ("tapeType", p.tapeType);
        o->setProperty ("ips", p.ips);
        o->setProperty ("bias", p.bias);
        o->setProperty ("wowFlutter", p.wowFlutter);
        o->setProperty ("noise", p.noise);
        o->setProperty ("lowCut", p.lowCut);
        o->setProperty ("highCut", p.highCut);
        o->setProperty ("mix", p.mix);
        o->setProperty ("output", p.output);
        o->setProperty ("trim", p.trim);
        return juce::var (o);
    }

    NovaTapePreset presetFromVar (const juce::var& v)
    {
        NovaTapePreset p;
        p.name = v.getProperty ("name", "User Preset").toString();
        p.input = (float) v.getProperty ("input", 0.0);
        p.tapeAmount = (float) v.getProperty ("tapeAmount", 65.0);
        p.tapeType = (int) v.getProperty ("tapeType", 0);
        p.ips = (int) v.getProperty ("ips", 1);
        p.bias = (float) v.getProperty ("bias", 0.0);
        p.wowFlutter = (float) v.getProperty ("wowFlutter", 2.4);
        p.noise = (float) v.getProperty ("noise", 1.8);
        p.lowCut = (float) v.getProperty ("lowCut", 30.0);
        p.highCut = (float) v.getProperty ("highCut", 18000.0);
        p.mix = (float) v.getProperty ("mix", 50.0);
        p.output = (float) v.getProperty ("output", -1.3);
        p.trim = (float) v.getProperty ("trim", 0.0);
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

NovaTapePresetManager::NovaTapePresetManager()
{
    // name, input, tapeAmount, tapeType(0=Studio,1=Vintage,2=Hot), ips(0=7.5,1=15,2=30),
    // bias, wowFlutter, noise, lowCut, highCut, mix, output, trim
    factoryPresets = {
        { "Default Tape",     0.0f, 65.0f, 0, 1,   0.0f, 2.4f, 1.8f, 30.0f, 18000.0f, 50.0f, -1.3f, 0.0f },
        { "Modern Master Bus",0.0f, 35.0f, 0, 2,   0.0f, 0.8f, 0.3f, 20.0f, 20000.0f, 35.0f, -0.5f, 0.0f },
        { "Vintage Master",   0.0f, 55.0f, 1, 1,  10.0f, 2.0f, 1.2f, 25.0f, 16000.0f, 45.0f, -1.0f, 0.0f },
        { "Warm Mix",         0.0f, 60.0f, 1, 1,  15.0f, 2.5f, 1.5f, 30.0f, 15500.0f, 55.0f, -1.2f, 0.0f },
        { "Hot Mix",          2.0f, 80.0f, 2, 1, -10.0f, 2.0f, 1.0f, 30.0f, 17500.0f, 60.0f, -2.0f, 0.0f },
        { "Vocal Tape",       0.0f, 45.0f, 0, 1,   0.0f, 1.5f, 1.0f, 80.0f, 17000.0f, 40.0f, -0.8f, 0.0f },
        { "Warm Vocal",       0.0f, 55.0f, 1, 1,  12.0f, 1.8f, 1.3f, 70.0f, 15000.0f, 50.0f, -1.0f, 0.0f },
        { "Drum Tape",        1.0f, 70.0f, 2, 1,   0.0f, 1.0f, 0.6f, 30.0f, 18000.0f, 65.0f, -1.8f, 0.0f },
        { "Drum Glue",        0.0f, 40.0f, 0, 2,   0.0f, 0.5f, 0.2f, 25.0f, 19000.0f, 45.0f, -0.6f, 0.0f },
        { "Bass Tape",        0.0f, 60.0f, 1, 0,  15.0f, 1.2f, 1.0f, 25.0f, 10000.0f, 55.0f, -1.2f, 0.0f },
        { "808 Tape",         1.0f, 75.0f, 2, 0,  10.0f, 0.8f, 0.8f, 20.0f,  9000.0f, 60.0f, -1.5f, 0.0f },
        { "Acoustic Warmth",  0.0f, 45.0f, 1, 1,   8.0f, 1.8f, 1.2f, 60.0f, 16000.0f, 40.0f, -0.7f, 0.0f },
        { "Vintage Keys",     0.0f, 50.0f, 1, 1,  10.0f, 3.0f, 1.8f, 40.0f, 15000.0f, 45.0f, -0.8f, 0.0f },
        { "Synth Tape",       0.0f, 65.0f, 2, 1,   0.0f, 1.5f, 0.8f, 30.0f, 17000.0f, 55.0f, -1.4f, 0.0f },
        { "Lo-Fi Reel",       0.0f, 85.0f, 1, 0,  20.0f, 6.0f, 4.5f, 40.0f,  8000.0f, 75.0f, -2.5f, 0.0f },
        { "Clean 30 IPS",     0.0f, 30.0f, 0, 2,   0.0f, 0.5f, 0.2f, 20.0f, 20000.0f, 35.0f, -0.4f, 0.0f },
        { "Classic 15 IPS",   0.0f, 50.0f, 0, 1,   0.0f, 2.0f, 1.0f, 30.0f, 17500.0f, 50.0f, -1.0f, 0.0f },
        { "Warm 7.5 IPS",     0.0f, 55.0f, 1, 0,  10.0f, 3.0f, 2.0f, 30.0f,  9500.0f, 50.0f, -1.2f, 0.0f },
    };

    scanUserPresets();
}

juce::File NovaTapePresetManager::getUserPresetsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
        .getChildFile ("Nova Audio")
        .getChildFile ("Nova Tape")
        .getChildFile ("User Presets");
}

void NovaTapePresetManager::scanUserPresets()
{
    userPresets.clear();
    auto folder = getUserPresetsFolder();
    if (! folder.isDirectory())
        return;

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.novatapepreset"))
    {
        auto parsed = juce::JSON::parse (f);
        if (parsed.isObject())
            userPresets.push_back (presetFromVar (parsed));
    }
}

const NovaTapePreset& NovaTapePresetManager::getPreset (int index) const noexcept
{
    if (index < (int) factoryPresets.size())
        return factoryPresets[(size_t) index];
    return userPresets[(size_t) (index - (int) factoryPresets.size())];
}

void NovaTapePresetManager::applyPreset (int index, juce::AudioProcessorValueTreeState& apvts) const
{
    if (index < 0 || index >= getNumPresets())
        return;
    const auto& p = getPreset (index);

    setNorm (apvts, ParameterIDs::input, p.input, -24.0f, 24.0f);
    setNorm (apvts, ParameterIDs::tapeAmount, p.tapeAmount, 0.0f, 100.0f);
    setNorm (apvts, ParameterIDs::tapeType, (float) p.tapeType, 0.0f, 2.0f);
    setNorm (apvts, ParameterIDs::ips, (float) p.ips, 0.0f, 2.0f);
    setNorm (apvts, ParameterIDs::bias, p.bias, -100.0f, 100.0f);
    setNorm (apvts, ParameterIDs::wowFlutter, p.wowFlutter, 0.0f, 10.0f);
    setNorm (apvts, ParameterIDs::noise, p.noise, 0.0f, 10.0f);
    setNorm (apvts, ParameterIDs::lowCut, p.lowCut, 20.0f, 200.0f);
    setNorm (apvts, ParameterIDs::highCut, p.highCut, 5000.0f, 20000.0f);
    setNorm (apvts, ParameterIDs::mix, p.mix, 0.0f, 100.0f);
    setNorm (apvts, ParameterIDs::output, p.output, -24.0f, 24.0f);
    setNorm (apvts, ParameterIDs::trim, p.trim, -6.0f, 6.0f);
}

int NovaTapePresetManager::saveCurrentAsUserPreset (const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    NovaTapePreset p;
    p.name = name.isNotEmpty() ? name : "User Preset";
    p.input = getReal (apvts, ParameterIDs::input, -24.0f, 24.0f);
    p.tapeAmount = getReal (apvts, ParameterIDs::tapeAmount, 0.0f, 100.0f);
    p.tapeType = (int) getReal (apvts, ParameterIDs::tapeType, 0.0f, 2.0f);
    p.ips = (int) getReal (apvts, ParameterIDs::ips, 0.0f, 2.0f);
    p.bias = getReal (apvts, ParameterIDs::bias, -100.0f, 100.0f);
    p.wowFlutter = getReal (apvts, ParameterIDs::wowFlutter, 0.0f, 10.0f);
    p.noise = getReal (apvts, ParameterIDs::noise, 0.0f, 10.0f);
    p.lowCut = getReal (apvts, ParameterIDs::lowCut, 20.0f, 200.0f);
    p.highCut = getReal (apvts, ParameterIDs::highCut, 5000.0f, 20000.0f);
    p.mix = getReal (apvts, ParameterIDs::mix, 0.0f, 100.0f);
    p.output = getReal (apvts, ParameterIDs::output, -24.0f, 24.0f);
    p.trim = getReal (apvts, ParameterIDs::trim, -6.0f, 6.0f);

    auto folder = getUserPresetsFolder();
    folder.createDirectory();

    juce::String slug = slugify (p.name);
    juce::File file = folder.getChildFile (slug + ".novatapepreset");
    int suffix = 2;
    while (file.existsAsFile())
    {
        file = folder.getChildFile (slug + "-" + juce::String (suffix) + ".novatapepreset");
        ++suffix;
    }

    file.replaceWithText (juce::JSON::toString (presetToVar (p)));

    userPresets.push_back (p);
    return (int) factoryPresets.size() + (int) userPresets.size() - 1;
}
