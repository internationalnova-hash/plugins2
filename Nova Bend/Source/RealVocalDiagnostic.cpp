// Throwaway diagnostic (not part of the permanent validation suite): loads
// a real vocal WAV file from argv[1], runs it through the actual
// NovaBendEngine at a few real Pitch settings, and reports the same
// click/discontinuity metric used in EngineValidation.cpp's synthetic
// diagnostics - this is the decisive real-material check that a synthetic
// test signal can't substitute for.

#include "NovaBendEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

using nova::bend::NovaBendEngine;

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
        // JUCE's FileOutputStream does NOT truncate an existing file - it
        // seeks to the end/overwrites from the start but leaves any of the
        // OLD file's trailing bytes beyond the new (possibly shorter)
        // content in place. Re-running this on the same output filename
        // with a shorter buffer than a previous run silently produced a
        // WAV whose RIFF header correctly describes the new short clip but
        // whose actual file is padded with megabytes of the previous run's
        // leftover data - most players don't strictly honor the header's
        // declared length and play straight into that garbage. Delete
        // first so every write starts from a clean, correctly-sized file.
        file.deleteFile();
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr) return;
        std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (stream.get(), sr, 1, 16, {}, 0));
        if (writer == nullptr) return;
        stream.release(); // writer owns it now
        juce::AudioBuffer<float> buf (1, (int) samples.size());
        buf.copyFrom (0, 0, samples.data(), (int) samples.size());
        writer->writeFromAudioSampleBuffer (buf, 0, (int) samples.size());
    }
}

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: NovaBendRealVocalDiagnostic <path-to-wav>\n";
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

    // Normalize to a sane, consistent peak level (real recordings vary
    // wildly in gain) so the click threshold means the same thing across
    // different source files.
    float peak = 0.0f;
    for (float x : dry) peak = juce::jmax (peak, std::abs (x));
    if (peak > 1.0e-6f)
        for (auto& x : dry) x = x / peak * 0.5f;

    std::cout << "Loaded " << inputFile.getFileName() << ": " << numSamples << " samples @ " << sr << "Hz ("
               << juce::String ((double) numSamples / sr, 2) << "s)\n\n";

    const int dryClicks = countClicks (dry, 512, 10.0f);
    std::cout << "BASELINE (dry): " << dryClicks << " click-like discontinuities\n\n";

    for (float pitchSt : { 0.0f, 5.0f, -5.0f, 7.0f })
    {
        for (int qualityIdx = 0; qualityIdx < 2; ++qualityIdx)
        {
            const bool highQuality = qualityIdx == 1;
            NovaBendEngine engine;
            engine.prepare (sr, 512, highQuality);
            NovaBendEngine::Params p;
            p.pitchSemitones = pitchSt;
            p.formantSemitones = 0.0f;
            p.flatten = false;
            p.flattenNoteIndex = 0;
            p.flattenOctave = 3;
            p.link = false;
            p.mixPercent = 100.0f;
            p.bypassed = false;
            engine.setParams (p);

            std::vector<float> outL ((size_t) dry.size(), 0.0f), outR ((size_t) dry.size(), 0.0f);
            const int blockSize = 512;
            int pos = 0;
            while (pos < (int) dry.size())
            {
                const int thisBlock = juce::jmin (blockSize, (int) dry.size() - pos);
                engine.process (dry.data() + pos, dry.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
                pos += thisBlock;
            }

            const bool finite = allFinite (outL);
            const int wetClicks = finite ? countClicks (outL, engine.getLatencySamples() + 512, 10.0f) : -1;
            std::cout << "Pitch=" << pitchSt << "st, Quality=" << (highQuality ? "Studio" : "Live")
                       << ": " << wetClicks << " click-like discontinuities (added=" << (wetClicks - dryClicks)
                       << "), finite=" << (finite ? "yes" : "NO") << "\n";

            if (pitchSt == 5.0f || pitchSt == 7.0f)
            {
                const juce::String suffix = "pitch" + juce::String ((int) pitchSt) + "_" + (highQuality ? "studio" : "live");
                const auto outFile = inputFile.getSiblingFile ("nova_bend_diagnostic_output_" + suffix + ".wav");
                writeWav (outFile, outL, sr);
                std::cout << "  -> wrote " << outFile.getFullPathName() << " for listening\n";
            }
        }
    }

    std::cout << "\nNEUTRAL settings check (Pitch=0, Formant=0, Flatten off, Mix=100) - does the engine's own\n"
                 "always-on analysis/resynthesis loop color the sound even when no shift is requested?\n";
    {
        NovaBendEngine engine;
        engine.prepare (sr, 512, true); // Studio
        NovaBendEngine::Params p;
        p.pitchSemitones = 0.0f;
        p.formantSemitones = 0.0f;
        p.flatten = false;
        p.flattenNoteIndex = 0;
        p.flattenOctave = 3;
        p.link = false;
        p.mixPercent = 100.0f;
        p.bypassed = false;
        engine.setParams (p);

        std::vector<float> outL ((size_t) dry.size(), 0.0f), outR ((size_t) dry.size(), 0.0f);
        const int blockSize = 512;
        int pos = 0;
        while (pos < (int) dry.size())
        {
            const int thisBlock = juce::jmin (blockSize, (int) dry.size() - pos);
            engine.process (dry.data() + pos, dry.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
            pos += thisBlock;
        }

        const int reportedLatency = engine.getLatencySamples();

        // Find the TRUE alignment via cross-correlation instead of trusting
        // getLatencySamples() blindly - a naive fixed-lag residual compare
        // on periodic (voiced) material can show a huge apparent residual
        // purely from being a few samples out of phase, even when the
        // reported latency is exactly correct.
        // outL[i] corresponds to dry[i - lag] (output LAGS input by `lag`
        // samples) - NOT outL[i-lag] vs dry[i], which tests the opposite
        // (backwards) relationship and silently measures nonsense.
        auto residualAtLag = [&] (int lag) -> double
        {
            double sumSqDiff = 0.0;
            int count = 0;
            for (size_t i = (size_t) (juce::jmax (0, lag) + 2000); i < outL.size(); ++i)
            {
                const int dryIdx = (int) i - lag;
                if (dryIdx < 0 || dryIdx >= (int) dry.size()) continue;
                const double diff = (double) outL[i] - (double) dry[(size_t) dryIdx];
                sumSqDiff += diff * diff;
                ++count;
            }
            return count > 0 ? std::sqrt (sumSqDiff / count) : 1.0e9;
        };

        int bestLag = reportedLatency;
        double bestResidual = residualAtLag (reportedLatency);
        for (int lag = juce::jmax (0, reportedLatency - 200); lag <= reportedLatency + 200; ++lag)
        {
            const double r = residualAtLag (lag);
            if (r < bestResidual) { bestResidual = r; bestLag = lag; }
        }

        double sumSqDry = 0.0;
        int countDry = 0;
        for (size_t i = (size_t) (reportedLatency + 2000); i < dry.size(); ++i) { sumSqDry += (double) dry[i] * (double) dry[i]; ++countDry; }
        const double drySignalRms = std::sqrt (sumSqDry / countDry);

        double sumSqWet = 0.0;
        int countWet = 0;
        for (size_t i = 2000; i < outL.size(); ++i) { sumSqWet += (double) outL[i] * (double) outL[i]; ++countWet; }
        const double wetSignalRms = std::sqrt (sumSqWet / countWet);
        std::cout << "  dry RMS=" << juce::String (drySignalRms, 5) << ", wet RMS=" << juce::String (wetSignalRms, 5)
                   << " (ratio=" << juce::String (wetSignalRms / juce::jmax (1.0e-9, drySignalRms), 2) << "x)\n";

        const double residualAtReported = residualAtLag (reportedLatency);

        std::cout << "  reported latency=" << reportedLatency << " samples (" << juce::String ((double) reportedLatency / sr * 1000.0, 2) << "ms)\n";
        std::cout << "  TRUE best-matching lag (cross-correlation search)=" << bestLag << " samples ("
                   << juce::String ((double) bestLag / sr * 1000.0, 2) << "ms) - "
                   << (bestLag == reportedLatency ? "MATCHES reported latency exactly" : "*** MISMATCH vs reported latency ***") << "\n";
        std::cout << "  residual at REPORTED latency: RMS=" << juce::String (residualAtReported, 5)
                   << " (" << juce::String (100.0 * residualAtReported / juce::jmax (1.0e-9, drySignalRms), 1) << "% of dry signal RMS)\n";
        std::cout << "  residual at TRUE best lag:    RMS=" << juce::String (bestResidual, 5)
                   << " (" << juce::String (100.0 * bestResidual / juce::jmax (1.0e-9, drySignalRms), 1) << "% of dry signal RMS)\n";

        const auto outFile = inputFile.getSiblingFile ("nova_bend_diagnostic_output_pitch0_formant0_studio.wav");
        writeWav (outFile, outL, sr);
        std::cout << "  -> wrote " << outFile.getFullPathName() << " for listening\n";
    }

    std::cout << "\nCPU / real-time headroom check (wall-clock time to process real-time-sized blocks vs. the real-time budget):\n";
    for (int blockSize : { 64, 128, 256, 512 })
    {
        for (int qualityIdx = 0; qualityIdx < 2; ++qualityIdx)
        {
            const bool highQuality = qualityIdx == 1;
            NovaBendEngine engine;
            engine.prepare (sr, blockSize, highQuality);
            NovaBendEngine::Params p;
            p.pitchSemitones = 5.0f;
            p.formantSemitones = 0.0f;
            p.flatten = false;
            p.flattenNoteIndex = 0;
            p.flattenOctave = 3;
            p.link = false;
            p.mixPercent = 100.0f;
            p.bypassed = false;
            engine.setParams (p);

            std::vector<float> outL ((size_t) blockSize, 0.0f), outR ((size_t) blockSize, 0.0f);
            const float* blockIn = dry.data();

            // Warm up (first few blocks pay one-time allocation-adjacent
            // costs like cache/branch warmup, not representative of
            // steady-state per-block cost).
            for (int i = 0; i < 20; ++i)
                engine.process (blockIn, blockIn, outL.data(), outR.data(), blockSize);

            const int numBlocks = 2000;
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < numBlocks; ++i)
                engine.process (blockIn, blockIn, outL.data(), outR.data(), blockSize);
            const auto t1 = std::chrono::steady_clock::now();

            const double totalSeconds = std::chrono::duration<double> (t1 - t0).count();
            const double perBlockSeconds = totalSeconds / numBlocks;
            const double realTimeBudgetSeconds = (double) blockSize / sr;
            const double cpuPercent = 100.0 * perBlockSeconds / realTimeBudgetSeconds;

            std::cout << "  blockSize=" << blockSize << ", Quality=" << (highQuality ? "Studio" : "Live")
                       << ": " << juce::String (perBlockSeconds * 1.0e6, 1) << "us/block vs "
                       << juce::String (realTimeBudgetSeconds * 1.0e6, 1) << "us budget = "
                       << juce::String (cpuPercent, 1) << "% of real-time"
                       << (cpuPercent > 80.0 ? "  <-- TIGHT" : "") << "\n";
        }
    }

    std::cout << "\nFormant-only tests (Pitch=0, no Link):\n";
    for (float formantSt : { 6.0f, -6.0f })
    {
        for (int qualityIdx = 0; qualityIdx < 2; ++qualityIdx)
        {
            const bool highQuality = qualityIdx == 1;
            NovaBendEngine engine;
            engine.prepare (sr, 512, highQuality);
            NovaBendEngine::Params p;
            p.pitchSemitones = 0.0f;
            p.formantSemitones = formantSt;
            p.flatten = false;
            p.flattenNoteIndex = 0;
            p.flattenOctave = 3;
            p.link = false;
            p.mixPercent = 100.0f;
            p.bypassed = false;
            engine.setParams (p);

            std::vector<float> outL ((size_t) dry.size(), 0.0f), outR ((size_t) dry.size(), 0.0f);
            const int blockSize = 512;
            int pos = 0;
            while (pos < (int) dry.size())
            {
                const int thisBlock = juce::jmin (blockSize, (int) dry.size() - pos);
                engine.process (dry.data() + pos, dry.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
                pos += thisBlock;
            }

            const bool finite = allFinite (outL);
            const int wetClicks = finite ? countClicks (outL, engine.getLatencySamples() + 512, 10.0f) : -1;
            std::cout << "Formant=" << formantSt << "st, Quality=" << (highQuality ? "Studio" : "Live")
                       << ": " << wetClicks << " click-like discontinuities (added=" << (wetClicks - dryClicks)
                       << "), finite=" << (finite ? "yes" : "NO") << "\n";

            const juce::String suffix = "formant" + juce::String ((int) formantSt) + "_" + (highQuality ? "studio" : "live");
            const auto outFile = inputFile.getSiblingFile ("nova_bend_diagnostic_output_" + suffix + ".wav");
            writeWav (outFile, outL, sr);
            std::cout << "  -> wrote " << outFile.getFullPathName() << " for listening\n";
        }
    }

    {
        const auto dryFile = inputFile.getSiblingFile ("nova_bend_diagnostic_dry_reference.wav");
        writeWav (dryFile, dry, sr);
        std::cout << "\n  -> wrote " << dryFile.getFullPathName() << " (normalized dry reference for A/B)\n";
    }

    return 0;
}
