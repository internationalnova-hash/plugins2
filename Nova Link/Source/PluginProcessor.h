#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LinkSession.h"
#include "LinkVideo.h"

// Nova Link: record an artist in another studio.
//  - Your studio (host): put Nova Link on the master bus. It sends your mix to the artist,
//    plays the artist back to you, and records their takes. A talkback mic can go into the
//    plugin's sidechain input.
//  - The artist's studio: put Nova Link on the vocal track. Their mic goes to you; your mix
//    comes back in their headphones. Takes are recorded lossless on their side and sent over.
class NovaLinkAudioProcessor : public juce::AudioProcessor
{
public:
    NovaLinkAudioProcessor();
    ~NovaLinkAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Link"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    NovaLink::LinkSession session;
    NovaLink::LinkVideo video { session };
    juce::String displayName;
    std::atomic<bool> hostPlaying { false }, hostRecording { false };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NovaLinkAudioProcessor)
};
