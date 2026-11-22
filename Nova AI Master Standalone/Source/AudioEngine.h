#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <functional>

#include "NovaAIMasterEngine.h"
#include "BufferAudioSource.h"

// Owns the standalone app's whole import -> analyze -> master -> preview ->
// export pipeline. Uses the SAME NovaAIMasterEngine the plugin uses
// (spec sections 3, 38 - one shared core, not parallel implementations).
//
// Threading: import and analyze+master both run on a background thread
// (juce::Thread::launch), never blocking the message thread - completion
// callbacks fire back on the message thread via juce::MessageManager::
// callAsync. Only one background job runs at a time (guarded by `busy`);
// callers should disable the relevant UI controls while isBusy().
class NovaAIMasterAudioEngine
{
public:
    NovaAIMasterAudioEngine();
    ~NovaAIMasterAudioEngine();

    // ---- Import (spec section 5: WAV/AIFF) ----
    void importFile (const juce::File& file);
    std::function<void (bool success, juce::String errorMessage)> onImportFinished;

    bool hasImportedAudio() const noexcept  { return importedBuffer.getNumSamples() > 0; }
    juce::String getImportedFileName() const noexcept { return importedFileName; }
    double getImportedSampleRate() const noexcept      { return importedSampleRate; }
    int getImportedBitDepth() const noexcept            { return importedBitDepth; }
    double getImportedLengthSeconds() const noexcept
    {
        return importedSampleRate > 0.0 ? (double) importedBuffer.getNumSamples() / importedSampleRate : 0.0;
    }
    bool getImportedIsStereo() const noexcept { return importedBuffer.getNumChannels() >= 2; }

    // ---- Analyze + decide + master, one background job (spec section 27) ----
    void requestMaster (const NovaMasterUserPrefs& prefs);
    std::function<void (const NovaAudioProfile&, const NovaMasteringPlan&)> onMasterFinished;
    std::function<void (const juce::String& errorMessage)> onMasterFailed;

    bool isBusy() const noexcept          { return busy.load(); }
    bool hasMasteredAudio() const noexcept { return masteredBuffer.getNumSamples() > 0; }

    // ---- Preview (spec section 23: level-matched original/master comparison) ----
    void playOriginal();
    void playMastered();
    void stopPlayback();
    bool isPlaying() const noexcept;
    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    // ---- Export (spec section 28) ----
    enum class ExportFormat { Wav16, Wav24, Aiff24 };
    bool exportMastered (const juce::File& outputFile, ExportFormat format, juce::String& errorMessageOut);

    // ---- Waveform display (spec section 25) ----
    // Downsampled peak envelope (0-1 per point) of the real imported/
    // mastered audio, for the UI to draw - not synthetic/decorative data.
    std::vector<float> getWaveformPeaks (bool mastered, int numPoints) const;

    const NovaAudioProfile& getLastProfile() const noexcept { return lastProfile; }

private:
    void runImportJob (juce::File file);
    void runMasterJob (NovaMasterUserPrefs prefs);
    void startPlayback (const juce::AudioBuffer<float>& bufferToPlay);

    juce::AudioFormatManager formatManager;

    juce::AudioBuffer<float> importedBuffer;
    juce::String importedFileName;
    double importedSampleRate = 44100.0;
    int    importedBitDepth = 24;

    juce::AudioBuffer<float> masteredBuffer;
    NovaAudioProfile lastProfile;
    NovaMasteringPlan lastPlan;

    std::atomic<bool> busy { false };

    // Playback stack: AudioDeviceManager -> AudioSourcePlayer -> AudioTransportSource -> BufferAudioSource.
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<BufferAudioSource> currentPlaybackSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAIMasterAudioEngine)
};
