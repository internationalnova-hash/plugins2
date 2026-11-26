#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <functional>

#include "NovaAIMasterEngine.h"
#include "NovaLicenseGate.h"

// UI-facing state for the MASTER AI button (spec section 26).
enum class NovaAIMasterButtonState { Idle, Analyzing, Ready, Processing, Complete };

// Realtime-safety design (spec section 31):
//
// TWO separate NovaAIMasterEngine instances, never touched by more than one
// thread each - not a locking scheme, just not sharing mutable state
// across threads in the first place:
//   - `analysisEngine`: prepare/analyze/finishAnalysis/decide, ONLY ever
//     called from the background analysis thread (or before that thread
//     exists, from the constructor/prepareToPlay on the message thread).
//   - `realtimeEngine`: prepare/setPlan/process, ONLY ever called from the
//     audio thread.
//
// Capture buffer is pre-allocated in prepareToPlay() to a fixed maximum
// duration - processBlock() only ever writes into already-owned memory,
// never allocates.
//
// A new MasteringPlan crosses from the message thread (where beginMaster()
// runs) to the audio thread via a juce::SpinLock guarding a small POD
// struct copy (NovaMasteringPlan has no heap-owned members) - cheap and
// bounded, not a real-time hazard the way a full lock/allocation would be.
//
// Starting the background analysis thread must never happen ON the audio
// thread (thread creation isn't realtime-safe) - processBlock only ever
// calls triggerAsyncUpdate() when capture completes (lock-free, designed
// for exactly this), and handleAsyncUpdate() (message thread) does the
// actual juce::Thread::launch().
class NovaAIMasterProcessor : public juce::AudioProcessor,
                              private juce::AsyncUpdater
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-aimaster", "Nova AI Master" };

    NovaAIMasterProcessor();
    ~NovaAIMasterProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nova AI Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- UI-facing control surface (spec section 30 workflow) ----
    // First call starts capturing audio into the ring buffer; a second
    // call (while already capturing) stops capture and kicks off
    // background analysis. Message-thread only.
    void beginAnalyze();

    // Runs decide(), then calibrates the loudness by rendering the chain on
    // the captured audio (a few seconds, on a background thread), and hands
    // the plan to the audio thread via the spinlock-guarded pending-plan slot.
    // buttonState reads Processing until it's done, then Complete.
    // Message-thread only. Only meaningful once buttonState is Ready or
    // Complete (i.e. analysis has produced a profile).
    void beginMaster (const NovaMasterUserPrefs& prefs);

    NovaAIMasterButtonState getButtonState() const noexcept { return buttonState.load(); }
    const NovaAudioProfile& getLastProfile() const noexcept { return lastProfile; }
    const NovaMasteringPlan& getLastPlan() const noexcept { return lastPlan; }

    // Fired on the message thread when analysis or mastering complete -
    // the editor (task #87) hooks these to refresh the WebView.
    std::function<void()> onAnalysisComplete;
    std::function<void()> onMasterApplied;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void handleAsyncUpdate() override; // message thread: launches the background analysis job
    void runAnalysisJob (int numCapturedSamples); // background thread
    void runMasterJob (NovaMasterUserPrefs prefs, juce::WeakReference<NovaAIMasterProcessor> safe);  // background thread

    NovaAIMasterEngine analysisEngine; // background-thread-only
    NovaAIMasterEngine realtimeEngine; // audio-thread-only
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> bypassDelay { 4096 };

    // ---- Capture buffer (pre-allocated, fixed capacity - spec section 31) ----
    static constexpr double kMaxCaptureSeconds = 180.0; // 3 minutes
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<int> captureWritePos { 0 };
    std::atomic<bool> isCapturing { false };
    std::atomic<bool> analysisJobRunning { false }; // guards against double-launching the background job
    int captureCapacitySamples = 0;
    std::atomic<int> capturedSamples { 0 };          // what the last analysis heard (for loudness calibration)
    std::atomic<bool> masterJobRunning { false };

    // ---- Plan handoff (message thread -> audio thread) ----
    juce::SpinLock planLock;
    NovaMasteringPlan pendingPlan;
    std::atomic<bool> planPending { false };

    NovaAudioProfile  lastProfile;
    NovaMasteringPlan lastPlan;

    std::atomic<NovaAIMasterButtonState> buttonState { NovaAIMasterButtonState::Idle };

    JUCE_DECLARE_WEAK_REFERENCEABLE (NovaAIMasterProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAIMasterProcessor)
};
