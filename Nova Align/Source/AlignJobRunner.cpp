#include "AlignJobRunner.h"

#include <juce_events/juce_events.h>

namespace NovaAlign
{
    void AlignJobRunner::requestAlign (const juce::File& guideFile,
                                       const juce::File& targetFile,
                                       const AlignEngine::AlignSettings& settings,
                                       CompletionCallback onComplete)
    {
        if (busy.exchange (true))
            return;

        juce::Thread::launch ([this, guideFile, targetFile, settings, onComplete]
        {
            auto buffer = std::make_shared<juce::AudioBuffer<float>>();
            double sampleRate = 44100.0;
            int64_t shiftSamples = 0;

            const bool ok = AlignEngine::alignTargetClipToBuffer (guideFile, targetFile, settings,
                                                                   *buffer, sampleRate, shiftSamples);

            if (ok)
                setAlignedBuffer (buffer, sampleRate);

            Result result;
            result.success = ok;
            result.error = ok ? juce::String() : "Alignment failed - check that Guide and Target are loaded and share the same sample rate.";
            result.shiftSamples = shiftSamples;

            juce::MessageManager::callAsync ([this, result, onComplete]
            {
                busy.store (false);
                if (onComplete)
                    onComplete (result);
            });
        });
    }

    void AlignJobRunner::setAlignedBuffer (std::shared_ptr<const juce::AudioBuffer<float>> newBuffer, double sampleRate)
    {
        const juce::SpinLock::ScopedLockType lock (bufferLock);
        alignedBuffer = std::move (newBuffer);
        alignedSampleRate.store (sampleRate);
    }

    std::shared_ptr<const juce::AudioBuffer<float>> AlignJobRunner::getAlignedBuffer() const noexcept
    {
        const juce::SpinLock::ScopedLockType lock (bufferLock);
        return alignedBuffer;
    }
}
