// Throwaway diagnostic: loads a real WAV file and runs it through the
// actual NovaPunchEngine at a few real Punch/Body/Focus settings,
// reporting peak levels, finiteness, and processing-activity stats. This
// is the decisive check a synthetic-only validation suite can't
// substitute for - see this catalog's own hard-won lesson
// ([[synthetic_vs_real_audio_validation]]) that clean synthetic test
// tones can validate a mechanism while telling you nothing about real-
// material sound quality.

#include "NovaPunchEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <vector>

using namespace nova::punch;

namespace
{
    bool allFinite (const std::vector<float>& v)
    {
        for (float x : v) if (! std::isfinite (x)) return false;
        return true;
    }

    void writeWav (const juce::File& file, const std::vector<float>& samples, double sr)
    {
        // See Nova Bend's own writeup: JUCE's FileOutputStream does NOT
        // truncate an existing file, so a shorter re-run on the same
        // filename can leave megabytes of a previous run's stale data
        // trailing the real audio. Delete first.
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
        std::cout << "Usage: NovaPunchRealMaterialDiagnostic <path-to-wav>\n";
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
        for (auto& x : dry) x = x / peak * 0.7f;

    std::cout << "Loaded " << inputFile.getFileName() << ": " << numSamples << " samples @ " << sr << "Hz ("
               << juce::String ((double) numSamples / sr, 2) << "s)\n\n";

    struct TestCase { const char* name; float punch; float body; NovaPunchEngine::Focus focus; };
    const TestCase cases[] = {
        { "punch70_body50_full",  70.0f, 50.0f, NovaPunchEngine::Focus::Full },
        { "punch40_body55_full",  40.0f, 55.0f, NovaPunchEngine::Focus::Full }, // spec's vocal-style conservative setting
        { "punch60_body55_full",  60.0f, 55.0f, NovaPunchEngine::Focus::Full }, // spec's drum-loop setting
        { "punch70_body50_low",   70.0f, 50.0f, NovaPunchEngine::Focus::Low  },
    };

    for (const auto& c : cases)
    {
        NovaPunchEngine engine;
        engine.prepare (sr, 512);
        NovaPunchEngine::Params p;
        p.punchPercent = c.punch;
        p.bodyPercent = c.body;
        p.focus = c.focus;
        p.sensitivityPercent = 65.0f;
        p.mixPercent = 100.0f;
        p.clipSafe = true;
        p.bypassed = false;
        engine.setParams (p);

        std::vector<float> outL (dry.size(), 0.0f), outR (dry.size(), 0.0f);
        const int blockSize = 512;
        int pos = 0;
        double sumPunchActivity = 0.0, sumBodyActivity = 0.0;
        int blocks = 0;
        while (pos < (int) dry.size())
        {
            const int thisBlock = juce::jmin (blockSize, (int) dry.size() - pos);
            engine.process (dry.data() + pos, dry.data() + pos, outL.data() + pos, outR.data() + pos, thisBlock);
            sumPunchActivity += engine.getPunchActivity();
            sumBodyActivity += engine.getBodyActivity();
            ++blocks;
            pos += thisBlock;
        }

        const bool finite = allFinite (outL);
        float outPeak = 0.0f, dryRms = 0.0, outRms = 0.0;
        for (size_t i = 0; i < outL.size(); ++i)
        {
            outPeak = juce::jmax (outPeak, std::abs (outL[i]));
            dryRms += (double) dry[i] * dry[i];
            outRms += (double) outL[i] * outL[i];
        }
        dryRms = std::sqrt (dryRms / dry.size());
        outRms = std::sqrt (outRms / outL.size());

        std::cout << c.name << ": finite=" << (finite ? "yes" : "NO") << " outPeak=" << juce::String (outPeak, 3)
                   << " dryRMS=" << juce::String ((float) dryRms, 4) << " outRMS=" << juce::String ((float) outRms, 4)
                   << " avgPunchActivity=" << juce::String ((float) (sumPunchActivity / blocks), 3)
                   << " avgBodyActivity=" << juce::String ((float) (sumBodyActivity / blocks), 3) << "\n";

        const auto outFile = inputFile.getSiblingFile (juce::String ("nova_punch_diagnostic_") + c.name + ".wav");
        writeWav (outFile, outL, sr);
        std::cout << "  -> wrote " << outFile.getFullPathName() << "\n";
    }

    {
        const auto dryFile = inputFile.getSiblingFile ("nova_punch_diagnostic_dry_reference.wav");
        writeWav (dryFile, dry, sr);
        std::cout << "\n  -> wrote " << dryFile.getFullPathName() << " (normalized dry reference for A/B)\n";
    }

    return 0;
}
