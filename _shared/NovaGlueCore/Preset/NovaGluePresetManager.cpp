#include "NovaGluePresetManager.h"

namespace
{
    // Mirrors Nova Glue/Source/ParameterIDs.hpp - duplicated as literals
    // here rather than including across the plugin/Core directory
    // boundary, matching the catalog's convention (see
    // NovaGlowPresetManager.cpp) of Core classes not reaching into a
    // specific plugin's Source/ headers.
    constexpr auto kThreshold = "threshold";
    constexpr auto kAttack = "attack";
    constexpr auto kRelease = "release";
    constexpr auto kMakeup = "makeup";
    constexpr auto kRatio = "ratio";
    constexpr auto kScHpf = "scHpf";
    constexpr auto kMix = "mix";
    constexpr auto kGlue = "glue";
    constexpr auto kDrive = "drive";
    constexpr auto kAutoGain = "autoGain";
    constexpr auto kStereoLink = "stereoLink";
    constexpr auto kOversampling = "oversampling";

    constexpr auto kPresetFileExtension = ".novagluepreset";
}

NovaGluePresetManager::NovaGluePresetManager()
{
    // Factory presets across Glue's main use cases (mix buss, drum buss,
    // vocal buss, aggressive drum limiting), matching the spec's own
    // reference listening targets.
    presets.push_back ({ "Default", true, {} });

    presets.push_back ({ "Mix Bus Glue", true,
        { -8.0f, 5, 4, 1.5f, 0, 80.0f, 100.0f, 55.0f, 10.0f, true, true, 1 } });

    presets.push_back ({ "Drum Bus Punch", true,
        { -14.0f, 4, 1, 2.0f, 1, 100.0f, 100.0f, 35.0f, 15.0f, true, true, 1 } });

    presets.push_back ({ "Vocal Bus Smooth", true,
        { -12.0f, 3, 4, 1.0f, 0, 120.0f, 100.0f, 50.0f, 5.0f, true, true, 1 } });

    presets.push_back ({ "Aggressive Drums", true,
        { -18.0f, 1, 0, 3.0f, 2, 60.0f, 100.0f, 20.0f, 25.0f, false, true, 2 } });
}

void NovaGluePresetManager::scanUserPresets()
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

const NovaGluePresetManager::Preset& NovaGluePresetManager::getPreset (int index) const noexcept
{
    static const Preset fallback {};
    if (index < 0 || index >= getNumPresets())
        return fallback;
    return presets[static_cast<size_t> (index)];
}

juce::StringArray NovaGluePresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : presets)
        names.add (p.name);
    return names;
}

juce::Array<bool> NovaGluePresetManager::getFactoryFlags() const
{
    juce::Array<bool> flags;
    for (const auto& p : presets)
        flags.add (p.isFactory);
    return flags;
}

void NovaGluePresetManager::setCurrentIndex (int index) noexcept
{
    currentIndex = juce::jlimit (0, juce::jmax (0, getNumPresets() - 1), index);
}

void NovaGluePresetManager::selectNext() noexcept
{
    if (getNumPresets() > 0)
        setCurrentIndex ((currentIndex + 1) % getNumPresets());
}

void NovaGluePresetManager::selectPrevious() noexcept
{
    if (getNumPresets() > 0)
        setCurrentIndex ((currentIndex - 1 + getNumPresets()) % getNumPresets());
}

void NovaGluePresetManager::applyTo (juce::AudioProcessorValueTreeState& apvts) const
{
    const auto& v = getPreset (currentIndex).values;

    auto setNorm = [&apvts] (const char* paramId, float value)
    {
        if (auto* p = apvts.getParameter (paramId))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    setNorm (kThreshold, v.thresholdDb);
    setNorm (kMakeup, v.makeupDb);
    setNorm (kScHpf, v.scHpfHz);
    setNorm (kMix, v.mixPercent);
    setNorm (kGlue, v.gluePercent);
    setNorm (kDrive, v.drivePercent);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (kAttack)))        *p = v.attackChoice;
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (kRelease)))        *p = v.releaseChoice;
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (kRatio)))          *p = v.ratioChoice;
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (kOversampling)))   *p = v.oversamplingChoice;
    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (kAutoGain)))         *p = v.autoGain;
    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (kStereoLink)))       *p = v.stereoLink;
}

NovaGluePresetManager::Values NovaGluePresetManager::captureFrom (const juce::AudioProcessorValueTreeState& apvts)
{
    Values v;
    auto getRaw = [&apvts] (const char* id) { auto* p = apvts.getRawParameterValue (id); return p != nullptr ? p->load() : 0.0f; };

    v.thresholdDb = getRaw (kThreshold);
    v.attackChoice = (int) getRaw (kAttack);
    v.releaseChoice = (int) getRaw (kRelease);
    v.makeupDb = getRaw (kMakeup);
    v.ratioChoice = (int) getRaw (kRatio);
    v.scHpfHz = getRaw (kScHpf);
    v.mixPercent = getRaw (kMix);
    v.gluePercent = getRaw (kGlue);
    v.drivePercent = getRaw (kDrive);
    v.autoGain = getRaw (kAutoGain) >= 0.5f;
    v.stereoLink = getRaw (kStereoLink) >= 0.5f;
    v.oversamplingChoice = (int) getRaw (kOversampling);
    return v;
}

bool NovaGluePresetManager::saveUserPreset (const juce::String& name, const Values& values)
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

juce::File NovaGluePresetManager::getUserPresetDirectory()
{
    // On macOS, userApplicationDataDirectory resolves to ~/Library itself,
    // not ~/Library/Application Support - that subfolder has to be added
    // explicitly to land in the conventional per-app data location (a bug
    // found and fixed on Nova Glow's own preset manager).
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support")
                   .getChildFile ("Nova Audio").getChildFile ("Nova Glue").getChildFile ("Presets");
    return dir;
}

juce::var NovaGluePresetManager::valuesToVar (const juce::String& name, const Values& v)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name);
    obj->setProperty (kThreshold, v.thresholdDb);
    obj->setProperty (kAttack, v.attackChoice);
    obj->setProperty (kRelease, v.releaseChoice);
    obj->setProperty (kMakeup, v.makeupDb);
    obj->setProperty (kRatio, v.ratioChoice);
    obj->setProperty (kScHpf, v.scHpfHz);
    obj->setProperty (kMix, v.mixPercent);
    obj->setProperty (kGlue, v.gluePercent);
    obj->setProperty (kDrive, v.drivePercent);
    obj->setProperty (kAutoGain, v.autoGain);
    obj->setProperty (kStereoLink, v.stereoLink);
    obj->setProperty (kOversampling, v.oversamplingChoice);
    return juce::var (obj);
}

bool NovaGluePresetManager::valuesFromVar (const juce::var& parsed, juce::String& outName, Values& outValues)
{
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return false;

    outName = obj->getProperty ("name").toString();
    outValues.thresholdDb = (float) obj->getProperty (kThreshold);
    outValues.attackChoice = (int) obj->getProperty (kAttack);
    outValues.releaseChoice = (int) obj->getProperty (kRelease);
    outValues.makeupDb = (float) obj->getProperty (kMakeup);
    outValues.ratioChoice = (int) obj->getProperty (kRatio);
    outValues.scHpfHz = (float) obj->getProperty (kScHpf);
    outValues.mixPercent = (float) obj->getProperty (kMix);
    outValues.gluePercent = (float) obj->getProperty (kGlue);
    outValues.drivePercent = (float) obj->getProperty (kDrive);
    outValues.autoGain = (bool) obj->getProperty (kAutoGain);
    outValues.stereoLink = (bool) obj->getProperty (kStereoLink);
    outValues.oversamplingChoice = (int) obj->getProperty (kOversampling);
    return true;
}
