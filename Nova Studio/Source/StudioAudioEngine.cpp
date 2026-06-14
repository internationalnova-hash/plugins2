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

        // Ensure readerSource is loaded for this clip.
        // setSource() internally stops the AudioTransportSource, so we must
        // restart it after a lazy load — otherwise getNextAudioBlock produces silence.
        if (readerSource == nullptr || loadedFile != activeClip->file)
        {
            loadClip(activeClip->file, sessionPtr->getSampleRate());
            if (isPlaying)
                transportSource.start();
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

        // Recalculate EQ biquad coefficients if parameters changed
        if (eqDirty)
        {
            eqDirty = false;
            const double sr = (sessionPtr && sessionPtr->getSampleRate() > 0.0)
                              ? sessionPtr->getSampleRate() : 44100.0;
            for (int b = 0; b < kNumEQBands; ++b)
            {
                auto& band = eqBands[b];
                const double freq   = band.freq.load();
                const double gainDb = band.gainDb.load();
                const double q      = band.q.load();
                const double w0     = 2.0 * juce::MathConstants<double>::pi * freq / sr;
                const double cosw   = std::cos(w0);
                const double sinw   = std::sin(w0);
                const double alpha  = sinw / (2.0 * q);
                double b0, b1, b2, a0, a1, a2;
                if (b == 0) // HPF
                {
                    b0=(1+cosw)/2; b1=-(1+cosw); b2=(1+cosw)/2;
                    a0=1+alpha;    a1=-2*cosw;    a2=1-alpha;
                }
                else if (b == kNumEQBands - 1) // LPF
                {
                    b0=(1-cosw)/2; b1=1-cosw;  b2=(1-cosw)/2;
                    a0=1+alpha;    a1=-2*cosw;  a2=1-alpha;
                }
                else // Peaking EQ
                {
                    const double A = std::pow(10.0, gainDb / 40.0);
                    b0=1+alpha*A; b1=-2*cosw; b2=1-alpha*A;
                    a0=1+alpha/A; a1=-2*cosw; a2=1-alpha/A;
                }
                band.b0=b0/a0; band.b1=b1/a0; band.b2=b2/a0;
                band.a1=a1/a0; band.a2=a2/a0;
            }
        }

        // Apply EQ bands in series (Direct Form II Transposed biquad)
        const int numSamples  = bufferToFill.numSamples;
        const int startSample = bufferToFill.startSample;
        for (int b = 0; b < kNumEQBands; ++b)
        {
            auto& band = eqBands[b];
            if (!band.enabled.load()) continue;
            const int numCh = bufferToFill.buffer->getNumChannels();
            if (numCh > 0)
            {
                float* L = bufferToFill.buffer->getWritePointer(0, startSample);
                for (int i = 0; i < numSamples; ++i)
                {
                    const double in  = L[i];
                    const double out = band.b0 * in + band.z1L;
                    band.z1L = band.b1 * in - band.a1 * out + band.z2L;
                    band.z2L = band.b2 * in - band.a2 * out;
                    L[i] = (float)out;
                }
            }
            if (numCh > 1)
            {
                float* R = bufferToFill.buffer->getWritePointer(1, startSample);
                for (int i = 0; i < numSamples; ++i)
                {
                    const double in  = R[i];
                    const double out = band.b0 * in + band.z1R;
                    band.z1R = band.b1 * in - band.a1 * out + band.z2R;
                    band.z2R = band.b2 * in - band.a2 * out;
                    R[i] = (float)out;
                }
            }
        }

        // Update peak meters
        const int numCh = bufferToFill.buffer->getNumChannels();
        if (numCh > 0)
            peakLevelLeft.store(bufferToFill.buffer->getMagnitude(0, startSample, numSamples));
        if (numCh > 1)
            peakLevelRight.store(bufferToFill.buffer->getMagnitude(1, startSample, numSamples));
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

    bool StudioAudioEngine::TrackPlayer::loadClip(const juce::File& file, double /*deviceSampleRate*/)
    {
        auto* reader = formatManager.createReaderFor(file);
        if (reader == nullptr)
            return false;

        // Capture the file's actual sample rate BEFORE passing ownership to AudioFormatReaderSource.
        // AudioTransportSource needs the source sample rate so it can resample to the device rate.
        const double fileSampleRate = reader->sampleRate;
        readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
        transportSource.setSource(readerSource.get(), 0, nullptr, fileSampleRate);
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

    bool StudioAudioEngine::TrackPlayer::removePlugin(int index)
    {
        if (!isPositiveAndBelow(index, pluginChain.size()))
            return false;
        pluginChain.remove(index);
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

        juce::File novaRoot = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                  .getChildFile("NovaStudio");
        novaRoot.createDirectory();

        // Recordings folder
        juce::File preferred = novaRoot.getChildFile("Recordings");
        if (preferred.createDirectory().wasOk())
            recordingFolder = preferred;
        else
        {
            recordingFolder = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile("NovaStudioProjects")
                                  .getChildFile("Recordings");
            recordingFolder.createDirectory();
        }

        // Sessions folder
        novaRoot.getChildFile("Sessions").createDirectory();

        // Diagnostics log alongside recordings
        diagLogger = std::make_unique<juce::FileLogger>(
            novaRoot.getChildFile("nova_diag.log"),
            "Nova Studio diagnostics", 1024 * 512);
        juce::Logger::setCurrentLogger(diagLogger.get());
        juce::Logger::writeToLog("StudioAudioEngine constructed. recordingFolder=" + recordingFolder.getFullPathName());

        recordingThread.startThread();

        transportState.setSampleRate(currentSampleRate);
        transportState.setTempo(static_cast<int>(session.getTempo()));
    }

    StudioAudioEngine::~StudioAudioEngine()
    {
        shutdown();
        recordingThread.stopThread(2000);
        juce::Logger::setCurrentLogger(nullptr);
    }

    bool StudioAudioEngine::initialize()
    {
        // Try progressively simpler configurations until a device opens.
        // On macOS, requesting 2 inputs while microphone permission is pending/denied
        // causes CoreAudio to reject the session; JUCE returns success but no device.
        struct Config { int ins; int outs; };
        const Config configs[] = { {2, 2}, {1, 2}, {0, 2} };

        juce::AudioIODevice* device = nullptr;
        for (auto& cfg : configs)
        {
            juce::String result = deviceManager.initialiseWithDefaultDevices(cfg.ins, cfg.outs);
            device = deviceManager.getCurrentAudioDevice();
            juce::Logger::writeToLog("initialiseWithDefaultDevices(" + juce::String(cfg.ins)
                + "," + juce::String(cfg.outs) + ")"
                + "  result=\"" + result + "\""
                + "  device=" + (device ? device->getName() : "null"));
            if (device != nullptr)
                break;
        }

        if (device == nullptr)
        {
            juce::Logger::writeToLog("FATAL: No audio device could be opened. "
                "Check System Preferences -> Sound and microphone permissions.");
            return false;
        }

        currentSampleRate = device->getCurrentSampleRate();
        currentBufferSize = device->getCurrentBufferSizeSamples();
        transportState.setSampleRate(currentSampleRate);
        transportState.setTempo(static_cast<int>(session.getTempo()));

        const int activeIns  = device->getActiveInputChannels().countNumberOfSetBits();
        const int activeOuts = device->getActiveOutputChannels().countNumberOfSetBits();
        juce::Logger::writeToLog("Device open: " + device->getName()
            + "  SR=" + juce::String(currentSampleRate)
            + "  buf=" + juce::String(currentBufferSize)
            + "  activeIn=" + juce::String(activeIns)
            + "  activeOut=" + juce::String(activeOuts));

        cachedActiveInputChannels = activeIns;

        if (activeIns == 0)
            juce::Logger::writeToLog("WARNING: Device opened with 0 input channels. "
                "Recording will capture silence. "
                "Grant microphone permission: System Preferences -> Security & Privacy -> Microphone.");

        deviceManager.addAudioCallback(this);
        return true;
    }

    void StudioAudioEngine::shutdown()
    {
        deviceManager.removeAudioCallback(this);
        recordingActive.store(false);
        recordingWriter.reset();
        mixerSource.releaseResources();
        for (auto& player : trackPlayers)
            player->releaseResources();
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
        const int newIndex = session.getNumTracks(); // capture before add
        session.addTrack(name, type);

        // Add a single new player without destroying running players.
        // Full rebuild (buildTrackPlayers) tears down all sources mid-stream
        // which races with the audio callback even under playerLock.
        auto newPlayer = std::make_unique<TrackPlayer>();
        newPlayer->setTrackMetadata(session.getTrack(newIndex));
        newPlayer->setSessionTrack(&session, newIndex);
        if (currentSampleRate > 0 && currentBufferSize > 0)
            newPlayer->prepareToPlay(currentBufferSize, currentSampleRate);

        TrackPlayer* rawPtr = newPlayer.get();
        {
            juce::ScopedLock sl(playerLock);
            trackPlayers.add(std::move(newPlayer));
        }
        mixerSource.addInputSource(rawPtr, false);
        updateSoloStates();
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
        juce::ScopedLock sl(playerLock);
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
        {
            juce::ScopedLock sl(playerLock);
            for (int i = 0; i < trackPlayers.size(); ++i)
                trackPlayers.getReference(i)->setPlaying(false);
        }

        if (recordingActive)
            stopRecordingInternal();
    }

    void StudioAudioEngine::startRecordingInternal()
    {
        // Determine how many input channels the device actually opened
        int deviceInputs = 0;
        if (auto* device = deviceManager.getCurrentAudioDevice())
            deviceInputs = device->getActiveInputChannels().countNumberOfSetBits();

        juce::Logger::writeToLog("startRecordingInternal: deviceInputs=" + juce::String(deviceInputs)
            + "  SR=" + juce::String(currentSampleRate));

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            auto inputNames = device->getInputChannelNames();
            auto activeInputs = device->getActiveInputChannels();
            for (int i = 0; i < inputNames.size(); ++i)
                juce::Logger::writeToLog("  input ch" + juce::String(i)
                    + " active=" + juce::String((int)activeInputs[i])
                    + " name=\"" + inputNames[i] + "\"");
        }

        if (deviceInputs == 0)
        {
            juce::Logger::writeToLog("WARNING: No active input channels. "
                "Check that your audio interface is set as the system default INPUT device "
                "in macOS System Preferences -> Sound -> Input. Recording may capture silence.");
        }

        // Use however many inputs the device has, minimum 1, maximum 2
        recordingWriterChannels = juce::jmax(1, juce::jmin(deviceInputs, 2));

        juce::String timestamp = juce::Time::getCurrentTime()
            .toString(true, true, false, true)
            .replaceCharacters(" :", "__")
            .replaceCharacters("/,", "--");
        currentRecordingFile = recordingFolder.getChildFile("NovaStudioRecording_" + timestamp + ".wav");
        currentRecordingFile = currentRecordingFile.getNonexistentSibling();

        juce::Logger::writeToLog("Recording to: " + currentRecordingFile.getFullPathName()
            + "  channels=" + juce::String(recordingWriterChannels));

        auto outputStream = std::make_unique<juce::FileOutputStream>(currentRecordingFile);
        if (!outputStream->openedOk())
        {
            juce::Logger::writeToLog("ERROR: Could not open file for writing: " + currentRecordingFile.getFullPathName());
            return;
        }

        juce::WavAudioFormat wavFormat;
        juce::StringPairArray metadata;
        auto* rawWriter = wavFormat.createWriterFor(
            outputStream.release(),
            currentSampleRate,
            (unsigned int)recordingWriterChannels,
            32, metadata, 0);

        if (rawWriter == nullptr)
        {
            juce::Logger::writeToLog("ERROR: createWriterFor returned null");
            return;
        }

        recordingWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(rawWriter, recordingThread, 32768);

        recordingStartSample = transportState.getPositionSamples();
        recordingSampleCount = 0;
        diagBlocksReceived.store(0);
        recordingActive.store(true);

        juce::Logger::writeToLog("Recording STARTED. startSample=" + juce::String(recordingStartSample));
    }

    void StudioAudioEngine::stopRecordingInternal()
    {
        recordingActive.store(false);
        const int64_t blocks = diagBlocksReceived.load();
        const int64_t samples = recordingSampleCount;
        juce::Logger::writeToLog("Recording STOPPED. blocks=" + juce::String(blocks)
            + "  samples=" + juce::String(samples)
            + "  file=" + currentRecordingFile.getFullPathName());
        recordingWriter.reset();   // flushes the ThreadedWriter FIFO to disk
        if (currentRecordingFile.existsAsFile())
            juce::Logger::writeToLog("File size after flush: " + juce::String(currentRecordingFile.getSize()) + " bytes");
        // createRecordingClipIfNeeded() reads recordingSampleCount — do NOT zero it before calling
        createRecordingClipIfNeeded();
        recordingSampleCount = 0;
    }

    void StudioAudioEngine::createRecordingClipIfNeeded()
    {
        if (!currentRecordingFile.existsAsFile())
            return;

        juce::Logger::writeToLog("createRecordingClip: numTracks=" + juce::String(session.getNumTracks())
            + "  file=" + currentRecordingFile.getFileName());

        bool clipAdded = false;
        for (int i = 0; i < session.getNumTracks(); ++i)
        {
            auto& track = session.getTrack(i);
            juce::Logger::writeToLog("  track[" + juce::String(i) + "] name=" + track.name
                + " type=" + juce::String((int)track.type)
                + " armed=" + juce::String((int)track.armed));
            if (track.type == TrackType::Audio && track.armed)
            {
                Clip clip;
                clip.file = currentRecordingFile;
                clip.startSample = recordingStartSample;
                clip.lengthSamples = recordingSampleCount;
                clip.isMidi = false;
                track.clips.add(clip);
                juce::Logger::writeToLog("  -> clip added at startSample=" + juce::String(recordingStartSample)
                    + " length=" + juce::String(recordingSampleCount));
                clipAdded = true;
                break;
            }
        }

        if (!clipAdded)
            juce::Logger::writeToLog("  -> WARNING: no armed audio track found, clip not added!");

        // Refresh player metadata only — do NOT rebuild players while the audio
        // device is still running. Players read clips from the session on the fly,
        // so they will pick up the new clip automatically at playback time.
        refreshTrackPlaybackStates();
    }

    void StudioAudioEngine::toggleRecord()
    {
        if (recordingActive)
        {
            transportState.stopRecording();
            {
                juce::ScopedLock sl(playerLock);
                for (int i = 0; i < trackPlayers.size(); ++i)
                    trackPlayers.getReference(i)->setPlaying(false);
            }
            stopRecordingInternal();
        }
        else
        {
            if (!transportState.isRecordArmed())
                transportState.setRecordArmed(true);

            // Ensure at least one audio track is armed in the session.
            // The transport arm state and session track arm are independent —
            // createRecordingClipIfNeeded() needs session.track.armed == true.
            bool anyArmed = false;
            for (int i = 0; i < session.getNumTracks(); ++i)
                if (session.getTrack(i).armed) { anyArmed = true; break; }

            if (!anyArmed)
            {
                for (int i = 0; i < session.getNumTracks(); ++i)
                {
                    if (session.getTrack(i).type == TrackType::Audio)
                    {
                        session.getTrack(i).armed = true;
                        if (isPositiveAndBelow(i, trackPlayers.size()))
                            trackPlayers.getReference(i)->armed = true;
                        juce::Logger::writeToLog("toggleRecord: auto-armed track " + juce::String(i)
                            + " (" + session.getTrack(i).name + ")");
                        break;
                    }
                }
            }

            // Start playback from wherever the playhead currently sits.
            // Do NOT reset to 0 — user sets the cursor to choose the record-in point.
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

    bool StudioAudioEngine::removePluginFromTrack(int trackIndex, int pluginSlot)
    {
        juce::ScopedLock sl(playerLock);
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return false;
        return trackPlayers.getReference(trackIndex)->removePlugin(pluginSlot);
    }

    // ── Metering ──────────────────────────────────────────────────────────────

    float StudioAudioEngine::getTrackPeakLevel(int trackIndex, int channel) const noexcept
    {
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return 0.0f;
        auto& player = *trackPlayers.getReference(trackIndex);
        return channel == 0 ? player.peakLevelLeft.load() : player.peakLevelRight.load();
    }

    void StudioAudioEngine::setTrackEQBand(int trackIndex, int band, bool enabled,
                                            float freq, float gainDb, float q)
    {
        if (!isPositiveAndBelow(trackIndex, trackPlayers.size())) return;
        if (band < 0 || band >= TrackPlayer::kNumEQBands) return;
        auto& player = *trackPlayers.getReference(trackIndex);
        auto& b = player.eqBands[band];
        b.enabled.store(enabled);
        b.freq.store(freq);
        b.gainDb.store(gainDb);
        b.q.store(q);
        player.eqDirty = true;
    }

    void StudioAudioEngine::setTrackSendLevel(int trackIndex, int sendIndex, float db)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        if (!isPositiveAndBelow(sendIndex, 6)) return;
        session.getTrack(trackIndex).sendLevels[sendIndex] = db;
    }

    float StudioAudioEngine::getTrackSendLevel(int trackIndex, int sendIndex) const noexcept
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return 0.0f;
        if (!isPositiveAndBelow(sendIndex, 6)) return 0.0f;
        return session.getTrack(trackIndex).sendLevels[sendIndex];
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

        // Update each track player's playback position (sample-accurate) from transport.
        // Hold playerLock so buildTrackPlayers() cannot clear the array mid-iteration.
        const double transportSeconds = static_cast<double>(transportState.getPositionSamples()) / transportState.getSampleRate();
        {
            juce::ScopedLock sl(playerLock);
            for (int i = 0; i < trackPlayers.size(); ++i)
                trackPlayers.getReference(i)->setPlaybackPosition(transportSeconds);
        }

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

        if (recordingActive.load() && recordingWriter != nullptr)
        {
            const int64_t blockNum = diagBlocksReceived.fetch_add(1);
            // Log signal levels for first 10 blocks to diagnose silent input
            if (blockNum < 10)
            {
                juce::String msg = "RecBlock#" + juce::String(blockNum)
                    + " numIn=" + juce::String(numInputChannels)
                    + " writerCh=" + juce::String(recordingWriterChannels) + " peaks:";
                for (int ch = 0; ch < numInputChannels; ++ch)
                {
                    float peak = 0.0f;
                    if (inputChannelData[ch] != nullptr)
                        for (int s = 0; s < numSamples; ++s)
                            peak = juce::jmax(peak, std::abs(inputChannelData[ch][s]));
                    msg += " ch" + juce::String(ch) + "=" + juce::String(peak, 6);
                }
                juce::Logger::writeToLog(msg);
            }

            const int writerCh = recordingWriterChannels;
            const int availIn   = numInputChannels;

            if (availIn <= 0)
            {
                // No live input at all — write silence so the clip has correct length
                juce::AudioBuffer<float> silence(writerCh, numSamples);
                silence.clear();
                recordingWriter->write(silence.getArrayOfReadPointers(), numSamples);
            }
            else
            {
                // Build a buffer with exactly writerCh channels from available inputs
                juce::AudioBuffer<float> inputBuffer(writerCh, numSamples);
                for (int ch = 0; ch < writerCh; ++ch)
                {
                    const int srcCh = juce::jmin(ch, availIn - 1);
                    if (inputChannelData[srcCh] != nullptr)
                        inputBuffer.copyFrom(ch, 0, inputChannelData[srcCh], numSamples);
                    else
                        inputBuffer.clear(ch, 0, numSamples);
                }
                recordingWriter->write(inputBuffer.getArrayOfReadPointers(), numSamples);
            }

            recordingSampleCount += numSamples;
        }
    }

    void StudioAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
    {
        currentSampleRate = device->getCurrentSampleRate();
        currentBufferSize = device->getCurrentBufferSizeSamples();
        session.setSampleRate(currentSampleRate);
        transportState.setSampleRate(currentSampleRate);
        cachedActiveInputChannels = device->getActiveInputChannels().countNumberOfSetBits();

        juce::Logger::writeToLog("audioDeviceAboutToStart: " + device->getName()
            + "  SR=" + juce::String(currentSampleRate)
            + "  buf=" + juce::String(currentBufferSize)
            + "  activeIn=" + juce::String(cachedActiveInputChannels)
            + "  activeOut=" + juce::String(device->getActiveOutputChannels().countNumberOfSetBits()));

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
        // Build new players outside the lock so prepareToPlay (which can be slow)
        // doesn't block the audio thread.
        juce::Array<std::unique_ptr<TrackPlayer>> newPlayers;
        for (int index = 0; index < session.getNumTracks(); ++index)
        {
            auto trackPlayer = std::make_unique<TrackPlayer>();
            trackPlayer->setTrackMetadata(session.getTrack(index));
            trackPlayer->setSessionTrack(&session, index);
            if (currentSampleRate > 0 && currentBufferSize > 0)
                trackPlayer->prepareToPlay(currentBufferSize, currentSampleRate);
            newPlayers.add(std::move(trackPlayer));
        }

        // Swap atomically: remove old inputs, install new ones, swap the array.
        // Hold playerLock for the entire replace so the audio callback cannot call
        // setPlaybackPosition on players that are being destroyed.
        // MixerAudioSource's own lock serialises getNextAudioBlock() against
        // removeAllInputs/addInputSource, preventing use-after-free via that path.
        {
            juce::ScopedLock sl(playerLock);
            mixerSource.removeAllInputs();
            trackPlayers = std::move(newPlayers);
            for (int i = 0; i < trackPlayers.size(); ++i)
                mixerSource.addInputSource(trackPlayers.getReference(i).get(), false);
        }

        updateSoloStates();
    }

    void StudioAudioEngine::refreshTrackPlaybackStates()
    {
        {
            juce::ScopedLock sl(playerLock);
            const int n = juce::jmin(trackPlayers.size(), session.getNumTracks());
            for (int index = 0; index < n; ++index)
                trackPlayers.getReference(index)->setTrackMetadata(session.getTrack(index));
        }
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

        juce::ScopedLock sl(playerLock);
        for (int index = 0; index < trackPlayers.size(); ++index)
            trackPlayers.getReference(index)->setSoloMode(anySolo);
    }
}
