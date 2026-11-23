#include "StudioAudioEngine.h"

namespace NovaStudio
{

    

    void StudioAudioEngine::setTrackVolume(int index, float volumeDb)
    {
        if (!isPositiveAndBelow(index, trackPlayers.size()))
            return;

        session.getTrack(index).volumeDb = volumeDb;
        trackPlayers.getReference(index)->volumeDb = volumeDb;
    }

    void StudioAudioEngine::setTrackPan(int index, float pan)
    {
        if (!isPositiveAndBelow(index, trackPlayers.size()))
            return;

        session.getTrack(index).pan = pan;
        trackPlayers.getReference(index)->pan = pan;
    }

    void StudioAudioEngine::setTrackMute(int index, bool muted)
    {
        if (!isPositiveAndBelow(index, trackPlayers.size()))
            return;

        session.getTrack(index).muted = muted;
        trackPlayers.getReference(index)->muted = muted;
    }

    void StudioAudioEngine::setTrackSolo(int index, bool solo)
    {
        if (!isPositiveAndBelow(index, trackPlayers.size()))
            return;

        session.getTrack(index).solo = solo;
        trackPlayers.getReference(index)->solo = solo;
        updateSoloStates();
    }


    void StudioAudioEngine::setTrackColour(int index, juce::Colour colour)
    {
        if (!isPositiveAndBelow(index, session.getNumTracks()))
            return;
        session.getTrack(index).colour = colour;
    }

    juce::Colour StudioAudioEngine::getTrackColour(int index) const noexcept
    {
        if (!isPositiveAndBelow(index, session.getNumTracks()))
            return juce::Colours::steelblue;
        return session.getTrack(index).colour;
    }

    void StudioAudioEngine::setTrackGroup(int index, const juce::String& groupName)
    {
        if (!isPositiveAndBelow(index, session.getNumTracks()))
            return;
        session.getTrack(index).groupName = groupName;
    }

    juce::String StudioAudioEngine::getTrackGroup(int index) const noexcept
    {
        if (!isPositiveAndBelow(index, session.getNumTracks()))
            return {};
        return session.getTrack(index).groupName;
    }

    void StudioAudioEngine::setTrackLocked(int index, bool locked)
    {
        if (!isPositiveAndBelow(index, session.getNumTracks()))
            return;
        session.getTrack(index).locked = locked;
    }

    bool StudioAudioEngine::isTrackLocked(int index) const noexcept
    {
        if (!isPositiveAndBelow(index, session.getNumTracks()))
            return false;
        return session.getTrack(index).locked;
    }

    int StudioAudioEngine::addAutomationLane(int trackIndex, const juce::String& parameterId)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return -1;

        AutomationLane lane;
        lane.parameterId = parameterId;
        lane.enabled = true;
        auto& track = session.getTrack(trackIndex);
        track.automationLanes.add(lane);
        return track.automationLanes.size() - 1;
    }

    void StudioAudioEngine::removeAutomationLane(int trackIndex, int laneIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return;
        auto& lanes = session.getTrack(trackIndex).automationLanes;
        if (isPositiveAndBelow(laneIndex, lanes.size()))
            lanes.remove(laneIndex);
    }

    void StudioAudioEngine::setAutomationLaneEnabled(int trackIndex, int laneIndex, bool enabled)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return;
        auto& lanes = session.getTrack(trackIndex).automationLanes;
        if (isPositiveAndBelow(laneIndex, lanes.size()))
            lanes.getReference(laneIndex).enabled = enabled;
    }

    int StudioAudioEngine::getNumAutomationLanes(int trackIndex) const noexcept
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return 0;
        return session.getTrack(trackIndex).automationLanes.size();
    }

    const AutomationLane* StudioAudioEngine::getAutomationLane(int trackIndex, int laneIndex) const
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return nullptr;
        const auto& lanes = session.getTrack(trackIndex).automationLanes;
        if (!isPositiveAndBelow(laneIndex, lanes.size()))
            return nullptr;
        return &lanes.getReference(laneIndex);
    }

    int StudioAudioEngine::addAutomationPoint(int trackIndex, int laneIndex, double timeSeconds, float value)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return -1;
        auto& lanes = session.getTrack(trackIndex).automationLanes;
        if (!isPositiveAndBelow(laneIndex, lanes.size()))
            return -1;

        auto& points = lanes.getReference(laneIndex).points;
        AutomationPoint point;
        point.timeSeconds = timeSeconds;
        point.value = value;

        int insertIndex = points.size();
        for (int i = 0; i < points.size(); ++i)
        {
            if (timeSeconds < points.getReference(i).timeSeconds)
            {
                insertIndex = i;
                break;
            }
        }
        points.insert(insertIndex, point);
        return insertIndex;
    }

    void StudioAudioEngine::moveAutomationPoint(int trackIndex, int laneIndex, int pointIndex, double timeSeconds, float value)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return;
        auto& lanes = session.getTrack(trackIndex).automationLanes;
        if (!isPositiveAndBelow(laneIndex, lanes.size()))
            return;
        auto& points = lanes.getReference(laneIndex).points;
        if (!isPositiveAndBelow(pointIndex, points.size()))
            return;

        points.remove(pointIndex);
        AutomationPoint point;
        point.timeSeconds = timeSeconds;
        point.value = value;
        int insertIndex = points.size();
        for (int i = 0; i < points.size(); ++i)
        {
            if (timeSeconds < points.getReference(i).timeSeconds)
            {
                insertIndex = i;
                break;
            }
        }
        points.insert(insertIndex, point);
    }

    void StudioAudioEngine::removeAutomationPoint(int trackIndex, int laneIndex, int pointIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks()))
            return;
        auto& lanes = session.getTrack(trackIndex).automationLanes;
        if (!isPositiveAndBelow(laneIndex, lanes.size()))
            return;
        auto& points = lanes.getReference(laneIndex).points;
        if (isPositiveAndBelow(pointIndex, points.size()))
            points.remove(pointIndex);
    }

    float StudioAudioEngine::evaluateAutomationLane(const AutomationLane& lane, double timeSeconds)
    {
        const auto& points = lane.points;
        if (points.isEmpty())
            return 0.0f;
        if (points.size() == 1 || timeSeconds <= points.getFirst().timeSeconds)
            return points.getFirst().value;
        if (timeSeconds >= points.getLast().timeSeconds)
            return points.getLast().value;

        for (int i = 0; i < points.size() - 1; ++i)
        {
            const auto& a = points.getReference(i);
            const auto& b = points.getReference(i + 1);
            if (timeSeconds >= a.timeSeconds && timeSeconds <= b.timeSeconds)
            {
                const double span = b.timeSeconds - a.timeSeconds;
                const float t = span > 0.0 ? (float)((timeSeconds - a.timeSeconds) / span) : 0.0f;
                return a.value + (b.value - a.value) * t;
            }
        }
        return points.getLast().value;
    }

    int StudioAudioEngine::getTrackCount() const noexcept
    {
        return session.getNumTracks();
    }

    const Session& StudioAudioEngine::getSession() const noexcept
    {
        return session;
    }

    Session& StudioAudioEngine::getSession() noexcept
    {
        return session;
    }

    void StudioAudioEngine::previewSample(const juce::File& file)
    {
        if (! file.existsAsFile())
            return;

        std::unique_ptr<juce::AudioFormatReader> reader(previewFormatManager.createReaderFor(file));
        if (reader == nullptr)
            return;

        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);

        previewTransport.stop();
        previewTransport.setSource(nullptr);
        previewReaderSource = std::move(newSource);
        previewTransport.setSource(previewReaderSource.get(), 0, nullptr, currentSampleRate);
        previewTransport.setPosition(0.0);
        previewTransport.start();
        previewActive.store(true);
    }

    void StudioAudioEngine::stopPreviewSample()
    {
        previewActive.store(false);
        previewTransport.stop();
    }

    // ── Plugin management ─────────────────────────────────────────────────────

    juce::AudioPluginInstance* StudioAudioEngine::getTrackPlugin(int trackIndex, int pluginSlot) const
    {
        if (trackIndex == kMasterTrackIndex) return getMasterPlugin(pluginSlot);
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return nullptr;
        return trackPlayers.getReference(trackIndex)->getPlugin(pluginSlot);
    }

    int StudioAudioEngine::getTrackPluginCount(int trackIndex) const
    {
        if (trackIndex == kMasterTrackIndex) return getMasterPluginCount();
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return 0;
        return trackPlayers.getReference(trackIndex)->getNumPlugins();
    }

    void StudioAudioEngine::getTrackPluginState(int trackIndex, int pluginSlot, juce::MemoryBlock& dest) const
    {
        if (trackIndex == kMasterTrackIndex) return; // TODO: master plugin state persistence
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return;
        trackPlayers.getReference(trackIndex)->getPluginState(pluginSlot, dest);
    }

    void StudioAudioEngine::setTrackPluginState(int trackIndex, int pluginSlot, const void* data, size_t size)
    {
        if (trackIndex == kMasterTrackIndex) return; // TODO: master plugin state persistence
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return;
        trackPlayers.getReference(trackIndex)->setPluginState(pluginSlot, data, size);
    }

    bool StudioAudioEngine::removePluginFromTrack(int trackIndex, int pluginSlot)
    {
        if (trackIndex == kMasterTrackIndex) return removeMasterPlugin(pluginSlot);
        bool removed = false;
        {
            juce::ScopedLock sl(playerLock);
            if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return false;
            removed = trackPlayers.getReference(trackIndex)->removePlugin(pluginSlot);
        }
        if (removed) updatePluginDelayCompensation();
        return removed;
    }

    bool StudioAudioEngine::movePluginInTrack(int trackIndex, int fromSlot, int toSlot)
    {
        if (trackIndex == kMasterTrackIndex) return false;
        juce::ScopedLock sl(playerLock);
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return false;
        return trackPlayers.getReference(trackIndex)->movePlugin(fromSlot, toSlot);
    }

    void StudioAudioEngine::setPluginBypassed(int trackIndex, int pluginSlot, bool bypassed)
    {
        if (trackIndex == kMasterTrackIndex) return;
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return;
        trackPlayers.getReference(trackIndex)->setPluginBypassed(pluginSlot, bypassed);
        updatePluginDelayCompensation();
    }

    bool StudioAudioEngine::isPluginBypassed(int trackIndex, int pluginSlot) const
    {
        if (trackIndex == kMasterTrackIndex) return false;
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return false;
        return trackPlayers.getReference(trackIndex)->isPluginBypassed(pluginSlot);
    }

    // ── Master plugin chain ───────────────────────────────────────────────────

    bool StudioAudioEngine::addMasterPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin)
    {
        if (!plugin) return false;
        juce::ScopedLock sl(masterPluginLock);
        if (masterPlugins.size() >= kMaxMasterPlugins) return false;
        if (currentSampleRate > 0 && currentBufferSize > 0)
            plugin->prepareToPlay(currentSampleRate, currentBufferSize);
        masterPlugins.add(plugin.release());
        juce::MessageManager::callAsync([this] { updatePluginDelayCompensation(); });
        return true;
    }

    bool StudioAudioEngine::removeMasterPlugin(int slot)
    {
        juce::ScopedLock sl(masterPluginLock);
        if (!isPositiveAndBelow(slot, masterPlugins.size())) return false;
        masterPlugins.remove(slot);
        juce::MessageManager::callAsync([this] { updatePluginDelayCompensation(); });
        return true;
    }

    juce::AudioPluginInstance* StudioAudioEngine::getMasterPlugin(int slot) const
    {
        juce::ScopedLock sl(masterPluginLock);
        if (!isPositiveAndBelow(slot, masterPlugins.size())) return nullptr;
        return masterPlugins[slot];
    }

    int StudioAudioEngine::getMasterPluginCount() const
    {
        juce::ScopedLock sl(masterPluginLock);
        return masterPlugins.size();
    }

    // ── Metering ──────────────────────────────────────────────────────────────

    void StudioAudioEngine::updateSoloStates()
    {
        bool anySolo = false;
        for (int index = 0; index < session.getNumTracks(); ++index)
        {
            if (session.getTrack(index).solo)
            {
                anySolo = true;
                break;
            }
        }

        juce::ScopedLock sl(playerLock);
        for (int index = 0; index < trackPlayers.size(); ++index)
            trackPlayers.getReference(index)->setSoloMode(anySolo);
    }

    // ── Send bus helpers ────────────────────────────────────────────────────────

    void StudioAudioEngine::prepareSendBuses(int numChannels, int numSamples)
    {
        for (int b = 0; b < kNumSendBuses; ++b)
            sendBusBuffers[b].setSize(numChannels, numSamples, false, true, true);
    }

    void StudioAudioEngine::clearSendBuses(int numSamples)
    {
        for (int b = 0; b < kNumSendBuses; ++b)
            sendBusBuffers[b].clear(0, numSamples);
    }

    void StudioAudioEngine::syncTrackPlayerSends(int trackIndex)
    {
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return;
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        const Track& tr = session.getTrack(trackIndex);
        auto& player    = *trackPlayers.getReference(trackIndex);

        for (int si = 0; si < TrackPlayer::kMaxSends; ++si)
        {
            player.sendBusIndex[si] = tr.sendBusIndex[si];
            player.sendLevels[si]   = tr.sendLevels[si];
            player.sendPreFader[si] = tr.sendPreFader[si];
        }
        for (int b = 0; b < kNumSendBuses; ++b)
        {
            bool used = false;
            for (int si = 0; si < TrackPlayer::kMaxSends; ++si)
                if (tr.sendBusIndex[si] == b) { used = true; break; }
            player.sendBusBuffers[b] = used ? &sendBusBuffers[b] : nullptr;
        }
    }

    void StudioAudioEngine::setTrackSendBus(int trackIndex, int sendIndex, int busIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        if (!isPositiveAndBelow(sendIndex, 6)) return;
        session.getTrack(trackIndex).sendBusIndex[sendIndex] = busIndex;
        juce::ScopedLock sl(playerLock);
        syncTrackPlayerSends(trackIndex);
    }

    int StudioAudioEngine::getTrackSendBus(int trackIndex, int sendIndex) const noexcept
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return -1;
        if (!isPositiveAndBelow(sendIndex, 6)) return -1;
        return session.getTrack(trackIndex).sendBusIndex[sendIndex];
    }

    void StudioAudioEngine::setTrackSendPreFader(int trackIndex, int sendIndex, bool preFader)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        if (!isPositiveAndBelow(sendIndex, 6)) return;
        session.getTrack(trackIndex).sendPreFader[sendIndex] = preFader;
        juce::ScopedLock sl(playerLock);
        syncTrackPlayerSends(trackIndex);
    }

    bool StudioAudioEngine::getTrackSendPreFader(int trackIndex, int sendIndex) const noexcept
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return false;
        if (!isPositiveAndBelow(sendIndex, 6)) return false;
        return session.getTrack(trackIndex).sendPreFader[sendIndex];
    }


    void StudioAudioEngine::setAuxTrackBus(int trackIndex, int busIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        session.getTrack(trackIndex).auxInputBusIndex = busIndex;
        {
            juce::ScopedLock sl(playerLock);
            if (isPositiveAndBelow(trackIndex, trackPlayers.size()))
            {
                trackPlayers.getReference(trackIndex)->isAuxTrack      = true;
                trackPlayers.getReference(trackIndex)->auxInputBusIndex = busIndex;
            }
        }
        updateRoutingOrder();
    }

    void StudioAudioEngine::setMacroValue(int macroIndex, float normalizedValue)
    {
        if (!isPositiveAndBelow(macroIndex, session.getNumMacros()))
            return;

        auto& macro = session.getMacro(macroIndex);
        macro.value = juce::jlimit(0.0f, 1.0f, normalizedValue);
        applyMacroTargets(macro);
    }

    void StudioAudioEngine::applyMacroTargets(const MacroControl& macro)
    {
        for (const auto& target : macro.targets)
            applyParameterTarget(target, macro.value);
    }

    void StudioAudioEngine::applyParameterTarget(const MacroTarget& target, float normalizedValue)
    {
        if (!isPositiveAndBelow(target.trackIndex, session.getNumTracks()))
            return;

        const float n = juce::jlimit(0.0f, 1.0f, normalizedValue);
        const float scaled = target.minValue + (target.maxValue - target.minValue) * n;

        if (target.parameterId == "volume")
            setTrackVolume(target.trackIndex, scaled);
        else if (target.parameterId == "pan")
            setTrackPan(target.trackIndex, scaled);
        else if (target.parameterId.startsWith("send"))
        {
            const int sendIndex = target.parameterId.substring(4).getIntValue() - 1;
            if (isPositiveAndBelow(sendIndex, 6))
                setTrackSendLevel(target.trackIndex, sendIndex, scaled);
        }
        else if (target.parameterId.startsWith("plugin:"))
        {
            auto tokens = juce::StringArray::fromTokens(target.parameterId, ":", "");
            if (tokens.size() == 3)
            {
                const int slot = tokens[1].getIntValue();
                const int paramIndex = tokens[2].getIntValue();
                if (auto* plugin = getTrackPlugin(target.trackIndex, slot))
                {
                    auto& params = plugin->getParameters();
                    if (isPositiveAndBelow(paramIndex, params.size()))
                        params[paramIndex]->setValue(juce::jlimit(0.0f, 1.0f, scaled));
                }
            }
        }
    }

    // ── MIDI Learn ──────────────────────────────────────────────────────────

    juce::var StudioAudioEngine::MidiCCBinding::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("ccNumber", ccNumber);
        obj->setProperty("channel", channel);
        obj->setProperty("macroIndex", macroIndex);
        obj->setProperty("target", target.toVar());
        return juce::var(obj);
    }

    StudioAudioEngine::MidiCCBinding StudioAudioEngine::MidiCCBinding::fromVar(const juce::var& v)
    {
        MidiCCBinding b;
        if (auto* obj = v.getDynamicObject())
        {
            b.ccNumber   = obj->getProperty("ccNumber");
            b.channel    = obj->getProperty("channel");
            b.macroIndex = obj->getProperty("macroIndex");
            b.target     = MacroTarget::fromVar(obj->getProperty("target"));
        }
        return b;
    }

    void StudioAudioEngine::beginMidiLearnForMacro(int macroIndex)
    {
        if (!isPositiveAndBelow(macroIndex, session.getNumMacros()))
            return;
        midiLearnIsForMacro = true;
        midiLearnMacroIndex = macroIndex;
        midiLearnTarget = MacroTarget();
        midiLearnPending.store(true);
    }

    void StudioAudioEngine::beginMidiLearnForTarget(const MacroTarget& target)
    {
        midiLearnIsForMacro = false;
        midiLearnMacroIndex = -1;
        midiLearnTarget = target;
        midiLearnPending.store(true);
    }

    void StudioAudioEngine::cancelMidiLearn()
    {
        midiLearnPending.store(false);
        midiLearnMacroIndex = -1;
    }

    void StudioAudioEngine::removeMidiBinding(int index)
    {
        if (isPositiveAndBelow(index, midiBindings.size()))
            midiBindings.remove(index);
    }

    juce::String StudioAudioEngine::describeMidiBinding(int index) const
    {
        if (!isPositiveAndBelow(index, midiBindings.size()))
            return {};

        const auto& b = midiBindings.getReference(index);
        juce::String ccDesc = "CC " + juce::String(b.ccNumber)
                            + (b.channel > 0 ? (" Ch" + juce::String(b.channel)) : juce::String(" (any ch)"));

        if (b.macroIndex >= 0 && isPositiveAndBelow(b.macroIndex, session.getNumMacros()))
            return ccDesc + "  ->  Macro: " + session.getMacro(b.macroIndex).name;

        if (isPositiveAndBelow(b.target.trackIndex, session.getNumTracks()))
            return ccDesc + "  ->  " + session.getTrack(b.target.trackIndex).name + " : " + b.target.parameterId;

        return ccDesc + "  ->  (unknown target)";
    }


    void StudioAudioEngine::setTrackSendLevel(int trackIndex, int sendIndex, float db)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        if (!isPositiveAndBelow(sendIndex, 6)) return;
        session.getTrack(trackIndex).sendLevels[sendIndex] = db;
        juce::ScopedLock sl(playerLock);
        if (isPositiveAndBelow(trackIndex, trackPlayers.size()))
            trackPlayers.getReference(trackIndex)->sendLevels[sendIndex] = db;
    }

    float StudioAudioEngine::getTrackSendLevel(int trackIndex, int sendIndex) const noexcept
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return -100.0f;
        if (!isPositiveAndBelow(sendIndex, 6)) return -100.0f;
        return session.getTrack(trackIndex).sendLevels[sendIndex];
    }

    // ── Step sequencer ──────────────────────────────────────────────────────────


    void StudioAudioEngine::setStepSeqStep(int row, int step, bool active)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        if (!isPositiveAndBelow(step, StepSequencerState::kNumSteps)) return;
        stepSeq.steps[row][step] = active;
    }

    void StudioAudioEngine::setStepSeqActiveStepCount(int count)
    {
        if (count == 16 || count == 32 || count == 64)
            stepSeq.activeStepCount = count;
    }

    void StudioAudioEngine::setStepSeqSwing(float swing)
    {
        stepSeq.swing = juce::jlimit(0.0f, 1.0f, swing);
    }

    // Groove/accent templates: a 4-step repeating pattern of velocity multipliers,
    // tiled across the whole pattern length. FL-style "feel" presets layered on
    // top of the user's drawn-in velocities and the existing global swing amount.
    static const float kGrooveAccentTables[StudioAudioEngine::StepSequencerState::kNumGrooveTemplates][4] =
    {
        { 1.00f, 1.00f, 1.00f, 1.00f },   // Straight — no accent
        { 1.00f, 0.78f, 0.92f, 0.78f },   // Backbeat — emphasise beats 1 & 3
        { 1.00f, 0.70f, 0.85f, 0.70f },   // Pop — strong downbeat, soft off-beats
        { 0.85f, 1.00f, 0.85f, 1.00f },   // Push — emphasise the off-beats
        { 1.00f, 0.60f, 0.75f, 0.55f },   // Hip-Hop — heavy downbeat, laid-back ghosts
    };

    static const char* kGrooveTemplateNames[StudioAudioEngine::StepSequencerState::kNumGrooveTemplates] =
    {
        "Straight", "Backbeat", "Pop", "Push", "Hip-Hop"
    };

    const char* StudioAudioEngine::StepSequencerState::grooveTemplateName(int index)
    {
        if (juce::isPositiveAndBelow(index, kNumGrooveTemplates))
            return kGrooveTemplateNames[index];
        return "Straight";
    }

    float StudioAudioEngine::StepSequencerState::grooveAccentForStep(int templateIndex, int stepIndex)
    {
        if (!juce::isPositiveAndBelow(templateIndex, kNumGrooveTemplates))
            templateIndex = 0;
        return kGrooveAccentTables[templateIndex][((stepIndex % 4) + 4) % 4];
    }

    void StudioAudioEngine::setStepSeqGroove(int templateIndex)
    {
        stepSeq.grooveTemplate = juce::jlimit(0, StepSequencerState::kNumGrooveTemplates - 1, templateIndex);
    }

    int StudioAudioEngine::getStepSeqGroove() const noexcept
    {
        return stepSeq.grooveTemplate;
    }

    void StudioAudioEngine::setStepSeqVelocity(int row, int step, float velocity)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        if (!isPositiveAndBelow(step, StepSequencerState::kNumSteps)) return;
        stepSeq.velocities[row][step] = juce::jlimit(0.0f, 1.0f, velocity);
    }

    void StudioAudioEngine::setStepSeqRowVolume(int row, float vol)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        stepSeq.rowVolumes[row] = juce::jlimit(0.0f, 1.0f, vol);
    }

    void StudioAudioEngine::setStepSeqRowPan(int row, float pan)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        stepSeq.rowPans[row] = juce::jlimit(-1.0f, 1.0f, pan);
    }

    void StudioAudioEngine::setStepSeqRowPitch(int row, float semitones)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        stepSeq.rowPitches[row] = juce::jlimit(-24.0f, 24.0f, semitones);
    }

    void StudioAudioEngine::setStepSeqRowMuted(int row, bool muted)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        stepSeq.rowMuted[row] = muted;
    }

    void StudioAudioEngine::setStepSeqRowEnvelope(int row, bool enabled, float attackSecs, float holdSecs,
                                                   float decaySecs, float sustainLevel, float releaseSecs)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        auto& ce = stepSeq.channelEngines[(size_t)row];
        ce.envEnabled = enabled;
        ce.envAttack  = juce::jmax(0.0005f, attackSecs);
        ce.envHold    = juce::jmax(0.0f, holdSecs);
        ce.envDecay   = juce::jmax(0.0005f, decaySecs);
        ce.envSustain = juce::jlimit(0.0f, 1.0f, sustainLevel);
        ce.envRelease = juce::jmax(0.0005f, releaseSecs);
    }

    void StudioAudioEngine::setStepSeqRowFilter(int row, bool enabled, int filterType,
                                                 float cutoffHz, float resonance)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        auto& ce = stepSeq.channelEngines[(size_t)row];
        ce.filterEnabled   = enabled;
        ce.filterType      = juce::jlimit(0, 1, filterType);
        ce.filterCutoffHz  = juce::jlimit(20.0f, 20000.0f, cutoffHz);
        ce.filterResonance = juce::jlimit(0.1f, 10.0f, resonance);
        ce.filterDirty     = true;
    }

    void StudioAudioEngine::setStepSeqRowLFO(int row, bool enabled, int target, float rateHz, float depth)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        auto& ce = stepSeq.channelEngines[(size_t)row];
        ce.lfoEnabled = enabled;
        ce.lfoTarget  = juce::jlimit(0, 2, target);
        ce.lfoRateHz  = juce::jlimit(0.01f, 30.0f, rateHz);
        ce.lfoDepth   = juce::jlimit(0.0f, 1.0f, depth);
    }

    void StudioAudioEngine::setStepSeqRowSampleRegion(int row, float startFrac, float endFrac, bool loop)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        auto& ce = stepSeq.channelEngines[(size_t)row];
        float s = juce::jlimit(0.0f, 1.0f, startFrac);
        float e = juce::jlimit(0.0f, 1.0f, endFrac);
        if (e <= s) e = juce::jmin(1.0f, s + 0.01f);
        ce.sampleStart  = s;
        ce.sampleEnd    = e;
        ce.loopEnabled  = loop;
    }

    float StudioAudioEngine::advanceChannelEnvelope(ChannelEngine& ce, double sampleRate)
    {
        const auto secsToSamples = [sampleRate](float secs) { return juce::jmax(1.0, (double) secs * sampleRate); };

        switch (ce.envStage)
        {
            case 0: // Idle -> begin Attack on (re)trigger
                ce.envStage = 1;
                ce.envPos   = 0.0;
                break;
            default: break;
        }

        switch (ce.envStage)
        {
            case 1: // Attack: 0 -> 1
            {
                const double atk = secsToSamples(ce.envAttack);
                ce.envLevel = (float) juce::jmin(1.0, ce.envPos / atk);
                ce.envPos += 1.0;
                if (ce.envPos >= atk) { ce.envStage = 2; ce.envPos = 0.0; }
                break;
            }
            case 2: // Hold at peak
            {
                ce.envLevel = 1.0f;
                const double hold = secsToSamples(ce.envHold);
                ce.envPos += 1.0;
                if (ce.envPos >= hold) { ce.envStage = 3; ce.envPos = 0.0; }
                break;
            }
            case 3: // Decay: 1 -> sustain level
            {
                const double dec = secsToSamples(ce.envDecay);
                const double t   = juce::jmin(1.0, ce.envPos / dec);
                ce.envLevel = (float) (1.0 + ((double) ce.envSustain - 1.0) * t);
                ce.envPos += 1.0;
                if (ce.envPos >= dec) { ce.envStage = 4; ce.envPos = 0.0; }
                break;
            }
            case 4: // Sustain: held until release is triggered
                ce.envLevel = ce.envSustain;
                break;
            case 5: // Release: sustain -> 0
            {
                const double rel = secsToSamples(ce.envRelease);
                const double t   = juce::jmin(1.0, ce.envPos / rel);
                ce.envLevel = (float) ((double) ce.envSustain * (1.0 - t));
                ce.envPos += 1.0;
                break;
            }
            default: break;
        }

        return ce.envLevel;
    }

    double StudioAudioEngine::applyChannelFilter(ChannelEngine& ce, int channel, double sample,
                                                  double sampleRate, float lfoValue)
    {
        const bool modulatingCutoff = ce.lfoEnabled && ce.lfoTarget == 2;

        if (ce.filterDirty || modulatingCutoff)
        {
            float cutoff = ce.filterCutoffHz;
            if (modulatingCutoff)
                cutoff *= std::pow(2.0f, lfoValue * ce.lfoDepth * 2.0f); // up to +/- 2 octaves
            cutoff = juce::jlimit(20.0f, (float) (sampleRate * 0.45), cutoff);

            const double w0    = juce::MathConstants<double>::twoPi * (double) cutoff / sampleRate;
            const double cosw0 = std::cos(w0);
            const double sinw0 = std::sin(w0);
            const double Q     = juce::jmax(0.1, (double) ce.filterResonance);
            const double alpha = sinw0 / (2.0 * Q);

            double b0, b1, b2, a0, a1, a2;
            if (ce.filterType == 1) // highpass
            {
                b0 =  (1.0 + cosw0) / 2.0;
                b1 = -(1.0 + cosw0);
                b2 =  (1.0 + cosw0) / 2.0;
            }
            else // lowpass
            {
                b0 = (1.0 - cosw0) / 2.0;
                b1 =  1.0 - cosw0;
                b2 = (1.0 - cosw0) / 2.0;
            }
            a0 =  1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 =  1.0 - alpha;

            ce.fb0 = b0 / a0; ce.fb1 = b1 / a0; ce.fb2 = b2 / a0;
            ce.fa1 = a1 / a0; ce.fa2 = a2 / a0;
            ce.filterDirty = false;
        }

        double& z1 = (channel == 0) ? ce.fz1L : ce.fz1R;
        double& z2 = (channel == 0) ? ce.fz2L : ce.fz2R;

        const double in  = sample;
        const double out = ce.fb0 * in + z1;
        z1 = ce.fb1 * in - ce.fa1 * out + z2;
        z2 = ce.fb2 * in - ce.fa2 * out;
        return out;
    }

    int StudioAudioEngine::getStepSeqCurrentStep() const noexcept
    {
        return currentPlayingStep.load();
    }
}
