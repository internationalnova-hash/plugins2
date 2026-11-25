#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <mutex>

#include "NotesCore.h"
#include "NovaLicenseGate.h"

// A captured take after analysis. Immutable once built, shared between the
// message thread, the render worker and the audio thread (via Playback).
struct NotesTake
{
    double sampleRate = 48000.0;
    juce::int64 hostStart = 0;      // host timeline position of the first sample
    double bpm = 0.0, ppqAtStart = 0.0;
    std::vector<float> audio;        // mono
    NovaNotes::PitchTrack track;
    std::vector<int> epochs;
    juce::MemoryBlock flac;          // compressed copy for the session file
};

// What the audio thread plays: the rendered take (and the original for compare).
struct NotesPlayback
{
    std::shared_ptr<const NotesTake> take;
    std::vector<float> edited;
    juce::int64 start = 0;
    int length = 0;
};

class NovaNotesProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    NovaNotesProcessor();
    ~NovaNotesProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova Notes"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    NovaLicenseGate licenseGate { *this, "nova-notes", "Nova Notes" };

    // ── Editor API (message thread) ─────────────────────────────────────────
    enum CaptureState { capIdle = 0, capArmed = 1, capRecording = 2 };
    void armCapture();
    void stopCapture();
    void clearTake();
    int getCaptureState() const noexcept { return capState.load(); }
    double getCapturedSeconds() const noexcept;

    bool hasTake() const;
    bool isBusy() const noexcept { return analysing.load() || rendering.load(); }
    bool isAnalysing() const noexcept { return analysing.load(); }
    double getTakePlayheadSeconds() const noexcept;   // -1 when outside the take / no take
    bool isHostPlaying() const noexcept { return hostPlaying.load(); }
    juce::String getStatusMessage() const;

    int getAnalysisVersion() const noexcept { return analysisVersion.load(); }
    int getNotesVersion() const noexcept { return notesVersion.load(); }
    juce::String getTrackJson() const;
    juce::String getNotesJson() const;
    juce::String getInfoJson() const;

    void applyPatches (const juce::var& patches, bool pushUndo);
    void splitNote (int id, int frame);
    void correctPitch (const std::vector<int>& ids, float centerAmt, float driftAmt, int snapMode);  // 0 scale, 1 chromatic
    juce::String runAssist (int style, const std::vector<int>& ids);
    void quantizeTime (const std::vector<int>& ids, float amount);
    void resetNotes (const std::vector<int>& ids);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    void setShowOriginal (bool b) noexcept { showOriginal.store (b); }
    bool getShowOriginal() const noexcept { return showOriginal.load(); }
    void setKey (int tonic, bool minor);

private:
    void timerCallback() override;
    void startAnalysis (std::vector<float> audio, double sr, juce::int64 hostStart, double bpm, double ppq,
                        juce::String restoredNotesJson, NovaNotes::Key restoredKey, bool keyFromState);
    void requestRender();
    void publish (std::unique_ptr<NotesPlayback> pb);
    void pushUndoSnapshot();
    void notesChanged();
    void ensureCaptureChunks (int spare);
    void freeCaptureChunks();

    // capture (audio thread writes, message thread reads)
    static constexpr int kChunkBits = 16, kChunk = 1 << kChunkBits, kMaxChunks = 8192;
    std::unique_ptr<float[]> chunks[kMaxChunks];
    std::atomic<int> chunksReady { 0 };
    std::atomic<int> capState { capIdle };
    std::atomic<bool> capFinished { false };
    std::atomic<juce::int64> capStart { 0 }, capLen { 0 };
    std::atomic<double> capBpm { 0.0 }, capPpq { 0.0 };
    std::atomic<bool> capOverflow { false };

    // playback
    std::atomic<NotesPlayback*> live { nullptr };
    std::vector<std::pair<std::unique_ptr<NotesPlayback>, double>> retired;
    std::unique_ptr<NotesPlayback> liveOwner;
    std::atomic<bool> showOriginal { false };
    std::atomic<bool> hostPlaying { false };
    std::atomic<juce::int64> hostPos { 0 };
    std::atomic<double> hostRate { 48000.0 };
    double currentSampleRate = 48000.0;

    // model (message thread; locked for getState from other threads)
    mutable juce::CriticalSection modelLock;
    std::shared_ptr<const NotesTake> take;
    std::vector<NovaNotes::Note> notes;
    NovaNotes::Key key;
    bool keyFromUser = false;   // the user picked the key: keep it for every new capture
    std::vector<std::vector<NovaNotes::Note>> undoStack, redoStack;
    std::atomic<int> analysisVersion { 0 }, notesVersion { 0 };
    juce::String status;

    // background work
    juce::ThreadPool pool { 1 };
    std::atomic<bool> analysing { false }, rendering { false };
    bool renderDirty = false;
    int renderSerial = 0;
    std::mutex pendingLock;
    struct AnalysisResult { std::shared_ptr<const NotesTake> take; std::vector<NovaNotes::Note> notes; NovaNotes::Key key; };
    std::unique_ptr<AnalysisResult> pendingAnalysis;
    std::unique_ptr<NotesPlayback> pendingRender;
    int pendingRenderSerial = 0;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaNotesProcessor)
};
