// Nova Pitch — offline diagnostic harness.
// Feeds a WAV file through the REAL NovaPitchAudioProcessor::processBlock()
// (not an external approximation) and logs its own internal
// detected/corrected-pitch state per block, plus renders the actual
// processed output to a WAV file for independent verification.
//
// Read-only observation via the processor's existing public getters
// (getDetectedPitch/getCorrectedPitch/getConfidence/getVoicedDebug/
// getShifterRatioDebug) — no PluginProcessor source changes.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdlib>

static float hzToMidiF (float hz) { return hz > 0.0f ? 69.0f + 12.0f * std::log2 (hz / 440.0f) : -1.0f; }

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 3)
    {
        std::cout << "Usage: NovaPitchOfflineDiag <input.wav> <output.wav> [blockSize] [retuneSpeed0to100] "
                     "[key0to11] [scale0to4] [amount0to100] [formant0to100] [mix0to100] [transients0to100]\n";
        return 1;
    }

    juce::File inFile (argv[1]);
    juce::File outFile (argv[2]);
    const int blockSize = argc >= 4 ? std::atoi (argv[3]) : 512;
    const float retuneSpeedParam = argc >= 5 ? (float) std::atof (argv[4]) : 30.0f;
    const int keyParam    = argc >= 6 ? std::atoi (argv[5]) : 0;
    const int scaleParam  = argc >= 7 ? std::atoi (argv[6]) : 0;
    const float amountParam  = argc >= 8 ? (float) std::atof (argv[7]) : 100.0f;
    const float formantParam = argc >= 9 ? (float) std::atof (argv[8]) : 100.0f;
    const float mixParam     = argc >= 10 ? (float) std::atof (argv[9]) : 100.0f;
    const float transientsParam = argc >= 11 ? (float) std::atof (argv[10]) : 30.0f;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (inFile));
    if (reader == nullptr)
    {
        std::cout << "Failed to open input: " << inFile.getFullPathName() << "\n";
        return 1;
    }

    const double sampleRate = reader->sampleRate;
    const int numChannels = 2; // process as stereo regardless of source channel count
    const juce::int64 totalSamples = reader->lengthInSamples;

    std::cout << "Input: " << inFile.getFileName() << "  sr=" << sampleRate
               << "  samples=" << totalSamples << "  blockSize=" << blockSize << "\n";

    NovaPitchAudioProcessor proc;
    auto setNorm = [&] (const char* id, float realValue, float rangeLo, float rangeHi)
    {
        const float norm = juce::jlimit (0.0f, 1.0f, (realValue - rangeLo) / (rangeHi - rangeLo));
        proc.apvts.getParameter (id)->setValueNotifyingHost (norm);
    };

    setNorm (ParameterIDs::key, (float) keyParam, 0.0f, 11.0f);
    setNorm (ParameterIDs::scale, (float) scaleParam, 0.0f, 4.0f);
    setNorm (ParameterIDs::tolerance, retuneSpeedParam, 0.0f, 100.0f);
    setNorm (ParameterIDs::amount, amountParam, 0.0f, 100.0f);
    setNorm (ParameterIDs::confidenceThreshold, 70.0f, 0.0f, 100.0f);
    setNorm (ParameterIDs::vibrato, 0.0f, 0.0f, 100.0f);
    setNorm (ParameterIDs::formant, formantParam, 0.0f, 100.0f);
    proc.apvts.getParameter (ParameterIDs::lowLatency)->setValueNotifyingHost (0.0f);
    setNorm (ParameterIDs::formantWarp, 0.0f, -100.0f, 100.0f);
    setNorm (ParameterIDs::mix, mixParam, 0.0f, 100.0f);
    setNorm (ParameterIDs::outputGain, 0.0f, -24.0f, 24.0f);
    setNorm (ParameterIDs::range, 100.0f, 0.0f, 100.0f);
    setNorm (ParameterIDs::vibratoRate, 5.0f, 2.0f, 8.0f);
    setNorm (ParameterIDs::transients, transientsParam, 0.0f, 100.0f);

    proc.setPlayConfigDetails (numChannels, numChannels, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    std::cout << "Declared latency: " << proc.getLatencySamples() << " samples\n";

    juce::AudioBuffer<float> outputAccum (numChannels, (int) totalSamples + blockSize);
    outputAccum.clear();

    juce::File diagLog = outFile.getSiblingFile (outFile.getFileNameWithoutExtension() + "_diag.csv");
    std::ofstream diagOut (diagLog.getFullPathName().toStdString());
    diagOut << "block,t_sec,detectedHz,detectedMidi,correctedHz,correctedMidi,confidence,voiced,shifterRatio,latencySamples\n";

    // Forensic instrumentation: every individual grain-capture decision the
    // shifter makes, at full resolution (captures happen roughly once per
    // pitch period - far finer than the per-block CSV above) - see
    // NovaPitchSynchronousShifter's onGrainCaptured comment for why this
    // exists.
    juce::File captureLog = outFile.getSiblingFile (outFile.getFileNameWithoutExtension() + "_captures.csv");
    std::ofstream captureOut (captureLog.getFullPathName().toStdString());
    captureOut << "sampleIndex,t_sec,periodInSamples,wsolaOffset,wsolaBestScore,wsolaBaselineScore,measuredGrainHz,assumedHz,centsError\n";
    // NOVA_PITCH_DIAG_SKIP_OWN_CAPTURE_LOG: lets the sentinel-file-driven
    // debug capture mechanism (maybeEnableDebugCaptureLog(), wired inside
    // prepareToPlay()) be smoke-tested in isolation, since registering a
    // second callback here would otherwise silently overwrite it.
    if (std::getenv ("NOVA_PITCH_DIAG_SKIP_OWN_CAPTURE_LOG") == nullptr)
    proc.setGrainCaptureDebugCallback ([&] (const nova::pitch::NovaPitchSynchronousShifter::CaptureDebugInfo& info)
    {
        const float centsError = (info.measuredGrainHz > 0.0f && info.assumedHz > 0.0f)
            ? 1200.0f * std::log2 (info.measuredGrainHz / info.assumedHz) : 0.0f;
        captureOut << info.sampleIndex << "," << ((double) info.sampleIndex / sampleRate) << ","
                   << info.periodInSamples << "," << info.wsolaOffset << ","
                   << info.wsolaBestScore << "," << info.wsolaBaselineScore << ","
                   << info.measuredGrainHz << "," << info.assumedHz << "," << centsError << "\n";
    });

    // Temporary test hook: NOVA_PITCH_RESET_AT_SAMPLE env var, if set,
    // calls proc.reset() (simulating a well-behaved host signalling a
    // fresh start before a bounce) once processing crosses that absolute
    // sample position - used to directly test whether the reset()
    // override actually restores clean, cold-start-equivalent behavior
    // after arbitrary prior processing history.
    juce::int64 resetAtSample = -1;
    if (const char* envVal = std::getenv ("NOVA_PITCH_RESET_AT_SAMPLE"))
        resetAtSample = (juce::int64) std::atoll (envVal);
    bool didReset = (resetAtSample < 0);

    int blockIndex = 0;
    juce::int64 pos = 0;
    while (pos < totalSamples)
    {
        if (! didReset && pos >= resetAtSample)
        {
            proc.reset();
            didReset = true;
            std::cout << "Called proc.reset() at sample " << pos << "\n";
        }

        const int thisBlock = (int) juce::jmin ((juce::int64) blockSize, totalSamples - pos);
        juce::AudioBuffer<float> block (numChannels, blockSize);
        block.clear();
        reader->read (&block, 0, thisBlock, pos, true, true);

        juce::MidiBuffer midi;
        proc.processBlock (block, midi);

        for (int ch = 0; ch < numChannels; ++ch)
            outputAccum.copyFrom (ch, (int) pos, block, ch, 0, thisBlock);

        const float detHz = proc.getDetectedPitch();
        const float corHz = proc.getCorrectedPitch();

        diagOut << blockIndex << "," << (pos / sampleRate) << ","
                 << detHz << "," << hzToMidiF (detHz) << ","
                 << corHz << "," << hzToMidiF (corHz) << ","
                 << proc.getConfidence() << ","
                 << (proc.getVoicedDebug() ? 1 : 0) << ","
                 << proc.getShifterRatioDebug() << ","
                 << proc.getLatencySamples() << "\n";

        pos += thisBlock;
        blockIndex++;
    }
    diagOut.close();
    captureOut.close();

    std::cout << "Processed " << blockIndex << " blocks.\n";
    std::cout << "Diag CSV: " << diagLog.getFullPathName() << "\n";
    std::cout << "Capture-events CSV: " << captureLog.getFullPathName() << "\n";

    juce::WavAudioFormat wavFormat;
    outFile.deleteFile();
    std::unique_ptr<juce::FileOutputStream> fos (outFile.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (fos.get(), sampleRate, (unsigned int) numChannels, 32, {}, 0));
    if (writer != nullptr)
    {
        fos.release();
        writer->writeFromAudioSampleBuffer (outputAccum, 0, (int) totalSamples);
        std::cout << "Output WAV: " << outFile.getFullPathName() << "\n";
    }
    else
    {
        std::cout << "Failed to create output writer\n";
    }

    return 0;
}
