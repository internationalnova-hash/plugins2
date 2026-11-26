#include "MixApplier.h"
#include "NovaAIMasterEngine.h"
#include "NovaMasteringPlanXml.h"

namespace NovaStudio::MixAI
{
namespace
{
    void setParams (juce::AudioPluginInstance& inst, const std::vector<Setting>& settings, juce::StringArray& log)
    {
        auto params = inst.getParameters();
        for (const auto& s : settings)
        {
            juce::AudioProcessorParameter* hit = nullptr;
            for (auto* p : params) if (p->getName (64).equalsIgnoreCase (s.name)) { hit = p; break; }
            if (hit == nullptr) { log.add (inst.getName() + ": no setting called \"" + s.name + "\""); continue; }
            const float v = hit->getValueForText (s.value);
            hit->setValueNotifyingHost (juce::jlimit (0.f, 1.f, v));
        }
    }

    // A hosted VST3 wraps the plugin's own state ("VST3PluginState" > IComponent, base64).
    // These two reach the plugin's own getStateInformation / setStateInformation data.
    juce::MemoryBlock getInnerState (juce::AudioPluginInstance& inst)
    {
        juce::MemoryBlock mb;
        inst.getStateInformation (mb);
        if (auto x = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()); x != nullptr && x->hasTagName ("VST3PluginState"))
            if (auto* comp = x->getChildByName ("IComponent"))
            {
                juce::MemoryBlock inner;
                inner.fromBase64Encoding (comp->getAllSubText());
                return inner;
            }
        return mb;   // AU / others: the host already gives the plugin's own blob
    }

    bool setInnerState (juce::AudioPluginInstance& inst, const juce::MemoryBlock& inner)
    {
        juce::MemoryBlock mb;
        inst.getStateInformation (mb);
        auto x = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        if (x == nullptr || ! x->hasTagName ("VST3PluginState"))
        {
            inst.setStateInformation (inner.getData(), (int) inner.getSize());
            return true;
        }
        auto* comp = x->getChildByName ("IComponent");
        if (comp == nullptr) return false;
        comp->deleteAllTextElements();
        comp->addTextElement (inner.toBase64Encoding());
        juce::MemoryBlock out;
        juce::AudioProcessor::copyXmlToBinary (*x, out);
        inst.setStateInformation (out.getData(), (int) out.getSize());
        return true;
    }

    juce::AudioPluginInstance* loadStep (StudioAudioEngine& e, int track, const PluginStep& step, ApplyResult& r)
    {
        juce::PluginDescription d;
        if (! findPlugin (e, step.plugin, d)) { r.missingPlugins.addIfNotAlreadyThere (step.plugin); return nullptr; }
        // set everything up before the plugin goes live, never while the audio thread runs it
        auto configure = [&] (juce::AudioPluginInstance& inst)
        {
            if (step.state.getSize() > 0) setInnerState (inst, step.state);
            setParams (inst, step.settings, r.log);
        };
        if (! e.loadPluginByDescription (d, track, configure)) { r.log.add ("couldn't load " + step.plugin); return nullptr; }
        ++r.pluginsLoaded;
        return track == StudioAudioEngine::kMasterTrackIndex
                   ? e.getMasterPlugin (e.getMasterPluginCount() - 1)
                   : e.getTrackPlugin (track, e.getTrackPluginCount (track) - 1);
    }

    void applySends (StudioAudioEngine& e, int track, const std::vector<Send>& sends, const std::map<juce::String, int>& busOf, ApplyResult& r)
    {
        for (const auto& s : sends)
        {
            auto it = busOf.find (s.toBus);
            if (it == busOf.end()) continue;
            int slot = -1;
            for (int k = 0; k < 6; ++k)
                if (e.getTrackSendBus (track, k) == it->second || e.getTrackSendBus (track, k) < 0) { slot = k; break; }
            if (slot < 0) { r.log.add ("no free send slot on track " + juce::String (track + 1)); continue; }
            e.setTrackSendBus (track, slot, it->second);
            e.setTrackSendLevel (track, slot, s.levelDb);
            e.setTrackSendPreFader (track, slot, false);
        }
    }

    template <typename E> bool enumFromName (const juce::String& name, std::initializer_list<const char*> names, E& out)
    {
        int i = 0;
        for (auto* n : names) { if (name.equalsIgnoreCase (n)) { out = (E) i; return true; } ++i; }
        return false;
    }

    /// Renders the mix as it now stands, runs Nova AI Master's own analysis + decision on it
    /// (the same engine the plugin uses), and returns the plugin state with that plan in it,
    /// exactly as if the user had pressed Analyze and then Master.
    NovaMasterUserPrefs prefsFrom (const PluginStep& step)
    {
        NovaMasterUserPrefs prefs;
        for (const auto& st : step.settings)
        {
            if (st.name == "Style") enumFromName (st.value, { "Clean", "Warm", "Punchy", "Loud", "Open" }, prefs.style);
            else if (st.name == "Loudness") enumFromName (st.value, { "Streaming", "Balanced", "Loud" }, prefs.loudness);
            else if (st.name == "Intensity") prefs.intensity = juce::jlimit (0.f, 1.f, st.value.getFloatValue());
        }
        return prefs;
    }

    void applyEq (StudioAudioEngine& e, int track, const std::vector<EqDip>& eq)
    {
        for (const auto& d : eq)
            e.setTrackEQBand (track, 3, true, d.freq, d.gainDb, d.q);   // band 3: a peaking band (0 = high-pass, 5 = low-pass)
    }
}

bool findPlugin (StudioAudioEngine& e, const juce::String& name, juce::PluginDescription& out)
{
    auto& list = e.getKnownPluginList();
    auto search = [&]
    {
        for (const char* fmt : { "VST3", "AudioUnit" })
            for (const auto& d : list.getTypes())
                if (d.name.equalsIgnoreCase (name) && d.pluginFormatName == fmt) { out = d; return true; }
        return false;
    };
    if (search()) return true;
    // not scanned yet: look for it in the VST3 folders
    for (auto* fmt : e.getPluginFormatManager().getFormats())
    {
        if (fmt->getName() != "VST3") continue;
        for (const auto& f : fmt->searchPathsForPlugins (fmt->getDefaultLocationsToSearch(), true, false))
        {
            if (! juce::File (f).getFileNameWithoutExtension().equalsIgnoreCase (name)) continue;
            juce::OwnedArray<juce::PluginDescription> found;
            list.scanAndAddFile (f, true, found, *fmt);
        }
    }
    return search();
}

juce::String analyseMixForMaster (StudioAudioEngine& e, const PluginStep& step, ApplyResult& r, std::function<bool (float)> progress)
{
    auto tmp = juce::File::createTempFile (".wav");
    StudioAudioEngine::ExportOptions o;
    o.tailSeconds = 0.0;
    if (progress) o.progress = progress;
    const auto res = e.exportAudio (tmp, o);
    if (! res.ok) { r.log.add ("couldn't render the mix for Nova AI Master: " + res.error); tmp.deleteFile(); return {}; }

    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (tmp));
    if (rd == nullptr) { tmp.deleteFile(); return {}; }
    juce::AudioBuffer<float> mix (2, (int) rd->lengthInSamples);
    rd->read (&mix, 0, mix.getNumSamples(), 0, true, true);
    NovaAIMasterEngine eng;
    eng.prepare ({ rd->sampleRate, 4096, 2 });
    eng.analyze (mix);
    eng.finishAnalysis();
    r.masterLufsBefore = eng.getProfile().integratedLufs;
    rd.reset();
    tmp.deleteFile();

    const auto plan = eng.calibrateLoudness (eng.decide (prefsFrom (step)), mix);
    r.log.add ("Nova AI Master heard the mix at " + juce::String (r.masterLufsBefore, 1) + " LUFS -> target "
               + juce::String (plan.output.targetLufs, 1) + ", gain " + juce::String (plan.output.gainDb, 1) + " dB");
    return NovaMasteringPlanXml::planToXml (plan)->toString();
}

bool loadAnalysedMaster (StudioAudioEngine& e, const PluginStep& step, const juce::String& planXml, ApplyResult& r)
{
    juce::PluginDescription d;
    if (! findPlugin (e, step.plugin, d)) { r.missingPlugins.addIfNotAlreadyThere (step.plugin); return false; }
    auto planEl = juce::parseXML (planXml);
    auto configure = [&] (juce::AudioPluginInstance& inst)
    {
        setParams (inst, step.settings, r.log);
        if (planEl == nullptr) return;                                  // no analysis: loads like a fresh Nova AI Master
        const auto mb = getInnerState (inst);
        auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        if (xml == nullptr || ! xml->hasTagName ("NovaAIMasterState")) { r.log.add ("Nova AI Master: unexpected state, plan not set"); return; }
        xml->setAttribute ("buttonState", 4);                           // Complete
        xml->deleteAllChildElementsWithTagName ("MasteringPlan");
        xml->addChildElement (new juce::XmlElement (*planEl));
        juce::MemoryBlock out;
        juce::AudioProcessor::copyXmlToBinary (*xml, out);
        setInnerState (inst, out);
        const auto back = getInnerState (inst);
        auto bx = juce::AudioProcessor::getXmlFromBinary (back.getData(), (int) back.getSize());
        if (bx == nullptr || bx->getChildByName ("MasteringPlan") == nullptr) r.log.add ("Nova AI Master didn't take the plan");
    };
    if (! e.loadPluginByDescription (d, StudioAudioEngine::kMasterTrackIndex, configure)) { r.log.add ("couldn't load " + step.plugin); return false; }
    ++r.pluginsLoaded;
    return true;
}

ApplyResult applyPlan (StudioAudioEngine& e, const Plan& p, std::function<void (float)> progress)
{
    auto r = applyPlanExceptMaster (e, p);
    // Nova AI Master listens to the finished mix last (everything else is already in place)
    for (const auto& step : p.master)
        if (step.analyseMix)
            loadAnalysedMaster (e, step, analyseMixForMaster (e, step, r, progress ? std::function<bool (float)> ([progress] (float f) { progress (f); return true; })
                                                                                      : std::function<bool (float)>()), r);
    return r;
}

ApplyResult applyPlanExceptMaster (StudioAudioEngine& e, const Plan& p)
{
    ApplyResult r;
    std::map<juce::String, int> busOf;      // bus name -> send bus index
    std::map<juce::String, int> busTrack;   // bus name -> its aux track index

    // 1. buses and returns
    for (const auto& b : p.buses)
    {
        e.addTrack (b.name, TrackType::Aux);
        const int ti = e.getTrackCount() - 1;
        auto& t = e.getSession().getTrack (ti);
        busOf[b.name] = t.auxInputBusIndex;
        busTrack[b.name] = ti;
        e.setTrackColour (ti, b.colour);
        e.setTrackVolume (ti, b.volumeDb);
        ++r.busesMade;
    }
    // 2. tracks: route, level, pan, EQ, chain, sends
    for (const auto& m : p.tracks)
    {
        if (! m.change || ! juce::isPositiveAndBelow (m.track, e.getTrackCount())) continue;
        if (m.toBus.isNotEmpty())
            if (auto it = busOf.find (m.toBus); it != busOf.end())
                e.setTrackOutputBus (m.track, StudioAudioEngine::sendBusName (it->second));
        e.addTrackClipGain (m.track, m.clipGainDb);
        e.setTrackVolume (m.track, m.volumeDb);
        e.setTrackPan (m.track, m.pan);
        e.setTrackColour (m.track, roleColour (m.role));
        applyEq (e, m.track, m.eq);
        for (const auto& step : m.chain) loadStep (e, m.track, step, r);
        applySends (e, m.track, m.sends, busOf, r);
        ++r.tracksChanged;
    }
    // 3. bus processing and bus sends
    for (const auto& b : p.buses)
    {
        const int ti = busTrack[b.name];
        applyEq (e, ti, b.eq);
        for (const auto& step : b.chain) loadStep (e, ti, step, r);
        applySends (e, ti, b.sends, busOf, r);
    }
    // 4. mix bus
    for (const auto& step : p.master)
        if (! step.analyseMix) loadStep (e, StudioAudioEngine::kMasterTrackIndex, step, r);
    if (p.builtInLimiterOff) e.setMasterLimiterEnabled (false);

    r.ok = true;
    return r;
}

juce::File takeSnapshot (StudioAudioEngine& e, const juce::String& tag)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Nova Studio Mix Assistant");
    dir.createDirectory();
    auto f = dir.getChildFile (tag + "-" + juce::String (juce::Time::currentTimeMillis()) + ".novastudio");
    return e.saveSession (f) ? f : juce::File();
}

bool restoreSnapshot (StudioAudioEngine& e, const juce::File& snapshot)
{
    if (! snapshot.existsAsFile()) return false;
    const auto keep = e.getCurrentSessionFile();
    const bool ok = e.loadSession (snapshot);
    e.setCurrentSessionFile (keep);          // Save keeps going to the user's own session file
    return ok;
}
}
