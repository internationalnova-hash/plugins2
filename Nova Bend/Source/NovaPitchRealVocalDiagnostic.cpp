// Throwaway diagnostic: drives Nova Pitch's ORIGINAL, unmodified shared DSP
// core (_shared/NovaPitchCore - NovaPitchDetector, NovaPitchSynchronousShifter,
// NovaFormantCorrector) directly, bypassing its scale-correction machinery,
// with a fixed direct pitch-shift ratio - the same shape of signal chain as
// NovaBendEngine::process(), on the SAME real vocal clip and the SAME
// click-counting method as RealVocalDiagnostic.cpp - to tell apart "the
// shared PSOLA/LPC/detector foundation itself has a problem on real
// material" from "something specific to how it was adapted for Nova Bend."

#include "NovaPitchTypes.h"
#include "Analysis/NovaPitchDetector.h"
#include "DSP/NovaPitchSynchronousShifter.h"
#include "DSP/NovaFormantCorrector.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <vector>

using namespace nova::pitch;

namespace
{
    bool allFinite (const std::vector<float>& v)
    {
        for (float x : v) if (! std::isfinite (x)) return false;
        return true;
    }

    int countClicks (const std::vector<float>& x, int skip, float thresholdMultiple)
    {
        const int win = 64;
        int clicks = 0;
        for (int i = (int) juce::jmax (skip, win + 1); i < (int) x.size() - 1; ++i)
        {
            double localSumSq = 0.0;
            for (int k = i - win; k < i; ++k)
            {
                const double d = (double) x[(size_t) k] - (double) x[(size_t) (k - 1)];
                localSumSq += d * d;
            }
            const double localRms = std::sqrt (localSumSq / win) + 1.0e-6;
            const double delta = std::abs ((double) x[(size_t) i] - (double) x[(size_t) (i - 1)]);
            if (delta > thresholdMultiple * localRms)
                ++clicks;
        }
        return clicks;
    }

    void writeWav (const juce::File& file, const std::vector<float>& samples, double sr)
    {
        file.deleteFile();
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr) return;
        std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (stream.get(), sr, 1, 16, {}, 0));
        if (writer == nullptr) return;
        stream.release();
        juce::AudioBuffer<float> buf (1, (int) samples.size());
        buf.copyFrom (0, 0, samples.data(), (int) samples.size());
        writer->writeFromAudioSampleBuffer (buf, 0, (int) samples.size());
    }
}

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: NovaPitchRealVocalDiagnostic <path-to-wav>\n";
        return 1;
    }

    juce::File inputFile (argv[1]);
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (inputFile));
    if (reader == nullptr)
    {
        std::cout << "Could not read file: " << inputFile.getFullPathName() << "\n";
        return 1;
    }

    const double sr = reader->sampleRate;
    const int numSamples = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> fileBuf (1, numSamples);
    reader->read (&fileBuf, 0, numSamples, 0, true, false);

    std::vector<float> dry (fileBuf.getReadPointer (0), fileBuf.getReadPointer (0) + numSamples);
    float peak = 0.0f;
    for (float x : dry) peak = juce::jmax (peak, std::abs (x));
    if (peak > 1.0e-6f)
        for (auto& x : dry) x = x / peak * 0.5f;

    std::cout << "Loaded " << inputFile.getFileName() << ": " << numSamples << " samples @ " << sr << "Hz ("
               << juce::String ((double) numSamples / sr, 2) << "s)\n\n";

    const int dryClicks = countClicks (dry, 512, 10.0f);
    std::cout << "BASELINE (dry): " << dryClicks << " click-like discontinuities\n\n";

    // Direct pitch-shift ratios matching Nova Bend's Pitch knob semantics
    // (semitoneToRatio), applied UNCONDITIONALLY (voiced or not) exactly
    // like NovaBendEngine's plain-pitch branch - the scale quantizer/key/
    // retune-speed machinery is intentionally not used here, so this is
    // testing the shared detector+shifter+formant foundation only.
    for (float pitchSemitones : { 0.0f, 5.0f, -5.0f, 7.0f })
    {
        NovaPitchDetector detector;
        NovaPitchSynchronousShifter shifter;
        NovaFormantCorrector formant;
        detector.prepare (sr, false); // lowLatencyMode=false -> Studio-equivalent window sizing
        shifter.prepare (sr);
        formant.prepare (sr);
        formant.setFormantAmount (1.0f);
        formant.setFormantWarp (0.0f);

        const float ratio = std::pow (2.0f, pitchSemitones / 12.0f);

        std::vector<float> outL ((size_t) dry.size(), 0.0f);
        for (size_t i = 0; i < dry.size(); ++i)
        {
            detector.pushBlock (&dry[i], 1);
            const float detectedHz = detector.getCurrentHz();
            const float safeInHz = detectedHz > 0.0f ? detectedHz : 220.0f;
            const float outHz = safeInHz * ratio;

            const float pre = formant.processPre (dry[i]);
            const float shifted = shifter.processSample (pre, safeInHz, outHz);
            const float post = formant.processPost (shifted);
            outL[i] = post;
        }

        const bool finite = allFinite (outL);
        const int latency = shifter.getLatencySamples();
        const int wetClicks = finite ? countClicks (outL, latency + 512, 10.0f) : -1;
        std::cout << "Pitch=" << pitchSemitones << "st: " << wetClicks << " click-like discontinuities (added="
                   << (wetClicks - dryClicks) << "), latency=" << latency << ", finite=" << (finite ? "yes" : "NO") << "\n";

        if (pitchSemitones == 5.0f || pitchSemitones == 0.0f)
        {
            const juce::String suffix = "novapitch_pitch" + juce::String ((int) pitchSemitones);
            const auto outFile = inputFile.getSiblingFile ("nova_bend_diagnostic_output_" + suffix + ".wav");
            writeWav (outFile, outL, sr);
            std::cout << "  -> wrote " << outFile.getFullPathName() << " for listening\n";
        }
    }

    return 0;
}
