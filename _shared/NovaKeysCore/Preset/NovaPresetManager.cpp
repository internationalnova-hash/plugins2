#include "NovaPresetManager.h"

namespace
{
    juce::String waveformToString (NovaOscWaveform w)
    {
        switch (w)
        {
            case NovaOscWaveform::Sine:     return "sine";
            case NovaOscWaveform::Saw:      return "saw";
            case NovaOscWaveform::Square:   return "square";
            case NovaOscWaveform::Triangle: return "triangle";
            case NovaOscWaveform::Noise:    return "noise";
        }
        return "saw";
    }

    NovaOscWaveform waveformFromString (const juce::String& s)
    {
        if (s == "sine")     return NovaOscWaveform::Sine;
        if (s == "square")   return NovaOscWaveform::Square;
        if (s == "triangle") return NovaOscWaveform::Triangle;
        if (s == "noise")    return NovaOscWaveform::Noise;
        return NovaOscWaveform::Saw;
    }

    juce::var layerParamsToVar (const NovaLayerParams& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("on", p.on);
        o->setProperty ("volumeDb", (double) p.volumeDb);
        o->setProperty ("pan", (double) p.pan);
        o->setProperty ("tuneCents", (double) p.tuneCents);
        o->setProperty ("octaveShift", p.octaveShift);
        o->setProperty ("lowKey", p.lowKey);
        o->setProperty ("highKey", p.highKey);
        o->setProperty ("lowVelocity", p.lowVelocity);
        o->setProperty ("highVelocity", p.highVelocity);
        o->setProperty ("filterCutoffHz", (double) p.filterCutoffHz);
        o->setProperty ("filterResonance", (double) p.filterResonance);
        o->setProperty ("filterHighPass", p.filterHighPass);
        o->setProperty ("attackMs", (double) p.attackMs);
        o->setProperty ("decayMs", (double) p.decayMs);
        o->setProperty ("sustain", (double) p.sustain);
        o->setProperty ("releaseMs", (double) p.releaseMs);
        o->setProperty ("filterEnvAmount", (double) p.filterEnvAmount);
        o->setProperty ("filterEnvAttackMs", (double) p.filterEnvAttackMs);
        o->setProperty ("filterEnvDecayMs", (double) p.filterEnvDecayMs);
        o->setProperty ("filterEnvSustain", (double) p.filterEnvSustain);
        o->setProperty ("filterEnvReleaseMs", (double) p.filterEnvReleaseMs);
        return juce::var (o);
    }

    NovaLayerParams layerParamsFromVar (const juce::var& v)
    {
        NovaLayerParams p;
        p.on = (bool) v.getProperty ("on", p.on);
        p.volumeDb = (float) (double) v.getProperty ("volumeDb", (double) p.volumeDb);
        p.pan = (float) (double) v.getProperty ("pan", (double) p.pan);
        p.tuneCents = (float) (double) v.getProperty ("tuneCents", (double) p.tuneCents);
        p.octaveShift = (int) v.getProperty ("octaveShift", p.octaveShift);
        p.lowKey = (int) v.getProperty ("lowKey", p.lowKey);
        p.highKey = (int) v.getProperty ("highKey", p.highKey);
        p.lowVelocity = (int) v.getProperty ("lowVelocity", p.lowVelocity);
        p.highVelocity = (int) v.getProperty ("highVelocity", p.highVelocity);
        p.filterCutoffHz = (float) (double) v.getProperty ("filterCutoffHz", (double) p.filterCutoffHz);
        p.filterResonance = (float) (double) v.getProperty ("filterResonance", (double) p.filterResonance);
        p.filterHighPass = (bool) v.getProperty ("filterHighPass", p.filterHighPass);
        p.attackMs = (float) (double) v.getProperty ("attackMs", (double) p.attackMs);
        p.decayMs = (float) (double) v.getProperty ("decayMs", (double) p.decayMs);
        p.sustain = (float) (double) v.getProperty ("sustain", (double) p.sustain);
        p.releaseMs = (float) (double) v.getProperty ("releaseMs", (double) p.releaseMs);
        p.filterEnvAmount = (float) (double) v.getProperty ("filterEnvAmount", (double) p.filterEnvAmount);
        p.filterEnvAttackMs = (float) (double) v.getProperty ("filterEnvAttackMs", (double) p.filterEnvAttackMs);
        p.filterEnvDecayMs = (float) (double) v.getProperty ("filterEnvDecayMs", (double) p.filterEnvDecayMs);
        p.filterEnvSustain = (float) (double) v.getProperty ("filterEnvSustain", (double) p.filterEnvSustain);
        p.filterEnvReleaseMs = (float) (double) v.getProperty ("filterEnvReleaseMs", (double) p.filterEnvReleaseMs);
        return p;
    }

    juce::var synthParamsToVar (const NovaSynthVoiceParams& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("waveform", waveformToString (p.waveform));
        o->setProperty ("unisonVoices", p.unisonVoices);
        o->setProperty ("unisonDetuneCents", (double) p.unisonDetuneCents);
        o->setProperty ("octaveShift", p.octaveShift);
        o->setProperty ("tuneCents", (double) p.tuneCents);
        o->setProperty ("filterCutoffHz", (double) p.filterCutoffHz);
        o->setProperty ("filterResonance", (double) p.filterResonance);
        o->setProperty ("filterHighPass", p.filterHighPass);
        o->setProperty ("ampAttackMs", (double) p.ampAttackMs);
        o->setProperty ("ampDecayMs", (double) p.ampDecayMs);
        o->setProperty ("ampSustain", (double) p.ampSustain);
        o->setProperty ("ampReleaseMs", (double) p.ampReleaseMs);
        o->setProperty ("lfoRateHz", (double) p.lfoRateHz);
        o->setProperty ("lfoToPitchCents", (double) p.lfoToPitchCents);
        o->setProperty ("lfoToFilterOctaves", (double) p.lfoToFilterOctaves);
        return juce::var (o);
    }

    NovaSynthVoiceParams synthParamsFromVar (const juce::var& v)
    {
        NovaSynthVoiceParams p;
        p.waveform = waveformFromString (v.getProperty ("waveform", "saw").toString());
        p.unisonVoices = (int) v.getProperty ("unisonVoices", p.unisonVoices);
        p.unisonDetuneCents = (float) (double) v.getProperty ("unisonDetuneCents", (double) p.unisonDetuneCents);
        p.octaveShift = (int) v.getProperty ("octaveShift", p.octaveShift);
        p.tuneCents = (float) (double) v.getProperty ("tuneCents", (double) p.tuneCents);
        p.filterCutoffHz = (float) (double) v.getProperty ("filterCutoffHz", (double) p.filterCutoffHz);
        p.filterResonance = (float) (double) v.getProperty ("filterResonance", (double) p.filterResonance);
        p.filterHighPass = (bool) v.getProperty ("filterHighPass", p.filterHighPass);
        p.ampAttackMs = (float) (double) v.getProperty ("ampAttackMs", (double) p.ampAttackMs);
        p.ampDecayMs = (float) (double) v.getProperty ("ampDecayMs", (double) p.ampDecayMs);
        p.ampSustain = (float) (double) v.getProperty ("ampSustain", (double) p.ampSustain);
        p.ampReleaseMs = (float) (double) v.getProperty ("ampReleaseMs", (double) p.ampReleaseMs);
        p.lfoRateHz = (float) (double) v.getProperty ("lfoRateHz", (double) p.lfoRateHz);
        p.lfoToPitchCents = (float) (double) v.getProperty ("lfoToPitchCents", (double) p.lfoToPitchCents);
        p.lfoToFilterOctaves = (float) (double) v.getProperty ("lfoToFilterOctaves", (double) p.lfoToFilterOctaves);
        return p;
    }

    juce::var effectsToVar (const NovaEffectsParams& p)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("eqActive", p.eqActive);
        o->setProperty ("eqLowGainDb", (double) p.eqLowGainDb);
        o->setProperty ("eqMidGainDb", (double) p.eqMidGainDb);
        o->setProperty ("eqHighGainDb", (double) p.eqHighGainDb);
        o->setProperty ("eqLowFreq", (double) p.eqLowFreq);
        o->setProperty ("eqMidFreq", (double) p.eqMidFreq);
        o->setProperty ("eqHighFreq", (double) p.eqHighFreq);
        o->setProperty ("saturationActive", p.saturationActive);
        o->setProperty ("saturationDrive", (double) p.saturationDrive);
        o->setProperty ("saturationMix", (double) p.saturationMix);
        o->setProperty ("compressionActive", p.compressionActive);
        o->setProperty ("compThresholdDb", (double) p.compThresholdDb);
        o->setProperty ("compRatio", (double) p.compRatio);
        o->setProperty ("compAttackMs", (double) p.compAttackMs);
        o->setProperty ("compReleaseMs", (double) p.compReleaseMs);
        o->setProperty ("compMakeupDb", (double) p.compMakeupDb);
        o->setProperty ("chorusActive", p.chorusActive);
        o->setProperty ("chorusRateHz", (double) p.chorusRateHz);
        o->setProperty ("chorusDepth", (double) p.chorusDepth);
        o->setProperty ("chorusMix", (double) p.chorusMix);
        o->setProperty ("delayActive", p.delayActive);
        o->setProperty ("delayTimeMs", (double) p.delayTimeMs);
        o->setProperty ("delayFeedback", (double) p.delayFeedback);
        o->setProperty ("delayMix", (double) p.delayMix);
        o->setProperty ("reverbActive", p.reverbActive);
        o->setProperty ("reverbRoomSize", (double) p.reverbRoomSize);
        o->setProperty ("reverbDamping", (double) p.reverbDamping);
        o->setProperty ("reverbMix", (double) p.reverbMix);
        o->setProperty ("reverbWidth", (double) p.reverbWidth);
        o->setProperty ("outputCeilingDb", (double) p.outputCeilingDb);
        return juce::var (o);
    }

    NovaEffectsParams effectsFromVar (const juce::var& v)
    {
        NovaEffectsParams p;
        p.eqActive = (bool) v.getProperty ("eqActive", p.eqActive);
        p.eqLowGainDb = (float) (double) v.getProperty ("eqLowGainDb", (double) p.eqLowGainDb);
        p.eqMidGainDb = (float) (double) v.getProperty ("eqMidGainDb", (double) p.eqMidGainDb);
        p.eqHighGainDb = (float) (double) v.getProperty ("eqHighGainDb", (double) p.eqHighGainDb);
        p.eqLowFreq = (float) (double) v.getProperty ("eqLowFreq", (double) p.eqLowFreq);
        p.eqMidFreq = (float) (double) v.getProperty ("eqMidFreq", (double) p.eqMidFreq);
        p.eqHighFreq = (float) (double) v.getProperty ("eqHighFreq", (double) p.eqHighFreq);
        p.saturationActive = (bool) v.getProperty ("saturationActive", p.saturationActive);
        p.saturationDrive = (float) (double) v.getProperty ("saturationDrive", (double) p.saturationDrive);
        p.saturationMix = (float) (double) v.getProperty ("saturationMix", (double) p.saturationMix);
        p.compressionActive = (bool) v.getProperty ("compressionActive", p.compressionActive);
        p.compThresholdDb = (float) (double) v.getProperty ("compThresholdDb", (double) p.compThresholdDb);
        p.compRatio = (float) (double) v.getProperty ("compRatio", (double) p.compRatio);
        p.compAttackMs = (float) (double) v.getProperty ("compAttackMs", (double) p.compAttackMs);
        p.compReleaseMs = (float) (double) v.getProperty ("compReleaseMs", (double) p.compReleaseMs);
        p.compMakeupDb = (float) (double) v.getProperty ("compMakeupDb", (double) p.compMakeupDb);
        p.chorusActive = (bool) v.getProperty ("chorusActive", p.chorusActive);
        p.chorusRateHz = (float) (double) v.getProperty ("chorusRateHz", (double) p.chorusRateHz);
        p.chorusDepth = (float) (double) v.getProperty ("chorusDepth", (double) p.chorusDepth);
        p.chorusMix = (float) (double) v.getProperty ("chorusMix", (double) p.chorusMix);
        p.delayActive = (bool) v.getProperty ("delayActive", p.delayActive);
        p.delayTimeMs = (float) (double) v.getProperty ("delayTimeMs", (double) p.delayTimeMs);
        p.delayFeedback = (float) (double) v.getProperty ("delayFeedback", (double) p.delayFeedback);
        p.delayMix = (float) (double) v.getProperty ("delayMix", (double) p.delayMix);
        p.reverbActive = (bool) v.getProperty ("reverbActive", p.reverbActive);
        p.reverbRoomSize = (float) (double) v.getProperty ("reverbRoomSize", (double) p.reverbRoomSize);
        p.reverbDamping = (float) (double) v.getProperty ("reverbDamping", (double) p.reverbDamping);
        p.reverbMix = (float) (double) v.getProperty ("reverbMix", (double) p.reverbMix);
        p.reverbWidth = (float) (double) v.getProperty ("reverbWidth", (double) p.reverbWidth);
        p.outputCeilingDb = (float) (double) v.getProperty ("outputCeilingDb", (double) p.outputCeilingDb);
        return p;
    }

    juce::var macrosToVar (const NovaMacroValues& m)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("tone", (double) m.tone);
        o->setProperty ("motion", (double) m.motion);
        o->setProperty ("space", (double) m.space);
        o->setProperty ("character", (double) m.character);
        return juce::var (o);
    }

    NovaMacroValues macrosFromVar (const juce::var& v)
    {
        NovaMacroValues m;
        m.tone = (float) (double) v.getProperty ("tone", (double) m.tone);
        m.motion = (float) (double) v.getProperty ("motion", (double) m.motion);
        m.space = (float) (double) v.getProperty ("space", (double) m.space);
        m.character = (float) (double) v.getProperty ("character", (double) m.character);
        return m;
    }

    juce::var layerToVar (const NovaPresetLayer& l)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("used", l.used);
        o->setProperty ("sourceType", l.sourceType == NovaLayerSourceType::Synth ? "synth" : "sample");
        o->setProperty ("sampleRef", l.sampleRef);
        o->setProperty ("displayName", l.displayName);
        o->setProperty ("synthParams", synthParamsToVar (l.synthParams));
        o->setProperty ("layerParams", layerParamsToVar (l.layerParams));
        return juce::var (o);
    }

    NovaPresetLayer layerFromVar (const juce::var& v)
    {
        NovaPresetLayer l;
        l.used = (bool) v.getProperty ("used", false);
        l.sourceType = v.getProperty ("sourceType", "sample").toString() == "synth"
                     ? NovaLayerSourceType::Synth : NovaLayerSourceType::Sample;
        l.sampleRef = v.getProperty ("sampleRef", "").toString();
        l.displayName = v.getProperty ("displayName", "").toString();
        l.synthParams = synthParamsFromVar (v.getProperty ("synthParams", juce::var()));
        l.layerParams = layerParamsFromVar (v.getProperty ("layerParams", juce::var()));
        return l;
    }

    juce::var metaToVar (const NovaPresetMeta& m)
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

    NovaPresetMeta metaFromVar (const juce::var& v)
    {
        NovaPresetMeta m;
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
}

juce::var NovaPresetManager::presetToVar (const NovaPreset& preset)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("formatVersion", 1);
    o->setProperty ("meta", metaToVar (preset.meta));

    juce::Array<juce::var> layersArr;
    for (auto& l : preset.layers) layersArr.add (layerToVar (l));
    o->setProperty ("layers", juce::var (layersArr));

    o->setProperty ("effects", effectsToVar (preset.effects));
    o->setProperty ("macros", macrosToVar (preset.macros));
    o->setProperty ("outputVolumeDb", (double) preset.outputVolumeDb);
    return juce::var (o);
}

bool NovaPresetManager::presetFromVar (const juce::var& v, NovaPreset& outPreset)
{
    if (! v.isObject()) return false;

    outPreset.meta = metaFromVar (v.getProperty ("meta", juce::var()));

    if (auto* arr = v.getProperty ("layers", juce::var()).getArray())
        for (int i = 0; i < 4 && i < arr->size(); ++i)
            outPreset.layers[(size_t) i] = layerFromVar ((*arr)[i]);

    outPreset.effects = effectsFromVar (v.getProperty ("effects", juce::var()));
    outPreset.macros = macrosFromVar (v.getProperty ("macros", juce::var()));
    outPreset.outputVolumeDb = (float) (double) v.getProperty ("outputVolumeDb", 0.0);
    return true;
}

bool NovaPresetManager::loadPresetFile (const juce::File& file, NovaPreset& outPreset)
{
    if (! file.existsAsFile()) return false;
    auto parsed = juce::JSON::parse (file);
    if (parsed.isVoid()) return false;
    return presetFromVar (parsed, outPreset);
}

bool NovaPresetManager::savePresetFile (const NovaPreset& preset, const juce::File& file)
{
    const auto jsonText = juce::JSON::toString (presetToVar (preset), false);
    return file.replaceWithText (jsonText);
}

void NovaPresetManager::scanDirectory (const juce::File& directory)
{
    clear();
    if (! directory.isDirectory()) return;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, true, "*.novapreset", juce::File::findFiles))
    {
        NovaPreset preset;
        if (loadPresetFile (entry.getFile(), preset))
            presets.push_back ({ preset, entry.getFile() });
    }

    if (! presets.empty()) selectedIndex = 0;
}

void NovaPresetManager::clear()
{
    presets.clear();
    selectedIndex = -1;
}

const NovaPreset* NovaPresetManager::getPreset (int index) const noexcept
{
    if (index < 0 || index >= (int) presets.size()) return nullptr;
    return &presets[(size_t) index].preset;
}

void NovaPresetManager::setSelectedIndex (int index) noexcept
{
    if (index >= 0 && index < (int) presets.size()) selectedIndex = index;
}

void NovaPresetManager::selectNext() noexcept
{
    if (presets.empty()) return;
    selectedIndex = (selectedIndex + 1) % (int) presets.size();
}

void NovaPresetManager::selectPrevious() noexcept
{
    if (presets.empty()) return;
    selectedIndex = (selectedIndex - 1 + (int) presets.size()) % (int) presets.size();
}

juce::StringArray NovaPresetManager::getCategories() const
{
    juce::StringArray cats;
    cats.add ("All");
    for (auto& lp : presets)
        if (lp.preset.meta.category.isNotEmpty() && ! cats.contains (lp.preset.meta.category))
            cats.add (lp.preset.meta.category);
    return cats;
}

std::vector<int> NovaPresetManager::getFilteredIndices (const juce::String& searchQuery, const juce::String& category, bool favoritesOnly) const
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

bool NovaPresetManager::setFavorite (int index, bool isFavorite)
{
    if (index < 0 || index >= (int) presets.size()) return false;
    presets[(size_t) index].preset.meta.favorite = isFavorite;
    return savePresetFile (presets[(size_t) index].preset, presets[(size_t) index].sourceFile);
}
