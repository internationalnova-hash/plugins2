#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "AlignEngine.h"

namespace NovaAlign
{
    // Runs AlignEngine::alignTargetClipToBuffer on a background juce::Thread so
    // the (potentially slow, file-I/O-bound) analysis+render never blocks the
    // message thread or the audio thread. The result is handed off to the
    // audio thread via an atomically-swapped shared_ptr guarded by a
    // juce::SpinLock (short, non-blocking critical section - safe to read from
    // processBlock).
    class AlignJobRunner
    {
    public:
        struct Result
        {
            bool success = false;
            juce::String error;
            int64_t shiftSamples = 0;
        };

        // onComplete is called on the message thread (via MessageManager::callAsync)
        // once the background job finishes - never call UI/WebView code directly
        // from the background thread itself.
        using CompletionCallback = std::function<void (Result)>;

        void requestAlign (const juce::File& guideFile,
                           const juce::File& targetFile,
                           const AlignEngine::AlignSettings& settings,
                           CompletionCallback onComplete);

        bool isBusy() const noexcept { return busy.load(); }

        // Real-time-safe: call from the audio thread. Returns nullptr if no
        // aligned result exists yet.
        std::shared_ptr<const juce::AudioBuffer<float>> getAlignedBuffer() const noexcept;
        double getAlignedSampleRate() const noexcept { return alignedSampleRate.load(); }

    private:
        void setAlignedBuffer (std::shared_ptr<const juce::AudioBuffer<float>> newBuffer, double sampleRate);

        std::atomic<bool> busy { false };
        std::atomic<double> alignedSampleRate { 44100.0 };

        juce::SpinLock bufferLock;
        std::shared_ptr<const juce::AudioBuffer<float>> alignedBuffer; // guarded by bufferLock
    };
}
