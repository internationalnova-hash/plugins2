#include "Session.h"

namespace NovaStudio
{
    static juce::var boolToVar(bool value)
    {
        return value ? juce::var(true) : juce::var(false);
    }

    static bool varToBool(const juce::var& value)
    {
        return value.isBool() ? (bool)value : false;
    }

    juce::var ClipToVar(const Clip& clip)
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("file", clip.file.getFullPathName());
        object->setProperty("startSample", (double)clip.startSample);
        object->setProperty("lengthSamples", (double)clip.lengthSamples);
        object->setProperty("isMidi", boolToVar(clip.isMidi));
        object->setProperty("gainDb", clip.gainDb);
        object->setProperty("muted", boolToVar(clip.muted));
        object->setProperty("locked", boolToVar(clip.locked));
        object->setProperty("aligned", boolToVar(clip.aligned));
    object->setProperty("isPreview", boolToVar(clip.isPreview));
    object->setProperty("alignmentOffsetSamples", (double)clip.alignmentOffsetSamples);
    object->setProperty("originalFile", clip.originalFile.getFullPathName());
    object->setProperty("alignOriginalOffset", (double) clip.alignOriginalOffset);
    object->setProperty("guideTrackIndex", clip.guideTrackIndex);
    object->setProperty("guideClipIndex", clip.guideClipIndex);
    object->setProperty("sourceTrackIndex", clip.sourceTrackIndex);
    object->setProperty("sourceClipIndex", clip.sourceClipIndex);
    object->setProperty("alignVersion", clip.alignVersion);
    object->setProperty("alignmentTimestamp", clip.alignmentTimestamp);
    object->setProperty("alignmentPhraseSensitivity", clip.alignmentPhraseSensitivity);
    object->setProperty("alignmentConsonantPriority", clip.alignmentConsonantPriority);
    object->setProperty("alignmentSourceGuidePath", clip.alignmentSourceGuidePath);
    object->setProperty("alignmentWarpPoints", clip.alignmentWarpPoints);
    object->setProperty("clipColor", clip.clipColor.toString());
    object->setProperty("fadeInSamples", (double)clip.fadeInSamples);
    object->setProperty("fadeOutSamples", (double)clip.fadeOutSamples);
    object->setProperty("fileOffsetSamples", (double)clip.fileOffsetSamples);
    object->setProperty("name", clip.name);
    if (clip.warped)
    {
        object->setProperty("warped", true);
        object->setProperty("warpSourceFile", clip.warpSourceFile.getFullPathName());
        object->setProperty("warpSourceOffset", (double) clip.warpSourceOffset);
        object->setProperty("warpSourceLength", (double) clip.warpSourceLength);
        object->setProperty("warpSourceBpm", clip.warpSourceBpm);
        object->setProperty("warpRatio", clip.warpRatio);
        object->setProperty("warpMode", clip.warpMode);
        object->setProperty("warpFollowTempo", clip.warpFollowTempo);
    }
    if (clip.isMidi)
    {
        juce::Array<juce::var> notesVar;
        for (const auto& n : clip.notes)
        {
            juce::Array<juce::var> nv { n.pitch, n.startBeat, n.lengthBeats, (double)n.velocity };
            notesVar.add(juce::var(nv));
        }
        object->setProperty("notes", notesVar);
    }
    return object.get();
}

Clip ClipFromVar(const juce::var& var)
{
    Clip clip;
    if (auto* object = var.getDynamicObject())
    {
        clip.file = juce::File(object->getProperty("file").toString());
        clip.startSample = static_cast<int64_t>((double)object->getProperty("startSample"));
        clip.lengthSamples = static_cast<int64_t>((double)object->getProperty("lengthSamples"));
        clip.isMidi = varToBool(object->getProperty("isMidi"));
        clip.gainDb = (float)(double)object->getProperty("gainDb");
        clip.muted = varToBool(object->getProperty("muted"));
        clip.locked = varToBool(object->getProperty("locked"));
        clip.aligned = varToBool(object->getProperty("aligned"));
        clip.isPreview = varToBool(object->getProperty("isPreview"));
        clip.alignmentOffsetSamples = static_cast<int64_t>((double)object->getProperty("alignmentOffsetSamples"));
        clip.originalFile = juce::File(object->getProperty("originalFile").toString());
        clip.alignOriginalOffset = (int64_t) (double) object->getProperty("alignOriginalOffset");
        clip.guideTrackIndex = static_cast<int>((double)object->getProperty("guideTrackIndex"));
        clip.guideClipIndex = static_cast<int>((double)object->getProperty("guideClipIndex"));
        clip.sourceTrackIndex = static_cast<int>((double)object->getProperty("sourceTrackIndex"));
        clip.sourceClipIndex = static_cast<int>((double)object->getProperty("sourceClipIndex"));
        clip.alignVersion = static_cast<int>((double)object->getProperty("alignVersion"));
        clip.alignmentTimestamp = object->getProperty("alignmentTimestamp").toString();
        clip.alignmentPhraseSensitivity = (float)(double)object->getProperty("alignmentPhraseSensitivity");
        clip.alignmentConsonantPriority = (float)(double)object->getProperty("alignmentConsonantPriority");
        clip.alignmentSourceGuidePath = object->getProperty("alignmentSourceGuidePath").toString();
        clip.clipColor = juce::Colour::fromString(object->getProperty("clipColor").toString());
        clip.fadeInSamples = static_cast<int64_t>((double)object->getProperty("fadeInSamples"));
        clip.fadeOutSamples = static_cast<int64_t>((double)object->getProperty("fadeOutSamples"));
        clip.fileOffsetSamples = static_cast<int64_t>((double)object->getProperty("fileOffsetSamples"));
        clip.name = object->getProperty("name").toString();
        clip.warped = varToBool(object->getProperty("warped"));
        if (clip.warped)
        {
            clip.warpSourceFile = juce::File(object->getProperty("warpSourceFile").toString());
            clip.warpSourceOffset = (int64_t) (double) object->getProperty("warpSourceOffset");
            clip.warpSourceLength = (int64_t) (double) object->getProperty("warpSourceLength");
            clip.warpSourceBpm = (double) object->getProperty("warpSourceBpm");
            clip.warpRatio = (double) object->getProperty("warpRatio");
            clip.warpMode = (int) object->getProperty("warpMode");
            clip.warpFollowTempo = varToBool(object->getProperty("warpFollowTempo"));
        }
        if (auto* notesArr = object->getProperty("notes").getArray())
        {
            for (auto& nv : *notesArr)
            {
                if (auto* a = nv.getArray(); a != nullptr && a->size() >= 4)
                {
                    MidiNote n;
                    n.pitch = juce::jlimit(0, 127, (int)(*a)[0]);
                    n.startBeat = (double)(*a)[1];
                    n.lengthBeats = juce::jmax(1.0 / 64.0, (double)(*a)[2]);
                    n.velocity = juce::jlimit(0.0f, 1.0f, (float)(double)(*a)[3]);
                    clip.notes.add(n);
                }
            }
        }

        auto warpPointsVar = object->getProperty("alignmentWarpPoints");
        if (warpPointsVar.isArray())
        {
            for (auto& warpPointVar : *warpPointsVar.getArray())
                clip.alignmentWarpPoints.add(warpPointVar);
        }
        return clip;
    }

    return clip;
}

    juce::var AutomationPoint::toVar() const
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("timeSeconds", timeSeconds);
        object->setProperty("value", value);
        return object.get();
    }

    AutomationPoint AutomationPoint::fromVar(const juce::var& var)
    {
        AutomationPoint point;
        if (auto* object = var.getDynamicObject())
        {
            point.timeSeconds = (double)object->getProperty("timeSeconds");
            point.value = (float)(double)object->getProperty("value");
        }
        return point;
    }

    juce::var AutomationLane::toVar() const
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("parameterId", parameterId);
        object->setProperty("enabled", boolToVar(enabled));

        juce::Array<juce::var> pointsVar;
        for (const auto& point : points)
            pointsVar.add(point.toVar());
        object->setProperty("points", pointsVar);
        return object.get();
    }

    AutomationLane AutomationLane::fromVar(const juce::var& var)
    {
        AutomationLane lane;
        if (auto* object = var.getDynamicObject())
        {
            lane.parameterId = object->getProperty("parameterId").toString();
            lane.enabled = varToBool(object->getProperty("enabled"));

            auto pointsVar = object->getProperty("points");
            if (pointsVar.isArray())
            {
                for (auto& pointVar : *pointsVar.getArray())
                    lane.points.add(AutomationPoint::fromVar(pointVar));
            }
        }
        return lane;
    }

    juce::var Track::toVar() const
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("name", name);
        object->setProperty("type", static_cast<int>(type));
        object->setProperty("volumeDb", volumeDb);
        object->setProperty("pan", pan);
        object->setProperty("muted", boolToVar(muted));
        object->setProperty("solo", boolToVar(solo));
        object->setProperty("armed", boolToVar(armed));
        object->setProperty("colour", colour.toString());
        object->setProperty("groupName", groupName);
        object->setProperty("locked", boolToVar(locked));
        object->setProperty("isStereo", boolToVar(isStereo));
        object->setProperty("inputBus", inputBus);
        object->setProperty("outputBus", outputBus);
        object->setProperty("auxInputBusIndex", auxInputBusIndex);
        object->setProperty("inputChannel", inputChannel);
        object->setProperty("monitorInput", boolToVar(monitorInput));
        object->setProperty("automationMode", automationMode);
        object->setProperty("frozen", boolToVar(frozen));
        object->setProperty("frozenFile", frozenFile.getFullPathName());
        object->setProperty("frozenOffsetSamples", (double) frozenOffsetSamples);
        object->setProperty("lockedBeforeFreeze", boolToVar(lockedBeforeFreeze));
        {
            juce::Array<juce::var> sendsVar;
            for (int i = 0; i < 6; ++i)
                sendsVar.add(juce::var(juce::Array<juce::var> { (double)sendLevels[i], sendBusIndex[i], sendPreFader[i] }));
            object->setProperty("sends", sendsVar);
            juce::Array<juce::var> eqVar;
            for (const auto& b : eq)
                eqVar.add(juce::var(juce::Array<juce::var> { b.enabled, (double)b.freq, (double)b.gainDb, (double)b.q }));
            object->setProperty("eq", eqVar);
        }

        juce::Array<juce::var> lanesVar;
        for (const auto& lane : automationLanes)
            lanesVar.add(lane.toVar());
        object->setProperty("automationLanes", lanesVar);

        juce::Array<juce::var> clipsVar;
        for (const auto& clip : clips)
            clipsVar.add(ClipToVar(clip));

        object->setProperty("clips", clipsVar);
        return object.get();
    }

    Track Track::fromVar(const juce::var& var)
    {
        Track track;
        if (auto* object = var.getDynamicObject())
        {
            track.name = object->getProperty("name").toString();
            track.type = static_cast<TrackType>((int)object->getProperty("type"));
            track.volumeDb = (float)(double)object->getProperty("volumeDb");
            track.pan = (float)(double)object->getProperty("pan");
            track.muted = varToBool(object->getProperty("muted"));
            track.solo = varToBool(object->getProperty("solo"));
            track.armed = varToBool(object->getProperty("armed"));
            if (object->hasProperty("colour"))
                track.colour = juce::Colour::fromString(object->getProperty("colour").toString());
            track.groupName = object->getProperty("groupName").toString();
            track.locked = varToBool(object->getProperty("locked"));
            if (object->hasProperty("isStereo")) track.isStereo = varToBool(object->getProperty("isStereo"));
            track.inputBus = object->getProperty("inputBus").toString();
            track.outputBus = object->getProperty("outputBus").toString();
            if (object->hasProperty("auxInputBusIndex")) track.auxInputBusIndex = (int)object->getProperty("auxInputBusIndex");
            track.inputChannel = juce::jmax(0, (int)object->getProperty("inputChannel"));
            track.monitorInput = varToBool(object->getProperty("monitorInput"));
            track.automationMode = juce::jlimit(0, 4, (int)object->getProperty("automationMode"));
            track.frozenFile = juce::File(object->getProperty("frozenFile").toString());
            track.frozen = varToBool(object->getProperty("frozen")) && track.frozenFile.existsAsFile();
            track.frozenOffsetSamples = (int64_t)(double)object->getProperty("frozenOffsetSamples");
            track.lockedBeforeFreeze = varToBool(object->getProperty("lockedBeforeFreeze"));
            if (auto* sends = object->getProperty("sends").getArray())
                for (int i = 0; i < juce::jmin(6, sends->size()); ++i)
                    if (auto* a = (*sends)[i].getArray(); a != nullptr && a->size() >= 3)
                    {
                        track.sendLevels[i]   = (float)(double)(*a)[0];
                        track.sendBusIndex[i] = (int)(*a)[1];
                        track.sendPreFader[i] = (bool)(*a)[2];
                    }
            if (auto* eqArr = object->getProperty("eq").getArray())
                for (int i = 0; i < juce::jmin(6, eqArr->size()); ++i)
                    if (auto* a = (*eqArr)[i].getArray(); a != nullptr && a->size() >= 4)
                    {
                        track.eq[i].enabled = (bool)(*a)[0];
                        track.eq[i].freq    = (float)(double)(*a)[1];
                        track.eq[i].gainDb  = (float)(double)(*a)[2];
                        track.eq[i].q       = (float)(double)(*a)[3];
                    }

            auto lanesVar = object->getProperty("automationLanes");
            if (lanesVar.isArray())
            {
                for (auto& laneVar : *lanesVar.getArray())
                    track.automationLanes.add(AutomationLane::fromVar(laneVar));
            }

            auto clipsVar = object->getProperty("clips");
            if (clipsVar.isArray())
            {
                for (auto& clipVar : *clipsVar.getArray())
                    track.clips.add(ClipFromVar(clipVar));
            }
        }
        return track;
    }

    juce::var MacroTarget::toVar() const
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("trackIndex", trackIndex);
        object->setProperty("parameterId", parameterId);
        object->setProperty("minValue", minValue);
        object->setProperty("maxValue", maxValue);
        return object.get();
    }

    MacroTarget MacroTarget::fromVar(const juce::var& var)
    {
        MacroTarget target;
        if (auto* object = var.getDynamicObject())
        {
            target.trackIndex  = (int)object->getProperty("trackIndex");
            target.parameterId = object->getProperty("parameterId").toString();
            target.minValue    = (float)(double)object->getProperty("minValue");
            target.maxValue    = (float)(double)object->getProperty("maxValue");
        }
        return target;
    }

    juce::var MacroControl::toVar() const
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("name", name);
        object->setProperty("value", value);

        juce::Array<juce::var> targetsVar;
        for (const auto& target : targets)
            targetsVar.add(target.toVar());
        object->setProperty("targets", targetsVar);
        return object.get();
    }

    MacroControl MacroControl::fromVar(const juce::var& var)
    {
        MacroControl macro;
        if (auto* object = var.getDynamicObject())
        {
            macro.name  = object->getProperty("name").toString();
            macro.value = (float)(double)object->getProperty("value");

            auto targetsVar = object->getProperty("targets");
            if (targetsVar.isArray())
                for (auto& targetVar : *targetsVar.getArray())
                    macro.targets.add(MacroTarget::fromVar(targetVar));
        }
        return macro;
    }

    juce::var Marker::toVar() const
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("name", name);
        object->setProperty("timeSeconds", timeSeconds);
        return object.get();
    }

    Marker Marker::fromVar(const juce::var& var)
    {
        Marker marker;
        if (auto* object = var.getDynamicObject())
        {
            marker.name = object->getProperty("name").toString();
            marker.timeSeconds = (double)object->getProperty("timeSeconds");
        }
        return marker;
    }

    juce::var TempoPoint::toVar() const
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("timeSeconds", timeSeconds);
        object->setProperty("bpm", bpm);
        return object.get();
    }

    TempoPoint TempoPoint::fromVar(const juce::var& var)
    {
        TempoPoint tempoPoint;
        if (auto* object = var.getDynamicObject())
        {
            tempoPoint.timeSeconds = (double)object->getProperty("timeSeconds");
            tempoPoint.bpm = (double)object->getProperty("bpm");
        }
        return tempoPoint;
    }

    Session::Session()
    {
        projectName = "Nova Studio Session";
        tempoBpm = 120.0;
        sampleRate = 44100.0;
        tracks.clear();
        markers.clear();
        tempoMap.clear();
    }

    Track& Session::addTrack(const juce::String& name, TrackType type)
    {
        tracks.add(Track(name, type));
        return tracks.getReference(tracks.size() - 1);
    }

    void Session::removeTrack(int index)
    {
        if (isPositiveAndBelow(index, tracks.size()))
            tracks.remove(index);
    }

    void Session::clear()
    {
        tracks.clear();
        markers.clear();
        tempoMap.clear();
        tempoBpm = 120.0;
        sampleRate = 44100.0;
        macros.clear();
        timeSigNumerator = 4; timeSigDenominator = 4;
        metronomeEnabled = false; countInBars = 0;
        loopEnabled = false; loopStartSample = loopEndSample = 0;
        punchEnabled = false; punchInSample = punchOutSample = 0;
        masterVolumeDb = 0.0f; masterLimiter = false;
        stepSequencerState = juce::var(); engineExtras = juce::var();
    }

    int Session::getNumTracks() const noexcept
    {
        return tracks.size();
    }

    const Track& Session::getTrack(int index) const
    {
        jassert(isPositiveAndBelow(index, tracks.size()));
        return tracks.getReference(index);
    }

    Track& Session::getTrack(int index)
    {
        jassert(isPositiveAndBelow(index, tracks.size()));
        return tracks.getReference(index);
    }

    void Session::setTempo(double bpm)
    {
        tempoBpm = bpm;
    }

    double Session::getTempo() const noexcept
    {
        return tempoBpm;
    }

    void Session::setSampleRate(double rate) noexcept
    {
        sampleRate = rate;
    }

    double Session::getSampleRate() const noexcept
    {
        return sampleRate;
    }

    void Session::setName(const juce::String& projectNameIn) noexcept
    {
        projectName = projectNameIn;
    }

    const juce::String& Session::getName() const noexcept
    {
        return projectName;
    }

    MacroControl& Session::addMacro(const juce::String& name)
    {
        MacroControl macro;
        macro.name = name;
        macros.add(macro);
        return macros.getReference(macros.size() - 1);
    }

    void Session::removeMacro(int index)
    {
        if (isPositiveAndBelow(index, macros.size()))
            macros.remove(index);
    }

    void Session::addMarker(const Marker& marker)
    {
        markers.add(marker);
    }

    void Session::removeMarker(int index)
    {
        if (isPositiveAndBelow(index, markers.size()))
            markers.remove(index);
    }

    void Session::renameMarker(int index, const juce::String& newName)
    {
        if (isPositiveAndBelow(index, markers.size()))
            markers.getReference(index).name = newName;
    }

    void Session::addTempoPoint(const TempoPoint& tempoPoint)
    {
        int i = 0;
        while (i < tempoMap.size() && tempoMap.getReference(i).timeSeconds <= tempoPoint.timeSeconds) ++i;
        tempoMap.insert(i, tempoPoint);
    }

    int Session::getNumMarkers() const noexcept
    {
        return markers.size();
    }

    const Marker& Session::getMarker(int index) const
    {
        jassert(isPositiveAndBelow(index, markers.size()));
        return markers.getReference(index);
    }

    bool Session::saveToFile(const juce::File& file) const
    {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty("projectName", projectName);
        root->setProperty("tempoBpm", tempoBpm);
        root->setProperty("sampleRate", sampleRate);

        juce::Array<juce::var> tracksVar;
        for (const auto& track : tracks)
            tracksVar.add(track.toVar());

        juce::Array<juce::var> macrosVar;
        for (const auto& macro : macros)
            macrosVar.add(macro.toVar());

        juce::Array<juce::var> markersVar;
        for (const auto& marker : markers)
            markersVar.add(marker.toVar());

        juce::Array<juce::var> tempoVar;
        for (const auto& tempoPoint : tempoMap)
            tempoVar.add(tempoPoint.toVar());

        root->setProperty("tracks", tracksVar);
        root->setProperty("macros", macrosVar);
        root->setProperty("markers", markersVar);
        root->setProperty("tempoMap", tempoVar);
        root->setProperty("novaAlignSettings", novaAlignSettings);
        root->setProperty("novaAlignPreviewEnabled", previewPlaybackEnabled);
        root->setProperty("formatVersion", 2);
        root->setProperty("timeSigNumerator", timeSigNumerator);
        root->setProperty("timeSigDenominator", timeSigDenominator);
        root->setProperty("metronomeEnabled", metronomeEnabled);
        root->setProperty("metronomeVolume", (double)metronomeVolume);
        root->setProperty("countInBars", countInBars);
        root->setProperty("loopEnabled", loopEnabled);
        root->setProperty("loopStartSample", (double)loopStartSample);
        root->setProperty("loopEndSample", (double)loopEndSample);
        root->setProperty("punchEnabled", punchEnabled);
        root->setProperty("punchInSample", (double)punchInSample);
        root->setProperty("punchOutSample", (double)punchOutSample);
        root->setProperty("masterVolumeDb", (double)masterVolumeDb);
        root->setProperty("masterSafetyCeiling", masterLimiter);   // new key: the old "masterLimiter" was the always-on squash/clip default
        root->setProperty("stepSequencer", stepSequencerState);
        root->setProperty("engineExtras", engineExtras);

        const juce::String jsonString = juce::JSON::toString(juce::var(root.get()), true);
        return file.replaceWithText(jsonString, false, true, "\n");
    }

    bool Session::loadFromFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        const juce::String jsonString = file.loadFileAsString();
        const juce::var parsed = juce::JSON::parse(jsonString);
        if (!parsed.isObject())
            return false;

        auto* root = parsed.getDynamicObject();
        if (root == nullptr)
            return false;

        projectName = root->getProperty("projectName").toString();
        tempoBpm = (double)root->getProperty("tempoBpm");
        sampleRate = (double)root->getProperty("sampleRate");

        tracks.clear();
        macros.clear();
        markers.clear();
        tempoMap.clear();

        const juce::var tracksVar = root->getProperty("tracks");
        if (tracksVar.isArray())
        {
            for (auto& trackVar : *tracksVar.getArray())
                tracks.add(Track::fromVar(trackVar));
        }

        const juce::var macrosVar = root->getProperty("macros");
        if (macrosVar.isArray())
        {
            for (auto& macroVar : *macrosVar.getArray())
                macros.add(MacroControl::fromVar(macroVar));
        }

        const juce::var markersVar = root->getProperty("markers");
        if (markersVar.isArray())
        {
            for (auto& markerVar : *markersVar.getArray())
                markers.add(Marker::fromVar(markerVar));
        }

        const juce::var tempoVar = root->getProperty("tempoMap");
        if (tempoVar.isArray())
        {
            for (auto& tempoPointVar : *tempoVar.getArray())
                tempoMap.add(TempoPoint::fromVar(tempoPointVar));
        }

        // load nova align settings if present
        novaAlignSettings = root->getProperty("novaAlignSettings");
        previewPlaybackEnabled = static_cast<bool>(root->getProperty("novaAlignPreviewEnabled"));

        auto getOr = [root] (const char* key, juce::var fallback) { return root->hasProperty(key) ? root->getProperty(key) : fallback; };
        timeSigNumerator   = juce::jlimit(1, 32, (int)getOr("timeSigNumerator", 4));
        timeSigDenominator = juce::jlimit(1, 32, (int)getOr("timeSigDenominator", 4));
        metronomeEnabled   = (bool)getOr("metronomeEnabled", false);
        metronomeVolume    = (float)(double)getOr("metronomeVolume", 0.6);
        countInBars        = juce::jlimit(0, 4, (int)getOr("countInBars", 0));
        loopEnabled        = (bool)getOr("loopEnabled", false);
        loopStartSample    = (int64_t)(double)getOr("loopStartSample", 0.0);
        loopEndSample      = (int64_t)(double)getOr("loopEndSample", 0.0);
        punchEnabled       = (bool)getOr("punchEnabled", false);
        punchInSample      = (int64_t)(double)getOr("punchInSample", 0.0);
        punchOutSample     = (int64_t)(double)getOr("punchOutSample", 0.0);
        masterVolumeDb     = (float)(double)getOr("masterVolumeDb", 0.0);
        // Older sessions saved "masterLimiter" = true just because it was the default (it compressed
        // and clipped every mix). They open with the master untouched, like Pro Tools.
        masterLimiter      = (bool)getOr("masterSafetyCeiling", false);
        stepSequencerState = root->getProperty("stepSequencer");
        engineExtras       = root->getProperty("engineExtras");
        if (tempoBpm <= 0.0) tempoBpm = 120.0;
        if (sampleRate <= 0.0) sampleRate = 44100.0;

        return true;
    }

    void Session::rescaleToSampleRate(double newRate)
    {
        if (newRate <= 0.0 || sampleRate <= 0.0 || std::abs(newRate - sampleRate) < 0.5)
        {
            if (newRate > 0.0) sampleRate = newRate;
            return;
        }
        const double r = newRate / sampleRate;
        auto sc = [r] (int64_t v) { return (int64_t) std::llround((double) v * r); };
        for (auto& t : tracks)
            for (auto& c : t.clips)
            {
                c.startSample = sc(c.startSample);
                c.lengthSamples = sc(c.lengthSamples);
                c.fadeInSamples = sc(c.fadeInSamples);
                c.fadeOutSamples = sc(c.fadeOutSamples);
                c.fileOffsetSamples = sc(c.fileOffsetSamples);
                c.alignmentOffsetSamples = sc(c.alignmentOffsetSamples);
                c.warpSourceOffset = sc(c.warpSourceOffset);
                c.warpSourceLength = sc(c.warpSourceLength);
            }
        for (auto& t : tracks) t.frozenOffsetSamples = sc(t.frozenOffsetSamples);
        loopStartSample = sc(loopStartSample);  loopEndSample = sc(loopEndSample);
        punchInSample = sc(punchInSample);      punchOutSample = sc(punchOutSample);
        sampleRate = newRate;
    }

    // Tempo map: points are (seconds, bpm) steps; the base tempo applies before the first point.
    double Session::getTempoAtSeconds(double seconds) const noexcept
    {
        double bpm = tempoBpm;
        for (const auto& p : tempoMap)
        {
            if (p.timeSeconds > seconds) break;
            bpm = p.bpm;
        }
        return juce::jlimit(10.0, 999.0, bpm);
    }

    double Session::secondsToBeats(double seconds) const noexcept
    {
        double beats = 0.0, t = 0.0, bpm = juce::jlimit(10.0, 999.0, tempoBpm);
        for (const auto& p : tempoMap)
        {
            if (p.timeSeconds >= seconds) break;
            if (p.timeSeconds > t) { beats += (p.timeSeconds - t) * bpm / 60.0; t = p.timeSeconds; }
            bpm = juce::jlimit(10.0, 999.0, p.bpm);
        }
        return beats + (seconds - t) * bpm / 60.0;
    }

    double Session::beatsToSeconds(double beats) const noexcept
    {
        double b = 0.0, t = 0.0, bpm = juce::jlimit(10.0, 999.0, tempoBpm);
        for (const auto& p : tempoMap)
        {
            if (p.timeSeconds <= t) { bpm = juce::jlimit(10.0, 999.0, p.bpm); continue; }
            const double segBeats = (p.timeSeconds - t) * bpm / 60.0;
            if (b + segBeats >= beats) break;
            b += segBeats; t = p.timeSeconds; bpm = juce::jlimit(10.0, 999.0, p.bpm);
        }
        return t + (beats - b) * 60.0 / bpm;
    }
}
