#include "NovaGlowPresetManager.h"

namespace
{
    // Mirrors Nova Glow/Source/ParameterIDs.hpp - duplicated as literals
    // here rather than including across the plugin/Core directory
    // boundary, since NovaGlowPresetManager is otherwise a self-contained
    // Core class (matches the catalog's convention of Core classes not
    // reaching into a specific plugin's Source/ headers).
    constexpr auto kPeakReduction = "peakReduction";
    constexpr auto kGain = "gain";
    constexpr auto kCompressLimit = "compressLimit";
    constexpr auto kScHpf = "scHpf";
    constexpr auto kEmphasis = "emphasis";
    constexpr auto kGlow = "glow";
    constexpr auto kAnalogMode = "analogMode";
    constexpr auto kMix = "mix";
    constexpr auto kStereoLink = "stereoLink";
    constexpr auto kNoise = "noise";
    constexpr auto kOversampling = "oversampling";

    constexpr auto kPresetFileExtension = ".novaglowpreset";
}

NovaGlowPresetManager::NovaGlowPresetManager()
{
    // Factory presets - musically-motivated starting points across Glow's
    // main use cases (spec section 1: lead/BG vocals, bass, acoustic
    // guitar, strings, keys, general mix elements), not just Default.
    presets.push_back ({ "Default", true, {} });

    presets.push_back ({ "Vocal Rider", true,
        { 35.0f, 55.0f, 0, 80.0f, 20.0f, 40.0f, 1, 100.0f, 100.0f, 0.0f, 1 } });

    presets.push_back ({ "Bass Glue", true,
        { 45.0f, 50.0f, 0, 20.0f, 0.0f, 55.0f, 1, 100.0f, 100.0f, 0.0f, 2 } });

    presets.push_back ({ "Broadcast Tight", true,
        { 70.0f, 50.0f, 1, 60.0f, 10.0f, 30.0f, 0, 100.0f, 100.0f, 0.0f, 1 } });

    presets.push_back ({ "Vintage Push", true,
        { 55.0f, 60.0f, 0, 40.0f, 15.0f, 75.0f, 2, 85.0f, 100.0f, 8.0f, 2 } });
}

void NovaGlowPresetManager::scanUserPresets()
{
    auto dir = getUserPresetDirectory();
    if (! dir.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, juce::String ("*") + kPresetFileExtension))
    {
        auto file = entry.getFile();
        auto parsed = juce::JSON::parse (file);
        if (parsed.isVoid())
            continue;

        juce::String name;
        Values values;
        if (! valuesFromVar (parsed, name, values))
            continue;

        Preset p;
        p.name = name.isNotEmpty() ? name : file.getFileNameWithoutExtension();
        p.isFactory = false;
        p.values = values;
        p.sourceFile = file;
        presets.push_back (p);
    }
}

const NovaGlowPresetManager::Preset& NovaGlowPresetManager::getPreset (int index) const noexcept
{
    static const Preset fallback {};
    if (index < 0 || index >= getNumPresets())
        return fallback;
    return presets[static_cast<size_t> (index)];
}

juce::StringArray NovaGlowPresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : presets)
        names.add (p.name);
    return names;
}

juce::Array<bool> NovaGlowPresetManager::getFactoryFlags() const
{
    juce::Array<bool> flags;
    for (const auto& p : presets)
        flags.add (p.isFactory);
    return flags;
}

void NovaGlowPresetManager::setCurrentIndex (int index) noexcept
{
    currentIndex = juce::jlimit (0, juce::jmax (0, getNumPresets() - 1), index);
}

void NovaGlowPresetManager::selectNext() noexcept
{
    if (getNumPresets() > 0)
        setCurrentIndex ((currentIndex + 1) % getNumPresets());
}

void NovaGlowPresetManager::selectPrevious() noexcept
{
    if (getNumPresets() > 0)
        setCurrentIndex ((currentIndex - 1 + getNumPresets()) % getNumPresets());
}

void NovaGlowPresetManager::applyTo (juce::AudioProcessorValueTreeState& apvts) const
{
    const auto& v = getPreset (currentIndex).values;

    auto setNorm = [&apvts] (const char* paramId, float value)
    {
        if (auto* p = apvts.getParameter (paramId))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    setNorm (kPeakReduction, v.peakReduction);
    setNorm (kGain, v.gain);
    setNorm (kScHpf, v.scHpf);
    setNorm (kEmphasis, v.emphasis);
    setNorm (kGlow, v.glow);
    setNorm (kMix, v.mix);
    setNorm (kStereoLink, v.stereoLink);
    setNorm (kNoise, v.noise);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (kCompressLimit))) *p = v.compressLimit;
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (kAnalogMode)))    *p = v.analogMode;
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (kOversampling)))  *p = v.oversampling;
}

NovaGlowPresetManager::Values NovaGlowPresetManager::captureFrom (const juce::AudioProcessorValueTreeState& apvts)
{
    Values v;
    auto getRaw = [&apvts] (const char* id) { auto* p = apvts.getRawParameterValue (id); return p != nullptr ? p->load() : 0.0f; };

    v.peakReduction = getRaw (kPeakReduction);
    v.gain = getRaw (kGain);
    v.compressLimit = (int) getRaw (kCompressLimit);
    v.scHpf = getRaw (kScHpf);
    v.emphasis = getRaw (kEmphasis);
    v.glow = getRaw (kGlow);
    v.analogMode = (int) getRaw (kAnalogMode);
    v.mix = getRaw (kMix);
    v.stereoLink = getRaw (kStereoLink);
    v.noise = getRaw (kNoise);
    v.oversampling = (int) getRaw (kOversampling);
    return v;
}

juce::File NovaGlowPresetManager::getUserPresetDirectory()
{
    // On macOS, userApplicationDataDirectory resolves to ~/Library itself,
    // not ~/Library/Application Support - that subfolder has to be added
    // explicitly to land in the conventional per-app data location.
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support")
                   .getChildFile ("Nova Audio").getChildFile ("Nova Glow").getChildFile ("Presets");
    return dir;
}

juce::var NovaGlowPresetManager::valuesToVar (const juce::String& name, const Values& v)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name);
    obj->setProperty (kPeakReduction, v.peakReduction);
    obj->setProperty (kGain, v.gain);
    obj->setProperty (kCompressLimit, v.compressLimit);
    obj->setProperty (kScHpf, v.scHpf);
    obj->setProperty (kEmphasis, v.emphasis);
    obj->setProperty (kGlow, v.glow);
    obj->setProperty (kAnalogMode, v.analogMode);
    obj->setProperty (kMix, v.mix);
    obj->setProperty (kStereoLink, v.stereoLink);
    obj->setProperty (kNoise, v.noise);
    obj->setProperty (kOversampling, v.oversampling);
    return juce::var (obj);
}

bool NovaGlowPresetManager::valuesFromVar (const juce::var& parsed, juce::String& outName, Values& outValues)
{
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return false;

    outName = obj->getProperty ("name").toString();
    outValues.peakReduction = (float) obj->getProperty (kPeakReduction);
    outValues.gain = (float) obj->getProperty (kGain);
    outValues.compressLimit = (int) obj->getProperty (kCompressLimit);
    outValues.scHpf = (float) obj->getProperty (kScHpf);
    outValues.emphasis = (float) obj->getProperty (kEmphasis);
    outValues.glow = (float) obj->getProperty (kGlow);
    outValues.analogMode = (int) obj->getProperty (kAnalogMode);
    outValues.mix = (float) obj->getProperty (kMix);
    outValues.stereoLink = (float) obj->getProperty (kStereoLink);
    outValues.noise = (float) obj->getProperty (kNoise);
    outValues.oversampling = (int) obj->getProperty (kOversampling);
    return true;
}

bool NovaGlowPresetManager::saveUserPreset (const juce::String& name, const Values& values)
{
    auto dir = getUserPresetDirectory();
    auto result = dir.createDirectory();
    if (result.failed() && ! dir.isDirectory())
        return false;

    auto safeName = name.isNotEmpty() ? name : juce::String ("User Preset");
    auto baseFile = dir.getChildFile (juce::File::createLegalFileName (safeName) + kPresetFileExtension);
    auto file = baseFile;
    int suffix = 2;
    while (file.existsAsFile())
    {
        file = dir.getChildFile (juce::File::createLegalFileName (safeName) + " " + juce::String (suffix) + kPresetFileExtension);
        ++suffix;
    }

    auto json = valuesToVar (safeName, values);
    if (! file.replaceWithText (juce::JSON::toString (json)))
        return false;

    Preset p;
    p.name = safeName;
    p.isFactory = false;
    p.values = values;
    p.sourceFile = file;
    presets.push_back (p);
    setCurrentIndex (getNumPresets() - 1);
    return true;
}
