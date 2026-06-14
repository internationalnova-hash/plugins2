#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <set>
#include <string>
#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

struct ChordDef {
    std::string name;           // e.g. "Cmaj7"
    int rootMidi;               // root note 0-127
    std::vector<int> intervals; // semitone intervals from root, e.g. {0,4,7,11}
};

class NovaChordAudioProcessor : public juce::AudioProcessor
{
public:
    NovaChordAudioProcessor();
    ~NovaChordAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    bool isBusesLayoutSupported (const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                             { return 1; }
    int getCurrentProgram() override                          { return 0; }
    void setCurrentProgram (int) override                     {}
    const juce::String getProgramName (int) override          { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Called from UI thread to trigger a chord
    void triggerChord (int chordIndex);
    void triggerProgressionChord (int index);

    // Called from UI to set API key and trigger AI generation
    void setApiKey (const juce::String& key);
    void generateFromPrompt (const juce::String& prompt,
                             std::function<void(const std::vector<ChordDef>&)> callback);

    // Called from UI to manage the progression queue
    void addToProgression (int suggestedIndex);
    void removeFromProgression (int index);
    void clearProgression();
    void setProgressionFromAI (const std::vector<ChordDef>& chords);
    void exportMidi (const juce::File& file);

    // Read-only access for UI
    const std::vector<ChordDef>& getSuggestions() const  { return suggestions; }
    const std::vector<ChordDef>& getProgression() const  { return progression; }
    std::atomic<int>& getDetectedRootNote()              { return detectedRootNote; }
    std::atomic<bool>& getHasInput()                     { return hasInput; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Chord theory engine
    std::vector<ChordDef> computeSuggestions (int rootMidi, int scale, int style);
    ChordDef buildChord (int rootMidi, const std::string& quality);
    int detectChordRoot (const std::set<int>& activeNotes);

    // MIDI state
    std::set<int> activeNotesMidi;
    std::atomic<int>  detectedRootNote { -1 };
    std::atomic<bool> hasInput         { false };

    // Chord data
    std::vector<ChordDef> suggestions;  // current theory suggestions (up to 6)
    std::vector<ChordDef> progression;  // user's queued progression (up to 8)
    juce::CriticalSection chordLock;

    // Chord trigger queue (audio thread safe)
    struct ChordEvent {
        std::vector<int> midiNotes;
        bool noteOn;
        int velocity;
    };
    juce::AbstractFifo triggerFifo { 32 };
    std::vector<ChordEvent> triggerBuffer { 32 };

    // Currently held chord (for note-off)
    std::vector<int> heldNotes;
    int heldNoteOffSampleCountdown { 0 };
    static constexpr int noteHoldSamples = 22050; // ~0.5s at 44.1kHz

    // API key
    juce::String apiKey;

    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaChordAudioProcessor)
};
