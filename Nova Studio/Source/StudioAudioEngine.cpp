#include "StudioAudioEngine.h"

namespace NovaStudio
{
    static float decibelsToGain(float db)
    {
        return juce::Decibels::decibelsToGain(db);
    }

    StudioAudioEngine::TrackPlayer::TrackPlayer()
    {
        formatManager.registerBasicFormats();
    }

    StudioAudioEngine::TrackPlayer::~TrackPlayer() = default;

    void StudioAudioEngine::TrackPlayer::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
    {
        transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
        scratchBuffer.setSize(trackChannels, samplesPerBlockExpected, false, false, true);

        for (auto& plugin : pluginChain)
        {
            if (plugin)
                plugin->prepareToPlay(sampleRate, samplesPerBlockExpected);
        }
    }

    void StudioAudioEngine::TrackPlayer::releaseResources()
    {
        transportSource.releaseResources();
        scratchBuffer.setSize(trackChannels, 0);
        readerSource.reset();
    }

    void StudioAudioEngine::TrackPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
    {
        bufferToFill.clearActiveBufferRegion();

        if (!sessionPtr)
            return;

        if (!isPlaying)
            return;

        // Determine current transport sample position from the last set transport seconds
        const double sr = sessionPtr->getSampleRate();
        const int64_t currentTransportSamples = sr > 0.0 ? static_cast<int64_t>(currentTransportSeconds * sr) : 0;

        // Find clip overlapping this track at the transport position
        const Track& track = sessionPtr->getTrack(trackIndex);

        const Clip* activeClip = nullptr;
        int64_t clipRelativeStart = 0;

        const bool preferPreview = sessionPtr->isPreviewPlaybackEnabled();
        if (preferPreview)
        {
            for (int i = 0; i < track.clips.size(); ++i)
            {
                const Clip& c = track.clips.getReference(i);
                if (!c.isPreview)
                    continue;
                const int64_t clipStart = c.startSample;
                const int64_t clipEnd = clipStart + c.lengthSamples;
                if (currentTransportSamples >= clipStart && currentTransportSamples < clipEnd)
                {
                    activeClip = &c;
                    clipRelativeStart = currentTransportSamples - clipStart;
                    break;
                }
            }
        }

        if (activeClip == nullptr)
        {
            for (int i = 0; i < track.clips.size(); ++i)
            {
                const Clip& c = track.clips.getReference(i);
                const int64_t clipStart = c.startSample;
                const int64_t clipEnd = clipStart + c.lengthSamples;
                if (currentTransportSamples >= clipStart && currentTransportSamples < clipEnd)
                {
                    activeClip = &c;
                    clipRelativeStart = currentTransportSamples - clipStart;
                    break;
                }
            }
        }

        if (activeClip == nullptr)
            return; // nothing to play for this block

        // Ensure readerSource is loaded for this clip
        if (readerSource == nullptr || loadedFile != activeClip->file)
        {
            loadClip(activeClip->file, sessionPtr->getSampleRate());
        }

        // Compute file playback position in seconds
        int64_t filePlaySample = clipRelativeStart + activeClip->fileOffsetSamples + activeClip->alignmentOffsetSamples;
        if (filePlaySample < 0) filePlaySample = 0;
        const double filePosSeconds = static_cast<double>(filePlaySample) / sr;

        transportSource.setPosition(filePosSeconds);

        scratchBuffer.setSize(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples, false, false, true);
        juce::AudioSourceChannelInfo scratchInfo(&scratchBuffer, 0, bufferToFill.numSamples);
        transportSource.getNextAudioBlock(scratchInfo);

        // Apply track volume/pan and clip gain
        const float trackLinearGain = decibelsToGain(volumeDb);
        const float clipLinearGain = decibelsToGain(activeClip->gainDb);
        const float leftGain = pan <= 0.0f ? 1.0f : 1.0f - pan;
        const float rightGain = pan >= 0.0f ? 1.0f : 1.0f + pan;

        for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
        {
            const float channelGain = (channel == 0) ? leftGain : rightGain;
            bufferToFill.buffer->addFrom(channel, bufferToFill.startSample,
                                        scratchBuffer, juce::jmin(channel, scratchBuffer.getNumChannels() - 1),
                                        0, bufferToFill.numSamples,
                                        trackLinearGain * clipLinearGain * channelGain);
        }

        if (muted || (soloModeActive && !solo))
            bufferToFill.clearActiveBufferRegion();

        juce::MidiBuffer emptyMidi;
        for (auto& plugin : pluginChain)
        {
            if (plugin)
                plugin->processBlock(*bufferToFill.buffer, emptyMidi);
        }

        // Update peak meters
        const int numCh = bufferToFill.buffer->getNumChannels();
        if (numCh > 0)
            peakLevelLeft.store(bufferToFill.buffer->getMagnitude(0, bufferToFill.startSample, bufferToFill.numSamples));
        if (numCh > 1)
            peakLevelRight.store(bufferToFill.buffer->getMagnitude(1, bufferToFill.startSample, bufferToFill.numSamples));
    }

    void StudioAudioEngine::TrackPlayer::setTrackMetadata(const Track& trackInfo)
    {
        volumeDb = trackInfo.volumeDb;
        pan = trackInfo.pan;
        muted = trackInfo.muted;
        solo = trackInfo.solo;
        armed = trackInfo.armed;
        transportSource.setGain(1.0f);
    }

    void StudioAudioEngine::TrackPlayer::setPlaying(bool shouldPlay)
    {
        isPlaying = shouldPlay;
        if (isPlaying)
            transportSource.start();
        else
            transportSource.stop();
    }

    void StudioAudioEngine::TrackPlayer::setSoloMode(bool soloActive)
    {
        soloModeActive = soloActive;
    }

    bool StudioAudioEngine::TrackPlayer::loadClip(const juce::File& file, double sampleRate)
    {
        auto* reader = formatManager.createReaderFor(file);
        if (reader == nullptr)
            return false;

        readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
        transportSource.setSource(readerSource.get(), 0, nullptr, sampleRate);
        transportSource.setPosition(0.0);
        loadedFile = file;
        return true;
    }

    

    void StudioAudioEngine::TrackPlayer::setLoopActive(bool looping)
    {
        transportSource.setLooping(looping);
    }

    bool StudioAudioEngine::TrackPlayer::addPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin)
    {
        if (!plugin)
            return false;

        // prepareToPlay will be called again with correct values in audioDeviceAboutToStart
        plugin->prepareToPlay(44100.0, 512);
        pluginChain.add(std::move(plugin));
        return true;
    }

    juce::AudioPluginInstance* StudioAudioEngine::TrackPlayer::getPlugin(int index) const
    {
        if (isPositiveAndBelow(index, pluginChain.size()))
            return pluginChain.getReference(index).get();
        return nullptr;
    }

    void StudioAudioEngine::TrackPlayer::getPluginState(int index, juce::MemoryBlock& dest) const
    {
        if (auto* p = getPlugin(index))
            p->getStateInformation(dest);
    }

    void StudioAudioEngine::TrackPlayer::setPluginState(int index, const void* data, size_t size)
    {
        if (auto* p = getPlugin(index))
            p->setStateInformation(data, (int)size);
    }

    StudioAudioEngine::StudioAudioEngine()
    {
        pluginFormatManager.addDefaultFormats();
        recordingFolder = juce::File::getCurrentWorkingDirectory().getChildFile("NovaStudioProjects").getChildFile("Recordings");
        recordingFolder.createDirectory();
        transportState.setSampleRate(currentSampleRate);
        transportState.setTempo(static_cast<int>(session.getTempo()));
    }

    StudioAudioEngine::~StudioAudioEngine()
    {
        shutdown();
    }

    bool StudioAudioEngine::initialize()
    {
        juce::String result = deviceManager.initialiseWithDefaultDevices(2, 2);
        if (result.isNotEmpty())
            return false;

        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr)
            return false;

        currentSampleRate = device->getCurrentSampleRate();
        currentBufferSize = device->getCurrentBufferSizeSamples();
        transportState.setSampleRate(currentSampleRate);
        transportState.setTempo(static_cast<int>(session.getTempo()));

        deviceManager.addAudioCallback(this);
        return true;
    }

    void StudioAudioEngine::shutdown()
    {
        deviceManager.removeAudioCallback(this);
        mixerSource.releaseResources();
        for (auto& player : trackPlayers)
            player->releaseResources();

        recordingWriter.reset();
    }

    bool StudioAudioEngine::setSampleRate(int newSampleRate, int newBufferSize)
    {
        currentSampleRate = newSampleRate;
        currentBufferSize = newBufferSize;

        transportState.setSampleRate(currentSampleRate);
        mixerSource.releaseResources();
        for (auto& player : trackPlayers)
            player->prepareToPlay(currentBufferSize, currentSampleRate);
        mixerSource.prepareToPlay(currentBufferSize, currentSampleRate);

        session.setSampleRate(currentSampleRate);
        return true;
    }

    bool StudioAudioEngine::loadPluginOnTrack(int trackIndex, const juce::File& pluginFile)
    {
        if (!pluginFile.existsAsFile())
            return false;

        juce::OwnedArray<juce::PluginDescription> typesFound;
        knownPlugins.scanAndAddDragAndDroppedFiles(pluginFormatManager,
                                                  juce::StringArray(pluginFile.getFullPathName()),
                                                  typesFound);
        if (typesFound.isEmpty())
            return false;

        juce::String errorMessage;
        auto instance = pluginFormatManager.createPluginInstance(*typesFound[0], currentSampleRate, currentBufferSize, errorMessage);
        if (!instance)
            return false;

        if (!isPositiveAndBelow(trackIndex, trackPlayers.size()))
            return false;

        trackPlayers.getReference(trackIndex)->addPlugin(std::move(instance));
        return true;
    }

    bool StudioAudioEngine::loadPluginByDescription(const juce::PluginDescription& desc, int trackIndex)
    {
        // If no track specified, use first armed track, then first track
        int targetTrack = trackIndex;
        if (targetTrack < 0)
        {
            targetTrack = 0;
            for (int i = 0; i < trackPlayers.size(); ++i)
                if (trackPlayers.getReference(i)->armed) { targetTrack = i; break; }
        }

        juce::String errorMessage;
        auto instance = pluginFormatManager.createPluginInstance(desc, currentSampleRate, currentBufferSize, errorMessage);
        if (!instance)
            return false;

        if (!isPositiveAndBelow(targetTrack, trackPlayers.size()))
            return false;

        trackPlayers.getReference(targetTrack)->addPlugin(std::move(instance));
        return true;
    }

    bool StudioAudioEngine::loadAudioClip(int trackIndex, const juce::File& audioFile)
    {
        if (!audioFile.existsAsFile())
            return false;

        if (!isPositiveAndBelow(trackIndex, trackPlayers.size()))
            return false;

        if (!trackPlayers.getReference(trackIndex)->loadClip(audioFile, currentSampleRate))
            return false;

        auto& track = session.getTrack(trackIndex);
        Clip clip;
        clip.file = audioFile;
        clip.startSample = 0;
        clip.lengthSamples = 0;
        clip.isMidi = false;

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        if (auto* reader = formatManager.createReaderFor(audioFile))
        {
            clip.lengthSamples = reader->lengthInSamples;
            delete reader;
        }

        track.clips.clear();
        track.clips.add(clip);
        return true;
    }

    void StudioAudioEngine::addTrack(const juce::String& name, TrackType type)
    {
        session.addTrack(name, type);
        buildTrackPlayers();
    }

    void StudioAudioEngine::removeTrack(int index)
    {
        session.removeTrack(index);
        buildTrackPlayers();
    }

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

    void StudioAudioEngine::setTrackArm(int index, bool arm)
    {
        if (!isPositiveAndBelow(index, trackPlayers.size()))
            return;

        session.getTrack(index).armed = arm;
        trackPlayers.getReference(index)->armed = arm;
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

    void StudioAudioEngine::play()
    {
        transportState.play();
        const double transportSeconds = static_cast<double>(transportState.getPositionSamples()) / transportState.getSampleRate();
        for (int i = 0; i < trackPlayers.size(); ++i)
        {
            auto* player = trackPlayers.getReference(i).get();
            player->setLoopActive(transportState.isLooping());
            player->setPlaybackPosition(transportSeconds);
            player->setPlaying(true);
        }
    }

    void StudioAudioEngine::stop()
    {
        transportState.stop();
        for (int i = 0; i < trackPlayers.size(); ++i)
            trackPlayers.getReference(i)->setPlaying(false);

        if (recordingActive)
            stopRecordingInternal();
    }

    void StudioAudioEngine::startRecordingInternal()
    {
        auto timestamp = juce::Time::getCurrentTime().toString(true, true);
        currentRecordingFile = recordingFolder.getChildFile("NovaStudioRecording_" + timestamp + ".wav");
        currentRecordingFile = currentRecordingFile.getNonexistentSibling();

        auto outputStream = std::make_unique<juce::FileOutputStream>(currentRecordingFile);
        if (!outputStream->openedOk())
            return;

        juce::WavAudioFormat wavFormat;
        juce::StringPairArray metadata;
        recordingWriter.reset(wavFormat.createWriterFor(outputStream.release(), currentSampleRate, 2, 32, metadata, 0));
        if (!recordingWriter)
            return;

        recordingStartSample = transportState.getPositionSamples();
        recordingSampleCount = 0;
        recordingActive = true;
    }

    void StudioAudioEngine::stopRecordingInternal()
    {
        recordingActive = false;
        recordingWriter.reset();
        recordingSampleCount = 0;
        createRecordingClipIfNeeded();
    }

    void StudioAudioEngine::createRecordingClipIfNeeded()
    {
        if (!currentRecordingFile.existsAsFile())
            return;

        for (int i = 0; i < session.getNumTracks(); ++i)
        {
            auto& track = session.getTrack(i);
            if (track.type == TrackType::Audio && track.armed)
            {
                Clip clip;
                clip.file = currentRecordingFile;
                clip.startSample = recordingStartSample;
                clip.lengthSamples = recordingSampleCount;
                clip.isMidi = false;
                track.clips.add(clip);
                break;
            }
        }

        buildTrackPlayers();
    }

    void StudioAudioEngine::toggleRecord()
    {
        if (recordingActive)
        {
            transportState.stopRecording();
            for (int i = 0; i < trackPlayers.size(); ++i)
                trackPlayers.getReference(i)->setPlaying(false);
            stopRecordingInternal();
        }
        else
        {
            if (!transportState.isRecordArmed())
                transportState.setRecordArmed(true);

            if (!transportState.isPlaying())
                play();

            transportState.startRecording();
            startRecordingInternal();
        }
    }

    bool StudioAudioEngine::isRecording() const noexcept
    {
        return recordingActive;
    }

    // ── Plugin management ─────────────────────────────────────────────────────

    juce::AudioPluginInstance* StudioAudioEngine::getTrackPlugin(int trackIndex, int pluginSlot) const
    {
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return nullptr;
        return trackPlayers.getReference(trackIndex)->getPlugin(pluginSlot);
    }

    int StudioAudioEngine::getTrackPluginCount(int trackIndex) const
    {
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return 0;
        return trackPlayers.getReference(trackIndex)->getNumPlugins();
    }

    void StudioAudioEngine::getTrackPluginState(int trackIndex, int pluginSlot, juce::MemoryBlock& dest) const
    {
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return;
        trackPlayers.getReference(trackIndex)->getPluginState(pluginSlot, dest);
    }

    void StudioAudioEngine::setTrackPluginState(int trackIndex, int pluginSlot, const void* data, size_t size)
    {
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return;
        trackPlayers.getReference(trackIndex)->setPluginState(pluginSlot, data, size);
    }

    // ── Metering ──────────────────────────────────────────────────────────────

    float StudioAudioEngine::getTrackPeakLevel(int trackIndex, int channel) const noexcept
    {
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return 0.0f;
        auto& player = *trackPlayers.getReference(trackIndex);
        return channel == 0 ? player.peakLevelLeft.load() : player.peakLevelRight.load();
    }

    bool StudioAudioEngine::saveSession(const juce::File& file) const
    {
        if (!session.saveToFile(file))
            return false;

        // Persist plugin chains as a sidecar .plugins.json next to the session file
        auto sidecar = file.getSiblingFile(file.getFileNameWithoutExtension() + ".plugins.json");
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        juce::Array<juce::var> tracksArray;

        for (int t = 0; t < trackPlayers.size(); ++t)
        {
            auto& player = *trackPlayers.getReference(t);
            juce::Array<juce::var> pluginsArray;

            for (int p = 0; p < player.getNumPlugins(); ++p)
            {
                auto* instance = player.getPlugin(p);
                if (!instance) continue;

                juce::MemoryBlock stateData;
                instance->getStateInformation(stateData);

                juce::DynamicObject::Ptr plugObj = new juce::DynamicObject();
                plugObj->setProperty("name",         instance->getName());
                plugObj->setProperty("uid",          instance->getPluginDescription().uniqueId);
                plugObj->setProperty("fileOrId",     instance->getPluginDescription().fileOrIdentifier);
                plugObj->setProperty("formatName",   instance->getPluginDescription().pluginFormatName);
                plugObj->setProperty("state",        juce::Base64::toBase64(stateData.getData(), stateData.getSize()));
                pluginsArray.add(juce::var(plugObj.get()));
            }

            juce::DynamicObject::Ptr trackObj = new juce::DynamicObject();
            trackObj->setProperty("plugins", juce::var(pluginsArray));
            tracksArray.add(juce::var(trackObj.get()));
        }

        root->setProperty("tracks", juce::var(tracksArray));
        sidecar.replaceWithText(juce::JSON::toString(juce::var(root.get())));
        return true;
    }

    bool StudioAudioEngine::loadSession(const juce::File& file)
    {
        if (!session.loadFromFile(file))
            return false;

        transportState.setTempo(static_cast<int>(session.getTempo()));
        buildTrackPlayers();

        // Restore plugin chains from sidecar
        auto sidecar = file.getSiblingFile(file.getFileNameWithoutExtension() + ".plugins.json");
        if (sidecar.existsAsFile())
        {
            auto parsed = juce::JSON::parse(sidecar.loadFileAsString());
            if (parsed.isObject())
            {
                auto* root = parsed.getDynamicObject();
                auto tracksVar = root->getProperty("tracks");
                if (auto* tracksArr = tracksVar.getArray())
                {
                    for (int t = 0; t < tracksArr->size() && t < trackPlayers.size(); ++t)
                    {
                        juce::var trackVar = (*tracksArr)[t];
                        if (!trackVar.isObject()) continue;
                        juce::var pluginsVar = trackVar.getDynamicObject()->getProperty("plugins");
                        if (auto* pluginsArr = pluginsVar.getArray())
                        {
                            for (int p = 0; p < pluginsArr->size(); ++p)
                            {
                                juce::var plugVar = (*pluginsArr)[p];
                                if (!plugVar.isObject()) continue;
                                auto* plugObj = plugVar.getDynamicObject();

                                juce::PluginDescription desc;
                                desc.name                = plugObj->getProperty("name").toString();
                                desc.uniqueId            = (int)plugObj->getProperty("uid");
                                desc.fileOrIdentifier    = plugObj->getProperty("fileOrId").toString();
                                desc.pluginFormatName    = plugObj->getProperty("formatName").toString();

                                juce::String errorMsg;
                                auto instance = pluginFormatManager.createPluginInstance(
                                    desc, currentSampleRate, currentBufferSize, errorMsg);

                                if (instance)
                                {
                                    juce::String stateBase64 = plugObj->getProperty("state").toString();
                                    juce::MemoryOutputStream decoded;
                                    juce::Base64::convertFromBase64(decoded, stateBase64);
                                    juce::MemoryBlock stateBlock(decoded.getData(), decoded.getDataSize());
                                    instance->setStateInformation(stateBlock.getData(), (int)stateBlock.getSize());

                                    trackPlayers.getReference(t)->addPlugin(std::move(instance));
                                }
                            }
                        }
                    }
                }
            }
        }

        return true;
    }

    bool StudioAudioEngine::exportStereoMix(const juce::File& destinationFile)
    {
        if (destinationFile.exists())
            destinationFile.deleteFile();

        auto outputStream = std::make_unique<juce::FileOutputStream>(destinationFile);
        if (!outputStream->openedOk())
            return false;

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outputStream.release(), currentSampleRate, 2, 32, {}, 0));
        if (writer == nullptr)
            return false;

        juce::AudioSampleBuffer renderBuffer(2, currentBufferSize);
        const int totalBlocks = static_cast<int>(currentSampleRate * 10 / currentBufferSize);

        for (int i = 0; i < totalBlocks; ++i)
        {
            renderBuffer.clear();
            juce::AudioSourceChannelInfo info(renderBuffer);
            mixerSource.getNextAudioBlock(info);
            writer->writeFromAudioSampleBuffer(renderBuffer, 0, renderBuffer.getNumSamples());
        }

        return true;
    }

    const TransportState& StudioAudioEngine::getTransportState() const noexcept
    {
        return transportState;
    }

    TransportState& StudioAudioEngine::getTransportState() noexcept
    {
        return transportState;
    }

    double StudioAudioEngine::getCurrentSampleRate() const noexcept
    {
        return currentSampleRate;
    }

    int StudioAudioEngine::getCurrentBufferSize() const noexcept
    {
        return currentBufferSize;
    }

    void StudioAudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                             int numInputChannels,
                                                             float* const* outputChannelData,
                                                             int numOutputChannels,
                                                             int numSamples,
                                                             const juce::AudioIODeviceCallbackContext& context)
    {
        juce::AudioBuffer<float> tempBuffer(numOutputChannels, numSamples);
        juce::AudioSourceChannelInfo info(tempBuffer);

        // Update each track player's playback position (sample-accurate) from transport
        const double transportSeconds = static_cast<double>(transportState.getPositionSamples()) / transportState.getSampleRate();
        for (int i = 0; i < trackPlayers.size(); ++i)
            trackPlayers.getReference(i)->setPlaybackPosition(transportSeconds);

        mixerSource.getNextAudioBlock(info);

        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            const float* sourceData = tempBuffer.getReadPointer(channel);
            float* destData = outputChannelData[channel];
            if (destData != nullptr)
                std::memcpy(destData, sourceData, static_cast<size_t>(sizeof(float) * numSamples));
        }

        // Input monitoring: mix live input into output when any armed track has monitoring on
        if (transportState.isInputMonitoring())
        {
            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                int srcCh = juce::jmin(channel, numInputChannels - 1);
                if (srcCh >= 0 && inputChannelData[srcCh] != nullptr && outputChannelData[channel] != nullptr)
                {
                    for (int s = 0; s < numSamples; ++s)
                        outputChannelData[channel][s] += inputChannelData[srcCh][s];
                }
            }
        }

        if (recordingActive && recordingWriter != nullptr)
        {
            juce::AudioBuffer<float> inputBuffer(numInputChannels, numSamples);
            for (int channel = 0; channel < numInputChannels; ++channel)
            {
                if (inputChannelData[channel] != nullptr)
                    inputBuffer.copyFrom(channel, 0, inputChannelData[channel], numSamples);
            }
            recordingWriter->writeFromAudioSampleBuffer(inputBuffer, 0, numSamples);
            recordingSampleCount += numSamples;
        }
    }

    void StudioAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
    {
        currentSampleRate = device->getCurrentSampleRate();
        currentBufferSize = device->getCurrentBufferSizeSamples();
        mixerSource.prepareToPlay(currentBufferSize, currentSampleRate);
        for (int i = 0; i < trackPlayers.size(); ++i)
            trackPlayers.getReference(i)->prepareToPlay(currentBufferSize, currentSampleRate);
    }

    void StudioAudioEngine::audioDeviceStopped()
    {
        mixerSource.releaseResources();
        for (int i = 0; i < trackPlayers.size(); ++i)
            trackPlayers.getReference(i)->releaseResources();
    }

    void StudioAudioEngine::buildTrackPlayers()
    {
        mixerSource.removeAllInputs();
        trackPlayers.clear();

        for (int index = 0; index < session.getNumTracks(); ++index)
        {
            auto trackPlayer = std::make_unique<TrackPlayer>();
            trackPlayer->setTrackMetadata(session.getTrack(index));
            // give the player a reference to the session and its track index so it can gate clips
            trackPlayer->setSessionTrack(&session, index);

            trackPlayers.add(std::move(trackPlayer));
            mixerSource.addInputSource(trackPlayers.getReference(trackPlayers.size() - 1).get(), false);
        }

        updateSoloStates();
    }

    void StudioAudioEngine::refreshTrackPlaybackStates()
    {
        for (int index = 0; index < trackPlayers.size(); ++index)
            trackPlayers.getReference(index)->setTrackMetadata(session.getTrack(index));

        updateSoloStates();
    }

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

        for (int index = 0; index < trackPlayers.size(); ++index)
            trackPlayers.getReference(index)->setSoloMode(anySolo);
    }
}
