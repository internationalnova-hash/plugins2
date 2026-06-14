#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class NovaChordAudioProcessor : public juce::AudioProcessor
{
public:
    NovaChordAudioProcessor();
    ~NovaChordAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Thread-safe state readbacks for the UI
    int  getLastNoteOn()        const noexcept { return lastNoteOn.load(); }
    int  getChordRootMidi()     const noexcept { return chordRootMidi.load(); }
    bool isChordActive()        const noexcept { return chordActive.load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // MIDI processing state
    std::atomic<int>  lastNoteOn    { -1 };
    std::atomic<int>  chordRootMidi { 60 };
    std::atomic<bool> chordActive   { false };

    // Current chord voicing (semitone offsets from root)
    // Max 6 notes in a chord
    std::array<int, 6> chordSemis {};
    int chordNoteCount { 0 };

    // Active MIDI notes we sent (to send note-offs cleanly)
    std::vector<int> activeMidiNotes;

    // Helper: choose semitone offsets based on styleIdx parameter
    void buildChordVoicing (int styleIdx, std::array<int, 6>& semis, int& count);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaChordAudioProcessor)
};
