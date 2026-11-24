#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "MatchEngine.h"
#include "../PluginLibrary.h"
#include "../RackPresets.h"

namespace nr::match
{

/** Everything Nova Match keeps inside one Nova Rack instance: the two vocals, capturing
    from the track, the background match, and A/B playback.

    Threads: load/capture-stop/start/cancel/preview-select run on the message thread;
    pushCapture() and renderPreview() are called from the audio thread. */
class MatchSession : private juce::Thread
{
public:
    explicit MatchSession (PluginLibrary& lib);
    ~MatchSession() override;

    struct Clip
    {
        juce::AudioBuffer<float> audio;
        double rate = 0.0;
        juce::String name, key;
        std::vector<float> peaks;      // 240 points for the waveform
        bool empty() const { return audio.getNumSamples() == 0; }
    };

    // Inputs
    juce::String loadFile (bool reference, const juce::File&);    // "" = ok, else an error
    void startCapture();
    void stopCapture();                                             // the capture becomes "your vocal"
    bool isCapturing() const noexcept { return capturing.load(); }
    bool takeCaptureFull() noexcept { return captureFull.exchange (false); }   // the 30 s filled up
    double captureSeconds() const noexcept { return (double) captureWrite.load() / juce::jmax (1.0, hostRate); }
    static constexpr double kCaptureSeconds = 30.0;

    // Audio thread
    void prepare (double sampleRate, int numChannels);
    void pushCapture (const juce::AudioBuffer<float>& input) noexcept;
    bool renderPreview (juce::AudioBuffer<float>& io) noexcept;    // true = the output was replaced

    // Matching
    juce::String start (const std::array<bool, kNumStages>& locked);   // "" = started
    void cancel();
    bool isRunning() const noexcept { return isThreadRunning(); }
    float progress() const noexcept { return progressValue.load(); }
    juce::String status() const;

    // Results (message thread)
    const MatchResult* result() const noexcept { return lastResult.get(); }
    RackData buildRack (float strength) const;

    // A/B: 0 = off, 1 = reference, 2 = your raw vocal, 3 = matched
    void setPreview (int mode);
    int getPreview() const noexcept { return previewMode.load(); }

    const Clip& reference() const noexcept { return ref; }
    const Clip& raw() const noexcept { return yours; }

    std::function<void()> onFinished;   // message thread

private:
    void run() override;
    void finishClip (Clip&);
    void preparePreviews();

    PluginLibrary& library;
    Clip ref, yours;

    // Capture (audio thread writes, message thread reads after stop)
    juce::AudioBuffer<float> captureBuf;
    std::atomic<int> captureWrite { 0 };
    std::atomic<bool> capturing { false }, captureFull { false };
    double hostRate = 48000.0;

    // Background match
    MatchEngine engine;
    std::array<bool, kNumStages> lockedStages {};
    std::unique_ptr<MatchResult> pending, lastResult;
    std::atomic<float> progressValue { 0.f };
    mutable juce::CriticalSection statusLock;
    juce::String statusText;
    std::atomic<bool> cancelFlag { false };

    // Preview (built on the message thread at the host rate, loudness-matched)
    juce::CriticalSection previewLock;
    std::array<juce::AudioBuffer<float>, 3> previews;
    std::atomic<int> previewMode { 0 };
    int previewPos = 0;

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

} // namespace nr::match
