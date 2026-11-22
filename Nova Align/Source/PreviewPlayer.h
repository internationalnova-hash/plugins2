#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace NovaAlign
{
    // Plays back an on-disk audio file (Guide / Target / the Aligned result's
    // temp render) for in-plugin auditioning, entirely independent of the
    // host's audio stream - a private juce::AudioDeviceManager, never touching
    // processBlock. Same pattern as Nova AI Master Standalone's AudioEngine,
    // simplified since every source here is already a file on disk (no need
    // for an in-memory BufferAudioSource).
    class PreviewPlayer
    {
    public:
        PreviewPlayer();
        ~PreviewPlayer();

        // Starts playing file from the top. Returns false if the file can't be
        // read (leaves any current playback stopped either way).
        bool play (const juce::File& file);
        void stop();
        bool isPlaying() const noexcept;

    private:
        juce::AudioFormatManager formatManager;
        juce::AudioDeviceManager deviceManager;
        juce::AudioSourcePlayer sourcePlayer;
        juce::AudioTransportSource transportSource;
        std::unique_ptr<juce::AudioFormatReaderSource> currentReaderSource;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewPlayer)
    };
}
