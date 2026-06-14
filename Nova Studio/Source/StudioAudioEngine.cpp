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

        plugin->prepareToPlay(44100.0, 512);
        pluginChain.add(std::move(plugin));
        return true;
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

    bool StudioAudioEngine::saveSession(const juce::File& file) const
    {
        return session.saveToFile(file);
    }

    bool StudioAudioEngine::loadSession(const juce::File& file)
    {
        if (!session.loadFromFile(file))
            return false;

        transportState.setTempo(static_cast<int>(session.getTempo()));
        buildTrackPlayers();
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
