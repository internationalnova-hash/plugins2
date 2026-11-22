#include "AudioEngine.h"
#include <cmath>

NovaAIMasterAudioEngine::NovaAIMasterAudioEngine()
{
    formatManager.registerBasicFormats(); // WAV, AIFF (spec section 5)

    deviceManager.initialiseWithDefaultDevices (0, 2);
    sourcePlayer.setSource (&transportSource);
    deviceManager.addAudioCallback (&sourcePlayer);
}

NovaAIMasterAudioEngine::~NovaAIMasterAudioEngine()
{
    stopPlayback();
    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
}

// ---- Import ----

void NovaAIMasterAudioEngine::importFile (const juce::File& file)
{
    if (busy.exchange (true)) return; // already busy - caller should have disabled the control
    juce::Thread::launch ([this, file] { runImportJob (file); });
}

void NovaAIMasterAudioEngine::runImportJob (juce::File file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        juce::MessageManager::callAsync ([this] {
            busy.store (false);
            if (onImportFinished) onImportFinished (false, "Unsupported or unreadable audio file.");
        });
        return;
    }

    const int numChannels = (int) reader->numChannels;
    const int numSamples  = (int) reader->lengthInSamples;

    juce::AudioBuffer<float> newBuffer (juce::jmax (1, numChannels), juce::jmax (0, numSamples));
    if (numSamples > 0)
        reader->read (&newBuffer, 0, numSamples, 0, true, true);

    importedBuffer      = std::move (newBuffer);
    importedFileName    = file.getFileName();
    importedSampleRate  = reader->sampleRate;
    importedBitDepth    = (int) reader->bitsPerSample;
    masteredBuffer.setSize (0, 0); // a fresh import invalidates any previous master

    juce::MessageManager::callAsync ([this] {
        busy.store (false);
        if (onImportFinished) onImportFinished (true, {});
    });
}

// ---- Analyze + decide + master ----

void NovaAIMasterAudioEngine::requestMaster (const NovaMasterUserPrefs& prefs)
{
    if (! hasImportedAudio())
    {
        juce::MessageManager::callAsync ([this] {
            if (onMasterFailed) onMasterFailed ("No mix imported yet.");
        });
        return;
    }

    if (busy.exchange (true)) return;
    juce::Thread::launch ([this, prefs] { runMasterJob (prefs); });
}

void NovaAIMasterAudioEngine::runMasterJob (NovaMasterUserPrefs prefs)
{
    constexpr int kOfflineBlockSize = 4096; // offline: faster-than-realtime, not tied to any device block size

    NovaAIMasterEngine engine;
    juce::dsp::ProcessSpec spec { importedSampleRate, (juce::uint32) kOfflineBlockSize,
                                  (juce::uint32) importedBuffer.getNumChannels() };
    engine.prepare (spec);

    // Analysis engine has no block-size assumption of its own (each
    // analyzer loops internally regardless of chunk size) - the whole file
    // in one call is fine and is exactly the "faster than realtime, don't
    // require real-time playback to analyze" advantage spec section 44
    // calls for.
    engine.analyze (importedBuffer);
    engine.finishAnalysis();

    const NovaMasteringPlan plan = engine.decide (prefs);
    engine.setPlan (plan);

    juce::AudioBuffer<float> newMastered;
    newMastered.makeCopyOf (importedBuffer);

    // The DSP chain's internal oversamplers WERE prepared for
    // kOfflineBlockSize, so processing must be chunked to that size (unlike
    // analysis, which has no such constraint).
    for (int start = 0; start < newMastered.getNumSamples(); start += kOfflineBlockSize)
    {
        const int len = juce::jmin (kOfflineBlockSize, newMastered.getNumSamples() - start);
        juce::AudioBuffer<float> chunk (newMastered.getArrayOfWritePointers(), newMastered.getNumChannels(), start, len);
        engine.process (chunk);
    }

    masteredBuffer = std::move (newMastered);
    lastProfile = engine.getProfile();
    lastPlan = plan;

    juce::MessageManager::callAsync ([this] {
        busy.store (false);
        if (onMasterFinished) onMasterFinished (lastProfile, lastPlan);
    });
}

// ---- Preview ----

void NovaAIMasterAudioEngine::startPlayback (const juce::AudioBuffer<float>& bufferToPlay)
{
    stopPlayback();
    currentPlaybackSource = std::make_unique<BufferAudioSource> (bufferToPlay);
    transportSource.setSource (currentPlaybackSource.get(), 0, nullptr,
                               importedSampleRate, bufferToPlay.getNumChannels());
    transportSource.setPosition (0.0);
    transportSource.start();
}

void NovaAIMasterAudioEngine::playOriginal()
{
    if (! hasImportedAudio()) return;
    transportSource.setGain (1.0f);
    startPlayback (importedBuffer);
}

void NovaAIMasterAudioEngine::playMastered()
{
    if (! hasMasteredAudio()) return;

    // Level-matched comparison (spec section 23): "do not make the
    // mastered version appear better simply because it is louder." Trim
    // the MASTER's preview-only playback gain so it plays at roughly the
    // same perceived loudness as the original - the exported/stored
    // masteredBuffer itself is untouched, this only affects this preview.
    float previewGainDb = 0.f;
    {
        NovaLoudnessAnalyzer probe;
        probe.prepare (importedSampleRate);
        probe.process (masteredBuffer);
        probe.finalize();
        const float masteredLufs = probe.getIntegratedLufs();
        if (masteredLufs > -60.f && lastProfile.integratedLufs > -60.f)
            previewGainDb = juce::jlimit (-24.f, 24.f, lastProfile.integratedLufs - masteredLufs);
    }

    transportSource.setGain (juce::Decibels::decibelsToGain (previewGainDb));
    startPlayback (masteredBuffer);
}

void NovaAIMasterAudioEngine::stopPlayback()
{
    transportSource.stop();
    transportSource.setSource (nullptr);
    currentPlaybackSource.reset();
}

bool NovaAIMasterAudioEngine::isPlaying() const noexcept
{
    return transportSource.isPlaying();
}

// ---- Export ----

bool NovaAIMasterAudioEngine::exportMastered (const juce::File& outputFile, ExportFormat format, juce::String& errorMessageOut)
{
    if (! hasMasteredAudio())
    {
        errorMessageOut = "No mastered audio to export.";
        return false;
    }

    const int bitDepth = (format == ExportFormat::Wav16) ? 16 : 24;

    std::unique_ptr<juce::AudioFormat> audioFormat;
    if (format == ExportFormat::Aiff24)
        audioFormat = std::make_unique<juce::AiffAudioFormat>();
    else
        audioFormat = std::make_unique<juce::WavAudioFormat>();

    outputFile.deleteFile();
    std::unique_ptr<juce::FileOutputStream> outStream (outputFile.createOutputStream());
    if (outStream == nullptr)
    {
        errorMessageOut = "Could not create output file.";
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (
        audioFormat->createWriterFor (outStream.get(), importedSampleRate,
                                      (unsigned int) masteredBuffer.getNumChannels(),
                                      (unsigned int) bitDepth, {}, 0));
    if (writer == nullptr)
    {
        errorMessageOut = "Could not create audio writer for this format.";
        return false;
    }
    outStream.release(); // writer now owns it

    // Dither when reducing to a finite bit depth (spec section 28) - TPDF
    // (sum of two uniform randoms) dithered at 1 LSB of the target depth,
    // then hard-clamped as a final safety net against unintentional
    // clipping on export.
    juce::AudioBuffer<float> ditherBuffer;
    ditherBuffer.makeCopyOf (masteredBuffer);
    {
        const float lsb = 1.0f / (float) (1 << (bitDepth - 1));
        juce::Random rng;
        for (int ch = 0; ch < ditherBuffer.getNumChannels(); ++ch)
        {
            auto* d = ditherBuffer.getWritePointer (ch);
            for (int i = 0; i < ditherBuffer.getNumSamples(); ++i)
            {
                const float dither = (rng.nextFloat() - rng.nextFloat()) * lsb; // triangular PDF
                d[i] = juce::jlimit (-1.f, 1.f, d[i] + dither);
            }
        }
    }

    const bool ok = writer->writeFromAudioSampleBuffer (ditherBuffer, 0, ditherBuffer.getNumSamples());
    writer.reset(); // flush + close

    if (! ok)
    {
        errorMessageOut = "Failed to write audio data.";
        return false;
    }
    return true;
}

std::vector<float> NovaAIMasterAudioEngine::getWaveformPeaks (bool mastered, int numPoints) const
{
    const auto& buffer = mastered ? masteredBuffer : importedBuffer;
    std::vector<float> peaks ((size_t) juce::jmax (1, numPoints), 0.f);

    const int N = buffer.getNumSamples();
    if (N == 0) return peaks;

    const int nCh = buffer.getNumChannels();
    const int samplesPerPoint = juce::jmax (1, N / numPoints);

    for (int p = 0; p < numPoints; ++p)
    {
        const int start = p * samplesPerPoint;
        const int len = juce::jmin (samplesPerPoint, N - start);
        if (len <= 0) break;

        float peak = 0.f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, start, len));

        peaks[(size_t) p] = peak;
    }

    return peaks;
}
