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

    // Called from the editor (UI thread, e.g. a "click to preview" native
    // function handler) to request a one-shot chord preview. Thread-safe:
    // written here, consumed at the start of the next processBlock(). Up to
    // 6 notes; velocity defaults to a sensible preview level; the notes
    // auto-release after previewHoldSeconds.
    void requestChordPreview (int rootMidiNote, const std::array<int, 6>& semis, int count, int velocity) noexcept;

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

    // ── UI chord-preview trigger (see requestChordPreview) ──────────────
    static constexpr double previewHoldSeconds = 0.6;

    std::atomic<bool> previewPending { false };
    std::atomic<int>  previewRoot    { 60 };
    std::atomic<int>  previewCount   { 0 };
    std::atomic<int>  previewVelocity { 90 };
    std::array<std::atomic<int>, 6> previewSemis { {} };

    // Notes currently sounding from a UI-triggered preview, with their
    // remaining sample countdown until auto note-off. Audio-thread-only.
    struct PreviewVoice { int midiNote = -1; int samplesRemaining = 0; };
    std::array<PreviewVoice, 6> previewVoices {};
    double currentSampleRate { 44100.0 };

    // ── Built-in synth engine ────────────────────────────────────────────
    // Nova Chord is a hybrid: it still forwards MIDI (so it can drive an
    // external instrument), but also renders its own audio per active chord
    // note, so it makes sound entirely on its own with nothing else in the
    // chain. Additive tone (up to 4 partials, guarded against exceeding
    // Nyquist) through an ADSR shaped by the selected Voice/Timbre; a
    // real-time-safe fixed voice pool, no allocation in processBlock.
    struct SynthVoice
    {
        bool active = false;
        int midiNote = -1;
        double phase = 0.0;
        double phaseIncrement = 0.0;

        enum class Stage { attack, decay, sustain, release, idle };
        Stage stage = Stage::idle;
        float envelopeLevel = 0.0f;
        float velocityGain = 1.0f;

        // Baked from the Voice/Timbre selection at trigger time, so a
        // sustaining note's character doesn't shift if the dropdown changes.
        std::array<float, 4> harmonicRatio { 1.0f, 0.0f, 0.0f, 0.0f };
        std::array<float, 4> harmonicAmp   { 1.0f, 0.0f, 0.0f, 0.0f };
        float attackCoeff = 0.0f, decayCoeff = 0.0f, releaseCoeff = 0.0f, sustainLevel = 0.7f;
    };

    static constexpr int maxSynthVoices = 16;
    std::array<SynthVoice, maxSynthVoices> synthVoices {};

    void startSynthVoice (int midiNote, int velocity) noexcept;
    void releaseSynthVoice (int midiNote) noexcept;
    void renderSynthVoices (float* left, float* right, int numSamples, bool stereo) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaChordAudioProcessor)
};
