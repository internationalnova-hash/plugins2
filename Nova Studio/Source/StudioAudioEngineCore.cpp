// Nova Studio engine core: playback graph, track strip, recording, MIDI, metronome,
// offline export, snapshots and session I/O.
//
// Threading model
//  - The message thread owns the Session. Whenever it changes, a PlaySnapshot (read-only
//    copy of clips, notes, automation and tempo) is built and swapped in atomically.
//  - The audio thread renders from the snapshot and the TrackPlayers. It never reads the
//    Session, never allocates, never blocks: it only *tries* playerLock (held by the message
//    thread for short structural edits) and outputs silence for that one block if it's busy.
//  - Clip audio streams from disk on readAheadThread (see ClipStreamer).
#include "StudioAudioEngine.h"

namespace NovaStudio
{
    namespace
    {
        inline float dbToGain(float db) noexcept
        {
            return db <= -99.0f ? 0.0f : juce::Decibels::decibelsToGain(db);
        }

        inline float fadeGainAt(int64_t rel, int64_t len, int64_t fadeIn, int64_t fadeOut) noexcept
        {
            float g = 1.0f;
            if (fadeIn > 0 && rel < fadeIn)
                g = juce::jmin(g, (float) rel / (float) fadeIn);
            if (fadeOut > 0 && rel > len - fadeOut)
                g = juce::jmin(g, (float) (len - rel) / (float) fadeOut);
            return juce::jlimit(0.0f, 1.0f, g);
        }

        // 4-point Hermite interpolation; d points at sample idx-1.
        inline float hermite(const float* d, float t) noexcept
        {
            const float xm1 = d[0], x0 = d[1], x1 = d[2], x2 = d[3];
            const float c = (x1 - xm1) * 0.5f;
            const float v = x0 - x1;
            const float w = c + v;
            const float a = w + v + (x2 - x0) * 0.5f;
            const float b = w + a;
            return ((((a * t) - b) * t + c) * t + x0);
        }

        struct Hasher
        {
            uint64_t h = 1469598103934665603ull;
            void add(const void* data, size_t n) { auto* p = (const unsigned char*) data; for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; } }
            template <typename T> void v(const T& x) { add(&x, sizeof(T)); }
            void s(const juce::String& str) { v(str.hashCode64()); }
        };

        juce::String sanitiseFileName(juce::String s)
        {
            s = juce::File::createLegalFileName(s.trim());
            return s.isEmpty() ? juce::String("Untitled") : s;
        }
    }

    //==========================================================================
    // TrackPlayer
    //==========================================================================
    StudioAudioEngine::TrackPlayer::TrackPlayer() = default;
    StudioAudioEngine::TrackPlayer::~TrackPlayer() = default;

    int StudioAudioEngine::TrackPlayer::requiredChannels() const
    {
        int c = 2;
        for (auto& pl : pluginChain)
            if (pl)
                c = juce::jmax(c, pl->getTotalNumInputChannels(), pl->getTotalNumOutputChannels());
        return juce::jmin(c, 32);
    }

    void StudioAudioEngine::TrackPlayer::prepare(double sr, int maxBlockSize)
    {
        sampleRate = sr;
        maxBlock = juce::jmax(64, maxBlockSize);
        procChannels = requiredChannels();
        buf.setSize(procChannels, maxBlock, false, true, false);
        clipTmp.setSize(2, maxBlock * 8 + 16, false, true, false);
        midi.ensureSize(8192);

        for (auto& plugin : pluginChain)
            if (plugin)
            {
                plugin->setRateAndBufferSizeDetails(sr, maxBlock);
                plugin->prepareToPlay(sr, maxBlock);
            }

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, (juce::uint32) procChannels };
        pdcDelayLine.prepare(spec);
        pdcDelayLine.setDelay((float) pdcDelaySamples);
        for (auto& b : eqBands) { b.z1L = b.z2L = b.z1R = b.z2R = 0.0; }
        eqDirty = true;
        lastGainL = lastGainR = -1.0f;
    }

    void StudioAudioEngine::TrackPlayer::release()
    {
        for (auto& plugin : pluginChain)
            if (plugin) plugin->releaseResources();
    }

    void StudioAudioEngine::TrackPlayer::setTrackMetadata(const Track& t)
    {
        volumeDb.store(t.volumeDb);
        pan.store(t.pan);
        muted.store(t.muted);
        solo.store(t.solo);
        armed.store(t.armed);
        monitorInput.store(t.monitorInput);
        isStereo.store(t.isStereo);
        inputChannel.store(t.inputChannel);
        outputBusIndex.store(StudioAudioEngine::parseBusIndex(t.outputBus));
        frozen.store(t.frozen);
        type = t.type;
        isAuxTrack = (t.type == TrackType::Aux);
        auxInputBusIndex = t.auxInputBusIndex;
        for (int i = 0; i < kMaxSends; ++i)
        {
            sendBusIndex[i] = t.sendBusIndex[i];
            sendLevels[i]   = t.sendLevels[i];
            sendPreFader[i] = t.sendPreFader[i];
        }
        for (int b = 0; b < kNumEQBands; ++b)
        {
            eqBands[(size_t) b].enabled.store(t.eq[b].enabled);
            eqBands[(size_t) b].freq.store(t.eq[b].freq);
            eqBands[(size_t) b].gainDb.store(t.eq[b].gainDb);
            eqBands[(size_t) b].q.store(t.eq[b].q);
        }
        eqDirty = true;
    }

    bool StudioAudioEngine::TrackPlayer::addPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin)
    {
        if (!plugin) return false;
        pluginChain.add(std::move(plugin));
        pluginBypassed.add(false);
        const int needed = requiredChannels();
        if (needed != procChannels)
            prepare(sampleRate, maxBlock);
        return true;
    }

    bool StudioAudioEngine::TrackPlayer::removePlugin(int index)
    {
        if (!juce::isPositiveAndBelow(index, pluginChain.size())) return false;
        pluginChain.remove(index);
        pluginBypassed.remove(index);
        return true;
    }

    bool StudioAudioEngine::TrackPlayer::movePlugin(int fromIndex, int toIndex)
    {
        if (!juce::isPositiveAndBelow(fromIndex, pluginChain.size()) || !juce::isPositiveAndBelow(toIndex, pluginChain.size()))
            return false;
        pluginChain.move(fromIndex, toIndex);
        pluginBypassed.move(fromIndex, toIndex);
        return true;
    }

    void StudioAudioEngine::TrackPlayer::setPluginBypassed(int index, bool bypassed)
    {
        if (juce::isPositiveAndBelow(index, pluginBypassed.size()))
            pluginBypassed.getReference(index) = bypassed;
    }

    bool StudioAudioEngine::TrackPlayer::isPluginBypassed(int index) const
    {
        return juce::isPositiveAndBelow(index, pluginBypassed.size()) && pluginBypassed.getReference(index);
    }

    int StudioAudioEngine::TrackPlayer::getPluginChainLatencySamples() const
    {
        if (frozen.load()) return 0;   // a frozen track plays pre-rendered audio
        int total = 0;
        for (int i = 0; i < pluginChain.size(); ++i)
            if (!isPluginBypassed(i))
                if (auto& plugin = pluginChain.getReference(i))
                    total += plugin->getLatencySamples();
        return total;
    }

    void StudioAudioEngine::TrackPlayer::setPdcDelaySamples(int samples)
    {
        samples = juce::jlimit(0, 190000, samples);
        if (samples == pdcDelaySamples) return;
        pdcDelaySamples = samples;
        pdcDelayLine.setDelay((float) pdcDelaySamples);
    }

    juce::AudioPluginInstance* StudioAudioEngine::TrackPlayer::getPlugin(int index) const
    {
        return juce::isPositiveAndBelow(index, pluginChain.size()) ? pluginChain.getReference(index).get() : nullptr;
    }

    void StudioAudioEngine::TrackPlayer::getPluginState(int index, juce::MemoryBlock& dest) const
    {
        if (auto* p = getPlugin(index)) p->getStateInformation(dest);
    }

    void StudioAudioEngine::TrackPlayer::setPluginState(int index, const void* data, size_t size)
    {
        if (auto* p = getPlugin(index)) p->setStateInformation(data, (int) size);
    }

    //==========================================================================
    // Construction / device
    //==========================================================================
    StudioAudioEngine::StudioAudioEngine()
    {
        pluginFormatManager.addDefaultFormats();
        previewFormatManager.registerBasicFormats();
        clipFormats.registerBasicFormats();

        juce::File novaRoot = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("NovaStudio");
        novaRoot.createDirectory();
        juce::File preferred = novaRoot.getChildFile("Recordings");
        if (preferred.createDirectory().wasOk())
            recordingFolder = preferred;
        else
        {
            recordingFolder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NovaStudio").getChildFile("Recordings");
            recordingFolder.createDirectory();
        }
        novaRoot.getChildFile("Sessions").createDirectory();

        diagLogger = std::make_unique<juce::FileLogger>(novaRoot.getChildFile("nova_diag.log"), "Nova Studio diagnostics", 1024 * 512);
        juce::Logger::setCurrentLogger(diagLogger.get());

        recordingThread.startThread();
        readAheadThread.startThread(juce::Thread::Priority::high);

        session.setSampleRate(currentSampleRate);
        transportState.setSampleRate(currentSampleRate);
        transportState.setTempo(session.getTempo());

        mixBuffer.setSize(2, maxBlockSize);
        silenceBuffer.setSize(2, maxBlockSize);
        silenceBuffer.clear();
        exportBuffer.setSize(2, maxBlockSize);
        prepareSendBuses(2, maxBlockSize);
        liveMidiBlock.ensureSize(4096);
        masterMidi.ensureSize(256);

        publishSnapshot(true);
        startTimerHz(20);
    }

    StudioAudioEngine::~StudioAudioEngine()
    {
        stopTimer();
        shutdown();
        {
            const juce::SpinLock::ScopedLockType sl(recordLock);
            recordTakes.clear();
        }
        liveSnapshot.store(nullptr);
        readAheadThread.stopThread(3000);
        for (auto& s : registeredStreamers)
            readAheadThread.removeTimeSliceClient(s.get());
        registeredStreamers.clear();
        retiredSnapshots.clear();
        currentSnapshot.reset();
        recordingThread.stopThread(3000);
        juce::Logger::setCurrentLogger(nullptr);
    }

    bool StudioAudioEngine::initialize()
    {
        struct Config { int ins; int outs; };
        const Config configs[] = { {2, 2}, {1, 2}, {0, 2} };

        juce::AudioIODevice* device = nullptr;
        for (auto& cfg : configs)
        {
            const juce::String result = deviceManager.initialiseWithDefaultDevices(cfg.ins, cfg.outs);
            device = deviceManager.getCurrentAudioDevice();
            juce::Logger::writeToLog("initialiseWithDefaultDevices(" + juce::String(cfg.ins) + "," + juce::String(cfg.outs)
                                     + ") result=\"" + result + "\" device=" + (device ? device->getName() : "null"));
            if (device != nullptr)
                break;
        }

        if (device == nullptr)
            return false;

        // Ask for every input the interface has so tracks can record from any of them.
        {
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            deviceManager.getAudioDeviceSetup(setup);
            const int numIns = device->getInputChannelNames().size();
            if (numIns > 0 && setup.inputChannels.countNumberOfSetBits() < numIns)
            {
                setup.useDefaultInputChannels = false;
                setup.inputChannels.setRange(0, numIns, true);
                deviceManager.setAudioDeviceSetup(setup, true);
                device = deviceManager.getCurrentAudioDevice();
            }
        }

        if (device != nullptr)
        {
            cachedActiveInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
            juce::Logger::writeToLog("Device open: " + device->getName() + " SR=" + juce::String(device->getCurrentSampleRate())
                                     + " buf=" + juce::String(device->getCurrentBufferSizeSamples())
                                     + " in=" + juce::String(cachedActiveInputChannels));
        }

        deviceManager.addAudioCallback(this);

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
        if (recordingActive.load() || countInRemaining.load() > 0)
            stopRecordingInternal();
        deviceManager.removeAudioCallback(this);
        for (auto& mi : juce::MidiInput::getAvailableDevices())
            deviceManager.removeMidiInputDeviceCallback(mi.identifier, this);
        previewActive.store(false);
        previewTransport.stop();
        previewTransport.setSource(nullptr);
        previewReaderSource.reset();
        const juce::ScopedLock sl(playerLock);
        for (auto& player : trackPlayers)
            player->release();
    }

    bool StudioAudioEngine::setSampleRate(int newSampleRate, int newBufferSize)
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);
        setup.sampleRate = (double) newSampleRate;
        setup.bufferSize = newBufferSize;
        const juce::String err = deviceManager.setAudioDeviceSetup(setup, true);
        if (err.isNotEmpty())
            juce::Logger::writeToLog("setSampleRate rejected: " + err);
        return err.isEmpty();
    }

    void StudioAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
    {
        const double newRate = device->getCurrentSampleRate();
        currentBufferSize = device->getCurrentBufferSizeSamples();
        maxBlockSize = juce::jmax(currentBufferSize, 1024);
        cachedActiveInputChannels = device->getActiveInputChannels().countNumberOfSetBits();

        // Keep the arrangement in time if the device rate differs from the session's.
        session.rescaleToSampleRate(newRate);
        currentSampleRate = newRate;
        transportState.setSampleRate(newRate);

        mixBuffer.setSize(2, maxBlockSize, false, true, false);
        silenceBuffer.setSize(2, maxBlockSize, false, true, false);
        silenceBuffer.clear();
        exportBuffer.setSize(2, maxBlockSize, false, true, false);
        prepareSendBuses(2, maxBlockSize);

        {
            const juce::ScopedLock sl(playerLock);
            for (auto& p : trackPlayers)
                p->prepare(newRate, maxBlockSize);
        }
        {
            const juce::ScopedLock ml(masterPluginLock);
            for (auto* p : masterPlugins)
                if (p) { p->setRateAndBufferSizeDetails(newRate, maxBlockSize); p->prepareToPlay(newRate, maxBlockSize); }
        }

        masterLimiter.prepare({ newRate, (juce::uint32) maxBlockSize, 2 });
        masterLimiter.setThreshold(-0.3f);
        masterLimiter.setRelease(60.0f);
        masterGain.reset(newRate, 0.03);
        masterGain.setCurrentAndTargetValue(dbToGain(session.masterVolumeDb));

        previewTransport.prepareToPlay(maxBlockSize, newRate);
        previewScratchBuffer.setSize(2, maxBlockSize);
        updatePluginDelayCompensation();
        lastRenderEnd = -1;

        juce::Logger::writeToLog("audioDeviceAboutToStart: " + device->getName() + " SR=" + juce::String(newRate)
                                 + " buf=" + juce::String(currentBufferSize) + " in=" + juce::String(cachedActiveInputChannels));

        if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
            && juce::MessageManager::getInstance()->isThisTheMessageThread())
            publishSnapshot(true);
        else
            juce::MessageManager::callAsync([this] { publishSnapshot(true); });
    }

    void StudioAudioEngine::audioDeviceStopped()
    {
        previewTransport.releaseResources();
    }

    //==========================================================================
    // Audio callback
    //==========================================================================
    void StudioAudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                                             float* const* outputChannelData, int numOutputChannels,
                                                             int numSamples, const juce::AudioIODeviceCallbackContext&)
    {
        renderBlock(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, false);
    }

    void StudioAudioEngine::pushLiveMidi(const juce::MidiMessage& m)
    {
        if (m.getRawDataSize() > 3 || m.getRawDataSize() <= 0)
            return;
        const juce::SpinLock::ScopedLockType sl(liveMidiWriteLock);
        int s1, n1, s2, n2;
        liveMidiFifo.prepareToWrite(1, s1, n1, s2, n2);
        if (n1 + n2 < 1) return;
        auto& e = liveMidiData[(size_t) (n1 > 0 ? s1 : s2)];
        e.size = m.getRawDataSize();
        std::memcpy(e.data, m.getRawData(), (size_t) e.size);
        liveMidiFifo.finishedWrite(1);
    }

    void StudioAudioEngine::renderBlock(const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                                       int numSamples, bool offline)
    {
        juce::ScopedNoDenormals noDenormals;

        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputs[ch], numSamples);

        // Hosts can hand us bigger blocks than announced; split them.
        if (numSamples > maxBlockSize)
        {
            float* outPtrs[64] = {};
            const float* inPtrs[64] = {};
            int done = 0;
            while (done < numSamples)
            {
                const int n = juce::jmin(maxBlockSize, numSamples - done);
                for (int ch = 0; ch < juce::jmin(numOutputs, 64); ++ch) outPtrs[ch] = outputs[ch] ? outputs[ch] + done : nullptr;
                for (int ch = 0; ch < juce::jmin(numInputs, 64); ++ch) inPtrs[ch] = inputs[ch] ? inputs[ch] + done : nullptr;
                renderBlock(inPtrs, juce::jmin(numInputs, 64), outPtrs, juce::jmin(numOutputs, 64), n, offline);
                done += n;
            }
            return;
        }

        const juce::ScopedTryLock tl(playerLock);
        if (!tl.isLocked())
            return;   // a structural edit is in progress: one block of silence beats blocking

        const PlaySnapshot* snap = liveSnapshot.load(std::memory_order_acquire);

        // Live MIDI from keyboards / on-screen keys
        liveMidiBlock.clear();
        if (!offline)
        {
            int s1, n1, s2, n2;
            const int ready = liveMidiFifo.getNumReady();
            liveMidiFifo.prepareToRead(ready, s1, n1, s2, n2);
            for (int i = 0; i < n1; ++i) { auto& e = liveMidiData[(size_t) (s1 + i)]; liveMidiBlock.addEvent(e.data, e.size, 0); }
            for (int i = 0; i < n2; ++i) { auto& e = liveMidiData[(size_t) (s2 + i)]; liveMidiBlock.addEvent(e.data, e.size, 0); }
            liveMidiFifo.finishedRead(n1 + n2);
        }

        int done = 0;
        float blockPeak = 0.0f;
        while (done < numSamples)
        {
            int len = numSamples - done;
            const int64_t cin = offline ? 0 : countInRemaining.load();

            if (cin > 0)
            {
                // Count-in: clicks only, the timeline doesn't move yet.
                len = (int) juce::jmin((int64_t) len, cin);
                const int64_t pos = transportState.getPositionSamples();
                renderSegment(inputs, numInputs, done, len, pos, false, offline, snap, liveMidiBlock);
                float* outPtrs[2] = { outputs[0] ? outputs[0] + done : nullptr, numOutputs > 1 && outputs[1] ? outputs[1] + done : nullptr };
                for (int ch = 0; ch < juce::jmin(2, numOutputs); ++ch)
                    if (outPtrs[ch]) juce::FloatVectorOperations::copy(outPtrs[ch], mixBuffer.getReadPointer(ch), len);
                renderClicks(outPtrs, juce::jmin(2, numOutputs), len, pos, true, snap);
                const int64_t left = cin - len;
                countInRemaining.store(left);
                if (left <= 0 && pendingRecordStart.exchange(false))
                {
                    recordedSamples.store(0);
                    recordingActive.store(true);
                    transportState.play();
                }
                liveMidiBlock.clear();
                done += len;
                continue;
            }

            const bool playing = transportState.isPlaying();
            int64_t pos = transportState.getPositionSamples();

            // Any jump in the timeline (seek, loop) -> release held MIDI notes.
            if (playing && lastRenderEnd >= 0 && pos != lastRenderEnd)
                for (auto& p : trackPlayers) p->allNotesOffPending = true;

            bool wrapAfter = false;
            int64_t loopStart = 0;
            if (playing && !offline && transportState.isLooping())
            {
                loopStart = transportState.getLoopStartSample();
                const int64_t loopEnd = transportState.getLoopEndSample();
                if (pos >= loopEnd || pos < loopStart - (int64_t) (currentSampleRate * 0.5) )
                {
                    if (pos >= loopEnd)
                    {
                        pos = loopStart;
                        transportState.setPositionFromAudioThread(pos);
                        for (auto& p : trackPlayers) p->allNotesOffPending = true;
                    }
                }
                if (pos < loopEnd && pos + len > loopEnd)
                {
                    len = (int) (loopEnd - pos);
                    wrapAfter = true;
                }
            }

            renderSegment(inputs, numInputs, done, len, pos, playing, offline, snap, liveMidiBlock);
            liveMidiBlock.clear();   // live events only once per callback

            for (int ch = 0; ch < numOutputs; ++ch)
            {
                if (outputs[ch] == nullptr) continue;
                if (numOutputs == 1)
                {
                    juce::FloatVectorOperations::copy(outputs[0] + done, mixBuffer.getReadPointer(0), len);
                    juce::FloatVectorOperations::add(outputs[0] + done, mixBuffer.getReadPointer(1), len);
                    juce::FloatVectorOperations::multiply(outputs[0] + done, 0.5f, len);
                }
                else if (ch < 2)
                    juce::FloatVectorOperations::copy(outputs[ch] + done, mixBuffer.getReadPointer(ch), len);
            }
            blockPeak = juce::jmax(blockPeak, mixBuffer.getMagnitude(0, 0, len), mixBuffer.getMagnitude(1, 0, len));

            if (!offline && playing && metronomeOn.load())
            {
                float* outPtrs[2] = { outputs[0] ? outputs[0] + done : nullptr, numOutputs > 1 && outputs[1] ? outputs[1] + done : nullptr };
                renderClicks(outPtrs, juce::jmin(2, numOutputs), len, pos, false, snap);
            }

            if (playing)
            {
                if (!offline && recordingActive.load())
                    writeRecording(inputs, numInputs, done, len);

                transportState.advancePosition(len);
                lastRenderEnd = pos + len;

                if (wrapAfter)
                {
                    transportState.setPositionFromAudioThread(loopStart);
                    lastRenderEnd = loopStart;
                    for (auto& p : trackPlayers) p->allNotesOffPending = true;
                    if (recordingActive.load())
                    {
                        const int k = loopPassCount.load();
                        if (k < (int) loopPassEnds.size())
                        {
                            loopPassEnds[(size_t) k] = recordedSamples.load();
                            loopPassCount.store(k + 1);
                        }
                    }
                }
            }
            else
            {
                lastRenderEnd = -1;
            }
            done += len;
        }

        masterOutputPeak.store(blockPeak);
        if (blockPeak > 1.0f)
            masterOutputClipped.store(true);

        if (offline)
            return;

        // ── Auditioning (not recorded, not exported) ─────────────────────────
        if (previewActive.load() && previewTransport.isPlaying())
        {
            previewScratchBuffer.setSize(juce::jmax(1, numOutputs), numSamples, false, false, true);
            juce::AudioSourceChannelInfo previewInfo(&previewScratchBuffer, 0, numSamples);
            previewTransport.getNextAudioBlock(previewInfo);
            for (int ch = 0; ch < numOutputs; ++ch)
                if (outputs[ch] != nullptr)
                    juce::FloatVectorOperations::add(outputs[ch],
                        previewScratchBuffer.getReadPointer(juce::jmin(ch, previewScratchBuffer.getNumChannels() - 1)), numSamples);
            if (!previewTransport.isPlaying())
                previewActive.store(false);
        }

        if (pianoKeyActive.load())
        {
            constexpr float kGain = 0.12f, kAttack = 0.01f, kRelease = 0.004f;
            const bool releasing = pianoKeyReleasing.load();
            for (int s = 0; s < numSamples; ++s)
            {
                if (releasing)
                {
                    pianoKeyEnvelope -= kRelease;
                    if (pianoKeyEnvelope <= 0.0f) { pianoKeyEnvelope = 0.0f; pianoKeyActive.store(false); }
                }
                else if (pianoKeyEnvelope < 1.0f)
                    pianoKeyEnvelope = juce::jmin(1.0f, pianoKeyEnvelope + kAttack);

                const float v = (float) std::sin(pianoKeyPhase) * pianoKeyEnvelope * kGain;
                pianoKeyPhase += pianoKeyPhaseIncrement;
                if (pianoKeyPhase > juce::MathConstants<double>::twoPi) pianoKeyPhase -= juce::MathConstants<double>::twoPi;
                for (int ch = 0; ch < numOutputs; ++ch)
                    if (outputs[ch] != nullptr) outputs[ch][s] += v;
                if (!pianoKeyActive.load()) break;
            }
        }

        // Last-resort protection for the speakers/ears only (exports are never clipped here).
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clip(outputs[ch], outputs[ch], -1.0f, 1.0f, numSamples);
    }

    void StudioAudioEngine::renderSegment(const float* const* inputs, int numInputs, int inputOffset, int n,
                                          int64_t pos, bool playing, bool offline, const PlaySnapshot* snap,
                                          juce::MidiBuffer& liveMidi)
    {
        mixBuffer.clear(0, n);
        clearSendBuses(n);

        bool anySolo = false, anyArmedMidi = false;
        for (auto& p : trackPlayers)
        {
            anySolo = anySolo || p->solo.load();
            anyArmedMidi = anyArmedMidi || (p->type == TrackType::Midi && p->armed.load());
        }
        const int midiTarget = midiInputTrack.load();

        // Regular tracks first, then aux returns in dependency order (so every send and
        // bus output reaches its aux before that aux is processed).
        const bool useOrder = (int) processOrder.size() == trackPlayers.size();
        for (int pass = 0; pass < (useOrder ? 1 : 2); ++pass)
        {
            for (int oi = 0; oi < trackPlayers.size(); ++oi)
            {
                auto& p = *trackPlayers.getReference(useOrder ? processOrder[(size_t) oi] : oi);
                if (!useOrder && p.isAuxTrack != (pass == 1)) continue;
                const TrackPlayData* td = (snap != nullptr && juce::isPositiveAndBelow(p.trackIndex, (int) snap->tracks.size()))
                                              ? &snap->tracks[(size_t) p.trackIndex] : nullptr;
                const bool liveTarget = !offline && !liveMidi.isEmpty()
                                        && (anyArmedMidi ? (p.type == TrackType::Midi && p.armed.load()) : p.trackIndex == midiTarget);
                renderTrack(p, td, inputs, numInputs, inputOffset, n, pos, playing, offline, anySolo, liveMidi, liveTarget);
            }
        }

        if (playing && pos >= 0 && !(offline && offlineSoloTrack >= 0))
        {
            float* mixPtrs[2] = { mixBuffer.getWritePointer(0), mixBuffer.getWritePointer(1) };
            stepSnapshot = snap;
            renderStepSequencer(mixPtrs, 2, n, pos + ((offline && !offlineIncludeMaster) ? 0 : masterChainLatency.load()));
        }
        else if (!playing)
            currentPlayingStep.store(-1);

        // ── Master section ───────────────────────────────────────────────────
        if (!offline || offlineIncludeMaster)
        {
            juce::AudioBuffer<float> view(mixBuffer.getArrayOfWritePointers(), 2, n);
            {
                const juce::ScopedTryLock ml(masterPluginLock);
                if (ml.isLocked())
                    for (auto* plugin : masterPlugins)
                        if (plugin != nullptr)
                        {
                            masterMidi.clear();
                            if (plugin->getTotalNumInputChannels() <= 2 && plugin->getTotalNumOutputChannels() <= 2)
                                plugin->processBlock(view, masterMidi);
                        }
            }
            masterGain.setTargetValue(dbToGain(masterVolumeDbAtomic.load()));
            masterGain.applyGain(view, n);
            if (limiterOn.load())
            {
                juce::dsp::AudioBlock<float> block(view);
                masterLimiter.process(juce::dsp::ProcessContextReplacing<float>(block));
            }
        }
    }

    void StudioAudioEngine::renderTrack(TrackPlayer& p, const TrackPlayData* td, const float* const* inputs, int numInputs,
                                        int inputOffset, int n, int64_t pos, bool playing, bool offline,
                                        bool anySolo, const juce::MidiBuffer& liveMidi, bool isLiveMidiTarget)
    {
        const int chans = p.buf.getNumChannels();
        if (chans < 2 || n > p.buf.getNumSamples())
            return;
        for (int ch = 0; ch < chans; ++ch)
            juce::FloatVectorOperations::clear(p.buf.getWritePointer(ch), n);
        p.midi.clear();

        if (p.allNotesOffPending)
        {
            for (int k = 0; k < 128; ++k)
                if (p.heldNotes[(size_t) k])
                    p.midi.addEvent(juce::MidiMessage::noteOff(1, k), 0);
            p.heldNotes.reset();
            p.midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
            p.allNotesOffPending = false;
        }

        const double sr = currentSampleRate;

        // Plugin latency compensation: read the arrangement ahead by the total latency so the
        // delayed output lines up with the timeline (a freeze render is trimmed instead).
        const bool freezeRender = offline && offlineFreezeTrack == p.trackIndex;
        const int64_t srcPos = freezeRender ? pos
            : pos + trackMaxLatency.load() + ((offline && !offlineIncludeMaster) ? 0 : masterChainLatency.load());

        if (p.isAuxTrack)
        {
            if (juce::isPositiveAndBelow(p.auxInputBusIndex, kNumSendBuses))
            {
                auto& bus = sendBusBuffers[p.auxInputBusIndex];
                for (int ch = 0; ch < 2; ++ch)
                    juce::FloatVectorOperations::copy(p.buf.getWritePointer(ch), bus.getReadPointer(juce::jmin(ch, bus.getNumChannels() - 1)), n);
            }
        }
        else
        {
            if (playing && td != nullptr)
            {
                renderClips(p, *td, n, srcPos, offline);

                const int64_t end = srcPos + n;
                if (!p.frozen.load())
                for (const auto& note : td->notes)
                {
                    if (note.on >= end) break;
                    if (note.off < srcPos) continue;
                    if (note.off < end)
                    {
                        // Only release notes we actually started (not ones skipped by a seek).
                        if (note.on >= srcPos || p.heldNotes[(size_t) note.pitch])
                            p.midi.addEvent(juce::MidiMessage::noteOff(1, note.pitch), (int) (note.off - srcPos));
                        if (note.on < srcPos)
                            p.heldNotes.reset((size_t) note.pitch);
                    }
                    if (note.on >= srcPos)
                    {
                        p.midi.addEvent(juce::MidiMessage::noteOn(1, note.pitch, note.velocity), (int) (note.on - srcPos));
                        if (note.off >= end) p.heldNotes.set((size_t) note.pitch);
                    }
                }
            }

            // Input monitoring through the track (plugins + fader), like any DAW.
            if (!offline && inputs != nullptr && numInputs > 0 && p.type != TrackType::Midi)
            {
                const bool monitor = p.monitorInput.load() || (transportState.isInputMonitoring() && p.armed.load());
                if (monitor)
                {
                    const int c0 = juce::jlimit(0, numInputs - 1, p.inputChannel.load());
                    const int c1 = (p.isStereo.load() && c0 + 1 < numInputs) ? c0 + 1 : c0;
                    if (inputs[c0] != nullptr) juce::FloatVectorOperations::add(p.buf.getWritePointer(0), inputs[c0] + inputOffset, n);
                    if (inputs[c1] != nullptr) juce::FloatVectorOperations::add(p.buf.getWritePointer(1), inputs[c1] + inputOffset, n);
                }
            }

            if (isLiveMidiTarget)
            {
                for (const auto meta : liveMidi)
                {
                    const auto msg = meta.getMessage();
                    p.midi.addEvent(msg, 0);
                    if (msg.isNoteOn()) p.heldNotes.set((size_t) msg.getNoteNumber());
                    else if (msg.isNoteOff()) p.heldNotes.reset((size_t) msg.getNoteNumber());

                    if (recordingActive.load() && playing && (msg.isNoteOnOrOff() || msg.isController() || msg.isPitchWheel()))
                    {
                        int s1, n1, s2, n2;
                        recMidiFifo.prepareToWrite(1, s1, n1, s2, n2);
                        if (n1 + n2 > 0)
                        {
                            auto& e = recMidiData[(size_t) (n1 > 0 ? s1 : s2)];
                            e.timelinePos = pos;
                            e.trackIndex = p.trackIndex;
                            std::memset(e.data, 0, 3);
                            std::memcpy(e.data, msg.getRawData(), (size_t) juce::jmin(3, msg.getRawDataSize()));
                            recMidiFifo.finishedWrite(1);
                        }
                    }
                }
            }
        }

        // ── Inserts ──────────────────────────────────────────────────────────
        if (!p.frozen.load())
        {
            juce::AudioBuffer<float> view(p.buf.getArrayOfWritePointers(), chans, n);
            for (int i = 0; i < p.pluginChain.size(); ++i)
                if (auto& plugin = p.pluginChain.getReference(i))
                    if (!p.isPluginBypassed(i))
                        plugin->processBlock(view, p.midi);
        }

        const bool freezing = offline && offlineFreezeTrack == p.trackIndex;
        if (freezing)
        {
            // Freeze render: capture the inserts' output only (EQ, fader and sends stay live).
            mixBuffer.addFrom(0, 0, p.buf, 0, 0, n);
            mixBuffer.addFrom(1, 0, p.buf, 1, 0, n);
            return;
        }

        if (p.pdcDelaySamples > 0)
        {
            juce::dsp::AudioBlock<float> block(p.buf.getArrayOfWritePointers(), (size_t) chans, 0, (size_t) n);
            p.pdcDelayLine.process(juce::dsp::ProcessContextReplacing<float>(block));
        }

        // ── Channel EQ ───────────────────────────────────────────────────────
        if (p.eqDirty.exchange(false))
        {
            for (int b = 0; b < TrackPlayer::kNumEQBands; ++b)
            {
                auto& band = p.eqBands[(size_t) b];
                const double freq = juce::jlimit(10.0, sr * 0.45, (double) band.freq.load());
                const double gainDb = band.gainDb.load();
                const double q = juce::jmax(0.1, (double) band.q.load());
                const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
                const double cosw = std::cos(w0), sinw = std::sin(w0), alpha = sinw / (2.0 * q);
                double b0, b1, b2, a0, a1, a2;
                if (b == 0)      { b0 = (1 + cosw) / 2; b1 = -(1 + cosw); b2 = (1 + cosw) / 2; a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha; }
                else if (b == TrackPlayer::kNumEQBands - 1) { b0 = (1 - cosw) / 2; b1 = 1 - cosw; b2 = (1 - cosw) / 2; a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha; }
                else
                {
                    const double A = std::pow(10.0, gainDb / 40.0);
                    b0 = 1 + alpha * A; b1 = -2 * cosw; b2 = 1 - alpha * A;
                    a0 = 1 + alpha / A; a1 = -2 * cosw; a2 = 1 - alpha / A;
                }
                band.b0 = b0 / a0; band.b1 = b1 / a0; band.b2 = b2 / a0; band.a1 = a1 / a0; band.a2 = a2 / a0;
            }
        }
        for (auto& band : p.eqBands)
        {
            if (!band.enabled.load()) continue;
            float* L = p.buf.getWritePointer(0);
            float* R = p.buf.getWritePointer(1);
            for (int i = 0; i < n; ++i)
            {
                const double inL = L[i], outL = band.b0 * inL + band.z1L;
                band.z1L = band.b1 * inL - band.a1 * outL + band.z2L;
                band.z2L = band.b2 * inL - band.a2 * outL;
                L[i] = (float) outL;
                const double inR = R[i], outR = band.b0 * inR + band.z1R;
                band.z1R = band.b1 * inR - band.a1 * outR + band.z2R;
                band.z2R = band.b2 * inR - band.a2 * outR;
                R[i] = (float) outR;
            }
        }

        // ── Fader, pan, automation ───────────────────────────────────────────
        float vol = p.volumeDb.load(), pan = p.pan.load();
        float sendLv[TrackPlayer::kMaxSends];
        for (int i = 0; i < TrackPlayer::kMaxSends; ++i) sendLv[i] = p.sendLevels[i];

        if (td != nullptr && playing && td->automationMode != 4 && !p.writingAutomation.load())
        {
            const double t = (double) srcPos / sr;
            for (const auto& lane : td->lanes)
            {
                const float value = evaluateAutomationLane(lane, t);
                if (lane.parameterId == "volume") vol = value;
                else if (lane.parameterId == "pan") pan = juce::jlimit(-1.0f, 1.0f, value);
                else if (lane.parameterId.startsWith("send"))
                {
                    const int si = lane.parameterId.substring(4).getIntValue() - 1;
                    if (juce::isPositiveAndBelow(si, TrackPlayer::kMaxSends)) sendLv[si] = value;
                }
                else if (lane.parameterId.startsWith("plugin:"))
                {
                    const int c1 = lane.parameterId.indexOfChar(':');
                    const int c2 = lane.parameterId.indexOfChar(c1 + 1, ':');
                    if (c2 > c1)
                    {
                        const int slot = lane.parameterId.substring(c1 + 1, c2).getIntValue();
                        const int paramIndex = lane.parameterId.substring(c2 + 1).getIntValue();
                        if (auto* plugin = p.getPlugin(slot))
                        {
                            auto& params = plugin->getParameters();
                            if (juce::isPositiveAndBelow(paramIndex, params.size()))
                            {
                                const float nv = juce::jlimit(0.0f, 1.0f, value);
                                if (std::abs(params[paramIndex]->getValue() - nv) > 1.0e-5f)
                                    params[paramIndex]->setValue(nv);
                            }
                        }
                    }
                }
            }
        }
        p.displayVolumeDb.store(vol);
        p.displayPan.store(pan);

        bool audible = !p.muted.load() && !(anySolo && !p.solo.load());
        if (offline && offlineSoloTrack >= 0)
            audible = (p.trackIndex == offlineSoloTrack);

        // Pre-fader sends (post inserts/EQ)
        if (audible && !p.isAuxTrack)
            for (int si = 0; si < TrackPlayer::kMaxSends; ++si)
                if (p.sendPreFader[si] && juce::isPositiveAndBelow(p.sendBusIndex[si], kNumSendBuses))
                {
                    const float g = dbToGain(sendLv[si]);
                    if (g <= 0.0f) continue;
                    auto& bus = sendBusBuffers[p.sendBusIndex[si]];
                    for (int ch = 0; ch < 2; ++ch) bus.addFrom(ch, 0, p.buf, ch, 0, n, g);
                }

        const float g = audible ? dbToGain(vol) : 0.0f;
        const float gL = g * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        const float gR = g * (pan >= 0.0f ? 1.0f : 1.0f + pan);
        if (p.lastGainL < 0.0f) { p.lastGainL = gL; p.lastGainR = gR; }   // fresh start: no ramp
        p.buf.applyGainRamp(0, 0, n, p.lastGainL, gL);
        p.buf.applyGainRamp(1, 0, n, p.lastGainR, gR);
        p.lastGainL = gL;
        p.lastGainR = gR;

        // Post-fader sends
        if (audible && !p.isAuxTrack)
            for (int si = 0; si < TrackPlayer::kMaxSends; ++si)
                if (!p.sendPreFader[si] && juce::isPositiveAndBelow(p.sendBusIndex[si], kNumSendBuses))
                {
                    const float sg = dbToGain(sendLv[si]);
                    if (sg <= 0.0f) continue;
                    auto& bus = sendBusBuffers[p.sendBusIndex[si]];
                    for (int ch = 0; ch < 2; ++ch) bus.addFrom(ch, 0, p.buf, ch, 0, n, sg);
                }

        // Meters show what the track actually sends to the mix.
        p.peakLevelLeft.store(p.buf.getMagnitude(0, 0, n));
        p.peakLevelRight.store(p.buf.getMagnitude(1, 0, n));

        const int outBus = p.outputBusIndex.load();
        auto& dest = (juce::isPositiveAndBelow(outBus, kNumSendBuses) && !(offline && offlineSoloTrack >= 0)
                      && !(p.isAuxTrack && outBus == p.auxInputBusIndex))
                         ? sendBusBuffers[outBus] : mixBuffer;
        dest.addFrom(0, 0, p.buf, 0, 0, n);
        dest.addFrom(1, 0, p.buf, 1, 0, n);
    }

    void StudioAudioEngine::renderClips(TrackPlayer& p, const TrackPlayData& td, int n, int64_t pos, bool offline)
    {
        const double sr = currentSampleRate;
        float* dst[2] = { p.buf.getWritePointer(0), p.buf.getWritePointer(1) };
        float* tmp[2] = { p.clipTmp.getWritePointer(0), p.clipTmp.getWritePointer(1) };
        const int tmpCap = p.clipTmp.getNumSamples();
        const int64_t end = pos + n;

        for (const auto& c : td.clips)
        {
            if (c.start >= end) break;
            if (c.start + c.length <= pos || c.src == nullptr) continue;

            const int64_t s0 = juce::jmax(pos, c.start);
            const int64_t s1 = juce::jmin(end, c.start + c.length);
            const int count = (int) (s1 - s0);
            const int off = (int) (s0 - pos);
            const int64_t rel = s0 - c.start;
            auto& src = *c.src;
            const double ratio = src.getFileSampleRate() / sr;
            const bool hasFades = c.fadeIn > 0 || c.fadeOut > 0;

            if (std::abs(ratio - 1.0) < 1.0e-6)
            {
                const int64_t fpos = c.srcStart + rel;
                if (!src.read(tmp, 2, fpos, count) && offline)
                {
                    src.prefetchBlocking(fpos, 4000);
                    src.read(tmp, 2, fpos, count);
                }
                for (int ch = 0; ch < 2; ++ch)
                {
                    if (!hasFades)
                        juce::FloatVectorOperations::addWithMultiply(dst[ch] + off, tmp[ch], c.gain, count);
                    else
                        for (int i = 0; i < count; ++i)
                            dst[ch][off + i] += tmp[ch][i] * c.gain
                                                * fadeGainAt(c.fadeOffset + rel + i, c.fullLength, c.fadeIn, c.fadeOut);
                }
            }
            else
            {
                const double fstart = (double) (c.srcStart + rel) * ratio;
                const int64_t i0 = (int64_t) std::floor(fstart) - 1;
                const int need = juce::jmin(tmpCap, (int) std::ceil(count * ratio) + 5);
                if (!src.read(tmp, 2, i0, need) && offline)
                {
                    src.prefetchBlocking(juce::jmax((int64_t) 0, i0), 4000);
                    src.read(tmp, 2, i0, need);
                }
                for (int i = 0; i < count; ++i)
                {
                    const double x = fstart + i * ratio - (double) i0;   // >= 1
                    const int idx = (int) x;
                    if (idx + 2 >= need) break;
                    const float t = (float) (x - idx);
                    const float gn = c.gain * (hasFades ? fadeGainAt(c.fadeOffset + rel + i, c.fullLength, c.fadeIn, c.fadeOut) : 1.0f);
                    dst[0][off + i] += hermite(tmp[0] + idx - 1, t) * gn;
                    dst[1][off + i] += hermite(tmp[1] + idx - 1, t) * gn;
                }
            }
        }
    }

    void StudioAudioEngine::renderClicks(float* const* out, int numOut, int n, int64_t pos, bool countingIn, const PlaySnapshot* snap)
    {
        const double sr = currentSampleRate;
        const float vol = metronomeGain.load();
        int triggers[16]; bool accents[16]; int numTriggers = 0;

        if (countingIn)
        {
            const double bpm = transportState.getTempo();
            const int num = transportState.getTimeSigNumerator();
            const double beatLen = 60.0 / bpm * sr * (4.0 / transportState.getTimeSigDenominator());
            const int64_t elapsed = countInTotal - countInRemaining.load();
            int64_t k = (int64_t) std::ceil((double) elapsed / beatLen - 1.0e-9);
            for (;; ++k)
            {
                const int64_t at = (int64_t) std::llround(k * beatLen) - elapsed;
                if (at >= n || numTriggers >= 16) break;
                if (at >= 0) { triggers[numTriggers] = (int) at; accents[numTriggers++] = (k % num) == 0; }
            }
        }
        else if (snap != nullptr)
        {
            const double b0 = snap->secondsToBeats((double) pos / sr);
            int64_t k = (int64_t) std::ceil(b0 - 1.0e-9);
            for (;; ++k)
            {
                const int64_t at = (int64_t) std::llround(snap->beatsToSeconds((double) k) * sr) - pos;
                if (at >= n || numTriggers >= 16) break;
                if (at >= 0) { triggers[numTriggers] = (int) at; accents[numTriggers++] = (k % juce::jmax(1, snap->timeSigNum)) == 0; }
            }
        }

        const int clickLen = (int) (sr * 0.035);
        int next = 0;
        for (int i = 0; i < n; ++i)
        {
            if (next < numTriggers && triggers[next] == i)
            {
                clickSamplesLeft = clickLen;
                clickFreq = accents[next] ? 2000.0 : 1200.0;
                clickGain = (accents[next] ? 0.9f : 0.6f) * vol;
                clickPhase = 0.0;
                ++next;
            }
            if (clickSamplesLeft > 0)
            {
                const float env = (float) clickSamplesLeft / (float) clickLen;
                const float v = (float) std::sin(clickPhase) * clickGain * env * env;
                clickPhase += juce::MathConstants<double>::twoPi * clickFreq / sr;
                --clickSamplesLeft;
                for (int ch = 0; ch < numOut; ++ch)
                    if (out[ch] != nullptr) out[ch][i] += v;
            }
        }
    }

    void StudioAudioEngine::writeRecording(const float* const* inputs, int numInputs, int inputOffset, int n)
    {
        const juce::SpinLock::ScopedTryLockType tl(recordLock);
        if (!tl.isLocked())
            return;
        if (recordedSamples.load() == 0)
            recordStartPos.store(transportState.getPositionSamples());

        for (auto* take : recordTakes)
        {
            if (take == nullptr || take->writer == nullptr) continue;
            const float* chans[2];
            for (int c = 0; c < take->numChannels; ++c)
            {
                const int idx = take->firstInput + c;
                chans[c] = (inputs != nullptr && idx < numInputs && inputs[idx] != nullptr)
                               ? inputs[idx] + inputOffset : silenceBuffer.getReadPointer(0);
            }
            take->writer->write(chans, n);
        }
        recordedSamples.fetch_add(n);
    }

    // Step sequencer (Beat window). Rendered in sub-blocks split exactly at step boundaries,
    // so steps trigger sample-accurately (and step 1 fires when playback starts).
    void StudioAudioEngine::renderStepSequencer(float* const* outBase, int numOutputChannels, int totalSamples, int64_t positionSamples)
    {
        const double sr  = currentSampleRate;
        const double bpm = transportState.getTempo();
        if (sr <= 0.0 || bpm <= 0.0 || totalSamples <= 0)
            return;

        const int   activeSteps  = juce::jmax(1, stepSeq.activeStepCount);
        const float swingAmount  = stepSeq.swing;
        const double stepsPerSec = (bpm / 60.0) * 4.0;
        const double stepDurationSamples = sr / stepsPerSec;

        auto stepWithSwing = [&](double rawPos) -> double {
            const double beatIndex = std::floor(rawPos / 2.0);
            const double posInBeat = rawPos - beatIndex * 2.0;
            const double evenEnd = 1.0 + swingAmount * 0.33;
            if (posInBeat < evenEnd)
                return beatIndex * 2.0 + posInBeat / evenEnd;
            return beatIndex * 2.0 + 1.0 + (posInBeat - evenEnd) / (2.0 - evenEnd);
        };
        // Steps are 1/16 notes on the musical grid, so the pattern follows tempo changes.
        const PlaySnapshot* sn = stepSnapshot;
        auto stepAt = [&](int64_t p)
        {
            const double raw = (sn != nullptr && !sn->tempoMap.empty())
                                   ? sn->secondsToBeats((double) p / sr) / (sn->timeSigDen / 4.0) * 4.0
                                   : (double) p / sr * stepsPerSec;
            return (int) stepWithSwing(raw) % activeSteps;
        };

        float* outPtrs[8] = {};
        const int nOut = juce::jmin(numOutputChannels, 8);

        auto renderSeg = [&](int from, int to, int currentStep, int prevStep)
        {
            const int numSamples = to - from;
            for (int ch = 0; ch < nOut; ++ch)
                outPtrs[ch] = outBase[ch] != nullptr ? outBase[ch] + from : nullptr;
            float* const* outputChannelData = outPtrs;
            const int numOutputChannels = nOut;
            juce::ignoreUnused(numOutputChannels, stepDurationSamples);
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
        };

        int prevCarry = (positionSamples == stepSeqExpectedPos) ? stepSeqLastStep : -1;
        int curStep = stepAt(positionSamples);
        int segStart = 0;
        for (int s = 1; s <= totalSamples; ++s)
        {
            const int st = s < totalSamples ? stepAt(positionSamples + s) : -2;
            if (s == totalSamples || st != curStep)
            {
                renderSeg(segStart, s, curStep, prevCarry);
                prevCarry = curStep;
                curStep = st;
                segStart = s;
            }
        }
        currentPlayingStep.store(prevCarry);
        stepSeqLastStep = prevCarry;
        stepSeqExpectedPos = positionSamples + totalSamples;
    }
}
