#include "NovaBassPresetManager.h"

namespace nova::bass
{

namespace
{
    juce::String retriggerToString (NovaVoiceRetrigger r) { return r == NovaVoiceRetrigger::Legato ? "legato" : "retrigger"; }
    NovaVoiceRetrigger retriggerFromString (const juce::String& s) { return s == "legato" ? NovaVoiceRetrigger::Legato : NovaVoiceRetrigger::Retrigger; }

    juce::String glideModeToString (NovaGlideMode m) { return m == NovaGlideMode::Always ? "always" : "legatoOnly"; }
    NovaGlideMode glideModeFromString (const juce::String& s) { return s == "always" ? NovaGlideMode::Always : NovaGlideMode::LegatoOnly; }

    juce::String colorModeToString (NovaColorMode m)
    {
        switch (m) { case NovaColorMode::Clean: return "clean"; case NovaColorMode::Gritty: return "gritty"; default: return "warm"; }
    }
    NovaColorMode colorModeFromString (const juce::String& s)
    {
        if (s == "clean") return NovaColorMode::Clean;
        if (s == "gritty") return NovaColorMode::Gritty;
        return NovaColorMode::Warm;
    }

    juce::String sourceTypeToString (NovaBassSourceType t)
    {
        switch (t) { case NovaBassSourceType::Sample: return "sample"; case NovaBassSourceType::Hybrid: return "hybrid"; default: return "synth"; }
    }
    NovaBassSourceType sourceTypeFromString (const juce::String& s)
    {
        if (s == "sample") return NovaBassSourceType::Sample;
        if (s == "hybrid") return NovaBassSourceType::Hybrid;
        return NovaBassSourceType::Synth;
    }

    juce::String curveToString (NovaMacroCurve c)
    {
        switch (c)
        {
            case NovaMacroCurve::Exponential: return "exponential";
            case NovaMacroCurve::Logarithmic: return "logarithmic";
            case NovaMacroCurve::SCurve:       return "sCurve";
            default:                          return "linear";
        }
    }
    NovaMacroCurve curveFromString (const juce::String& s)
    {
        if (s == "exponential") return NovaMacroCurve::Exponential;
        if (s == "logarithmic") return NovaMacroCurve::Logarithmic;
        if (s == "sCurve")      return NovaMacroCurve::SCurve;
        return NovaMacroCurve::Linear;
    }

    juce::String polarityToString (NovaMacroPolarity p) { return p == NovaMacroPolarity::Inverted ? "inverted" : "normal"; }
    NovaMacroPolarity polarityFromString (const juce::String& s) { return s == "inverted" ? NovaMacroPolarity::Inverted : NovaMacroPolarity::Normal; }

    juce::var voiceParamsToVar (const NovaBassVoiceParams& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("sineTriBlend", (double) p.sineTriBlend);
        o->setProperty ("fineTuneCents", (double) p.fineTuneCents);
        o->setProperty ("pitchEnvAmountSemitones", (double) p.pitchEnvAmountSemitones);
        o->setProperty ("pitchEnvDecaySeconds", (double) p.pitchEnvDecaySeconds);
        o->setProperty ("pitchEnvCurve", (double) p.pitchEnvCurve);
        o->setProperty ("ampAttackSeconds", (double) p.ampAttackSeconds);
        o->setProperty ("ampDecaySeconds", (double) p.ampDecaySeconds);
        o->setProperty ("ampSustain", (double) p.ampSustain);
        o->setProperty ("ampReleaseSeconds", (double) p.ampReleaseSeconds);
        o->setProperty ("transientAmount", (double) p.transientAmount);
        o->setProperty ("transientToneHz", (double) p.transientToneHz);
        o->setProperty ("transientDecaySeconds", (double) p.transientDecaySeconds);
        o->setProperty ("filterHighPass", p.filterHighPass);
        o->setProperty ("filterCutoffHz", (double) p.filterCutoffHz);
        o->setProperty ("filterResonance", (double) p.filterResonance);
        o->setProperty ("retrigger", retriggerToString (p.retrigger));
        o->setProperty ("glideMode", glideModeToString (p.glideMode));
        o->setProperty ("glideTimeSeconds", (double) p.glideTimeSeconds);
        return juce::var (o);
    }

    NovaBassVoiceParams voiceParamsFromVar (const juce::var& v)
    {
        NovaBassVoiceParams p;
        p.sineTriBlend = (float) (double) v.getProperty ("sineTriBlend", (double) p.sineTriBlend);
        p.fineTuneCents = (float) (double) v.getProperty ("fineTuneCents", (double) p.fineTuneCents);
        p.pitchEnvAmountSemitones = (float) (double) v.getProperty ("pitchEnvAmountSemitones", (double) p.pitchEnvAmountSemitones);
        p.pitchEnvDecaySeconds = (float) (double) v.getProperty ("pitchEnvDecaySeconds", (double) p.pitchEnvDecaySeconds);
        p.pitchEnvCurve = (float) (double) v.getProperty ("pitchEnvCurve", (double) p.pitchEnvCurve);
        p.ampAttackSeconds = (float) (double) v.getProperty ("ampAttackSeconds", (double) p.ampAttackSeconds);
        p.ampDecaySeconds = (float) (double) v.getProperty ("ampDecaySeconds", (double) p.ampDecaySeconds);
        p.ampSustain = (float) (double) v.getProperty ("ampSustain", (double) p.ampSustain);
        p.ampReleaseSeconds = (float) (double) v.getProperty ("ampReleaseSeconds", (double) p.ampReleaseSeconds);
        p.transientAmount = (float) (double) v.getProperty ("transientAmount", (double) p.transientAmount);
        p.transientToneHz = (float) (double) v.getProperty ("transientToneHz", (double) p.transientToneHz);
        p.transientDecaySeconds = (float) (double) v.getProperty ("transientDecaySeconds", (double) p.transientDecaySeconds);
        p.filterHighPass = (bool) v.getProperty ("filterHighPass", p.filterHighPass);
        p.filterCutoffHz = (float) (double) v.getProperty ("filterCutoffHz", (double) p.filterCutoffHz);
        p.filterResonance = (float) (double) v.getProperty ("filterResonance", (double) p.filterResonance);
        p.retrigger = retriggerFromString (v.getProperty ("retrigger", "retrigger").toString());
        p.glideMode = glideModeFromString (v.getProperty ("glideMode", "legatoOnly").toString());
        p.glideTimeSeconds = (float) (double) v.getProperty ("glideTimeSeconds", (double) p.glideTimeSeconds);
        return p;
    }

    juce::var stereoParamsToVar (const NovaBassStereoParams& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("widthAmount", (double) p.widthAmount);
        o->setProperty ("crossoverHz", (double) p.crossoverHz);
        return juce::var (o);
    }

    NovaBassStereoParams stereoParamsFromVar (const juce::var& v)
    {
        NovaBassStereoParams p;
        p.widthAmount = (float) (double) v.getProperty ("widthAmount", (double) p.widthAmount);
        p.crossoverHz = (float) (double) v.getProperty ("crossoverHz", (double) p.crossoverHz);
        return p;
    }

    juce::var mappingToVar (const NovaMacroMapping& m)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("targetParam", m.targetParam);
        o->setProperty ("min", (double) m.min);
        o->setProperty ("max", (double) m.max);
        o->setProperty ("curve", curveToString (m.curve));
        o->setProperty ("polarity", polarityToString (m.polarity));
        return juce::var (o);
    }

    NovaMacroMapping mappingFromVar (const juce::var& v)
    {
        NovaMacroMapping m;
        m.targetParam = v.getProperty ("targetParam", "").toString();
        m.min = (float) (double) v.getProperty ("min", (double) m.min);
        m.max = (float) (double) v.getProperty ("max", (double) m.max);
        m.curve = curveFromString (v.getProperty ("curve", "linear").toString());
        m.polarity = polarityFromString (v.getProperty ("polarity", "normal").toString());
        return m;
    }

    juce::var macroTableToVar (const NovaMacroTable& table)
    {
        auto* o = new juce::DynamicObject();
        for (const auto& [macroName, mappings] : table)
        {
            juce::Array<juce::var> arr;
            for (const auto& m : mappings) arr.add (mappingToVar (m));
            o->setProperty (macroName, juce::var (arr));
        }
        return juce::var (o);
    }

    NovaMacroTable macroTableFromVar (const juce::var& v)
    {
        NovaMacroTable table;
        if (auto* obj = v.getDynamicObject())
        {
            for (const auto& prop : obj->getProperties())
            {
                std::vector<NovaMacroMapping> mappings;
                if (auto* arr = prop.value.getArray())
                    for (auto& entry : *arr)
                        mappings.push_back (mappingFromVar (entry));
                table[prop.name.toString()] = std::move (mappings);
            }
        }
        return table;
    }

    juce::var metaToVar (const NovaBassPresetMeta& m)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", m.name);
        o->setProperty ("author", m.author);
        o->setProperty ("category", m.category);
        juce::Array<juce::var> tagsArr;
        for (auto& t : m.tags) tagsArr.add (t);
        o->setProperty ("tags", juce::var (tagsArr));
        o->setProperty ("description", m.description);
        o->setProperty ("artworkRef", m.artworkRef);
        o->setProperty ("favorite", m.favorite);
        return juce::var (o);
    }

    NovaBassPresetMeta metaFromVar (const juce::var& v)
    {
        NovaBassPresetMeta m;
        m.name = v.getProperty ("name", "").toString();
        m.author = v.getProperty ("author", "Nova Audio").toString();
        m.category = v.getProperty ("category", "").toString();
        if (auto* arr = v.getProperty ("tags", juce::var()).getArray())
            for (auto& t : *arr) m.tags.add (t.toString());
        m.description = v.getProperty ("description", "").toString();
        m.artworkRef = v.getProperty ("artworkRef", "").toString();
        m.favorite = (bool) v.getProperty ("favorite", false);
        return m;
    }
} // anonymous namespace

juce::var NovaBassPresetManager::presetToVar (const NovaBassPreset& preset)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("formatVersion", 1);
    o->setProperty ("meta", metaToVar (preset.meta));
    o->setProperty ("sourceType", sourceTypeToString (preset.sourceType));
    o->setProperty ("sampleRef", preset.sampleRef);
    o->setProperty ("voiceParams", voiceParamsToVar (preset.voiceParams));
    o->setProperty ("colorMode", colorModeToString (preset.colorMode));
    o->setProperty ("colorDrive", (double) preset.colorDrive);
    o->setProperty ("stereoParams", stereoParamsToVar (preset.stereoParams));
    o->setProperty ("macros", macroTableToVar (preset.macros));
    o->setProperty ("outputVolumeDb", (double) preset.outputVolumeDb);
    return juce::var (o);
}

bool NovaBassPresetManager::presetFromVar (const juce::var& v, NovaBassPreset& outPreset)
{
    if (! v.isObject()) return false;

    outPreset.meta = metaFromVar (v.getProperty ("meta", juce::var()));
    outPreset.sourceType = sourceTypeFromString (v.getProperty ("sourceType", "synth").toString());
    outPreset.sampleRef = v.getProperty ("sampleRef", "").toString();
    outPreset.voiceParams = voiceParamsFromVar (v.getProperty ("voiceParams", juce::var()));
    outPreset.colorMode = colorModeFromString (v.getProperty ("colorMode", "warm").toString());
    outPreset.colorDrive = (float) (double) v.getProperty ("colorDrive", (double) outPreset.colorDrive);
    outPreset.stereoParams = stereoParamsFromVar (v.getProperty ("stereoParams", juce::var()));
    outPreset.macros = macroTableFromVar (v.getProperty ("macros", juce::var()));
    outPreset.outputVolumeDb = (float) (double) v.getProperty ("outputVolumeDb", 0.0);
    return true;
}

bool NovaBassPresetManager::loadPresetFile (const juce::File& file, NovaBassPreset& outPreset)
{
    if (! file.existsAsFile()) return false;
    auto parsed = juce::JSON::parse (file);
    if (parsed.isVoid()) return false;
    return presetFromVar (parsed, outPreset);
}

bool NovaBassPresetManager::savePresetFile (const NovaBassPreset& preset, const juce::File& file)
{
    const auto jsonText = juce::JSON::toString (presetToVar (preset), false);
    return file.replaceWithText (jsonText);
}

void NovaBassPresetManager::scanDirectory (const juce::File& directory)
{
    clear();
    if (! directory.isDirectory()) return;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, true, "*.novabasspreset", juce::File::findFiles))
    {
        NovaBassPreset preset;
        if (loadPresetFile (entry.getFile(), preset))
            presets.push_back ({ preset, entry.getFile() });
    }

    if (! presets.empty()) selectedIndex = 0;
}

void NovaBassPresetManager::clear()
{
    presets.clear();
    selectedIndex = -1;
}

const NovaBassPreset* NovaBassPresetManager::getPreset (int index) const noexcept
{
    if (index < 0 || index >= (int) presets.size()) return nullptr;
    return &presets[(size_t) index].preset;
}

void NovaBassPresetManager::setSelectedIndex (int index) noexcept
{
    if (index >= 0 && index < (int) presets.size()) selectedIndex = index;
}

void NovaBassPresetManager::selectNext() noexcept
{
    if (presets.empty()) return;
    selectedIndex = (selectedIndex + 1) % (int) presets.size();
}

void NovaBassPresetManager::selectPrevious() noexcept
{
    if (presets.empty()) return;
    selectedIndex = (selectedIndex - 1 + (int) presets.size()) % (int) presets.size();
}

juce::StringArray NovaBassPresetManager::getCategories() const
{
    juce::StringArray cats;
    cats.add ("All");
    for (auto& lp : presets)
        if (lp.preset.meta.category.isNotEmpty() && ! cats.contains (lp.preset.meta.category))
            cats.add (lp.preset.meta.category);
    return cats;
}

std::vector<int> NovaBassPresetManager::getFilteredIndices (const juce::String& searchQuery, const juce::String& category, bool favoritesOnly) const
{
    std::vector<int> result;
    const auto q = searchQuery.toLowerCase();

    for (int i = 0; i < (int) presets.size(); ++i)
    {
        const auto& p = presets[(size_t) i].preset;
        if (favoritesOnly && ! p.meta.favorite) continue;
        if (category.isNotEmpty() && category != "All" && p.meta.category != category) continue;
        if (q.isNotEmpty() && ! p.meta.name.toLowerCase().contains (q)) continue;
        result.push_back (i);
    }
    return result;
}

bool NovaBassPresetManager::setFavorite (int index, bool isFavorite)
{
    if (index < 0 || index >= (int) presets.size()) return false;
    presets[(size_t) index].preset.meta.favorite = isFavorite;
    return savePresetFile (presets[(size_t) index].preset, presets[(size_t) index].sourceFile);
}

} // namespace nova::bass
