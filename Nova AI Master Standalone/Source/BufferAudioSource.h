#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

// Plays back an in-memory AudioBuffer<float> - JUCE's AudioTransportSource
// needs a PositionableAudioSource, and there's no built-in one that wraps a
// raw buffer directly (AudioFormatReaderSource is file-based). Used for
// both the "original" and "mastered" preview sources (spec section 23) so
// neither needs a temp file on disk just to be auditioned.
class BufferAudioSource : public juce::PositionableAudioSource
{
public:
    // Does not copy - caller owns the buffer and must keep it alive and
    // unchanged for the lifetime of this source's use in playback.
    explicit BufferAudioSource (const juce::AudioBuffer<float>& bufferToPlay) noexcept
        : buffer (bufferToPlay) {}

    void prepareToPlay (int, double) override {}
    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        const int numSamplesAvailable = juce::jmax (0, buffer.getNumSamples() - position);
        const int numToCopy = juce::jmin (info.numSamples, numSamplesAvailable);

        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            const int sourceCh = juce::jmin (ch, buffer.getNumChannels() - 1);
            if (numToCopy > 0)
                info.buffer->copyFrom (ch, info.startSample, buffer, sourceCh, position, numToCopy);
            if (numToCopy < info.numSamples)
                info.buffer->clear (ch, info.startSample + numToCopy, info.numSamples - numToCopy);
        }

        position += numToCopy;
    }

    void setNextReadPosition (juce::int64 newPosition) override
    {
        position = (int) juce::jlimit ((juce::int64) 0, (juce::int64) buffer.getNumSamples(), newPosition);
    }

    juce::int64 getNextReadPosition() const override { return (juce::int64) position; }
    juce::int64 getTotalLength() const override      { return (juce::int64) buffer.getNumSamples(); }
    bool isLooping() const override                   { return false; }
    void setLooping (bool) override                    {}

private:
    const juce::AudioBuffer<float>& buffer;
    int position = 0;
};
