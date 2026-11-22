#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include "ABSnapshot.h"
#include "DSP/NovaDeskEQBands.h"
#include "DSP/NovaDeskDrive.h"
#include "Presets/NovaDeskPresetManager.h"

// Full signal chain (build spec section 4, per the mockup's own info
// panel: "INPUT TRIM -> DRIVE -> HPF -> LOW -> LOW MID -> HIGH MID ->
// HIGH -> LPF -> OUTPUT -> MIX -> OUTPUT"): Input -> [Drive -> HPF -> Low
// -> Low-Mid -> High-Mid -> High -> LPF] -> Output -> Mix. Output trim
// applies to the WET/processed signal only, before blending with dry, so
// it controls how much of the processed character sits in the Mix blend
// rather than scaling the whole plugin output equally.
//
// CLICK-SAFETY (gap-closure pass): bypass and the Input/Output "In"
// toggles are smoothed crossfades, not hard branches - which means the
// Drive/EQ chain runs UNCONDITIONALLY every block, even fully bypassed.
// This is a deliberate tradeoff, not an oversight: NovaDeskDrive's
// oversampler must stay continuously fed with real audio to avoid an FIR
// cold-start artifact on re-engage (see its own class comment), so
// skipping the chain while "bypassed" would just move that same click to
// the bypass toggle instead of fixing it.
class NovaDeskEQAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    NovaDeskEQAudioProcessor();
    ~NovaDeskEQAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
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

    // Peak-style meter telemetry for the input/output LED ladders (build
    // spec section 25) - never an APVTS param, not automatable. Linear
    // 0-1-ish peak level, smoothed on the audio thread, read by the editor
    // timer for the WebView meter push.
    std::atomic<float> inputMeterLevel { 0.0f };
    std::atomic<float> outputMeterLevel { 0.0f };

    ABSnapshot abSnapshot;

    // Factory preset browser (build spec sections 3-30). loadPreset()
    // applies a preset's values through the same setValueNotifyingHost()
    // path automation uses - a preset is just APVTS values, not a
    // separate engine. The selected index is persisted in
    // getStateInformation/setStateInformation (spec section 35's explicit
    // "confirm the selected preset/state recalls correctly" requirement).
    NovaDeskPresetManager presetManager;
    void loadPreset (int index) { presetManager.applyToApvts (index, apvts); }
    void loadNextPreset() { presetManager.selectNext (apvts); }
    void loadPreviousPreset() { presetManager.selectPrevious (apvts); }
    int getCurrentPresetIndex() const noexcept { return presetManager.getCurrentIndex(); }

    // Captures the current knob state as a new user preset, written to
    // disk under NovaDeskPresetManager::getUserPresetsFolder() so it
    // persists across sessions and appears in the same browsable list as
    // the factory presets. Returns the new preset's index, or -1 if the
    // name was empty or the file couldn't be written.
    int saveCurrentAsPreset (const juce::String& name) { return presetManager.saveCurrentAsUserPreset (name, apvts); }

    // Real analytic magnitude response at the CURRENT target parameter
    // values (not the realtime-smoothed shadow - the UI curve must track
    // the knob immediately, matching NovaDeskEQBands.h's own documented
    // design for exactly this purpose). Used by the small EQ curve
    // display - a genuine computed response, not a stylized visual
    // approximation.
    float getEqCurveMagnitudeDb (double frequencyHz) const { return eqBands.getMagnitudeResponseDb (frequencyHz); }

    // Oversampling mode (build spec gap-closure item 3) is a rare,
    // deliberate settings-menu action, not a real-time-automated
    // parameter - so it is NOT read or applied inside processBlock().
    // Reallocating NovaDeskDrive's oversampler (a heap allocation) would
    // be a real-time-safety violation if done on the audio thread. A
    // timer polls for a change on the MESSAGE thread instead (see
    // timerCallback()), and only there calls suspendProcessing() to
    // safely pause the audio callback while the oversampler is rebuilt.
    // Public (not just the private timerCallback) so tests can trigger a
    // reconfiguration deterministically without needing a running message
    // loop; the real host-facing editor never needs to call this itself.
    void reconfigureOversamplingIfNeeded();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    template <typename FloatType>
    static float peakFollow (float currentLevel, const juce::AudioBuffer<FloatType>& buffer);

    void timerCallback() override { reconfigureOversamplingIfNeeded(); }
    int lastAppliedOversamplingStages { 1 };

    // The full signal chain, single-sourced as a template so float and
    // double precision genuinely share one implementation rather than two
    // hand-maintained copies (defined in the .cpp - both instantiations
    // are used there, so no header-visibility issue). SmoothedValue
    // arguments stay float regardless of FloatType: Mix/bypass are
    // control-rate blend weights, not audio samples, and a host commits
    // to one precision mode per session, so sharing them across both
    // processBlock overrides is safe.
    template <typename FloatType>
    void processChainImpl (juce::AudioBuffer<FloatType>& buffer,
                            nova::desk::NovaDeskEQBands<FloatType>& eqBandsRef,
                            nova::desk::NovaDeskDrive<FloatType>& driveRef,
                            juce::dsp::Gain<FloatType>& inputGainRef,
                            juce::dsp::Gain<FloatType>& outputGainRef,
                            juce::AudioBuffer<FloatType>& dryBufferRef,
                            juce::AudioBuffer<FloatType>& rawInputBufferRef,
                            std::array<FloatType, 2>& inputDcX1Ref, std::array<FloatType, 2>& inputDcY1Ref,
                            std::array<FloatType, 2>& outputDcX1Ref, std::array<FloatType, 2>& outputDcY1Ref,
                            FloatType dcBlockRVal);

    nova::desk::NovaDeskEQBands<float> eqBands;
    nova::desk::NovaDeskDrive<float> drive;
    juce::dsp::Gain<float> inputGain, outputGain;
    juce::AudioBuffer<float> dryBuffer, rawInputBuffer;
    std::array<float, 2> inputDcX1 {}, inputDcY1 {}, outputDcX1 {}, outputDcY1 {};
    float dcBlockR { 0.999f };

    nova::desk::NovaDeskEQBands<double> eqBandsD;
    nova::desk::NovaDeskDrive<double> driveD;
    juce::dsp::Gain<double> inputGainD, outputGainD;
    juce::AudioBuffer<double> dryBufferD, rawInputBufferD;
    std::array<double, 2> inputDcX1D {}, inputDcY1D {}, outputDcX1D {}, outputDcY1D {};
    double dcBlockRD { 0.999 };

    static constexpr double kSmoothingSeconds = 0.02; // ~20ms, Mix/gain ramps
    static constexpr double kBypassSeconds = 0.02;    // ~20ms, bypass crossfade
    juce::SmoothedValue<float> mixSm;
    juce::SmoothedValue<float> bypassBlendSm; // 0 = fully wet, 1 = fully bypassed (dry)

    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaDeskEQAudioProcessor)
};
