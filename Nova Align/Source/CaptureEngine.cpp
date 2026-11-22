#include "CaptureEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace NovaAlign
{
    void CaptureEngine::prepare (double sampleRate)
    {
        preparedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const int maxSamples = static_cast<int> (kMaxCaptureSeconds * preparedSampleRate);
        captureBuffer.setSize (2, maxSamples, false, true, true);
        captureBuffer.clear();
        writePosition.store (0);
        capturedSeconds.store (0.0);
    }

    void CaptureEngine::arm (Slot slot) noexcept
    {
        writePosition.store (0);
        capturedSeconds.store (0.0);
        capturing.store (false);
        wasPlayingLastBlock = false;
        armedSlot.store (static_cast<int> (slot));
    }

    void CaptureEngine::cancel() noexcept
    {
        armedSlot.store (static_cast<int> (Slot::none));
        capturing.store (false);
        writePosition.store (0);
        capturedSeconds.store (0.0);
    }

    void CaptureEngine::processBlock (const juce::AudioBuffer<float>& guideBus,
                                      const juce::AudioBuffer<float>& targetBus,
                                      bool isPlaying, double sampleRate,
                                      juce::int64 timelineSample)
    {
        const auto slot = static_cast<Slot> (armedSlot.load());
        if (slot == Slot::none || captureBuffer.getNumSamples() == 0)
        {
            wasPlayingLastBlock = isPlaying;
            return;
        }

        if (isPlaying && ! wasPlayingLastBlock)
        {
            // Transport just started - (re)start capture from the top, same
            // "hit play, it starts" convention as the Aligned-substitution
            // playback path in PluginProcessor::processBlock.
            writePosition.store (0);
            startTimelineSample.store (timelineSample);
            capturing.store (true);
        }

        if (isPlaying && capturing.load())
        {
            const auto& source = (slot == Slot::guide) ? guideBus : targetBus;
            const int sourceChannels = source.getNumChannels();

            if (sourceChannels > 0)
            {
                const int pos = writePosition.load();
                const int numSamples = source.getNumSamples();
                const int available = captureBuffer.getNumSamples() - pos;
                const int toCopy = juce::jmin (numSamples, available);

                if (toCopy > 0)
                {
                    for (int ch = 0; ch < captureBuffer.getNumChannels(); ++ch)
                    {
                        const int srcCh = juce::jmin (ch, sourceChannels - 1);
                        captureBuffer.copyFrom (ch, pos, source, srcCh, 0, toCopy);
                    }
                    writePosition.store (pos + toCopy);
                }

                captureSampleRate = sampleRate;
                capturedSeconds.store ((pos + toCopy) / sampleRate);
            }
        }

        wasPlayingLastBlock = isPlaying;
    }

    juce::File CaptureEngine::finishAndWriteToFile()
    {
        const auto slot = static_cast<Slot> (armedSlot.load());
        armedSlot.store (static_cast<int> (Slot::none));
        capturing.store (false);

        const int numSamples = writePosition.load();
        writePosition.store (0);

        if (slot == Slot::none || numSamples < 1024)
            return {};

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("NovaAlign_captured_" + juce::Uuid().toString() + ".wav");

        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr || ! stream->openedOk())
            return {};

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (stream.release(), captureSampleRate,
                                       static_cast<unsigned int> (captureBuffer.getNumChannels()), 32, {}, 0));
        if (writer == nullptr)
            return {};

        writer->writeFromAudioSampleBuffer (captureBuffer, 0, numSamples);
        writer.reset();

        return file;
    }
}
