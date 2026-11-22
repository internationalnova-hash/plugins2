#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "AlignEngine.h"
#include "AlignJobRunner.h"
#include "PreviewPlayer.h"
#include "CaptureEngine.h"

class NovaAlignAudioProcessor : public juce::AudioProcessor
{
public:
    NovaAlignAudioProcessor();
    ~NovaAlignAudioProcessor() override;

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

    // Guide/Target file management - these aren't real DSP parameters, so they
    // live alongside apvts.state rather than as AudioParameters (see
    // getStateInformation/setStateInformation).
    void setGuideFile (const juce::File& file);
    void setTargetFile (const juce::File& file);
    juce::File getGuideFile() const noexcept { return guideFile; }
    juce::File getTargetFile() const noexcept { return targetFile; }

    // Reads the current APVTS parameter values into an AlignEngine::AlignSettings.
    NovaAlign::AlignEngine::AlignSettings settingsFromApvts() const;

    // Kicks off a background alignment job if Guide+Target are both set and no
    // job is already running. Safe to call repeatedly (a no-op while busy).
    void requestAlign (std::function<void (NovaAlign::AlignJobRunner::Result)> onComplete = nullptr);

    NovaAlign::AlignJobRunner& getAlignJobRunner() noexcept { return alignJobRunner; }
    NovaAlign::PreviewPlayer& getPreviewPlayer() noexcept { return previewPlayer; }

    // Writes the current aligned buffer (if any) to destinationFile as a
    // 32-bit float WAV. Returns false if there's no aligned result yet or the
    // file couldn't be written.
    bool renderAlignedTo (const juce::File& destinationFile) const;

    // The Aligned result is also kept as a per-instance temp file (rewritten
    // each time a new alignment completes) purely so PreviewPlayer - which
    // works off on-disk files, like Guide/Target - can audition it. Not used
    // by processBlock's live substitution path, which reads the in-memory
    // buffer directly.
    juce::File getAlignedTempFile() const noexcept { return alignedTempFile; }

    // ---- Capture (VocALign-style sidechain capture, for hosts without ARA:
    // Guide is captured from the plugin's sidechain input bus, Target from
    // its main input, both while the host transport plays back) ----
    void armCaptureGuide()  { captureEngine.arm (NovaAlign::CaptureEngine::Slot::guide); }
    void armCaptureTarget() { captureEngine.arm (NovaAlign::CaptureEngine::Slot::target); }
    void cancelCapture()    { captureEngine.cancel(); }

    // Stops capture, writes what was captured to a temp file, and (on
    // success) sets it as the Guide/Target file - same as if it had been
    // loaded via the file picker. Returns the resulting file (invalid File{}
    // if nothing/too little was captured).
    juce::File finishCapture();

    NovaAlign::CaptureEngine& getCaptureEngine() noexcept { return captureEngine; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::File guideFile, targetFile;
    const juce::File alignedTempFile;
    NovaAlign::AlignJobRunner alignJobRunner;
    NovaAlign::PreviewPlayer previewPlayer;
    NovaAlign::CaptureEngine captureEngine;

    // Playback position for the "aligned substitution" path in processBlock -
    // advances while the host transport is playing, independent of whatever
    // absolute host timeline position is reported (see processBlock for why).
    int64_t substitutionReadPosition = 0;
    bool wasHostPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaAlignAudioProcessor)
};
