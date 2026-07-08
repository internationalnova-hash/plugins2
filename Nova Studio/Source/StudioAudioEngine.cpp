#include "StudioAudioEngine.h"

namespace NovaStudio
{
    static float decibelsToGain(float db)
    {
        return juce::Decibels::decibelsToGain(db);
    }

    // Linear fade-in/fade-out envelope: 1.0 in the steady region, ramping from
    // 0 at the clip edges over fadeInSamples/fadeOutSamples.
    static float clipFadeGainAt(int64_t relativeSample, int64_t lengthSamples,
                                int64_t fadeInSamples, int64_t fadeOutSamples)
    {
        float gain = 1.0f;
        if (fadeInSamples > 0)
            gain = juce::jmin(gain, juce::jlimit(0.0f, 1.0f,
                       (float) relativeSample / (float) fadeInSamples));
        if (fadeOutSamples > 0)
            gain = juce::jmin(gain, juce::jlimit(0.0f, 1.0f,
                       (float) (lengthSamples - relativeSample) / (float) fadeOutSamples));
        return gain;
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

        juce::dsp::ProcessSpec pdcSpec { sampleRate,
                                         (juce::uint32) samplesPerBlockExpected,
                                         (juce::uint32) juce::jmax(1, trackChannels) };
        pdcDelayLine.prepare(pdcSpec);
        pdcDelayLine.setDelay((float) pdcDelaySamples);
    }

    void StudioAudioEngine::TrackPlayer::releaseResources()
    {
        transportSource.releaseResources();
        scratchBuffer.setSize(trackChannels, 0);
        readerSource.reset();
        pdcDelayLine.reset();
    }

    void StudioAudioEngine::TrackPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
    {
        bufferToFill.clearActiveBufferRegion();

        if (!sessionPtr)
            return;

        if (!isPlaying)
            return;

        // ── Aux track: read from send bus, process through plugin chain ──────
        if (isAuxTrack)
        {
            if (muted || (soloModeActive && !solo)) return;
            if (auxInputBusIndex < 0 || auxInputBusIndex >= 8) return;
            juce::AudioBuffer<float>* busBuf = sendBusBuffers[auxInputBusIndex];
            if (busBuf == nullptr) return;

            const int numSamples  = bufferToFill.numSamples;
            const int startSample = bufferToFill.startSample;
            const float trackGain = juce::Decibels::decibelsToGain(volumeDb);
            const float leftGain  = pan <= 0.0f ? 1.0f : 1.0f - pan;
            const float rightGain = pan >= 0.0f ? 1.0f : 1.0f + pan;

            for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
            {
                const int srcCh = juce::jmin(ch, busBuf->getNumChannels() - 1);
                const float chanGain = (ch == 0 ? leftGain : rightGain) * trackGain;
                bufferToFill.buffer->addFrom(ch, startSample, *busBuf, srcCh, 0, numSamples, chanGain);
            }

            // Plugin chain (e.g. reverb, delay on the aux return)
            juce::MidiBuffer emptyMidi;
            for (int pi = 0; pi < pluginChain.size(); ++pi)
                if (auto& plugin = pluginChain.getReference(pi))
                    if (!isPluginBypassed(pi))
                        plugin->processBlock(*bufferToFill.buffer, emptyMidi);

            if (pdcDelaySamples > 0)
            {
                auto block = juce::dsp::AudioBlock<float>(*bufferToFill.buffer)
                                 .getSubBlock((size_t) bufferToFill.startSample, (size_t) bufferToFill.numSamples);
                juce::dsp::ProcessContextReplacing<float> pdcContext(block);
                pdcDelayLine.process(pdcContext);
            }

            // Meter: aux return level from the send bus buffer directly
            if (busBuf->getNumChannels() > 0)
                peakLevelLeft .store(busBuf->getMagnitude(0, 0, numSamples) * trackGain * leftGain);
            if (busBuf->getNumChannels() > 1)
                peakLevelRight.store(busBuf->getMagnitude(1, 0, numSamples) * trackGain * rightGain);
            else if (busBuf->getNumChannels() > 0)
                peakLevelRight.store(busBuf->getMagnitude(0, 0, numSamples) * trackGain * rightGain);
            return;
        }

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

        // Compute target file position for this block
        int64_t filePlaySample = clipRelativeStart + activeClip->fileOffsetSamples + activeClip->alignmentOffsetSamples;
        if (filePlaySample < 0) filePlaySample = 0;

        // NEVER call loadClip() from the audio callback — AudioTransportSource uses
        // a non-reentrant SpinLock internally, and setSource() would deadlock if called
        // while getNextAudioBlock() already holds that lock. Clips must be pre-loaded
        // on the main thread (via preloadClip) before playback starts.
        if (readerSource == nullptr || loadedFile != activeClip->file)
            return; // clip not yet loaded — produce silence until main thread loads it

        // Only seek when the clip first loads, or when the user has jumped to a
        // new position (difference > 2 blocks). During normal playback the
        // AudioTransportSource advances naturally — calling setPosition() every
        // block forces a disk seek each callback, which is why playback lagged.
        const int64_t seekThreshold = (int64_t)bufferToFill.numSamples * 2;
        const bool needsSeek = (lastSeekSample < 0)
                             || (std::abs(filePlaySample - lastSeekSample) > seekThreshold);
        if (needsSeek)
        {
            const double filePosSeconds = static_cast<double>(filePlaySample) / sr;
            transportSource.setPosition(filePosSeconds);
            lastSeekSample = filePlaySample;
        }
        else
        {
            lastSeekSample += bufferToFill.numSamples;
        }

        scratchBuffer.setSize(bufferToFill.buffer->getNumChannels(), bufferToFill.numSamples, false, false, true);
        juce::AudioSourceChannelInfo scratchInfo(&scratchBuffer, 0, bufferToFill.numSamples);
        transportSource.getNextAudioBlock(scratchInfo);

        // Apply non-destructive clip fade in/out: ramp the block's gain across
        // the clip's fade regions so edits stay sample-accurate without baking
        // the fade into the source file.
        if (activeClip->fadeInSamples > 0 || activeClip->fadeOutSamples > 0)
        {
            const float startGain = clipFadeGainAt(clipRelativeStart, activeClip->lengthSamples,
                                                    activeClip->fadeInSamples, activeClip->fadeOutSamples);
            const float endGain   = clipFadeGainAt(clipRelativeStart + bufferToFill.numSamples - 1,
                                                    activeClip->lengthSamples,
                                                    activeClip->fadeInSamples, activeClip->fadeOutSamples);
            for (int channel = 0; channel < scratchBuffer.getNumChannels(); ++channel)
                scratchBuffer.applyGainRamp(channel, 0, bufferToFill.numSamples, startGain, endGain);
        }

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

        // Meter: measure from scratchBuffer (this track's audio only, pre-mix) with final gain applied.
        // Using scratchBuffer guarantees we read only this track, regardless of MixerAudioSource ordering.
        if (!muted && !(soloModeActive && !solo))
        {
            const float combGain = trackLinearGain * clipLinearGain;
            if (scratchBuffer.getNumChannels() > 0)
                peakLevelLeft .store(scratchBuffer.getMagnitude(0, 0, bufferToFill.numSamples) * combGain * leftGain);
            if (scratchBuffer.getNumChannels() > 1)
                peakLevelRight.store(scratchBuffer.getMagnitude(1, 0, bufferToFill.numSamples) * combGain * rightGain);
            else if (scratchBuffer.getNumChannels() > 0)
                peakLevelRight.store(scratchBuffer.getMagnitude(0, 0, bufferToFill.numSamples) * combGain * rightGain);
        }
        else
        {
            peakLevelLeft .store(0.0f);
            peakLevelRight.store(0.0f);
        }

        if (muted || (soloModeActive && !solo))
            bufferToFill.clearActiveBufferRegion();

        juce::MidiBuffer emptyMidi;
        for (int pi = 0; pi < pluginChain.size(); ++pi)
        {
            auto& plugin = pluginChain.getReference(pi);
            if (plugin && !isPluginBypassed(pi))
                plugin->processBlock(*bufferToFill.buffer, emptyMidi);
        }

        // Plugin delay compensation — pad this track's output so all tracks'
        // signals reach the mix bus in sync regardless of plugin latency.
        if (pdcDelaySamples > 0)
        {
            auto block = juce::dsp::AudioBlock<float>(*bufferToFill.buffer)
                             .getSubBlock((size_t) bufferToFill.startSample, (size_t) bufferToFill.numSamples);
            juce::dsp::ProcessContextReplacing<float> pdcContext(block);
            pdcDelayLine.process(pdcContext);
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

        // Peak meters are updated from scratchBuffer above (before EQ/routing).
        // EQ changes level slightly but the scratchBuffer measurement is accurate enough.

        // ── Send bus writes ──────────────────────────────────────────────────
        // Pre-fader sends read from scratchBuffer (before gain); post-fader from bufferToFill.
        if (!isAuxTrack)
        {
            for (int si = 0; si < kMaxSends; ++si)
            {
                const int busIdx = sendBusIndex[si];
                if (busIdx < 0 || busIdx >= 8) continue;
                juce::AudioBuffer<float>* busBuf = sendBusBuffers[busIdx];
                if (busBuf == nullptr) continue;

                const float sendLin = juce::Decibels::decibelsToGain(sendLevels[si]);
                if (sendPreFader[si])
                {
                    // Pre-fader: source is scratchBuffer (dry, before track fader)
                    for (int ch = 0; ch < busBuf->getNumChannels(); ++ch)
                    {
                        const int srcCh = juce::jmin(ch, scratchBuffer.getNumChannels() - 1);
                        busBuf->addFrom(ch, 0, scratchBuffer, srcCh, 0, numSamples, sendLin);
                    }
                }
                else
                {
                    // Post-fader: source is bufferToFill (after volume/pan/EQ)
                    for (int ch = 0; ch < busBuf->getNumChannels(); ++ch)
                    {
                        const int srcCh = juce::jmin(ch, bufferToFill.buffer->getNumChannels() - 1);
                        busBuf->addFrom(ch, 0, *bufferToFill.buffer, srcCh, startSample, numSamples, sendLin);
                    }
                }
            }
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
        isPlaying.store(shouldPlay);
        if (shouldPlay)
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
        // Use a 2-second read-ahead buffer so disk I/O happens on a background
        // thread instead of the audio callback. Without this, every block stalls
        // waiting for a synchronous disk read — the main cause of playback lag.
        const int readAhead = readAheadThread ? (int)(fileSampleRate * 2.0) : 0;
        transportSource.setSource(readerSource.get(), readAhead, readAheadThread, fileSampleRate);
        transportSource.setPosition(0.0);
        loadedFile = file;
        return true;
    }

    

    void StudioAudioEngine::TrackPlayer::setLoopActive(bool looping)
    {
        transportSource.setLooping(looping);
    }

    void StudioAudioEngine::TrackPlayer::preloadClipAtPosition(double transportSeconds)
    {
        if (sessionPtr == nullptr || trackIndex < 0) return;
        const double sr = sessionPtr->getSampleRate();
        if (sr <= 0.0) return;

        const int64_t currentTransportSamples = static_cast<int64_t>(transportSeconds * sr);
        const Track& track = sessionPtr->getTrack(trackIndex);

        for (int i = 0; i < track.clips.size(); ++i)
        {
            const Clip& c = track.clips.getReference(i);
            if (c.isMidi) continue;
            const int64_t clipEnd = c.startSample + c.lengthSamples;
            if (currentTransportSamples < clipEnd && c.startSample <= currentTransportSamples + (int64_t)(sr * 5.0))
            {
                if (loadedFile != c.file)
                {
                    loadClip(c.file, sr);
                    lastSeekSample = -1;
                }
                // Seek to correct position within the file
                const int64_t clipRelative = currentTransportSamples - c.startSample;
                const int64_t fileSample = juce::jmax((int64_t)0,
                    clipRelative + c.fileOffsetSamples + c.alignmentOffsetSamples);
                transportSource.setPosition(static_cast<double>(fileSample) / sr);
                lastSeekSample = fileSample;
                return;
            }
        }
    }

    bool StudioAudioEngine::TrackPlayer::addPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin)
    {
        if (!plugin)
            return false;

        // prepareToPlay will be called again with correct values in audioDeviceAboutToStart
        plugin->prepareToPlay(44100.0, 512);
        pluginChain.add(std::move(plugin));
        pluginBypassed.add(false);
        return true;
    }

    bool StudioAudioEngine::TrackPlayer::removePlugin(int index)
    {
        if (!isPositiveAndBelow(index, pluginChain.size()))
            return false;
        pluginChain.remove(index);
        pluginBypassed.remove(index);
        return true;
    }

    bool StudioAudioEngine::TrackPlayer::movePlugin(int fromIndex, int toIndex)
    {
        if (!isPositiveAndBelow(fromIndex, pluginChain.size()) || !isPositiveAndBelow(toIndex, pluginChain.size()))
            return false;
        pluginChain.move(fromIndex, toIndex);
        pluginBypassed.move(fromIndex, toIndex);
        return true;
    }

    void StudioAudioEngine::TrackPlayer::setPluginBypassed(int index, bool bypassed)
    {
        if (isPositiveAndBelow(index, pluginBypassed.size()))
            pluginBypassed.getReference(index) = bypassed;
    }

    bool StudioAudioEngine::TrackPlayer::isPluginBypassed(int index) const
    {
        if (isPositiveAndBelow(index, pluginBypassed.size()))
            return pluginBypassed.getReference(index);
        return false;
    }

    int StudioAudioEngine::TrackPlayer::getPluginChainLatencySamples() const
    {
        int total = 0;
        for (int i = 0; i < pluginChain.size(); ++i)
        {
            if (i < pluginBypassed.size() && pluginBypassed.getReference(i))
                continue;
            if (auto& plugin = pluginChain.getReference(i))
                total += plugin->getLatencySamples();
        }
        return total;
    }

    void StudioAudioEngine::TrackPlayer::setPdcDelaySamples(int samples)
    {
        samples = juce::jmax(0, samples);
        if (samples == pdcDelaySamples)
            return;
        pdcDelaySamples = samples;
        pdcDelayLine.setDelay((float) pdcDelaySamples);
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
        previewFormatManager.registerBasicFormats();

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
        readAheadThread.startThread(juce::Thread::Priority::normal);

        transportState.setSampleRate(currentSampleRate);
        transportState.setTempo(static_cast<int>(session.getTempo()));
    }

    StudioAudioEngine::~StudioAudioEngine()
    {
        shutdown();
        readAheadThread.stopThread(2000);
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

        // Enable all currently-available MIDI inputs for MIDI Learn / controller mapping
        for (auto& mi : juce::MidiInput::getAvailableDevices())
        {
            if (!deviceManager.isMidiInputDeviceEnabled(mi.identifier))
                deviceManager.setMidiInputDeviceEnabled(mi.identifier, true);
            deviceManager.addMidiInputDeviceCallback(mi.identifier, this);
        }

        return true;
    }

    void StudioAudioEngine::shutdown()
    {
        deviceManager.removeAudioCallback(this);
        for (auto& mi : juce::MidiInput::getAvailableDevices())
            deviceManager.removeMidiInputDeviceCallback(mi.identifier, this);
        recordingActive.store(false);
        recordingWriter.reset();
        previewActive.store(false);
        previewTransport.stop();
        previewTransport.setSource(nullptr);
        previewReaderSource.reset();
        mixerSource.releaseResources();
        for (auto& player : trackPlayers)
            player->releaseResources();
    }

    bool StudioAudioEngine::setSampleRate(int newSampleRate, int newBufferSize)
    {
        // Apply the change to the hardware device first so audioDeviceAboutToStart
        // fires and updates all engine state consistently.
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);
        setup.sampleRate = static_cast<double>(newSampleRate);
        setup.bufferSize = newBufferSize;

        juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
        if (err.isEmpty())
        {
            // audioDeviceAboutToStart already fired and updated everything — don't double-prepare.
            juce::Logger::writeToLog("setSampleRate: device now at "
                + juce::String(currentSampleRate) + " Hz buf=" + juce::String(currentBufferSize));
        }
        else
        {
            // Hardware rejected — apply software-only update
            juce::Logger::writeToLog("setSampleRate: device rejected " + juce::String(newSampleRate)
                                     + " Hz: " + err);
            currentSampleRate = newSampleRate;
            currentBufferSize = newBufferSize;
            transportState.setSampleRate(currentSampleRate);
            mixerSource.releaseResources();
            for (auto& player : trackPlayers)
                player->prepareToPlay(currentBufferSize, currentSampleRate);
            mixerSource.prepareToPlay(currentBufferSize, currentSampleRate);
            {
                juce::ScopedLock sl(masterPluginLock);
                for (auto* p : masterPlugins)
                    if (p) p->prepareToPlay(currentSampleRate, currentBufferSize);
            }
            session.setSampleRate(currentSampleRate);
        }
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

        if (trackIndex == kMasterTrackIndex)
            return addMasterPlugin(std::move(instance));

        if (!isPositiveAndBelow(trackIndex, trackPlayers.size()))
            return false;

        trackPlayers.getReference(trackIndex)->addPlugin(std::move(instance));
        updatePluginDelayCompensation();
        return true;
    }

    bool StudioAudioEngine::loadPluginByDescription(const juce::PluginDescription& desc, int trackIndex)
    {
        juce::String errorMessage;
        auto instance = pluginFormatManager.createPluginInstance(desc, currentSampleRate, currentBufferSize, errorMessage);
        if (!instance) return false;

        // Master channel
        if (trackIndex == kMasterTrackIndex)
            return addMasterPlugin(std::move(instance));

        // If no track specified, use first armed track, then first track
        int targetTrack = trackIndex;
        if (targetTrack < 0)
        {
            targetTrack = 0;
            for (int i = 0; i < trackPlayers.size(); ++i)
                if (trackPlayers.getReference(i)->armed) { targetTrack = i; break; }
        }

        if (!isPositiveAndBelow(targetTrack, trackPlayers.size()))
            return false;

        trackPlayers.getReference(targetTrack)->addPlugin(std::move(instance));
        updatePluginDelayCompensation();
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
        auto& newTrack = session.addTrack(name, type);

        static const juce::Colour kTrackPalette[] = {
            juce::Colour::fromRGB(120, 80, 200), juce::Colour::fromRGB(80, 100, 200),
            juce::Colour::fromRGB(50, 160, 160), juce::Colour::fromRGB(60, 120, 200),
            juce::Colour::fromRGB(40, 180, 160), juce::Colour::fromRGB(60, 180, 80),
            juce::Colour::fromRGB(160, 190, 50), juce::Colour::fromRGB(190, 110, 40),
        };
        newTrack.colour = kTrackPalette[newIndex % (int)juce::numElementsInArray(kTrackPalette)];

        // Add a single new player without destroying running players.
        // Full rebuild (buildTrackPlayers) tears down all sources mid-stream
        // which races with the audio callback even under playerLock.
        auto newPlayer = std::make_unique<TrackPlayer>();
        newPlayer->readAheadThread = &readAheadThread;
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

    void StudioAudioEngine::play()
    {
        const double transportSeconds = static_cast<double>(transportState.getPositionSamples()) / transportState.getSampleRate();

        // Pre-load clips on the main thread BEFORE starting the audio callback.
        // AudioTransportSource uses a non-reentrant SpinLock — calling setSource()
        // from the audio callback (while getNextAudioBlock holds that lock) deadlocks.
        {
            juce::ScopedLock sl(playerLock);
            for (int i = 0; i < trackPlayers.size(); ++i)
                trackPlayers.getReference(i)->preloadClipAtPosition(transportSeconds);
        }

        transportState.play();
        {
            juce::ScopedLock sl(playerLock);
            for (int i = 0; i < trackPlayers.size(); ++i)
            {
                auto* player = trackPlayers.getReference(i).get();
                player->setLoopActive(transportState.isLooping());
                player->setPlaybackPosition(transportSeconds);
                player->setPlaying(true);
            }
        }
    }

    void StudioAudioEngine::stop()
    {
        transportState.stop();

        // Set isPlaying atomically first — instant UI response, no lock needed.
        // The audio callback checks isPlaying.load() each block, so it stops
        // outputting audio immediately without waiting for playerLock.
        for (int i = 0; i < trackPlayers.size(); ++i)
            trackPlayers.getReference(i)->isPlaying.store(false);

        // Then call transportSource.stop() under the lock (JUCE requires it).
        {
            juce::ScopedLock sl(playerLock);
            for (int i = 0; i < trackPlayers.size(); ++i)
                trackPlayers.getReference(i)->transportSource.stop();
        }

        if (recordingActive)
            stopRecordingInternal();
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

    void StudioAudioEngine::previewPianoKey(int midiNoteNumber, bool noteOn)
    {
        if (noteOn)
        {
            const double freqHz = 440.0 * std::pow(2.0, (midiNoteNumber - 69) / 12.0);
            const double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
            pianoKeyPhaseIncrement = juce::MathConstants<double>::twoPi * freqHz / sr;
            pianoKeyReleasing.store(false);
            pianoKeyActive.store(true);
        }
        else
        {
            pianoKeyReleasing.store(true);
        }
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
            juce::Logger::writeToLog("ERROR: Could not open file for writing: " + currentRecordingFile.getFullPathName()
                + " — falling back to temp folder");
            // Fallback to system temp directory
            recordingFolder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NovaStudio");
            recordingFolder.createDirectory();
            currentRecordingFile = recordingFolder.getChildFile("NovaStudioRecording_" + timestamp + ".wav");
            currentRecordingFile = currentRecordingFile.getNonexistentSibling();
            outputStream = std::make_unique<juce::FileOutputStream>(currentRecordingFile);
            if (!outputStream->openedOk())
            {
                juce::Logger::writeToLog("ERROR: Fallback also failed: " + currentRecordingFile.getFullPathName());
                return;
            }
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
        return true;
    }

    bool StudioAudioEngine::removeMasterPlugin(int slot)
    {
        juce::ScopedLock sl(masterPluginLock);
        if (!isPositiveAndBelow(slot, masterPlugins.size())) return false;
        masterPlugins.remove(slot);
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

    void StudioAudioEngine::newSession()
    {
        session.clear();
        session.setName("Untitled Session");
        session.setTempo(120.0);
        session.setSampleRate(44100.0);
        transportState.setTempo(120);
        transportState.stop();
        transportState.setPositionSamples(0);
        buildTrackPlayers();
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
            {
                auto* player = trackPlayers.getReference(i).get();
                player->setPlaybackPosition(transportSeconds);

                // Apply automation lanes (volume/pan/sends) for the current position —
                // overrides the static mixer values fetched from the session while playing.
                if (transportState.isPlaying() && isPositiveAndBelow(player->trackIndex, session.getNumTracks()))
                {
                    const auto& lanes = session.getTrack(player->trackIndex).automationLanes;
                    for (const auto& lane : lanes)
                    {
                        if (!lane.enabled || lane.points.isEmpty())
                            continue;

                        const float value = evaluateAutomationLane(lane, transportSeconds);
                        if (lane.parameterId == "volume")
                            player->volumeDb = value;
                        else if (lane.parameterId == "pan")
                            player->pan = value;
                        else if (lane.parameterId.startsWith("send"))
                        {
                            const int sendIndex = lane.parameterId.substring(4).getIntValue() - 1;
                            if (isPositiveAndBelow(sendIndex, TrackPlayer::kMaxSends))
                                player->sendLevels[sendIndex] = value;
                        }
                        else if (lane.parameterId.startsWith("plugin:"))
                        {
                            // "plugin:<slot>:<paramIndex>" — value is normalised 0..1
                            auto tokens = juce::StringArray::fromTokens(lane.parameterId, ":", "");
                            if (tokens.size() == 3)
                            {
                                const int slot = tokens[1].getIntValue();
                                const int paramIndex = tokens[2].getIntValue();
                                if (auto* plugin = player->getPlugin(slot))
                                {
                                    auto& params = plugin->getParameters();
                                    if (isPositiveAndBelow(paramIndex, params.size()))
                                        params[paramIndex]->setValue(juce::jlimit(0.0f, 1.0f, value));
                                }
                            }
                        }
                    }
                }
            }
        }

        // Clear send buses before the regular track mix writes into them
        clearSendBuses(numSamples);

        mixerSource.getNextAudioBlock(info);  // regular + aux tracks all run here

        // ── Master plugin chain ───────────────────────────────────────────────
        {
            juce::ScopedLock sl(masterPluginLock);
            juce::MidiBuffer emptyMidi;
            for (auto* plugin : masterPlugins)
                if (plugin) plugin->processBlock(tempBuffer, emptyMidi);
        }

        // ── Aux track pass: mix aux return outputs into the main output ───────
        // Aux TrackPlayers are already part of mixerSource and wrote their output
        // into tempBuffer. This is the correct behaviour: aux tracks are normal
        // MixerAudioSource inputs whose getNextAudioBlock reads from send bus buffers.
        // The send writes happen during the same mixerSource call above (in each
        // non-aux TrackPlayer's getNextAudioBlock via the send bus write block).
        // Ordering: MixerAudioSource calls players in insertion order, so aux tracks
        // that were added AFTER regular tracks see the already-written bus data.

        // Pro Tools-style master output protection: hard clip at ±1.0f.
        // Completely transparent below 0 dBFS. Any sample that exceeds 0 dBFS is
        // clamped and the clip indicator is set so the UI can warn the user.
        {
            float peakThisBlock = 0.0f;
            bool  clippedThisBlock = false;
            for (int channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
            {
                float* data = tempBuffer.getWritePointer(channel);
                for (int s = 0; s < numSamples; ++s)
                {
                    const float x = data[s];
                    const float ax = std::abs(x);
                    if (ax > peakThisBlock) peakThisBlock = ax;
                    if (ax > 1.0f)
                    {
                        data[s] = x > 0.0f ? 1.0f : -1.0f;
                        clippedThisBlock = true;
                    }
                }
            }
            masterOutputPeak.store(peakThisBlock);
            if (clippedThisBlock)
                masterOutputClipped.store(true);
        }

        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            const float* sourceData = tempBuffer.getReadPointer(channel);
            float* destData = outputChannelData[channel];
            if (destData != nullptr)
                std::memcpy(destData, sourceData, static_cast<size_t>(sizeof(float) * numSamples));
        }

        // ── Sample audition / preview — mixed straight into the main output ──
        if (previewActive.load() && previewTransport.isPlaying())
        {
            previewScratchBuffer.setSize(numOutputChannels, numSamples, false, false, true);
            juce::AudioSourceChannelInfo previewInfo(previewScratchBuffer);
            previewTransport.getNextAudioBlock(previewInfo);

            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                if (outputChannelData[channel] == nullptr) continue;
                const float* src = previewScratchBuffer.getReadPointer(juce::jmin(channel, previewScratchBuffer.getNumChannels() - 1));
                for (int s = 0; s < numSamples; ++s)
                    outputChannelData[channel][s] += src[s];
            }

            if (! previewTransport.isPlaying())
                previewActive.store(false);
        }

        // ── On-screen piano-key preview synth — short sine voice with attack/release ──
        if (pianoKeyActive.load())
        {
            constexpr float kGain    = 0.12f;
            constexpr float kAttack  = 0.01f;   // envelope step per sample (~ a few ms)
            constexpr float kRelease = 0.004f;
            const bool releasing = pianoKeyReleasing.load();

            for (int s = 0; s < numSamples; ++s)
            {
                if (releasing)
                {
                    pianoKeyEnvelope -= kRelease;
                    if (pianoKeyEnvelope <= 0.0f)
                    {
                        pianoKeyEnvelope = 0.0f;
                        pianoKeyActive.store(false);
                    }
                }
                else if (pianoKeyEnvelope < 1.0f)
                {
                    pianoKeyEnvelope = juce::jmin(1.0f, pianoKeyEnvelope + kAttack);
                }

                const float sampleValue = (float) std::sin(pianoKeyPhase) * pianoKeyEnvelope * kGain;
                pianoKeyPhase += pianoKeyPhaseIncrement;
                if (pianoKeyPhase > juce::MathConstants<double>::twoPi)
                    pianoKeyPhase -= juce::MathConstants<double>::twoPi;

                for (int channel = 0; channel < numOutputChannels; ++channel)
                    if (outputChannelData[channel] != nullptr)
                        outputChannelData[channel][s] += sampleValue;

                if (! pianoKeyActive.load())
                    break;
            }
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

        // ── Step sequencer playback ──────────────────────────────────────────
        if (transportState.isPlaying())
        {
            const int64_t positionSamples = transportState.getPositionSamples();
            const double  sr  = transportState.getSampleRate();
            const double  bpm = (double)transportState.getTempo();
            if (sr > 0.0 && bpm > 0.0)
            {
                const int   activeSteps  = stepSeq.activeStepCount;
                const float swingAmount  = stepSeq.swing;
                const double stepsPerSec = (bpm / 60.0) * 4.0;
                const double stepDurationSamples = sr / stepsPerSec;

                // Calculate current step with swing applied
                // Swing shifts odd-numbered steps later by swing * stepDuration * 0.33
                const double rawStep     = (double)positionSamples / sr * stepsPerSec;
                const double rawStepPrev = (double)juce::jmax<int64_t>(0, positionSamples - numSamples)
                                           / sr * stepsPerSec;

                // Convert raw step position to swung step position
                auto stepWithSwing = [&](double rawPos) -> double {
                    // Each pair of steps (0-1, 2-3, ...) is a "beat"
                    // Even step: starts at normal time
                    // Odd step: delayed by swing * 0.33 * stepDuration
                    const double beatIndex = std::floor(rawPos / 2.0);
                    const double posInBeat = rawPos - beatIndex * 2.0;
                    // Within a beat: even step occupies [0, 1+swing*0.33], odd step follows
                    const double evenEnd = 1.0 + swingAmount * 0.33;
                    if (posInBeat < evenEnd)
                        return beatIndex * 2.0 + posInBeat / evenEnd;          // normalise even portion
                    else
                        return beatIndex * 2.0 + 1.0 + (posInBeat - evenEnd) / (2.0 - evenEnd);
                };

                const int currentStep = (int)stepWithSwing(rawStep)     % activeSteps;
                const int prevStep    = (int)stepWithSwing(rawStepPrev)  % activeSteps;

                currentPlayingStep.store(currentStep);

                if (currentStep != prevStep)
                {
                    for (int row = 0; row < StepSequencerState::kNumRows; ++row)
                    {
                        if (stepSeq.rowMuted[row]) continue;
                        if (stepSeq.steps[row][currentStep] && stepSeq.sampleLoaded[row])
                        {
                            stepSeq.playPositions[row] = 0;
                            stepSeq.playPosFrac[row]   = 0.0;
                            stepSeq.rowTriggered[row]  = true;

                            // Re-trigger the channel engine: reset envelope/LFO so each
                            // hit starts a fresh AHDSR cycle and modulation phase.
                            auto& ceTrig = stepSeq.channelEngines[(size_t)row];
                            ceTrig.envStage = 0;
                            ceTrig.envPos   = 0.0;
                            ceTrig.envLevel = 0.0f;
                            ceTrig.lfoPhase = 0.0;
                        }
                    }
                }

                for (int row = 0; row < StepSequencerState::kNumRows; ++row)
                {
                    if (!stepSeq.rowTriggered[row] || !stepSeq.sampleLoaded[row]) continue;
                    if (stepSeq.rowMuted[row]) { stepSeq.rowTriggered[row] = false; continue; }

                    const auto& buf = stepSeq.sampleBuffers[row];
                    const int   numSrcSamples = buf.getNumSamples();
                    auto&       ce = stepSeq.channelEngines[(size_t)row];

                    // Determine velocity for the last triggered step
                    // We use currentStep as an approximation; for per-step accuracy this is sufficient
                    const float groove     = StepSequencerState::grooveAccentForStep(stepSeq.grooveTemplate, currentStep);
                    const float velocity   = juce::jlimit(0.0f, 1.0f, stepSeq.velocities[row][currentStep] * groove);
                    const float rowVolume  = stepSeq.rowVolumes[row];
                    const float rowPan     = stepSeq.rowPans[row];
                    const float leftGain   = (rowPan <= 0.0f ? 1.0f : 1.0f - rowPan)  * rowVolume * velocity;
                    const float rightGain  = (rowPan >= 0.0f ? 1.0f : 1.0f + rowPan)  * rowVolume * velocity;

                    // FL-style per-channel pitch knob — resample the one-shot at a
                    // playback rate of 2^(semitones/12) using linear interpolation.
                    const float semitones = stepSeq.rowPitches[row];
                    const double rate = std::pow(2.0, (double) semitones / 12.0);

                    const bool useChannelEngine = ce.envEnabled || ce.filterEnabled || ce.lfoEnabled
                                                    || ce.loopEnabled || ce.sampleStart > 0.0f || ce.sampleEnd < 1.0f;

                    if (useChannelEngine)
                    {
                        const double sr = currentSampleRate;
                        const int regionStart = juce::jlimit(0, juce::jmax(0, numSrcSamples - 1),
                                                              (int) (ce.sampleStart * (float) numSrcSamples));
                        const int regionEnd   = juce::jlimit(regionStart + 1, numSrcSamples,
                                                              (int) (ce.sampleEnd * (float) numSrcSamples));
                        const double releaseSamples = juce::jmax(1.0f, ce.envRelease) * sr;

                        double readPos = juce::jmax(stepSeq.playPosFrac[row], (double) regionStart);

                        for (int s = 0; s < numSamples; ++s)
                        {
                            if (readPos >= (double) (regionEnd - 1))
                            {
                                if (ce.loopEnabled)
                                {
                                    readPos = (double) regionStart;
                                }
                                else
                                {
                                    stepSeq.rowTriggered[row] = false;
                                    break;
                                }
                            }

                            // Auto-release: begin the release stage shortly before the
                            // (non-looping) region ends so AHDSR completes naturally.
                            if (ce.envEnabled && ! ce.loopEnabled && ce.envStage < 5
                                && (double) regionEnd - readPos <= releaseSamples)
                            {
                                ce.envStage = 5;
                                ce.envPos   = 0.0;
                            }

                            float lfoVal = 0.0f;
                            if (ce.lfoEnabled)
                            {
                                lfoVal = (float) std::sin(ce.lfoPhase * juce::MathConstants<double>::twoPi);
                                ce.lfoPhase += ce.lfoRateHz / sr;
                                if (ce.lfoPhase >= 1.0) ce.lfoPhase -= 1.0;
                            }

                            double sampleRate2 = rate;
                            float  ampMod = 1.0f;
                            if (ce.lfoEnabled)
                            {
                                if (ce.lfoTarget == 0)       // pitch — up to one octave swing * depth
                                    sampleRate2 *= std::pow(2.0, (double) (lfoVal * ce.lfoDepth));
                                else if (ce.lfoTarget == 1)  // volume — tremolo
                                    ampMod = 1.0f + lfoVal * ce.lfoDepth * 0.5f;
                            }

                            const float envGain = ce.envEnabled ? advanceChannelEnvelope(ce, sr) : 1.0f;

                            const int   i0   = (int) readPos;
                            const int   i1   = juce::jmin(i0 + 1, numSrcSamples - 1);
                            const float frac = (float) (readPos - (double) i0);

                            for (int ch = 0; ch < numOutputChannels; ++ch)
                            {
                                if (outputChannelData[ch] == nullptr) continue;
                                const int srcCh = juce::jmin(ch, buf.getNumChannels() - 1);
                                const float* src = buf.getReadPointer(srcCh);
                                float samp = src[i0] + (src[i1] - src[i0]) * frac;

                                if (ce.filterEnabled)
                                    samp = (float) applyChannelFilter(ce, ch, (double) samp, sr, lfoVal);

                                const float chanGain = (ch == 0) ? leftGain : rightGain;
                                outputChannelData[ch][s] += samp * chanGain * envGain * ampMod;
                            }

                            readPos += sampleRate2;
                        }

                        stepSeq.playPosFrac[row]   = readPos;
                        stepSeq.playPositions[row] = (int64_t) readPos;
                        continue;
                    }

                    if (juce::approximatelyEqual(rate, 1.0))
                    {
                        const int64_t remaining = numSrcSamples - stepSeq.playPositions[row];
                        if (remaining <= 0) { stepSeq.rowTriggered[row] = false; continue; }
                        const int toCopy = (int) juce::jmin<int64_t>(remaining, numSamples);

                        for (int ch = 0; ch < numOutputChannels; ++ch)
                        {
                            if (outputChannelData[ch] == nullptr) continue;
                            const int srcCh = juce::jmin(ch, buf.getNumChannels() - 1);
                            const float* src = buf.getReadPointer(srcCh, (int) stepSeq.playPositions[row]);
                            const float chanGain = (ch == 0) ? leftGain : rightGain;
                            for (int s = 0; s < toCopy; ++s)
                                outputChannelData[ch][s] += src[s] * chanGain;
                        }
                        stepSeq.playPositions[row] += toCopy;
                        if (stepSeq.playPositions[row] >= numSrcSamples)
                            stepSeq.rowTriggered[row] = false;
                    }
                    else
                    {
                        double readPos = stepSeq.playPosFrac[row];
                        for (int s = 0; s < numSamples; ++s)
                        {
                            if (readPos >= (double) (numSrcSamples - 1))
                            {
                                stepSeq.rowTriggered[row] = false;
                                break;
                            }

                            const int    i0   = (int) readPos;
                            const float  frac = (float) (readPos - (double) i0);

                            for (int ch = 0; ch < numOutputChannels; ++ch)
                            {
                                if (outputChannelData[ch] == nullptr) continue;
                                const int srcCh = juce::jmin(ch, buf.getNumChannels() - 1);
                                const float* src = buf.getReadPointer(srcCh);
                                const float samp = src[i0] + (src[i0 + 1] - src[i0]) * frac;
                                const float chanGain = (ch == 0) ? leftGain : rightGain;
                                outputChannelData[ch][s] += samp * chanGain;
                            }

                            readPos += rate;
                        }
                        stepSeq.playPosFrac[row] = readPos;
                        stepSeq.playPositions[row] = (int64_t) readPos;
                    }
                }
            }
        }
        else
        {
            currentPlayingStep.store(-1);
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

            // Punch gating: skip write if punch is enabled and we're outside the range
            if (transportState.isPunchEnabled())
            {
                const int64_t pos  = transportState.getPositionSamples();
                const int64_t pIn  = transportState.getPunchInSample();
                const int64_t pOut = transportState.getPunchOutSample();
                if (pos < pIn || pos >= pOut)
                {
                    recordingSampleCount += numSamples; // advance clip length counter
                    return;
                }
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

        prepareSendBuses(2, currentBufferSize);

        previewTransport.prepareToPlay(currentBufferSize, currentSampleRate);
        previewScratchBuffer.setSize(2, currentBufferSize);
    }

    void StudioAudioEngine::audioDeviceStopped()
    {
        mixerSource.releaseResources();
        for (int i = 0; i < trackPlayers.size(); ++i)
            trackPlayers.getReference(i)->releaseResources();

        previewTransport.releaseResources();
    }

    void StudioAudioEngine::buildTrackPlayers()
    {
        // Build new players outside the lock so prepareToPlay (which can be slow)
        // doesn't block the audio thread.
        juce::Array<std::unique_ptr<TrackPlayer>> newPlayers;
        for (int index = 0; index < session.getNumTracks(); ++index)
        {
            const Track& tr = session.getTrack(index);
            auto trackPlayer = std::make_unique<TrackPlayer>();
            trackPlayer->readAheadThread = &readAheadThread;
            trackPlayer->setTrackMetadata(tr);
            trackPlayer->setSessionTrack(&session, index);

            // Wire send bus pointers
            for (int si = 0; si < TrackPlayer::kMaxSends; ++si)
            {
                trackPlayer->sendBusIndex[si] = tr.sendBusIndex[si];
                trackPlayer->sendLevels[si]   = tr.sendLevels[si];
                trackPlayer->sendPreFader[si]  = tr.sendPreFader[si];
                const int busIdx = tr.sendBusIndex[si];
                trackPlayer->sendBusBuffers[busIdx >= 0 && busIdx < kNumSendBuses ? busIdx : 0]
                    = (busIdx >= 0 && busIdx < kNumSendBuses) ? &sendBusBuffers[busIdx] : nullptr;
            }
            // Zero out any unassigned bus slots
            for (int b = 0; b < kNumSendBuses; ++b)
            {
                bool used = false;
                for (int si = 0; si < TrackPlayer::kMaxSends; ++si)
                    if (tr.sendBusIndex[si] == b) { used = true; break; }
                if (!used) trackPlayer->sendBusBuffers[b] = nullptr;
            }

            // Aux track config
            if (tr.type == TrackType::Aux)
            {
                trackPlayer->isAuxTrack = true;
                trackPlayer->auxInputBusIndex = tr.auxInputBusIndex;
            }

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
        updatePluginDelayCompensation();

        // Pre-load clips at the current transport position so the first play()
        // call doesn't have to load from the audio callback (which would deadlock).
        const double pos = static_cast<double>(transportState.getPositionSamples()) / transportState.getSampleRate();
        for (int i = 0; i < trackPlayers.size(); ++i)
            trackPlayers.getReference(i)->preloadClipAtPosition(pos);
    }

    void StudioAudioEngine::updatePluginDelayCompensation()
    {
        juce::ScopedLock sl(playerLock);

        int maxLatency = 0;
        for (int i = 0; i < trackPlayers.size(); ++i)
            maxLatency = juce::jmax(maxLatency, trackPlayers.getReference(i)->getPluginChainLatencySamples());

        for (int i = 0; i < trackPlayers.size(); ++i)
        {
            auto* player = trackPlayers.getReference(i).get();
            player->setPdcDelaySamples(maxLatency - player->getPluginChainLatencySamples());
        }
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

    int StudioAudioEngine::addAuxTrack(const juce::String& name, int busIndex)
    {
        Track& t = session.addTrack(name, TrackType::Aux);
        t.auxInputBusIndex = busIndex;
        buildTrackPlayers();
        return session.getNumTracks() - 1;
    }

    void StudioAudioEngine::setAuxTrackBus(int trackIndex, int busIndex)
    {
        if (!isPositiveAndBelow(trackIndex, session.getNumTracks())) return;
        session.getTrack(trackIndex).auxInputBusIndex = busIndex;
        juce::ScopedLock sl(playerLock);
        if (isPositiveAndBelow(trackIndex, trackPlayers.size()))
        {
            trackPlayers.getReference(trackIndex)->isAuxTrack      = true;
            trackPlayers.getReference(trackIndex)->auxInputBusIndex = busIndex;
        }
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

    void StudioAudioEngine::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
    {
        if (!message.isController())
            return;

        const int ccNumber = message.getControllerNumber();
        const int channel  = message.getChannel();
        const float normalized = message.getControllerValue() / 127.0f;

        if (midiLearnPending.load())
        {
            MidiCCBinding binding;
            binding.ccNumber = ccNumber;
            binding.channel  = channel;

            if (midiLearnIsForMacro)
                binding.macroIndex = midiLearnMacroIndex;
            else
                binding.target = midiLearnTarget;

            // Replace any existing binding for the same destination
            for (int i = midiBindings.size(); --i >= 0;)
            {
                const auto& existing = midiBindings.getReference(i);
                const bool sameMacro  = midiLearnIsForMacro && existing.macroIndex == midiLearnMacroIndex;
                const bool sameTarget = !midiLearnIsForMacro && existing.macroIndex < 0
                                     && existing.target.trackIndex == midiLearnTarget.trackIndex
                                     && existing.target.parameterId == midiLearnTarget.parameterId;
                if (sameMacro || sameTarget)
                    midiBindings.remove(i);
            }

            const int newIndex = midiBindings.size();
            midiBindings.add(binding);

            midiLearnPending.store(false);
            midiLearnMacroIndex = -1;

            if (onMidiLearned)
            {
                juce::MessageManager::callAsync([this, newIndex]() { if (onMidiLearned) onMidiLearned(newIndex); });
            }
            return;
        }

        for (const auto& binding : midiBindings)
        {
            if (binding.ccNumber != ccNumber)
                continue;
            if (binding.channel != 0 && binding.channel != channel)
                continue;

            if (binding.macroIndex >= 0)
                setMacroValue(binding.macroIndex, normalized);
            else
                applyParameterTarget(binding.target, normalized);
        }
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

    void StudioAudioEngine::setStepSeqSample(int row, const juce::String& filePath)
    {
        if (!isPositiveAndBelow(row, StepSequencerState::kNumRows)) return;
        stepSeq.sampleFiles[row]  = filePath;
        stepSeq.sampleLoaded[row] = false;
        stepSeq.sampleBuffers[row].setSize(0, 0);

        juce::File f(filePath);
        if (!f.existsAsFile()) return;

        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fmtMgr.createReaderFor(f));
        if (reader == nullptr) return;

        const int     numCh      = (int)reader->numChannels;
        const int64_t numSamples = reader->lengthInSamples;
        stepSeq.sampleBuffers[row].setSize(numCh, (int)numSamples);
        reader->read(&stepSeq.sampleBuffers[row], 0, (int)numSamples, 0, true, true);

        stepSeq.sampleLoaded[row]  = true;
        stepSeq.playPositions[row] = 0;
        stepSeq.rowTriggered[row]  = false;
    }

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
