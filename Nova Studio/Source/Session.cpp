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

            auto clipsVar = object->getProperty("clips");
            if (clipsVar.isArray())
            {
                for (auto& clipVar : *clipsVar.getArray())
                    track.clips.add(ClipFromVar(clipVar));
            }
        }
        return track;
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
        tempoMap.add(tempoPoint);
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

        juce::Array<juce::var> markersVar;
        for (const auto& marker : markers)
            markersVar.add(marker.toVar());

        juce::Array<juce::var> tempoVar;
        for (const auto& tempoPoint : tempoMap)
            tempoVar.add(tempoPoint.toVar());

        root->setProperty("tracks", tracksVar);
        root->setProperty("markers", markersVar);
        root->setProperty("tempoMap", tempoVar);
        root->setProperty("novaAlignSettings", novaAlignSettings);
        root->setProperty("novaAlignPreviewEnabled", previewPlaybackEnabled);

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
        markers.clear();
        tempoMap.clear();

        const juce::var tracksVar = root->getProperty("tracks");
        if (tracksVar.isArray())
        {
            for (auto& trackVar : *tracksVar.getArray())
                tracks.add(Track::fromVar(trackVar));
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

        return true;
    }
}
