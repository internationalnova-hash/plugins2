// Nova Studio engine: snapshots, transport control, tracks, recording, MIDI, export and
// session I/O. Everything in this file runs on the message thread.
#include "StudioAudioEngine.h"

namespace NovaStudio
{
    namespace
    {
        inline float dbToGain(float db) noexcept { return db <= -99.0f ? 0.0f : juce::Decibels::decibelsToGain(db); }

        struct Hasher
        {
            uint64_t h = 1469598103934665603ull;
            void add(const void* data, size_t n) { auto* p = (const unsigned char*) data; for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; } }
            template <typename T> void v(const T& x) { add(&x, sizeof(T)); }
            void s(const juce::String& str) { v(str.hashCode64()); }
        };

        juce::String legalName(const juce::String& s)
        {
            auto r = juce::File::createLegalFileName(s.trim());
            return r.isEmpty() ? juce::String("Untitled") : r;
        }

        juce::File autosaveFolder()
        {
            return StudioAudioEngine::getDefaultSessionsFolder().getChildFile("Autosave");
        }
    }

    //==========================================================================
    // Snapshots
    //==========================================================================
    uint64_t StudioAudioEngine::computeSessionHash() const
    {
        Hasher h;
        h.v(session.getNumTracks());
        h.v(session.getTempo());
        h.v(session.getSampleRate());
        h.v(session.timeSigNumerator); h.v(session.timeSigDenominator);
        h.v(session.isPreviewPlaybackEnabled());
        h.v(session.loopEnabled); h.v(session.loopStartSample); h.v(session.loopEndSample);
        h.v(session.punchEnabled); h.v(session.punchInSample); h.v(session.punchOutSample);
        h.v(session.masterVolumeDb); h.v(session.masterLimiter); h.v(session.metronomeEnabled); h.v(session.countInBars);
        h.s(session.getName());
        for (const auto& tp : session.getTempoMap()) { h.v(tp.timeSeconds); h.v(tp.bpm); }
        for (int i = 0; i < session.getNumMarkers(); ++i) { h.v(session.getMarker(i).timeSeconds); h.s(session.getMarker(i).name); }

        for (int t = 0; t < session.getNumTracks(); ++t)
        {
            const auto& tr = session.getTrack(t);
            h.s(tr.name); h.v((int) tr.type); h.v(tr.volumeDb); h.v(tr.pan); h.v(tr.muted); h.v(tr.solo);
            h.v(tr.armed); h.v(tr.isStereo); h.v(tr.inputChannel); h.v(tr.monitorInput); h.v(tr.automationMode);
            h.v(tr.auxInputBusIndex); h.v(tr.colour.getARGB()); h.s(tr.groupName);
            h.s(tr.outputBus); h.v(tr.frozen); h.s(tr.frozenFile.getFullPathName()); h.v(tr.frozenOffsetSamples);
            for (int i = 0; i < 6; ++i) { h.v(tr.sendLevels[i]); h.v(tr.sendBusIndex[i]); h.v(tr.sendPreFader[i]); }
            for (auto& b : tr.eq) { h.v(b.enabled); h.v(b.freq); h.v(b.gainDb); h.v(b.q); }
            for (const auto& c : tr.clips)
            {
                h.s(c.file.getFullPathName()); h.v(c.startSample); h.v(c.lengthSamples); h.v(c.fileOffsetSamples);
                h.v(c.alignmentOffsetSamples); h.v(c.gainDb); h.v(c.fadeInSamples); h.v(c.fadeOutSamples);
                h.v(c.muted); h.v(c.isMidi); h.v(c.isPreview); h.v(c.sourceTrackIndex); h.v(c.sourceClipIndex);
                h.s(c.name); h.v(c.clipColor.getARGB());
                for (const auto& n : c.notes) { h.v(n.pitch); h.v(n.startBeat); h.v(n.lengthBeats); h.v(n.velocity); }
            }
            for (const auto& lane : tr.automationLanes)
            {
                h.s(lane.parameterId); h.v(lane.enabled);
                for (const auto& pt : lane.points) { h.v(pt.timeSeconds); h.v(pt.value); }
            }
        }
        return h.h;
    }

    std::shared_ptr<ClipStreamer> StudioAudioEngine::getStreamer(const juce::File& file, int64_t startSessionSamples, int64_t pinSessionSamples)
    {
        const double sr = session.getSampleRate();
        const juce::String key = file.getFullPathName() + "|" + juce::String(startSessionSamples) + "|"
                               + juce::String(pinSessionSamples) + "|" + juce::String(sr, 1);
        auto it = streamerCache.find(key);
        if (it != streamerCache.end())
            if (auto existing = it->second.lock())
                return existing;

        auto s = std::make_shared<ClipStreamer>(clipFormats, file, startSessionSamples, sr);
        if (!s->isValid())
            return nullptr;
        if (pinSessionSamples >= 0)
        {
            const double ratio = s->getFileSampleRate() / juce::jmax(1.0, sr);
            s->setPinnedRegion((int64_t) std::llround((double) pinSessionSamples * ratio),
                               (int64_t) (s->getFileSampleRate() * ClipStreamer::kHeadSeconds));
        }
        streamerCache[key] = s;
        registeredStreamers.push_back(s);
        readAheadThread.addTimeSliceClient(s.get());
        return s;
    }

    std::unique_ptr<PlaySnapshot> StudioAudioEngine::buildSnapshot(uint64_t hash)
    {
        auto snap = std::make_unique<PlaySnapshot>();
        const double sr = session.getSampleRate();
        snap->sampleRate = sr;
        snap->baseBpm = juce::jlimit(10.0, 999.0, session.getTempo());
        for (const auto& tp : session.getTempoMap())
            snap->tempoMap.emplace_back(tp.timeSeconds, juce::jlimit(10.0, 999.0, tp.bpm));
        snap->timeSigNum = session.timeSigNumerator;
        snap->timeSigDen = session.timeSigDenominator;
        snap->hash = hash;

        const bool preview = session.isPreviewPlaybackEnabled();
        const bool pinLoop = session.loopEnabled && session.loopEndSample > session.loopStartSample;

        snap->tracks.resize((size_t) session.getNumTracks());
        for (int t = 0; t < session.getNumTracks(); ++t)
        {
            const auto& tr = session.getTrack(t);
            auto& td = snap->tracks[(size_t) t];
            td.automationMode = tr.automationMode;
            for (const auto& lane : tr.automationLanes)
                if (lane.enabled && !lane.points.isEmpty())
                    td.lanes.push_back(lane);

            if (tr.frozen && tr.frozenFile.existsAsFile())
            {
                if (auto src = getStreamer(tr.frozenFile, tr.frozenOffsetSamples, -1))
                {
                    const double ratio = sr / src->getFileSampleRate();
                    PlayClip pc;
                    pc.src = src;
                    pc.start = 0;
                    pc.length = (int64_t) ((double) src->getFileLength() * ratio) - tr.frozenOffsetSamples;
                    pc.srcStart = tr.frozenOffsetSamples;
                    pc.fullLength = pc.length;
                    if (pc.length > 0) td.clips.push_back(pc);
                }
                continue;
            }

            // Align previews replace the clips they were made from.
            std::vector<int> replaced;
            if (preview)
                for (const auto& c : tr.clips)
                    if (c.isPreview && c.sourceTrackIndex == t)
                        replaced.push_back(c.sourceClipIndex);

            std::vector<PlayClip> ordered;   // clip-list order = stacking order (later is on top)
            for (int ci = 0; ci < tr.clips.size(); ++ci)
            {
                const auto& c = tr.clips.getReference(ci);
                if (c.muted || c.lengthSamples <= 0) continue;

                if (c.isMidi)
                {
                    const double clipStartBeat = session.secondsToBeats((double) c.startSample / sr);
                    const int64_t clipEnd = c.startSample + c.lengthSamples;
                    for (const auto& n : c.notes)
                    {
                        PlayNote pn;
                        pn.pitch = juce::jlimit(0, 127, n.pitch);
                        pn.velocity = (juce::uint8) juce::jlimit(1, 127, (int) std::lround(n.velocity * 127.0f));
                        pn.on  = (int64_t) std::llround(session.beatsToSeconds(clipStartBeat + n.startBeat) * sr);
                        pn.off = (int64_t) std::llround(session.beatsToSeconds(clipStartBeat + n.startBeat + n.lengthBeats) * sr);
                        if (pn.on < c.startSample || pn.on >= clipEnd) continue;
                        pn.off = juce::jlimit(pn.on + 1, clipEnd, pn.off);
                        td.notes.push_back(pn);
                    }
                    continue;
                }

                if (c.isPreview && !preview) continue;
                if (std::find(replaced.begin(), replaced.end(), ci) != replaced.end()) continue;
                if (!c.file.existsAsFile()) continue;

                const int64_t srcStart = c.fileOffsetSamples + c.alignmentOffsetSamples;
                int64_t pin = -1;
                if (pinLoop && session.loopStartSample > c.startSample && session.loopStartSample < c.startSample + c.lengthSamples)
                    pin = srcStart + (session.loopStartSample - c.startSample);

                auto src = getStreamer(c.file, srcStart, pin);
                if (src == nullptr) continue;

                PlayClip pc;
                pc.src = src;
                pc.start = c.startSample;
                pc.length = c.lengthSamples;
                pc.srcStart = srcStart;
                pc.gain = dbToGain(c.gainDb);
                pc.fadeIn = c.fadeInSamples;
                pc.fadeOut = c.fadeOutSamples;
                pc.fadeOffset = 0;
                pc.fullLength = c.lengthSamples;
                ordered.push_back(pc);
            }

            // Overlaps: the clip on top (later in the list) plays; lower clips are cut around it.
            std::vector<std::pair<int64_t, int64_t>> covered;
            for (int i = (int) ordered.size() - 1; i >= 0; --i)
            {
                const auto& c = ordered[(size_t) i];
                std::vector<std::pair<int64_t, int64_t>> visible { { c.start, c.start + c.length } };
                for (const auto& cv : covered)
                {
                    std::vector<std::pair<int64_t, int64_t>> next;
                    for (const auto& v : visible)
                    {
                        if (cv.second <= v.first || cv.first >= v.second) { next.push_back(v); continue; }
                        if (cv.first > v.first) next.push_back({ v.first, cv.first });
                        if (cv.second < v.second) next.push_back({ cv.second, v.second });
                    }
                    visible.swap(next);
                }
                for (const auto& v : visible)
                {
                    PlayClip seg = c;
                    seg.fadeOffset = v.first - c.start;
                    seg.srcStart = c.srcStart + seg.fadeOffset;
                    seg.start = v.first;
                    seg.length = v.second - v.first;
                    if (seg.length > 0) td.clips.push_back(seg);
                }
                covered.push_back({ c.start, c.start + c.length });
            }

            std::sort(td.clips.begin(), td.clips.end(), [] (const PlayClip& a, const PlayClip& b) { return a.start < b.start; });
            std::sort(td.notes.begin(), td.notes.end(), [] (const PlayNote& a, const PlayNote& b) { return a.on < b.on; });
        }
        return snap;
    }

    void StudioAudioEngine::installSnapshot(std::unique_ptr<PlaySnapshot> snap)
    {
        lastSnapshotHash = snap->hash;
        auto old = std::move(currentSnapshot);
        currentSnapshot = std::move(snap);
        liveSnapshot.store(currentSnapshot.get(), std::memory_order_release);
        if (old != nullptr)
            retiredSnapshots.push_back({ std::move(old), juce::Time::getMillisecondCounter() });
    }

    void StudioAudioEngine::publishSnapshot(bool force)
    {
        const uint64_t hash = computeSessionHash();
        if (!force && currentSnapshot != nullptr && hash == lastSnapshotHash)
            return;
        installSnapshot(buildSnapshot(hash));
    }

    void StudioAudioEngine::retireOldSnapshots(bool all)
    {
        const auto now = juce::Time::getMillisecondCounter();
        retiredSnapshots.erase(std::remove_if(retiredSnapshots.begin(), retiredSnapshots.end(),
                                              [&] (const Retired& r) { return all || now - r.time > 1000; }),
                               retiredSnapshots.end());

        // Streamers nobody plays any more: stop streaming and free them.
        for (auto it = registeredStreamers.begin(); it != registeredStreamers.end();)
        {
            if (it->use_count() == 1)
            {
                readAheadThread.removeTimeSliceClient(it->get());
                it = registeredStreamers.erase(it);
            }
            else ++it;
        }
        for (auto it = streamerCache.begin(); it != streamerCache.end();)
            it = it->second.expired() ? streamerCache.erase(it) : std::next(it);
    }

    void StudioAudioEngine::notifySessionChanged()
    {
        refreshTrackPlaybackStates();
        publishSnapshot(false);
    }

    void StudioAudioEngine::prefetchAt(int64_t timelinePos)
    {
        auto* snap = currentSnapshot.get();
        if (snap == nullptr) return;
        const double sr = currentSampleRate;
        const int64_t horizon = timelinePos + (int64_t) (sr * 0.5);
        for (auto& td : snap->tracks)
            for (auto& c : td.clips)
                if (c.src != nullptr && c.start <= horizon && c.start + c.length > timelinePos)
                {
                    const int64_t rel = juce::jmax((int64_t) 0, timelinePos - c.start);
                    const double ratio = c.src->getFileSampleRate() / sr;
                    c.src->prefetchBlocking((int64_t) std::llround((double) (c.srcStart + rel) * ratio), 400);
                }
    }

    void StudioAudioEngine::timerCallback()
    {
        publishSnapshot(false);
        retireOldSnapshots(false);

        if (recordingActive.load())
        {
            int s1, n1, s2, n2;
            const int ready = recMidiFifo.getNumReady();
            recMidiFifo.prepareToRead(ready, s1, n1, s2, n2);
            for (int i = 0; i < n1; ++i) recordedMidi.add(recMidiData[(size_t) (s1 + i)]);
            for (int i = 0; i < n2; ++i) recordedMidi.add(recMidiData[(size_t) (s2 + i)]);
            recMidiFifo.finishedRead(n1 + n2);
        }

        writeAutomationTick();
        autosaveIfNeeded();
    }

    //==========================================================================
    // Transport
    //==========================================================================
    void StudioAudioEngine::play()
    {
        if (transportState.isPlaying())
            return;
        publishSnapshot(false);
        prefetchAt(transportState.getPositionSamples());
        transportState.play();
    }

    void StudioAudioEngine::stop()
    {
        const bool wasRecording = recordingActive.load() || countInRemaining.load() > 0;
        transportState.stop();
        countInRemaining.store(0);
        pendingRecordStart.store(false);
        if (wasRecording)
            stopRecordingInternal();
        {
            const juce::ScopedLock sl(playerLock);
            for (auto& p : trackPlayers) p->allNotesOffPending = true;
        }
        for (auto& w : autoWrite) { w.latched = false; w.lastWriteSec = -1.0; }
        for (auto& p : trackPlayers) p->writingAutomation.store(false);
    }

    void StudioAudioEngine::seek(int64_t samplePosition)
    {
        samplePosition = juce::jmax((int64_t) 0, samplePosition);
        const bool wasPlaying = transportState.isPlaying() && !recordingActive.load();
        if (wasPlaying) transportState.stop();
        transportState.setPositionSamples(samplePosition);
        prefetchAt(samplePosition);
        {
            const juce::ScopedLock sl(playerLock);
            for (auto& p : trackPlayers) p->allNotesOffPending = true;
        }
        if (wasPlaying) transportState.play();
    }

    void StudioAudioEngine::applyTempoEdit(const std::function<void()>& edit)
    {
        // Remember musical positions (in beats) before the edit...
        struct Pos { int t, c; double startBeat, endBeat; };
        std::vector<Pos> keep;
        for (int t = 0; t < session.getNumTracks(); ++t)
        {
            const auto& clips = session.getTrack(t).clips;
            for (int c = 0; c < clips.size(); ++c)
            {
                const auto& cl = clips.getReference(c);
                if (cl.isMidi || (cl.warped && cl.warpFollowTempo && cl.warpSourceBpm > 0.0))
                    keep.push_back({ t, c, session.samplesToBeats(cl.startSample), session.samplesToBeats(cl.startSample + cl.lengthSamples) });
            }
        }
        const double loopA = session.samplesToBeats(session.loopStartSample), loopB = session.samplesToBeats(session.loopEndSample);
        const double punchA = session.samplesToBeats(session.punchInSample), punchB = session.samplesToBeats(session.punchOutSample);
        const auto playBeat = session.samplesToBeats(transportState.getPositionSamples());

        edit();

        // ...and put everything back on the same bars.
        int rewarped = 0;
        for (const auto& k : keep)
        {
            auto& cl = session.getTrack(k.t).clips.getReference(k.c);
            cl.startSample = session.beatsToSamples(k.startBeat);
            if (cl.isMidi)
                cl.lengthSamples = juce::jmax((int64_t) 1, session.beatsToSamples(k.endBeat) - cl.startSample);
            else
            {
                const double ratio = cl.warpSourceBpm / session.getTempoAtSeconds((double) cl.startSample / session.getSampleRate());
                if (std::abs(ratio - cl.warpRatio) > 1.0e-6 && renderWarp(cl, ratio, cl.warpMode, nullptr))
                    ++rewarped;
            }
        }
        session.loopStartSample = session.beatsToSamples(loopA);
        session.loopEndSample = session.beatsToSamples(loopB);
        session.punchInSample = session.beatsToSamples(punchA);
        session.punchOutSample = session.beatsToSamples(punchB);
        transportState.setLoopRange(session.loopStartSample, session.loopEndSample);
        transportState.setPunchRange(session.punchInSample, session.punchOutSample);
        if (!transportState.isPlaying())
            transportState.setPositionSamples(session.beatsToSamples(playBeat), false);
        transportState.setTempo(session.getTempo());
        if (rewarped > 0)
            juce::Logger::writeToLog("Re-warped " + juce::String(rewarped) + " clip(s) for the tempo change");
        publishSnapshot(false);
    }

    void StudioAudioEngine::setTempo(double bpm)
    {
        bpm = juce::jlimit(10.0, 999.0, bpm);
        if (std::abs(bpm - session.getTempo()) < 1.0e-9) { transportState.setTempo(bpm); return; }
        // The base tempo scales the whole map (changes keep their bars and their own tempos).
        auto changes = getTempoChanges();
        applyTempoEdit([&]
        {
            session.setTempo(bpm);
            std::vector<std::pair<double, double>> beatBpm;
            const double bpb = session.beatsPerBar();
            for (const auto& ch : changes)
                if (ch.bar > 1.0) beatBpm.push_back({ (ch.bar - 1.0) * bpb, ch.bpm });
            session.setTempoMapFromBeats(beatBpm);
        });
    }

    std::vector<StudioAudioEngine::TempoChange> StudioAudioEngine::getTempoChanges() const
    {
        std::vector<TempoChange> out;
        const double bpb = session.beatsPerBar();
        for (const auto& p : session.getTempoMap())
            out.push_back({ session.secondsToBeats(p.timeSeconds) / bpb + 1.0, p.bpm });
        return out;
    }

    void StudioAudioEngine::setTempoChanges(const std::vector<TempoChange>& changes)
    {
        applyTempoEdit([&]
        {
            std::vector<std::pair<double, double>> beatBpm;
            const double bpb = session.beatsPerBar();
            for (const auto& ch : changes)
                if (ch.bar > 1.0) beatBpm.push_back({ (ch.bar - 1.0) * bpb, ch.bpm });
            session.setTempoMapFromBeats(beatBpm);
        });
    }

    double StudioAudioEngine::getTempoAt(int64_t samplePosition) const
    {
        return session.getTempoAtSeconds((double) samplePosition / juce::jmax(1.0, session.getSampleRate()));
    }

    void StudioAudioEngine::setTimeSignature(int numerator, int denominator)
    {
        session.timeSigNumerator = juce::jlimit(1, 32, numerator);
        session.timeSigDenominator = juce::jlimit(1, 32, denominator);
        transportState.setTimeSignature(session.timeSigNumerator, session.timeSigDenominator);
        publishSnapshot(false);
    }

    void StudioAudioEngine::setLoopRange(int64_t start, int64_t end, bool enabled)
    {
        session.loopStartSample = juce::jmax((int64_t) 0, start);
        session.loopEndSample = juce::jmax(session.loopStartSample, end);
        session.loopEnabled = enabled && session.loopEndSample > session.loopStartSample;
        transportState.setLoopRange(session.loopStartSample, session.loopEndSample);
        transportState.setLooping(session.loopEnabled);
        publishSnapshot(false);
    }

    void StudioAudioEngine::setPunchRange(int64_t in, int64_t out, bool enabled)
    {
        session.punchInSample = juce::jmax((int64_t) 0, in);
        session.punchOutSample = juce::jmax(session.punchInSample, out);
        session.punchEnabled = enabled && session.punchOutSample > session.punchInSample;
        transportState.setPunchRange(session.punchInSample, session.punchOutSample);
        transportState.setPunchEnabled(session.punchEnabled);
    }

    void StudioAudioEngine::setMetronomeEnabled(bool on) { session.metronomeEnabled = on; metronomeOn.store(on); }
    void StudioAudioEngine::setMetronomeVolume(float g) { session.metronomeVolume = juce::jlimit(0.0f, 1.0f, g); metronomeGain.store(session.metronomeVolume); }
    void StudioAudioEngine::setCountInBars(int bars) { session.countInBars = juce::jlimit(0, 4, bars); }
    void StudioAudioEngine::setRecordingOffsetSamples(int samples) { recordingOffsetSamples.store(juce::jlimit(-48000, 48000, samples)); }
    void StudioAudioEngine::setMasterVolume(float db) { session.masterVolumeDb = juce::jlimit(-100.0f, 12.0f, db); masterVolumeDbAtomic.store(session.masterVolumeDb); }
    void StudioAudioEngine::setMasterLimiterEnabled(bool on) { session.masterLimiter = on; limiterOn.store(on); }

    int StudioAudioEngine::getRoundTripLatencySamples() const
    {
        if (auto* device = const_cast<juce::AudioDeviceManager&>(deviceManager).getCurrentAudioDevice())
            return device->getInputLatencyInSamples() + device->getOutputLatencyInSamples();
        return 0;
    }

    const TransportState& StudioAudioEngine::getTransportState() const noexcept { return transportState; }
    TransportState& StudioAudioEngine::getTransportState() noexcept { return transportState; }
    double StudioAudioEngine::getCurrentSampleRate() const noexcept { return currentSampleRate; }
    int StudioAudioEngine::getCurrentBufferSize() const noexcept { return currentBufferSize; }

    //==========================================================================
    // Tracks
    //==========================================================================
    void StudioAudioEngine::buildTrackPlayers()
    {
        juce::Array<std::unique_ptr<TrackPlayer>> newPlayers;
        for (int index = 0; index < session.getNumTracks(); ++index)
        {
            auto tp = std::make_unique<TrackPlayer>();
            tp->trackIndex = index;
            tp->setTrackMetadata(session.getTrack(index));
            tp->prepare(currentSampleRate, maxBlockSize);
            newPlayers.add(std::move(tp));
        }
        auto snap = buildSnapshot(computeSessionHash());
        {
            const juce::ScopedLock sl(playerLock);
            std::swap(trackPlayers, newPlayers);
            installSnapshot(std::move(snap));
        }
        newPlayers.clear();   // old players (and their plugins) die outside the lock
        for (int i = 0; i < trackPlayers.size(); ++i)
            syncTrackPlayerSends(i);
        updateSoloStates();
        updateRoutingOrder();
        updatePluginDelayCompensation();
    }

    void StudioAudioEngine::refreshTrackPlaybackStates()
    {
        const int n = juce::jmin(trackPlayers.size(), session.getNumTracks());
        for (int i = 0; i < n; ++i)
        {
            auto& p = *trackPlayers.getReference(i);
            const auto& tr = session.getTrack(i);
            // Only touch the cheap mixer fields; plugin chains stay as they are.
            p.volumeDb.store(tr.volumeDb); p.pan.store(tr.pan); p.muted.store(tr.muted); p.solo.store(tr.solo);
            p.armed.store(tr.armed); p.monitorInput.store(tr.monitorInput); p.isStereo.store(tr.isStereo);
            p.inputChannel.store(tr.inputChannel);
            p.outputBusIndex.store(parseBusIndex(tr.outputBus));
        }
        updateSoloStates();
        updateRoutingOrder();
    }

    void StudioAudioEngine::addTrack(const juce::String& name, TrackType type)
    {
        const int newIndex = session.getNumTracks();
        auto& newTrack = session.addTrack(name, type);
        static const juce::Colour kTrackPalette[] = {
            juce::Colour::fromRGB(120, 80, 200), juce::Colour::fromRGB(80, 100, 200),
            juce::Colour::fromRGB(50, 160, 160), juce::Colour::fromRGB(60, 120, 200),
            juce::Colour::fromRGB(40, 180, 160), juce::Colour::fromRGB(60, 180, 80),
            juce::Colour::fromRGB(160, 190, 50), juce::Colour::fromRGB(190, 110, 40),
        };
        newTrack.colour = kTrackPalette[newIndex % (int) juce::numElementsInArray(kTrackPalette)];
        if (type == TrackType::Midi)
            newTrack.volumeDb = -6.0f;
        if (type == TrackType::Aux && newTrack.auxInputBusIndex < 0)
        {
            // Give every new aux return its own bus so sends to it are audible.
            for (int b = 0; b < kNumSendBuses; ++b)
            {
                bool used = false;
                for (int i = 0; i < session.getNumTracks() - 1; ++i)
                    used = used || (session.getTrack(i).type == TrackType::Aux && session.getTrack(i).auxInputBusIndex == b);
                if (!used) { newTrack.auxInputBusIndex = b; break; }
            }
            newTrack.inputBus = sendBusName(juce::jmax(0, newTrack.auxInputBusIndex));
        }

        auto player = std::make_unique<TrackPlayer>();
        player->trackIndex = newIndex;
        player->setTrackMetadata(session.getTrack(newIndex));
        player->prepare(currentSampleRate, maxBlockSize);
        auto snap = buildSnapshot(computeSessionHash());
        {
            const juce::ScopedLock sl(playerLock);
            trackPlayers.add(std::move(player));
            installSnapshot(std::move(snap));
        }
        syncTrackPlayerSends(newIndex);
        updateSoloStates();
        updateRoutingOrder();
        if (type == TrackType::Midi && midiInputTrack.load() < 0)
            midiInputTrack.store(newIndex);
    }

    int StudioAudioEngine::addAuxTrack(const juce::String& name, int busIndex)
    {
        addTrack(name, TrackType::Aux);
        const int idx = session.getNumTracks() - 1;
        setAuxTrackBus(idx, busIndex);
        return idx;
    }

    void StudioAudioEngine::shiftTrackReferencesAfterRemoval(int removed)
    {
        for (int m = 0; m < session.getNumMacros(); ++m)
        {
            auto& targets = session.getMacro(m).targets;
            for (int i = targets.size(); --i >= 0;)
            {
                auto& tg = targets.getReference(i);
                if (tg.trackIndex == removed) targets.remove(i);
                else if (tg.trackIndex > removed) --tg.trackIndex;
            }
        }
        for (int i = midiBindings.size(); --i >= 0;)
        {
            auto& b = midiBindings.getReference(i);
            if (b.macroIndex < 0)
            {
                if (b.target.trackIndex == removed) midiBindings.remove(i);
                else if (b.target.trackIndex > removed) --b.target.trackIndex;
            }
        }
        for (int t = 0; t < session.getNumTracks(); ++t)
            for (auto& c : session.getTrack(t).clips)
            {
                if (c.sourceTrackIndex > removed) --c.sourceTrackIndex;
                if (c.guideTrackIndex > removed) --c.guideTrackIndex;
            }
        const int mt = midiInputTrack.load();
        if (mt == removed) midiInputTrack.store(-1);
        else if (mt > removed) midiInputTrack.store(mt - 1);
    }

    void StudioAudioEngine::removeTrack(int index)
    {
        if (!juce::isPositiveAndBelow(index, session.getNumTracks()))
            return;
        session.removeTrack(index);
        shiftTrackReferencesAfterRemoval(index);
        auto snap = buildSnapshot(computeSessionHash());
        std::unique_ptr<TrackPlayer> doomed;
        {
            const juce::ScopedLock sl(playerLock);
            if (juce::isPositiveAndBelow(index, trackPlayers.size()))
            {
                doomed = std::move(trackPlayers.getReference(index));
                trackPlayers.remove(index);
            }
            for (int i = 0; i < trackPlayers.size(); ++i)
                trackPlayers.getReference(i)->trackIndex = i;
            installSnapshot(std::move(snap));
        }
        doomed.reset();   // plugin teardown outside the lock
        updateSoloStates();
        updateRoutingOrder();
        updatePluginDelayCompensation();
    }

    void StudioAudioEngine::setTrackArm(int index, bool arm)
    {
        if (!juce::isPositiveAndBelow(index, session.getNumTracks())) return;
        session.getTrack(index).armed = arm;
        if (juce::isPositiveAndBelow(index, trackPlayers.size()))
            trackPlayers.getReference(index)->armed.store(arm);
    }

    void StudioAudioEngine::setTrackEQBand(int trackIndex, int band, bool enabled, float freq, float gainDb, float q)
    {
        if (!juce::isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        if (band < 0 || band >= TrackPlayer::kNumEQBands) return;
        auto& eb = session.getTrack(trackIndex).eq[band];
        eb.enabled = enabled; eb.freq = freq; eb.gainDb = gainDb; eb.q = q;
        if (!juce::isPositiveAndBelow(trackIndex, trackPlayers.size())) return;
        auto& player = *trackPlayers.getReference(trackIndex);
        auto& b = player.eqBands[(size_t) band];
        b.enabled.store(enabled); b.freq.store(freq); b.gainDb.store(gainDb); b.q.store(q);
        player.eqDirty = true;
    }

    float StudioAudioEngine::getTrackPeakLevel(int trackIndex, int channel) const noexcept
    {
        if (!juce::isPositiveAndBelow(trackIndex, trackPlayers.size())) return 0.0f;
        auto& player = *trackPlayers.getReference(trackIndex);
        return channel == 0 ? player.peakLevelLeft.load() : player.peakLevelRight.load();
    }

    juce::StringArray StudioAudioEngine::getInputChannelNames() const
    {
        juce::StringArray names;
        if (auto* device = const_cast<juce::AudioDeviceManager&>(deviceManager).getCurrentAudioDevice())
        {
            const auto all = device->getInputChannelNames();
            const auto active = device->getActiveInputChannels();
            for (int i = 0; i < all.size(); ++i)
                if (active[i]) names.add(all[i]);
        }
        return names;
    }

    void StudioAudioEngine::setTrackInputChannel(int t, int ch)
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks())) return;
        session.getTrack(t).inputChannel = juce::jmax(0, ch);
        if (juce::isPositiveAndBelow(t, trackPlayers.size())) trackPlayers.getReference(t)->inputChannel.store(juce::jmax(0, ch));
    }
    int StudioAudioEngine::getTrackInputChannel(int t) const noexcept
    {
        return juce::isPositiveAndBelow(t, session.getNumTracks()) ? session.getTrack(t).inputChannel : 0;
    }
    void StudioAudioEngine::setTrackStereo(int t, bool stereo)
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks())) return;
        session.getTrack(t).isStereo = stereo;
        if (juce::isPositiveAndBelow(t, trackPlayers.size())) trackPlayers.getReference(t)->isStereo.store(stereo);
    }
    void StudioAudioEngine::setTrackMonitorInput(int t, bool on)
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks())) return;
        session.getTrack(t).monitorInput = on;
        if (juce::isPositiveAndBelow(t, trackPlayers.size())) trackPlayers.getReference(t)->monitorInput.store(on);
    }
    bool StudioAudioEngine::getTrackMonitorInput(int t) const noexcept
    {
        return juce::isPositiveAndBelow(t, session.getNumTracks()) && session.getTrack(t).monitorInput;
    }
    void StudioAudioEngine::setTrackAutomationMode(int t, int mode)
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks())) return;
        session.getTrack(t).automationMode = juce::jlimit(0, 4, mode);
        publishSnapshot(false);
    }
    int StudioAudioEngine::getTrackAutomationMode(int t) const noexcept
    {
        return juce::isPositiveAndBelow(t, session.getNumTracks()) ? session.getTrack(t).automationMode : 0;
    }
    float StudioAudioEngine::getTrackDisplayVolume(int t) const noexcept
    {
        return juce::isPositiveAndBelow(t, trackPlayers.size()) ? trackPlayers.getReference(t)->displayVolumeDb.load() : 0.0f;
    }
    float StudioAudioEngine::getTrackDisplayPan(int t) const noexcept
    {
        return juce::isPositiveAndBelow(t, trackPlayers.size()) ? trackPlayers.getReference(t)->displayPan.load() : 0.0f;
    }

    //==========================================================================
    // Automation writing (Touch / Latch / Write)
    //==========================================================================
    void StudioAudioEngine::writeAutomationTick()
    {
        const int numTracks = session.getNumTracks();
        if ((int) autoWrite.size() != numTracks)
            autoWrite.resize((size_t) numTracks);
        if (!transportState.isPlaying())
            return;

        const double now = (double) transportState.getPositionSamples() / currentSampleRate;
        const auto ms = juce::Time::getMillisecondCounter();

        for (int t = 0; t < numTracks; ++t)
        {
            auto& tr = session.getTrack(t);
            auto& st = autoWrite[(size_t) t];
            const int mode = tr.automationMode;
            const bool moved = std::abs(tr.volumeDb - st.lastVol) > 0.01f || std::abs(tr.pan - st.lastPan) > 0.001f;
            if (moved) st.lastMoveMs = ms;
            st.lastVol = tr.volumeDb; st.lastPan = tr.pan;

            bool writing = false;
            if (mode == 3) writing = true;                                   // Write
            else if (mode == 2) { if (moved) st.latched = true; writing = st.latched; }   // Latch
            else if (mode == 1) writing = (ms - st.lastMoveMs) < 400;        // Touch
            if (juce::isPositiveAndBelow(t, trackPlayers.size()))
                trackPlayers.getReference(t)->writingAutomation.store(writing);
            if (!writing) { st.lastWriteSec = -1.0; continue; }

            auto writeLane = [&] (const juce::String& id, float value)
            {
                int laneIndex = -1;
                for (int i = 0; i < tr.automationLanes.size(); ++i)
                    if (tr.automationLanes.getReference(i).parameterId == id) { laneIndex = i; break; }
                if (laneIndex < 0)
                {
                    AutomationLane lane; lane.parameterId = id; lane.enabled = true;
                    tr.automationLanes.add(lane);
                    laneIndex = tr.automationLanes.size() - 1;
                }
                auto& pts = tr.automationLanes.getReference(laneIndex).points;
                const double from = st.lastWriteSec < 0.0 ? now - 0.001 : st.lastWriteSec;
                for (int i = pts.size(); --i >= 0;)
                    if (pts.getReference(i).timeSeconds > from && pts.getReference(i).timeSeconds <= now)
                        pts.remove(i);
                AutomationPoint pt; pt.timeSeconds = now; pt.value = value;
                int ins = 0;
                while (ins < pts.size() && pts.getReference(ins).timeSeconds < now) ++ins;
                pts.insert(ins, pt);
            };
            writeLane("volume", tr.volumeDb);
            writeLane("pan", tr.pan);
            st.lastWriteSec = now;
        }
    }

    //==========================================================================
    // Recording
    //==========================================================================
    void StudioAudioEngine::toggleRecord()
    {
        if (recordingActive.load() || countInRemaining.load() > 0)
        {
            stop();
            return;
        }

        if (!transportState.isRecordArmed())
            transportState.setRecordArmed(true);

        bool anyArmed = false;
        for (int i = 0; i < session.getNumTracks(); ++i)
            anyArmed = anyArmed || session.getTrack(i).armed;
        if (!anyArmed)
        {
            // Nothing armed: arm the MIDI input track if it's MIDI, otherwise the first audio track.
            const int mt = midiInputTrack.load();
            if (juce::isPositiveAndBelow(mt, session.getNumTracks()) && session.getTrack(mt).type == TrackType::Midi)
                setTrackArm(mt, true);
            else
                for (int i = 0; i < session.getNumTracks(); ++i)
                    if (session.getTrack(i).type == TrackType::Audio) { setTrackArm(i, true); break; }
        }
        startRecordingInternal();
    }

    bool StudioAudioEngine::isRecording() const noexcept
    {
        return recordingActive.load() || countInRemaining.load() > 0;
    }

    void StudioAudioEngine::startRecordingInternal()
    {
        const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        juce::OwnedArray<RecordTake> takes;
        const int numIns = juce::jmax(0, cachedActiveInputChannels);

        for (int i = 0; i < session.getNumTracks(); ++i)
        {
            const auto& tr = session.getTrack(i);
            if (!tr.armed || tr.type != TrackType::Audio) continue;

            auto take = std::make_unique<RecordTake>();
            take->trackIndex = i;
            take->firstInput = juce::jlimit(0, juce::jmax(0, numIns - 1), tr.inputChannel);
            take->numChannels = (tr.isStereo && numIns > take->firstInput + 1) ? 2 : 1;
            take->file = recordingFolder.getChildFile(legalName(tr.name) + "_" + timestamp + ".wav").getNonexistentSibling();

            std::unique_ptr<juce::FileOutputStream> out(take->file.createOutputStream());
            if (out == nullptr || !out->openedOk()) continue;
            juce::WavAudioFormat wav;
            auto* raw = wav.createWriterFor(out.get(), currentSampleRate, (unsigned int) take->numChannels, 32, {}, 0);
            if (raw == nullptr) continue;
            out.release();
            take->writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(raw, recordingThread, 65536);
            takes.add(take.release());
        }

        {
            const juce::SpinLock::ScopedLockType sl(recordLock);
            recordTakes.swapWith(takes);
        }
        takes.clear();
        currentRecordingFile = recordTakes.isEmpty() ? juce::File() : recordTakes[0]->file;

        recMidiFifo.reset();
        recordedMidi.clear();
        recordedSamples.store(0);
        loopPassCount.store(0);
        recordLoopStart = transportState.getLoopStartSample();
        recordLoopEnd = transportState.getLoopEndSample();
        recordStartPos.store(transportState.getPositionSamples());
        transportState.startRecording();

        if (session.countInBars > 0 && !transportState.isPlaying())
        {
            const double beatLen = 60.0 / transportState.getTempo() * currentSampleRate * (4.0 / session.timeSigDenominator);
            countInTotal = (int64_t) std::llround(beatLen * session.timeSigNumerator * session.countInBars);
            transportState.stop();
            pendingRecordStart.store(true);
            countInRemaining.store(countInTotal);
            prefetchAt(transportState.getPositionSamples());
        }
        else
        {
            if (!transportState.isPlaying())
            {
                publishSnapshot(false);
                prefetchAt(transportState.getPositionSamples());
            }
            recordingActive.store(true);
            transportState.play();
        }
        juce::Logger::writeToLog("Recording started: " + juce::String(recordTakes.size()) + " audio take(s)");
    }

    void StudioAudioEngine::stopRecordingInternal()
    {
        recordingActive.store(false);
        pendingRecordStart.store(false);
        countInRemaining.store(0);
        transportState.stopRecording();

        juce::OwnedArray<RecordTake> finished;
        {
            const juce::SpinLock::ScopedLockType sl(recordLock);   // waits for any write in progress
            finished.swapWith(recordTakes);
        }
        for (auto* t : finished) t->writer.reset();   // flush to disk

        // Hand the finished takes to createRecordingClips via recordTakes-like info
        const int64_t total = recordedSamples.load();
        const int64_t startPos = recordStartPos.load();
        const int latency = getRoundTripLatencySamples() + recordingOffsetSamples.load();
        const int passes = loopPassCount.load();

        for (auto* take : finished)
        {
            if (total <= 0 || !take->file.existsAsFile()) continue;
            auto& track = session.getTrack(juce::jlimit(0, session.getNumTracks() - 1, take->trackIndex));

            juce::Array<std::pair<int64_t, int64_t>> segs;   // (fileStart, length) per pass
            int64_t prev = 0;
            for (int i = 0; i < passes; ++i) { segs.add({ prev, loopPassEnds[(size_t) i] - prev }); prev = loopPassEnds[(size_t) i]; }
            segs.add({ prev, total - prev });

            for (int i = 0; i < segs.size(); ++i)
            {
                int64_t tlStart = (i == 0) ? startPos : recordLoopStart;
                int64_t fileStart = segs[i].first + latency;
                int64_t len = juce::jmin(segs[i].second, total - fileStart);
                if (fileStart < 0) { tlStart -= fileStart; len += fileStart; fileStart = 0; }

                // Punch in/out trims the take to the punch range.
                if (session.punchEnabled && passes == 0 && session.punchOutSample > session.punchInSample)
                {
                    const int64_t a = juce::jmax(tlStart, session.punchInSample);
                    const int64_t b = juce::jmin(tlStart + len, session.punchOutSample);
                    if (b <= a) continue;
                    fileStart += a - tlStart;
                    len = b - a;
                    tlStart = a;
                }
                if (len <= 0) continue;

                Clip clip;
                clip.file = take->file;
                clip.startSample = tlStart;
                clip.lengthSamples = len;
                clip.fileOffsetSamples = fileStart;
                clip.clipColor = track.colour;
                clip.name = segs.size() > 1 ? "Take " + juce::String(i + 1) : take->file.getFileNameWithoutExtension();
                clip.muted = (segs.size() > 1 && i != segs.size() - 1);   // loop takes: last one plays
                clip.fadeInSamples = clip.fadeOutSamples = (int64_t) (currentSampleRate * 0.003);
                track.clips.add(clip);
            }
        }

        createMidiRecordingClips();
        publishSnapshot(true);
        juce::Logger::writeToLog("Recording stopped: " + juce::String(total) + " samples, " + juce::String(passes) + " loop wraps");
    }

    void StudioAudioEngine::createRecordingClips() {}

    void StudioAudioEngine::createMidiRecordingClips()
    {
        // Drain anything left in the FIFO.
        int s1, n1, s2, n2;
        const int ready = recMidiFifo.getNumReady();
        recMidiFifo.prepareToRead(ready, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) recordedMidi.add(recMidiData[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) recordedMidi.add(recMidiData[(size_t) (s2 + i)]);
        recMidiFifo.finishedRead(n1 + n2);
        if (recordedMidi.isEmpty()) return;

        const double sr = currentSampleRate;
        std::map<int, juce::Array<RecordedMidiEvent>> perTrack;
        for (auto& e : recordedMidi) perTrack[e.trackIndex].add(e);
        recordedMidi.clear();

        for (auto& [trackIndex, events] : perTrack)
        {
            if (!juce::isPositiveAndBelow(trackIndex, session.getNumTracks())) continue;
            int64_t first = events.getFirst().timelinePos, last = first;
            for (auto& e : events) { first = juce::jmin(first, e.timelinePos); last = juce::jmax(last, e.timelinePos); }
            const int64_t clipStart = juce::jmin(first, recordStartPos.load());
            const double clipStartBeat = session.secondsToBeats((double) clipStart / sr);

            juce::Array<MidiNote> notes;
            std::array<int, 128> openIndex; openIndex.fill(-1);
            for (auto& e : events)
            {
                const juce::MidiMessage m(e.data, juce::MidiMessage::getMessageLengthFromFirstByte(e.data[0]), 0.0);
                const double beat = session.secondsToBeats((double) e.timelinePos / sr) - clipStartBeat;
                if (m.isNoteOn())
                {
                    MidiNote n; n.pitch = m.getNoteNumber(); n.startBeat = juce::jmax(0.0, beat);
                    n.lengthBeats = 0.25; n.velocity = m.getFloatVelocity();
                    openIndex[(size_t) n.pitch] = notes.size();
                    notes.add(n);
                }
                else if (m.isNoteOff() && openIndex[(size_t) m.getNoteNumber()] >= 0)
                {
                    auto& n = notes.getReference(openIndex[(size_t) m.getNoteNumber()]);
                    n.lengthBeats = juce::jmax(1.0 / 64.0, beat - n.startBeat);
                    openIndex[(size_t) m.getNoteNumber()] = -1;
                }
            }
            if (notes.isEmpty()) continue;

            // Round the clip out to whole bars.
            const double beatsPerBar = session.timeSigNumerator * 4.0 / session.timeSigDenominator;
            double endBeat = 0.0;
            for (auto& n : notes) endBeat = juce::jmax(endBeat, n.startBeat + n.lengthBeats);
            endBeat = std::ceil(juce::jmax(endBeat, (double) (last - clipStart) / sr * session.getTempo() / 60.0) / beatsPerBar) * beatsPerBar;

            Clip clip;
            clip.isMidi = true;
            clip.startSample = clipStart;
            clip.lengthSamples = (int64_t) std::llround(session.beatsToSeconds(clipStartBeat + endBeat) * sr) - clipStart;
            clip.notes = notes;
            clip.name = "MIDI Take";
            clip.clipColor = session.getTrack(trackIndex).colour;
            session.getTrack(trackIndex).clips.add(clip);
        }
    }

    //==========================================================================
    // MIDI
    //==========================================================================
    void StudioAudioEngine::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
    {
        if (message.isController())
        {
            const int cc = message.getControllerNumber();
            bool learned = midiLearnPending.load();
            // midiBindings is only modified on the message thread; a stale read here just
            // means a CC is forwarded once more to the instrument.
            for (int i = 0; i < midiBindings.size() && !learned; ++i)
                learned = midiBindings.getReference(i).ccNumber == cc;
            if (learned)
            {
                const int ch = message.getChannel(), val = message.getControllerValue();
                juce::MessageManager::callAsync([this, cc, ch, val] { handleControllerOnMessageThread(cc, ch, val); });
                return;
            }
        }
        if (message.isNoteOnOrOff() || message.isController() || message.isPitchWheel()
            || message.isAftertouch() || message.isChannelPressure() || message.isProgramChange())
            pushLiveMidi(message);
    }

    void StudioAudioEngine::handleControllerOnMessageThread(int ccNumber, int channel, int value)
    {
        const float normalized = value / 127.0f;
        if (midiLearnPending.load())
        {
            MidiCCBinding binding;
            binding.ccNumber = ccNumber;
            binding.channel  = channel;
            if (midiLearnIsForMacro) binding.macroIndex = midiLearnMacroIndex;
            else binding.target = midiLearnTarget;
            for (int i = midiBindings.size(); --i >= 0;)
            {
                const auto& ex = midiBindings.getReference(i);
                const bool sameMacro  = midiLearnIsForMacro && ex.macroIndex == midiLearnMacroIndex;
                const bool sameTarget = !midiLearnIsForMacro && ex.macroIndex < 0
                                     && ex.target.trackIndex == midiLearnTarget.trackIndex
                                     && ex.target.parameterId == midiLearnTarget.parameterId;
                if (sameMacro || sameTarget) midiBindings.remove(i);
            }
            const int newIndex = midiBindings.size();
            midiBindings.add(binding);
            midiLearnPending.store(false);
            midiLearnMacroIndex = -1;
            if (onMidiLearned) onMidiLearned(newIndex);
            return;
        }
        for (const auto& b : midiBindings)
        {
            if (b.ccNumber != ccNumber || (b.channel != 0 && b.channel != channel)) continue;
            if (b.macroIndex >= 0) setMacroValue(b.macroIndex, normalized);
            else applyParameterTarget(b.target, normalized);
        }
    }

    void StudioAudioEngine::sendLiveNote(int note, float velocity, bool noteOn)
    {
        note = juce::jlimit(0, 127, note);
        pushLiveMidi(noteOn ? juce::MidiMessage::noteOn(1, note, juce::jlimit(0.01f, 1.0f, velocity))
                            : juce::MidiMessage::noteOff(1, note));
    }

    void StudioAudioEngine::previewPianoKey(int midiNoteNumber, bool noteOn)
    {
        // If the target track has an instrument, play it; otherwise use the preview beep.
        int target = midiInputTrack.load();
        for (int i = 0; i < session.getNumTracks(); ++i)
            if (session.getTrack(i).type == TrackType::Midi && session.getTrack(i).armed) { target = i; break; }
        if (juce::isPositiveAndBelow(target, trackPlayers.size()) && trackPlayers.getReference(target)->getNumPlugins() > 0)
        {
            sendLiveNote(midiNoteNumber, 0.8f, noteOn);
            return;
        }
        if (noteOn)
        {
            const double freqHz = 440.0 * std::pow(2.0, (midiNoteNumber - 69) / 12.0);
            pianoKeyPhaseIncrement = juce::MathConstants<double>::twoPi * freqHz / juce::jmax(1.0, currentSampleRate);
            pianoKeyReleasing.store(false);
            pianoKeyActive.store(true);
        }
        else
            pianoKeyReleasing.store(true);
    }

    int StudioAudioEngine::createMidiClip(int trackIndex, int64_t startSample, int64_t lengthSamples, const juce::String& name)
    {
        if (!juce::isPositiveAndBelow(trackIndex, session.getNumTracks()) || lengthSamples <= 0) return -1;
        auto& tr = session.getTrack(trackIndex);
        Clip c;
        c.isMidi = true;
        c.startSample = juce::jmax((int64_t) 0, startSample);
        c.lengthSamples = lengthSamples;
        c.name = name.isNotEmpty() ? name : tr.name;
        c.clipColor = tr.colour;
        tr.clips.add(c);
        publishSnapshot(false);
        return tr.clips.size() - 1;
    }

    bool StudioAudioEngine::setMidiClipNotes(int trackIndex, int clipIndex, const juce::Array<MidiNote>& notes)
    {
        if (!juce::isPositiveAndBelow(trackIndex, session.getNumTracks())) return false;
        auto& clips = session.getTrack(trackIndex).clips;
        if (!juce::isPositiveAndBelow(clipIndex, clips.size()) || !clips.getReference(clipIndex).isMidi) return false;
        clips.getReference(clipIndex).notes = notes;
        publishSnapshot(false);
        return true;
    }

    int StudioAudioEngine::importMidiFile(const juce::File& midiFile, int64_t startSample, int targetTrack)
    {
        juce::FileInputStream in(midiFile);
        juce::MidiFile mf;
        if (!in.openedOk() || !mf.readFrom(in)) return 0;
        const int tpq = mf.getTimeFormat() > 0 ? mf.getTimeFormat() : 960;
        const double sr = currentSampleRate;
        const double startBeat = session.secondsToBeats((double) startSample / sr);
        const double beatsPerBar = session.timeSigNumerator * 4.0 / session.timeSigDenominator;
        int created = 0;

        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            juce::MidiMessageSequence seq(*mf.getTrack(t));
            seq.updateMatchedPairs();
            juce::String trackName;
            juce::Array<MidiNote> notes;
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                auto* ev = seq.getEventPointer(i);
                const auto& m = ev->message;
                if (m.isTrackNameEvent()) trackName = m.getTextFromTextMetaEvent();
                if (!m.isNoteOn()) continue;
                MidiNote n;
                n.pitch = m.getNoteNumber();
                n.velocity = m.getFloatVelocity();
                n.startBeat = m.getTimeStamp() / tpq;
                const double offTicks = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() : m.getTimeStamp() + tpq / 4.0;
                n.lengthBeats = juce::jmax(1.0 / 64.0, (offTicks - m.getTimeStamp()) / tpq);
                notes.add(n);
            }
            if (notes.isEmpty()) continue;

            int dest = (created == 0 && juce::isPositiveAndBelow(targetTrack, session.getNumTracks())) ? targetTrack : -1;
            if (dest < 0)
            {
                addTrack(trackName.isNotEmpty() ? trackName : midiFile.getFileNameWithoutExtension(), TrackType::Midi);
                dest = session.getNumTracks() - 1;
            }
            double endBeat = 0.0;
            for (auto& n : notes) endBeat = juce::jmax(endBeat, n.startBeat + n.lengthBeats);
            endBeat = std::ceil(endBeat / beatsPerBar) * beatsPerBar;
            const int64_t len = (int64_t) std::llround(session.beatsToSeconds(startBeat + endBeat) * sr) - startSample;
            const int ci = createMidiClip(dest, startSample, len, trackName.isNotEmpty() ? trackName : midiFile.getFileNameWithoutExtension());
            if (ci >= 0) { setMidiClipNotes(dest, ci, notes); ++created; }
        }
        return created;
    }

    bool StudioAudioEngine::exportMidiClip(int trackIndex, int clipIndex, const juce::File& destination) const
    {
        if (!juce::isPositiveAndBelow(trackIndex, session.getNumTracks())) return false;
        const auto& clips = session.getTrack(trackIndex).clips;
        if (!juce::isPositiveAndBelow(clipIndex, clips.size())) return false;
        const auto& clip = clips.getReference(clipIndex);
        if (!clip.isMidi) return false;

        constexpr int tpq = 960;
        juce::MidiMessageSequence seq;
        seq.addEvent(juce::MidiMessage::tempoMetaEvent((int) std::lround(60000000.0 / session.getTempo())), 0.0);
        seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(session.timeSigNumerator, session.timeSigDenominator), 0.0);
        for (const auto& n : clip.notes)
        {
            seq.addEvent(juce::MidiMessage::noteOn(1, n.pitch, n.velocity), n.startBeat * tpq);
            seq.addEvent(juce::MidiMessage::noteOff(1, n.pitch), (n.startBeat + n.lengthBeats) * tpq);
        }
        seq.updateMatchedPairs();
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote(tpq);
        mf.addTrack(seq);
        destination.deleteFile();
        juce::FileOutputStream out(destination);
        return out.openedOk() && mf.writeTo(out);
    }

    //==========================================================================
    // Clips
    //==========================================================================
    bool StudioAudioEngine::loadAudioClip(int trackIndex, const juce::File& audioFile)
    {
        if (!audioFile.existsAsFile() || !juce::isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return false;
        std::unique_ptr<juce::AudioFormatReader> reader(clipFormats.createReaderFor(audioFile));
        if (reader == nullptr)
            return false;
        Clip clip;
        clip.file = audioFile;
        clip.startSample = 0;
        clip.lengthSamples = (int64_t) std::llround((double) reader->lengthInSamples * currentSampleRate / juce::jmax(1.0, reader->sampleRate));
        auto& track = session.getTrack(trackIndex);
        clip.clipColor = track.colour;
        track.clips.clear();
        track.clips.add(clip);
        publishSnapshot(false);
        return true;
    }

    int64_t StudioAudioEngine::getArrangementEndSample() const
    {
        int64_t end = 0;
        for (int t = 0; t < session.getNumTracks(); ++t)
            for (const auto& c : session.getTrack(t).clips)
                if (!c.muted && !(c.isPreview && !session.isPreviewPlaybackEnabled()))
                    end = juce::jmax(end, c.startSample + c.lengthSamples);
        return end;
    }

    //==========================================================================
    // Plugins (lock-protected so the audio thread never sees a half-edited chain)
    //==========================================================================
    bool StudioAudioEngine::loadPluginOnTrack(int trackIndex, const juce::File& pluginFile)
    {
        if (!pluginFile.exists())
            return false;
        juce::OwnedArray<juce::PluginDescription> typesFound;
        knownPlugins.scanAndAddDragAndDroppedFiles(pluginFormatManager, juce::StringArray(pluginFile.getFullPathName()), typesFound);
        if (typesFound.isEmpty())
            return false;
        return loadPluginByDescription(*typesFound[0], trackIndex);
    }

    bool StudioAudioEngine::loadPluginByDescription(const juce::PluginDescription& desc, int trackIndex,
                                                    const std::function<void(juce::AudioPluginInstance&)>& configure)
    {
        juce::String errorMessage;
        auto instance = pluginFormatManager.createPluginInstance(desc, currentSampleRate, maxBlockSize, errorMessage);
        if (!instance)
        {
            juce::Logger::writeToLog("Plugin load failed: " + desc.name + " - " + errorMessage);
            return false;
        }

        if (trackIndex == kMasterTrackIndex)
        {
            if (configure)
            {
                instance->enableAllBuses();
                instance->setRateAndBufferSizeDetails(currentSampleRate, maxBlockSize);
                instance->prepareToPlay(currentSampleRate, maxBlockSize);
                configure(*instance);
            }
            return addMasterPlugin(std::move(instance));
        }

        int target = trackIndex;
        if (target < 0)
        {
            target = 0;
            for (int i = 0; i < trackPlayers.size(); ++i)
                if (trackPlayers.getReference(i)->armed.load()) { target = i; break; }
        }
        if (!juce::isPositiveAndBelow(target, trackPlayers.size()))
            return false;

        instance->enableAllBuses();
        instance->setRateAndBufferSizeDetails(currentSampleRate, maxBlockSize);
        instance->prepareToPlay(currentSampleRate, maxBlockSize);
        if (configure) configure(*instance);
        {
            const juce::ScopedLock sl(playerLock);
            trackPlayers.getReference(target)->addPlugin(std::move(instance));
        }
        updatePluginDelayCompensation();
        return true;
    }

    void StudioAudioEngine::updatePluginDelayCompensation()
    {
        const juce::ScopedLock sl(playerLock);
        int maxLatency = 0;
        for (auto& p : trackPlayers)
            maxLatency = juce::jmax(maxLatency, p->getPluginChainLatencySamples());
        for (auto& p : trackPlayers)
            p->setPdcDelaySamples(maxLatency - p->getPluginChainLatencySamples());
        trackMaxLatency.store(maxLatency);
        int master = 0;
        {
            const juce::ScopedLock ml(masterPluginLock);
            for (auto* pl : masterPlugins) if (pl) master += pl->getLatencySamples();
        }
        masterChainLatency.store(master);
    }

    //==========================================================================
    // Offline export
    //==========================================================================
    StudioAudioEngine::ExportResult StudioAudioEngine::exportAudio(const juce::File& destinationFile, const ExportOptions& opts)
    {
        ExportResult r;
        const double sr = currentSampleRate;
        const int64_t start = juce::jmax((int64_t) 0, opts.startSample);
        const int64_t end = opts.endSample >= 0 ? opts.endSample : getArrangementEndSample();
        if (end <= start) { r.error = "There's nothing to export (the arrangement is empty)."; return r; }

        if (recordingActive.load()) stop();
        publishSnapshot(true);

        destinationFile.getParentDirectory().createDirectory();
        destinationFile.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream(destinationFile.createOutputStream());
        if (stream == nullptr || !stream->openedOk()) { r.error = "Couldn't write to " + destinationFile.getFullPathName(); return r; }
        juce::WavAudioFormat wav;
        const int bits = opts.bitDepth == 16 ? 16 : (opts.bitDepth == 32 ? 32 : 24);
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), sr, 2, bits, {}, 0));
        if (writer == nullptr) { r.error = "Couldn't create the WAV writer."; return r; }
        stream.release();

        const int64_t savedPos = transportState.getPositionSamples();
        const bool savedPlaying = transportState.isPlaying();
        const bool savedLooping = transportState.isLooping();

        deviceManager.removeAudioCallback(this);   // waits for any running callback to finish
        transportState.stop();
        transportState.setLooping(false);
        offlineRendering.store(true);
        offlineSoloTrack = opts.soloTrack;
        offlineIncludeMaster = opts.includeMaster;
        // Pre-roll by the compensated plugin latency so the first samples aren't swallowed.
        const int64_t preroll = offlineFreezeTrack >= 0 ? 0
            : (int64_t) trackMaxLatency.load() + (opts.includeMaster ? masterChainLatency.load() : 0);
        transportState.setPositionFromAudioThread(start - preroll);

        bool cancelled = false;
        float peak = 0.0f;
        {
            const juce::ScopedLock sl(playerLock);
            const juce::ScopedLock ml(masterPluginLock);

            auto forAllPlugins = [&] (auto&& fn)
            {
                for (auto& p : trackPlayers) for (auto& pl : p->pluginChain) if (pl) fn(*pl);
                for (auto* pl : masterPlugins) if (pl) fn(*pl);
            };
            forAllPlugins([] (juce::AudioPluginInstance& pl) { pl.setNonRealtime(true); pl.reset(); });
            for (auto& p : trackPlayers) { p->allNotesOffPending = true; p->lastGainL = p->lastGainR = -1.0f; p->heldNotes.reset(); }
            masterLimiter.reset();
            masterGain.setCurrentAndTargetValue(dbToGain(masterVolumeDbAtomic.load()));
            stepSeqExpectedPos = -1;
            lastRenderEnd = -1;
            transportState.play();

            const int block = juce::jmin(1024, maxBlockSize);
            exportBuffer.setSize(2, block, false, false, true);
            float* outs[2] = { exportBuffer.getWritePointer(0), exportBuffer.getWritePointer(1) };
            const int64_t tail = (int64_t) (juce::jmax(0.0, opts.tailSeconds) * sr);
            const int64_t total = (end - start) + tail;
            int64_t written = 0, silentRun = 0;
            int lastProgress = -1;

            for (int64_t skip = preroll; skip > 0;)
            {
                const int n = (int) juce::jmin((int64_t) block, skip);
                renderBlock(nullptr, 0, outs, 2, n, true);
                skip -= n;
            }

            while (written < total)
            {
                const int n = (int) juce::jmin((int64_t) block, total - written);
                renderBlock(nullptr, 0, outs, 2, n, true);
                const float bp = juce::jmax(exportBuffer.getMagnitude(0, 0, n), exportBuffer.getMagnitude(1, 0, n));
                peak = juce::jmax(peak, bp);

                if (start + written >= end)
                {
                    silentRun = bp < 1.0e-5f ? silentRun + n : 0;
                    if (silentRun > (int64_t) (sr * 0.3)) break;   // tail has died away
                }
                writer->writeFromAudioSampleBuffer(exportBuffer, 0, n);
                written += n;

                const int pct = (int) (written * 1000 / juce::jmax((int64_t) 1, total));
                if (opts.progress && pct != lastProgress)
                {
                    lastProgress = pct;
                    if (!opts.progress((float) written / (float) total)) { cancelled = true; break; }
                }
            }
            r.seconds = (double) written / sr;

            transportState.stop();
            forAllPlugins([] (juce::AudioPluginInstance& pl) { pl.setNonRealtime(false); pl.reset(); });
            for (auto& p : trackPlayers) p->allNotesOffPending = true;
        }

        offlineRendering.store(false);
        offlineSoloTrack = -1;
        offlineIncludeMaster = true;
        writer.reset();
        transportState.setLooping(savedLooping);
        transportState.setPositionSamples(savedPos, false);
        lastRenderEnd = -1;
        deviceManager.addAudioCallback(this);
        if (savedPlaying) play();

        if (cancelled) { destinationFile.deleteFile(); r.error = "Export cancelled."; return r; }
        r.ok = true;
        r.peakDb = juce::Decibels::gainToDecibels(peak, -100.0f);
        r.clipped = peak > 1.0f && bits != 32;
        juce::Logger::writeToLog("Exported " + destinationFile.getFullPathName() + " peak=" + juce::String(r.peakDb, 2) + " dBFS");
        return r;
    }

    int StudioAudioEngine::exportStems(const juce::File& folder, const ExportOptions& options, juce::StringArray* errors)
    {
        folder.createDirectory();
        int count = 0;
        const auto base = legalName(session.getName());
        for (int t = 0; t < session.getNumTracks(); ++t)
        {
            const auto& tr = session.getTrack(t);
            const bool hasContent = !tr.clips.isEmpty() || tr.type == TrackType::Aux
                                    || (juce::isPositiveAndBelow(t, trackPlayers.size()) && trackPlayers.getReference(t)->getNumPlugins() > 0);
            if (!hasContent) continue;
            ExportOptions o = options;
            o.soloTrack = t;
            o.includeMaster = false;
            if (o.endSample < 0) o.endSample = getArrangementEndSample();
            const auto res = exportAudio(folder.getChildFile(base + " - " + juce::String(t + 1).paddedLeft('0', 2) + " " + legalName(tr.name) + ".wav"), o);
            if (res.ok) ++count;
            else if (errors != nullptr) errors->add(tr.name + ": " + res.error);
            if (res.error == "Export cancelled.") break;
        }
        return count;
    }

    bool StudioAudioEngine::exportStereoMix(const juce::File& destinationFile)
    {
        return exportAudio(destinationFile, ExportOptions()).ok;
    }

    //==========================================================================
    // Session I/O
    //==========================================================================
    void StudioAudioEngine::stepSequencerToSession()
    {
        auto* root = new juce::DynamicObject();
        root->setProperty("activeStepCount", stepSeq.activeStepCount);
        root->setProperty("swing", (double) stepSeq.swing);
        root->setProperty("groove", stepSeq.grooveTemplate);
        juce::Array<juce::var> rows;
        for (int r = 0; r < StepSequencerState::kNumRows; ++r)
        {
            auto* row = new juce::DynamicObject();
            row->setProperty("sample", stepSeq.sampleFiles[r]);
            juce::String steps;
            juce::Array<juce::var> vels;
            for (int s = 0; s < StepSequencerState::kNumSteps; ++s)
            {
                steps << (stepSeq.steps[r][s] ? "1" : "0");
                vels.add((double) stepSeq.velocities[r][s]);
            }
            row->setProperty("steps", steps);
            row->setProperty("velocities", vels);
            row->setProperty("volume", (double) stepSeq.rowVolumes[r]);
            row->setProperty("pan", (double) stepSeq.rowPans[r]);
            row->setProperty("pitch", (double) stepSeq.rowPitches[r]);
            row->setProperty("muted", stepSeq.rowMuted[r]);
            const auto& ce = stepSeq.channelEngines[(size_t) r];
            row->setProperty("env", juce::Array<juce::var> { ce.envEnabled, (double) ce.envAttack, (double) ce.envHold, (double) ce.envDecay, (double) ce.envSustain, (double) ce.envRelease });
            row->setProperty("filter", juce::Array<juce::var> { ce.filterEnabled, ce.filterType, (double) ce.filterCutoffHz, (double) ce.filterResonance });
            row->setProperty("lfo", juce::Array<juce::var> { ce.lfoEnabled, ce.lfoTarget, (double) ce.lfoRateHz, (double) ce.lfoDepth });
            row->setProperty("region", juce::Array<juce::var> { (double) ce.sampleStart, (double) ce.sampleEnd, ce.loopEnabled });
            rows.add(juce::var(row));
        }
        root->setProperty("rows", rows);
        session.stepSequencerState = juce::var(root);
    }

    void StudioAudioEngine::sessionToStepSequencer()
    {
        auto* root = session.stepSequencerState.getDynamicObject();
        if (root == nullptr) return;
        setStepSeqActiveStepCount((int) root->getProperty("activeStepCount"));
        setStepSeqSwing((float) (double) root->getProperty("swing"));
        setStepSeqGroove((int) root->getProperty("groove"));
        if (auto* rows = root->getProperty("rows").getArray())
            for (int r = 0; r < juce::jmin(rows->size(), (int) StepSequencerState::kNumRows); ++r)
            {
                auto* row = (*rows)[r].getDynamicObject();
                if (row == nullptr) continue;
                const auto sample = row->getProperty("sample").toString();
                if (sample.isNotEmpty()) setStepSeqSample(r, sample);
                const auto steps = row->getProperty("steps").toString();
                for (int s = 0; s < juce::jmin(steps.length(), (int) StepSequencerState::kNumSteps); ++s)
                    setStepSeqStep(r, s, steps[s] == '1');
                if (auto* vels = row->getProperty("velocities").getArray())
                    for (int s = 0; s < juce::jmin(vels->size(), (int) StepSequencerState::kNumSteps); ++s)
                        setStepSeqVelocity(r, s, (float) (double) (*vels)[s]);
                setStepSeqRowVolume(r, (float) (double) row->getProperty("volume"));
                setStepSeqRowPan(r, (float) (double) row->getProperty("pan"));
                setStepSeqRowPitch(r, (float) (double) row->getProperty("pitch"));
                setStepSeqRowMuted(r, (bool) row->getProperty("muted"));
                if (auto* e = row->getProperty("env").getArray(); e != nullptr && e->size() >= 6)
                    setStepSeqRowEnvelope(r, (bool) (*e)[0], (float) (double) (*e)[1], (float) (double) (*e)[2], (float) (double) (*e)[3], (float) (double) (*e)[4], (float) (double) (*e)[5]);
                if (auto* f = row->getProperty("filter").getArray(); f != nullptr && f->size() >= 4)
                    setStepSeqRowFilter(r, (bool) (*f)[0], (int) (*f)[1], (float) (double) (*f)[2], (float) (double) (*f)[3]);
                if (auto* l = row->getProperty("lfo").getArray(); l != nullptr && l->size() >= 4)
                    setStepSeqRowLFO(r, (bool) (*l)[0], (int) (*l)[1], (float) (double) (*l)[2], (float) (double) (*l)[3]);
                if (auto* g = row->getProperty("region").getArray(); g != nullptr && g->size() >= 3)
                    setStepSeqRowSampleRegion(r, (float) (double) (*g)[0], (float) (double) (*g)[1], (bool) (*g)[2]);
            }
    }

    void StudioAudioEngine::setStepSeqSample(int row, const juce::String& filePath)
    {
        if (!juce::isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        juce::AudioBuffer<float> loaded;
        bool ok = false;
        juce::File f(filePath);
        if (f.existsAsFile())
        {
            std::unique_ptr<juce::AudioFormatReader> reader(clipFormats.createReaderFor(f));
            if (reader != nullptr && reader->lengthInSamples < (int64_t) (reader->sampleRate * 60.0))
            {
                loaded.setSize((int) juce::jmax(1u, reader->numChannels), (int) reader->lengthInSamples);
                reader->read(&loaded, 0, (int) reader->lengthInSamples, 0, true, true);
                ok = true;
            }
        }
        // Swap under the lock so the audio thread never reads a half-replaced buffer.
        const juce::ScopedLock sl(playerLock);
        stepSeq.sampleFiles[row] = filePath;
        std::swap(stepSeq.sampleBuffers[row], loaded);
        stepSeq.sampleLoaded[row] = ok;
        stepSeq.playPositions[row] = 0;
        stepSeq.playPosFrac[row] = 0.0;
        stepSeq.rowTriggered[row] = false;
    }

    bool StudioAudioEngine::saveSession(const juce::File& file) const
    {
        auto* self = const_cast<StudioAudioEngine*>(this);
        self->stepSequencerToSession();

        // Mixer extras that aren't per-track (MIDI learn bindings).
        {
            auto* extras = new juce::DynamicObject();
            juce::Array<juce::var> binds;
            for (const auto& b : midiBindings) binds.add(b.toVar());
            extras->setProperty("midiBindings", binds);
            self->session.engineExtras = juce::var(extras);
        }

        if (!session.saveToFile(file))
            return false;

        auto pluginToVar = [] (juce::AudioPluginInstance& instance, bool bypassed)
        {
            juce::MemoryBlock stateData;
            instance.getStateInformation(stateData);
            auto* o = new juce::DynamicObject();
            const auto desc = instance.getPluginDescription();
            o->setProperty("name", instance.getName());
            o->setProperty("uid", desc.uniqueId);
            o->setProperty("deprecatedUid", desc.deprecatedUid);
            o->setProperty("fileOrId", desc.fileOrIdentifier);
            o->setProperty("formatName", desc.pluginFormatName);
            o->setProperty("isInstrument", desc.isInstrument);
            o->setProperty("bypassed", bypassed);
            o->setProperty("state", juce::Base64::toBase64(stateData.getData(), stateData.getSize()));
            return juce::var(o);
        };

        auto* root = new juce::DynamicObject();
        juce::Array<juce::var> tracksArray;
        for (int t = 0; t < trackPlayers.size(); ++t)
        {
            auto& player = *trackPlayers.getReference(t);
            juce::Array<juce::var> plugs;
            for (int p = 0; p < player.getNumPlugins(); ++p)
                if (auto* inst = player.getPlugin(p))
                    plugs.add(pluginToVar(*inst, player.isPluginBypassed(p)));
            auto* tobj = new juce::DynamicObject();
            tobj->setProperty("plugins", plugs);
            tracksArray.add(juce::var(tobj));
        }
        root->setProperty("tracks", tracksArray);
        juce::Array<juce::var> master;
        {
            const juce::ScopedLock ml(masterPluginLock);
            for (auto* p : masterPlugins) if (p) master.add(pluginToVar(*p, false));
        }
        root->setProperty("master", master);

        const auto sidecar = file.getSiblingFile(file.getFileNameWithoutExtension() + ".plugins.json");
        return sidecar.replaceWithText(juce::JSON::toString(juce::var(root)));
    }

    void StudioAudioEngine::applySessionToPlayers()
    {
        transportState.setTempo(session.getTempo());
        transportState.setTimeSignature(session.timeSigNumerator, session.timeSigDenominator);
        transportState.setLoopRange(session.loopStartSample, session.loopEndSample);
        transportState.setLooping(session.loopEnabled);
        transportState.setPunchRange(session.punchInSample, session.punchOutSample);
        transportState.setPunchEnabled(session.punchEnabled);
        metronomeOn.store(session.metronomeEnabled);
        metronomeGain.store(session.metronomeVolume);
        limiterOn.store(session.masterLimiter);
        masterVolumeDbAtomic.store(session.masterVolumeDb);
    }

    void StudioAudioEngine::newSession()
    {
        stop();
        session.clear();
        session.setName("Untitled Session");
        session.setTempo(120.0);
        session.setSampleRate(currentSampleRate);
        transportState.setPositionSamples(0);
        {
            const juce::ScopedLock ml(masterPluginLock);
            masterPlugins.clear();
        }
        midiBindings.clear();
        applySessionToPlayers();
        buildTrackPlayers();
        currentSessionFile = juce::File();
        markSaved();
        lastAutosaveHash = lastSnapshotHash;
    }

    bool StudioAudioEngine::loadSession(const juce::File& file)
    {
        stop();
        if (!session.loadFromFile(file))
            return false;
        session.rescaleToSampleRate(currentSampleRate);
        transportState.setPositionSamples(0);
        applySessionToPlayers();

        midiBindings.clear();
        if (auto* extras = session.engineExtras.getDynamicObject())
            if (auto* binds = extras->getProperty("midiBindings").getArray())
                for (auto& b : *binds) midiBindings.add(MidiCCBinding::fromVar(b));

        {
            const juce::ScopedLock ml(masterPluginLock);
            masterPlugins.clear();
        }
        buildTrackPlayers();

        auto makePlugin = [this] (juce::DynamicObject* o) -> std::unique_ptr<juce::AudioPluginInstance>
        {
            juce::PluginDescription desc;
            desc.name = o->getProperty("name").toString();
            desc.uniqueId = (int) o->getProperty("uid");
            desc.deprecatedUid = (int) o->getProperty("deprecatedUid");
            desc.fileOrIdentifier = o->getProperty("fileOrId").toString();
            desc.pluginFormatName = o->getProperty("formatName").toString();
            desc.isInstrument = (bool) o->getProperty("isInstrument");
            juce::String err;
            auto inst = pluginFormatManager.createPluginInstance(desc, currentSampleRate, maxBlockSize, err);
            if (inst == nullptr)
            {
                juce::Logger::writeToLog("Missing plugin on load: " + desc.name + " (" + err + ")");
                return nullptr;
            }
            juce::MemoryOutputStream decoded;
            juce::Base64::convertFromBase64(decoded, o->getProperty("state").toString());
            inst->setStateInformation(decoded.getData(), (int) decoded.getDataSize());
            inst->enableAllBuses();
            inst->setRateAndBufferSizeDetails(currentSampleRate, maxBlockSize);
            inst->prepareToPlay(currentSampleRate, maxBlockSize);
            return inst;
        };

        const auto sidecar = file.getSiblingFile(file.getFileNameWithoutExtension() + ".plugins.json");
        if (sidecar.existsAsFile())
        {
            const auto parsed = juce::JSON::parse(sidecar.loadFileAsString());
            if (auto* root = parsed.getDynamicObject())
            {
                if (auto* tracksArr = root->getProperty("tracks").getArray())
                    for (int t = 0; t < juce::jmin(tracksArr->size(), trackPlayers.size()); ++t)
                        if (auto* tobj = (*tracksArr)[t].getDynamicObject())
                            if (auto* plugs = tobj->getProperty("plugins").getArray())
                                for (auto& pv : *plugs)
                                    if (auto* o = pv.getDynamicObject())
                                        if (auto inst = makePlugin(o))
                                        {
                                            const bool byp = (bool) o->getProperty("bypassed");
                                            const juce::ScopedLock sl(playerLock);
                                            auto& player = *trackPlayers.getReference(t);
                                            player.addPlugin(std::move(inst));
                                            player.setPluginBypassed(player.getNumPlugins() - 1, byp);
                                        }
                if (auto* master = root->getProperty("master").getArray())
                    for (auto& pv : *master)
                        if (auto* o = pv.getDynamicObject())
                            if (auto inst = makePlugin(o))
                                addMasterPlugin(std::move(inst));
            }
        }

        sessionToStepSequencer();
        updatePluginDelayCompensation();
        publishSnapshot(true);
        currentSessionFile = file;
        markSaved();
        lastAutosaveHash = lastSnapshotHash;
        return true;
    }

    juce::File StudioAudioEngine::getAutosaveFile() const
    {
        const auto name = currentSessionFile.existsAsFile() ? currentSessionFile.getFileNameWithoutExtension() : legalName(session.getName());
        return autosaveFolder().getChildFile(name + " (autosave).novastudio");
    }

    juce::File StudioAudioEngine::findNewestAutosave() const
    {
        juce::File newest;
        for (const auto& f : autosaveFolder().findChildFiles(juce::File::findFiles, false, "*.novastudio"))
            if (!newest.existsAsFile() || f.getLastModificationTime() > newest.getLastModificationTime())
                newest = f;
        return newest;
    }

    void StudioAudioEngine::autosaveIfNeeded()
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (recordingActive.load() || offlineRendering.load()) return;
        if (lastSnapshotHash == lastAutosaveHash || lastSnapshotHash == savedHash) return;
        if (now - lastAutosaveTime < 60000) return;
        lastAutosaveTime = now;
        autosaveFolder().createDirectory();
        if (saveSession(getAutosaveFile()))
            lastAutosaveHash = lastSnapshotHash;
    }

    //==========================================================================
    // Bus routing
    //==========================================================================
    int StudioAudioEngine::parseBusIndex(const juce::String& busName)
    {
        const auto t = busName.trim();
        if (!t.startsWithIgnoreCase("Bus ")) return -1;
        const int n = t.substring(4).getIntValue();
        return n >= 1 ? juce::jlimit(0, kNumSendBuses - 1, (n - 1) / 2) : -1;
    }

    juce::String StudioAudioEngine::getBusDisplayName(int busIndex) const
    {
        if (!juce::isPositiveAndBelow(busIndex, kNumSendBuses)) return "Main Out";
        for (int i = 0; i < session.getNumTracks(); ++i)
        {
            const auto& t = session.getTrack(i);
            if (t.type == TrackType::Aux && t.auxInputBusIndex == busIndex)
                return sendBusName(busIndex) + "  (" + t.name + ")";
        }
        return sendBusName(busIndex);
    }

    juce::StringArray StudioAudioEngine::getOutputChoices(int forTrack) const
    {
        juce::StringArray out { "Main Out" };
        const bool isAux = juce::isPositiveAndBelow(forTrack, session.getNumTracks()) && session.getTrack(forTrack).type == TrackType::Aux;
        const int ownBus = isAux ? session.getTrack(forTrack).auxInputBusIndex : -1;
        for (int i = 0; i < session.getNumTracks(); ++i)
        {
            const auto& t = session.getTrack(i);
            if (t.type != TrackType::Aux || t.auxInputBusIndex < 0 || i == forTrack) continue;
            // Skip buses whose aux chain leads back into this aux (feedback loop).
            bool loops = false;
            int bus = t.auxInputBusIndex;
            for (int hop = 0; hop < kNumSendBuses && !loops; ++hop)
            {
                int next = -1;
                for (int k = 0; k < session.getNumTracks(); ++k)
                    if (session.getTrack(k).type == TrackType::Aux && session.getTrack(k).auxInputBusIndex == bus)
                        next = parseBusIndex(session.getTrack(k).outputBus);
                if (next < 0) break;
                if (next == ownBus) loops = true;
                bus = next;
            }
            if (!loops && t.auxInputBusIndex != ownBus)
                out.add(getBusDisplayName(t.auxInputBusIndex));
        }
        return out;
    }

    void StudioAudioEngine::addTrackClipGain(int trackIndex, float deltaDb)
    {
        if (!juce::isPositiveAndBelow(trackIndex, session.getNumTracks()) || std::abs(deltaDb) < 0.01f) return;
        for (auto& c : session.getTrack(trackIndex).clips)
            if (!c.isMidi) c.gainDb = juce::jlimit(-48.0f, 24.0f, c.gainDb + deltaDb);
        publishSnapshot(false);
    }

    void StudioAudioEngine::setTrackOutputBus(int trackIndex, const juce::String& busName)
    {
        if (!juce::isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        auto& tr = session.getTrack(trackIndex);
        const int idx = parseBusIndex(busName);
        if (tr.type == TrackType::Aux && idx >= 0 && idx == tr.auxInputBusIndex) return;   // no self-feedback
        tr.outputBus = idx < 0 ? juce::String("Main Out") : sendBusName(idx);
        if (juce::isPositiveAndBelow(trackIndex, trackPlayers.size()))
            trackPlayers.getReference(trackIndex)->outputBusIndex.store(idx);
        updateRoutingOrder();
    }

    void StudioAudioEngine::updateRoutingOrder()
    {
        const int n = trackPlayers.size();
        std::vector<int> order;
        std::vector<int> aux;
        for (int i = 0; i < n; ++i)
            (trackPlayers.getReference(i)->isAuxTrack ? aux : order).push_back(i);

        // Kahn's algorithm: aux A must run before aux B if A outputs into B's bus.
        std::vector<int> remaining = aux;
        while (!remaining.empty())
        {
            bool progressed = false;
            for (size_t k = 0; k < remaining.size(); ++k)
            {
                const int b = remaining[k];
                const int busB = trackPlayers.getReference(b)->auxInputBusIndex;
                bool waits = false;
                for (int a : remaining)
                    if (a != b && trackPlayers.getReference(a)->outputBusIndex.load() == busB && busB >= 0)
                        waits = true;
                if (!waits)
                {
                    order.push_back(b);
                    remaining.erase(remaining.begin() + (long) k);
                    progressed = true;
                    break;
                }
            }
            if (!progressed) { for (int r : remaining) order.push_back(r); break; }   // cycle: keep list order
        }

        const juce::ScopedLock sl(playerLock);
        processOrder.swap(order);
    }

    //==========================================================================
    // Freeze
    //==========================================================================
    bool StudioAudioEngine::isTrackFrozen(int t) const noexcept
    {
        return juce::isPositiveAndBelow(t, session.getNumTracks()) && session.getTrack(t).frozen;
    }

    bool StudioAudioEngine::freezeTrack(int t, juce::String* error)
    {
        if (!juce::isPositiveAndBelow(t, session.getNumTracks()) || !juce::isPositiveAndBelow(t, trackPlayers.size()))
            return false;
        auto& tr = session.getTrack(t);
        if (tr.frozen) return true;
        auto& player = *trackPlayers.getReference(t);
        if (player.getNumPlugins() == 0)
        {
            if (error) *error = "There are no plugins on this track to freeze.";
            return false;
        }
        int64_t end = 0;
        for (const auto& c : tr.clips)
            if (!c.muted) end = juce::jmax(end, c.startSample + c.lengthSamples);
        if (end <= 0)
        {
            if (error) *error = "The track has no clips to freeze.";
            return false;
        }

        const auto folder = getDefaultSessionsFolder().getChildFile("Freeze");
        folder.createDirectory();
        const auto file = folder.getChildFile(legalName(session.getName()) + " - " + legalName(tr.name) + " (freeze).wav").getNonexistentSibling();

        const int ownLatency = [&] { int l = 0; for (int i = 0; i < player.getNumPlugins(); ++i) if (!player.isPluginBypassed(i)) if (auto* pl = player.getPlugin(i)) l += pl->getLatencySamples(); return l; }();

        ExportOptions o;
        o.startSample = 0;
        o.endSample = end + ownLatency;
        o.tailSeconds = 5.0;
        o.bitDepth = 32;
        o.soloTrack = t;
        o.includeMaster = false;
        offlineFreezeTrack = t;
        const auto res = exportAudio(file, o);
        offlineFreezeTrack = -1;
        if (!res.ok)
        {
            if (error) *error = res.error;
            return false;
        }

        tr.frozen = true;
        tr.frozenFile = file;
        tr.frozenOffsetSamples = ownLatency;
        tr.lockedBeforeFreeze = tr.locked;
        tr.locked = true;
        player.frozen.store(true);
        updatePluginDelayCompensation();
        publishSnapshot(true);
        return true;
    }

    void StudioAudioEngine::unfreezeTrack(int t)
    {
        if (!isTrackFrozen(t)) return;
        auto& tr = session.getTrack(t);
        tr.frozen = false;
        tr.locked = tr.lockedBeforeFreeze;
        const auto old = tr.frozenFile;
        tr.frozenFile = juce::File();
        tr.frozenOffsetSamples = 0;
        if (juce::isPositiveAndBelow(t, trackPlayers.size()))
            trackPlayers.getReference(t)->frozen.store(false);
        updatePluginDelayCompensation();
        publishSnapshot(true);
        retireOldSnapshots(false);
        juce::Timer::callAfterDelay(2000, [old] { old.deleteFile(); });   // after the audio thread lets go of it
    }
}
