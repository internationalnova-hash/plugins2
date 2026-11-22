#include "PreviewPlayer.h"

namespace NovaAlign
{
    PreviewPlayer::PreviewPlayer()
    {
        formatManager.registerBasicFormats();
        deviceManager.initialiseWithDefaultDevices (0, 2);
        sourcePlayer.setSource (&transportSource);
        deviceManager.addAudioCallback (&sourcePlayer);
    }

    PreviewPlayer::~PreviewPlayer()
    {
        stop();
        deviceManager.removeAudioCallback (&sourcePlayer);
        sourcePlayer.setSource (nullptr);
    }

    bool PreviewPlayer::play (const juce::File& file)
    {
        stop();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr)
            return false;

        const double sampleRate = reader->sampleRate;
        currentReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
        transportSource.setSource (currentReaderSource.get(), 0, nullptr, sampleRate);
        transportSource.setPosition (0.0);
        transportSource.start();
        return true;
    }

    void PreviewPlayer::stop()
    {
        transportSource.stop();
        transportSource.setSource (nullptr);
        currentReaderSource.reset();
    }

    bool PreviewPlayer::isPlaying() const noexcept
    {
        return transportSource.isPlaying();
    }
}
