#include "NovaBassRenderer.h"
#include <cmath>

using namespace nova::bass;

namespace nova::basslab
{

namespace
{
    juce::String slugify (const juce::String& name)
    {
        juce::String out;
        for (auto c : name)
            out += juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (juce::CharacterFunctions::toLowerCase (c)) : "_";
        while (out.contains ("__")) out = out.replace ("__", "_");
        return out.trimCharactersAtStart ("_").trimCharactersAtEnd ("_");
    }

    juce::String midiNoteName (int note)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int octave = (note / 12) - 1;
        return juce::String (names[note % 12]) + juce::String (octave);
    }
}

int NovaBassRenderer::findTrimStartSample (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return 0;

    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

    if (peak < 1.0e-6f) return 0; // effectively silent - nothing to trim

    // Threshold scales with the buffer's own peak (not an absolute
    // constant) so this works across widely different render levels, with
    // a small absolute floor so a near-silent buffer doesn't trigger on
    // numerical noise.
    const float threshold = juce::jmax (0.0005f, peak * 0.02f);
    const int windowSize = 32;
    const int hop = windowSize / 2;

    int onsetSample = 0;
    for (int i = 0; i + windowSize <= numSamples; i += hop)
    {
        double sumSquares = 0.0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* d = buffer.getReadPointer (ch);
            for (int k = 0; k < windowSize; ++k)
                sumSquares += (double) d[i + k] * d[i + k];
        }
        const float rms = (float) std::sqrt (sumSquares / (windowSize * buffer.getNumChannels()));
        if (rms > threshold) { onsetSample = i; break; }
    }

    // Preserve a few ms of pre-roll so the true transient onset (which the
    // window-based search can slightly overshoot) is never chopped -
    // spec section 41's explicit "do not accidentally chop initial
    // waveform cycles".
    const int preRollSamples = (int) (0.005 * sampleRate); // 5ms
    return juce::jmax (0, onsetSample - preRollSamples);
}

void NovaBassRenderer::normalizeToHeadroom (juce::AudioBuffer<float>& buffer, float headroomTargetDb)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

    if (peak < 1.0e-9f) return; // silent - nothing to normalize

    const float targetLinear = juce::Decibels::decibelsToGain (headroomTargetDb);
    const float gain = targetLinear / peak;
    buffer.applyGain (gain);
}

NovaBassRenderResult NovaBassRenderer::renderToBuffer (NovaBassEngine& headlessEngine, const NovaBassPreset& preset, const NovaBassRenderOptions& options)
{
    NovaBassRenderResult result;
    result.sampleRate = options.sampleRate;

    if (options.durationSeconds < 0.0f || options.tailSeconds < 0.0f)
    {
        result.errorMessage = "negative duration/tail";
        return result;
    }

    constexpr int blockSize = 512;
    headlessEngine.prepare (options.sampleRate, blockSize);
    headlessEngine.reset();
    headlessEngine.loadPreset (preset, nullptr, 0);

    const int totalSamples = (int) std::round ((options.durationSeconds + options.tailSeconds) * options.sampleRate);
    const int noteOffSample = (int) std::round (options.durationSeconds * options.sampleRate);

    if (totalSamples <= 0)
    {
        result.errorMessage = "resulting render length is zero or negative";
        return result;
    }

    juce::AudioBuffer<float> full (2, totalSamples);
    full.clear();

    headlessEngine.noteOn (options.rootNote, options.velocity);

    int rendered = 0;
    bool noteOffSent = false;

    while (rendered < totalSamples)
    {
        if (! noteOffSent && rendered + juce::jmin (blockSize, totalSamples - rendered) > noteOffSample)
        {
            const int preNoteOff = juce::jlimit (0, totalSamples - rendered, noteOffSample - rendered);
            if (preNoteOff > 0)
            {
                juce::AudioBuffer<float> chunk (full.getArrayOfWritePointers(), 2, rendered, preNoteOff);
                headlessEngine.process (chunk);
                rendered += preNoteOff;
            }
            headlessEngine.noteOff (options.rootNote);
            noteOffSent = true;
            continue;
        }

        const int thisBlock = juce::jmin (blockSize, totalSamples - rendered);
        juce::AudioBuffer<float> chunk (full.getArrayOfWritePointers(), 2, rendered, thisBlock);
        headlessEngine.process (chunk);
        rendered += thisBlock;
    }

    const int trimStart = findTrimStartSample (full, options.sampleRate);
    const int trimmedLength = full.getNumSamples() - trimStart;

    juce::AudioBuffer<float> trimmed (2, juce::jmax (1, trimmedLength));
    for (int ch = 0; ch < 2; ++ch)
        trimmed.copyFrom (ch, 0, full, ch, trimStart, trimmedLength);

    normalizeToHeadroom (trimmed, options.headroomTargetDb);

    result.buffer = std::move (trimmed);
    result.trimmedLeadingSamples = trimStart;
    result.success = true;
    return result;
}

bool NovaBassRenderer::writeToWavFile (const NovaBassRenderResult& result, const juce::File& outputFile, int bitDepth)
{
    if (! result.success) return false;

    outputFile.getParentDirectory().createDirectory();
    outputFile.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream (outputFile.createOutputStream());
    if (stream == nullptr) return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (stream.get(), result.sampleRate, (unsigned int) result.buffer.getNumChannels(), bitDepth, {}, 0));

    if (writer == nullptr) return false;
    stream.release(); // writer now owns the stream

    return writer->writeFromAudioSampleBuffer (result.buffer, 0, result.buffer.getNumSamples());
}

juce::String NovaBassRenderer::makeAssetFileName (const juce::String& category, const juce::String& name, int rootNote, int version)
{
    return "NVB_" + slugify (category) + "_" + slugify (name) + "_" + midiNoteName (rootNote)
         + "_v" + juce::String (version).paddedLeft ('0', 3) + ".wav";
}

} // namespace nova::basslab
