#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "NovaPitchTunedEngine.h"
#include "NovaPitchHarmony.h" // shared NovaTuneCore tuner (same as Nova AI Vocal)
#include "ABSnapshot.h"
#include "NovaPitchPresetManager.h"
#include "NovaLicenseGate.h"

class NovaPitchAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    // Nova license: one-time key for this plugin, or Nova All Access
    NovaLicenseGate licenseGate { *this, "nova-pitch", "Nova Pitch" };

    NovaPitchAudioProcessor();
    ~NovaPitchAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    // Hosts call this to tell a plugin to clear its internal state (tails,
    // delay lines, any history-dependent memory) before starting playback
    // from a new position or before a bounce/render - JUCE's own
    // AudioProcessor::reset() default is an empty no-op specifically so
    // that a plugin has to opt in by overriding it, which this class never
    // did until now. Without this override, this shifter's phase
    // accumulators and WSOLA "previous grain" memory silently kept
    // accumulating across an ENTIRE SESSION's worth of playback/scrubbing
    // (however much happened before the user triggered an export),
    // regardless of how well-behaved the host was about signalling a fresh
    // start - directly confirmed as a real, measured effect: byte-
    // identical vocal content preceded by different amounts of unrelated
    // prior audio produced RMS-scale-magnitude different shifter output,
    // because the capture-alignment logic has no absolute reference and
    // only ever corrects relative to whatever grain came before it.
    void reset() override { engine.reset(); }

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
    ABSnapshot abSnapshot;
    NovaPitchPresetManager presetManager;

    float getDetectedPitch()  const noexcept { return detectedPitch.load(); }
    float getCorrectedPitch() const noexcept { return correctedPitch.load(); }
    float getConfidence()     const noexcept { return pitchConfidence.load(); }

    // Keyboard: what was sung and the note it is snapping to (MIDI, 0 = none)
    float getInputMidi()  const noexcept { return inputMidi.load(); }
    float getTargetMidi() const noexcept { return targetMidi.load(); }
    const NovaPitchHarmony& getHarmony() const noexcept { return harmony; }

    // UI view choice (Compact = the original screen, Full = keyboard + Harmony). Saved with the session.
    std::atomic<bool> compactView { false };

    // Debug-only introspection (used by OfflineDiag) - not part of the
    // plugin's normal UI-facing state. Replaces the retired dual-head-
    // shifter's getLockedRefHzDebug/getRatioDebug/getXfadeCountDebug -
    // those concepts (a frozen reference pitch, crossfade count) don't
    // exist in this true-OLA engine.
    bool  getVoicedDebug() const noexcept { return voicedDebug.load(); }
    float getShifterRatioDebug() const noexcept { return shifterRatioDebug.load(); }

    void setGrainCaptureDebugCallback (std::function<void (const nova::pitch::NovaPitchSynchronousShifter::CaptureDebugInfo&)> cb) noexcept
    {
        engine.setGrainCaptureDebugCallback (std::move (cb));
    }

    // Real (not decorative) level history for the hero display's waveform
    // visualization - one RMS value per processed block, from the ACTUAL
    // output audio. Ring buffer; index 0 is oldest.
    static constexpr int kLevelHistorySize = 96;
    float getLevelHistoryAt (int indexFromOldest) const noexcept;

    int saveCurrentAsPreset (const juce::String& name) { return presetManager.saveCurrentAsUserPreset (name, apvts); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;

    // Real-host forensic capture logging - opt-in only, see the public
    // comment above setGrainCaptureDebugCallback(). Checked once per
    // prepareToPlay(); armed only if a sentinel file exists at that
    // moment. debugCaptureLog is reserved to a fixed capacity up front
    // (message thread, prepareToPlay()) so the audio-thread push_back
    // inside the callback never reallocates - once full, new events are
    // silently dropped rather than growing further.
    void maybeEnableDebugCaptureLog();
    void flushDebugCaptureLog();
    std::vector<nova::pitch::NovaPitchSynchronousShifter::CaptureDebugInfo> debugCaptureLog;
    bool debugCaptureActive = false;
    size_t debugCaptureFlushedCount = 0;
    juce::File debugCaptureOutputFile;

    NovaPitchTunedEngine engine;
    NovaPitchHarmony harmony;
    std::vector<float> harmDryL, harmDryR;   // dry copy for the harmony parts (in-place hosts)
    int preparedBlockSize = 512;
    std::atomic<float> inputMidi { 0.0f }, targetMidi { 0.0f };

    double currentSampleRate { 44100.0 };
    bool   lastAppliedLowLatencyMode { false };
    bool   lastAppliedTracking { false };

    std::atomic<float> detectedPitch   { 0.0f };
    std::atomic<float> correctedPitch  { 0.0f };
    std::atomic<float> pitchConfidence { 0.0f };
    std::atomic<bool>  voicedDebug        { false };
    std::atomic<float> shifterRatioDebug  { 1.0f };

    std::array<std::atomic<float>, kLevelHistorySize> levelHistory {};
    std::atomic<int> levelHistoryWrite { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaPitchAudioProcessor)
};
