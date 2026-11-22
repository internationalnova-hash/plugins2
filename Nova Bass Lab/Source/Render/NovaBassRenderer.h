#pragma once

#include "NovaBassEngine.h"
#include "Preset/NovaBassPreset.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace nova::basslab
{

struct NovaBassRenderOptions
{
    int rootNote = 36;
    float velocity = 1.0f;
    float durationSeconds = 1.0f; // how long the note is held before noteOff
    float tailSeconds = 1.0f;     // additional render time after noteOff for release/ring
    double sampleRate = 48000.0;
    int bitDepth = 24;
    float headroomTargetDb = -1.0f; // peak-normalization target, not blind 0dBFS (spec section 42)
};

struct NovaBassRenderResult
{
    bool success = false;
    juce::String errorMessage;
    juce::AudioBuffer<float> buffer; // trimmed + normalized, ready to write or inspect
    double sampleRate = 48000.0;
    int trimmedLeadingSamples = 0; // diagnostic: how much leading silence was removed
};

// Offline render pipeline (spec sections 38-42). Drives a SECOND, headless
// NovaBassEngine instance sample-accurately - never touches the live
// audition engine, so rendering never disturbs what's currently sounding
// through the speakers. Uses the exact writer-creation shape proven in
// Nova AI Master Standalone's AudioEngine::exportMastered (single-buffer
// juce::WavAudioFormat -> AudioFormatWriter).
class NovaBassRenderer
{
public:
    static NovaBassRenderResult renderToBuffer (nova::bass::NovaBassEngine& headlessEngine,
                                                 const nova::bass::NovaBassPreset& preset,
                                                 const NovaBassRenderOptions& options);

    static bool writeToWavFile (const NovaBassRenderResult& result, const juce::File& outputFile, int bitDepth);

    // "NVB_<category>_<slug(name)>_<noteName>_v001.wav" (spec section 49).
    static juce::String makeAssetFileName (const juce::String& category, const juce::String& name, int rootNote, int version = 1);

private:
    static int findTrimStartSample (const juce::AudioBuffer<float>& buffer, double sampleRate);
    static void normalizeToHeadroom (juce::AudioBuffer<float>& buffer, float headroomTargetDb);
};

} // namespace nova::basslab
