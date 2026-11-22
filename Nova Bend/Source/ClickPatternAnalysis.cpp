// Throwaway diagnostic: loads an arbitrary WAV (e.g. a real FL Studio
// export with Nova Bend active) and reports WHEN click-like discontinuities
// occur - timestamps and the interval between consecutive ones - rather
// than just a count. A periodic pattern (clicks at a suspiciously regular
// interval) would point at something timer/buffer-driven; an irregular,
// vocal-transient-correlated pattern would point back at the DSP itself.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <vector>

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: ClickPatternAnalysis <path-to-wav> [thresholdMultiple]\n";
        return 1;
    }

    juce::File inputFile (argv[1]);
    const float thresholdMultiple = argc >= 3 ? (float) std::atof (argv[2]) : 12.0f;

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
    const int numChannels = (int) reader->numChannels;
    juce::AudioBuffer<float> buf (numChannels, numSamples);
    reader->read (&buf, 0, numSamples, 0, true, true);

    std::cout << "Loaded " << inputFile.getFileName() << ": " << numChannels << "ch, " << numSamples
               << " samples @ " << sr << "Hz (" << juce::String ((double) numSamples / sr, 2) << "s), threshold="
               << thresholdMultiple << "x\n\n";

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* x = buf.getReadPointer (ch);
        const int win = 64;
        std::vector<int> clickIndices;
        for (int i = win + 1; i < numSamples - 1; ++i)
        {
            double localSumSq = 0.0;
            for (int k = i - win; k < i; ++k)
            {
                const double d = (double) x[k] - (double) x[k - 1];
                localSumSq += d * d;
            }
            const double localRms = std::sqrt (localSumSq / win) + 1.0e-6;
            const double delta = std::abs ((double) x[i] - (double) x[i - 1]);
            if (delta > thresholdMultiple * localRms)
                clickIndices.push_back (i);
        }

        std::cout << "Channel " << ch << ": " << clickIndices.size() << " click-like events over "
                   << juce::String ((double) numSamples / sr, 2) << "s ("
                   << juce::String ((double) clickIndices.size() / ((double) numSamples / sr), 2) << "/s)\n";

        if (! clickIndices.empty())
        {
            // Report intervals between consecutive click events (in ms) -
            // a periodic pattern would show up as a repeated interval
            // value; report the first 40 timestamps for direct inspection.
            std::cout << "  First " << juce::jmin (40, (int) clickIndices.size()) << " click timestamps (seconds):\n  ";
            for (int i = 0; i < juce::jmin (40, (int) clickIndices.size()); ++i)
            {
                std::cout << juce::String ((double) clickIndices[(size_t) i] / sr, 3) << " ";
            }
            std::cout << "\n";

            std::vector<double> intervalsMs;
            for (size_t i = 1; i < clickIndices.size(); ++i)
                intervalsMs.push_back ((double) (clickIndices[i] - clickIndices[i - 1]) / sr * 1000.0);

            if (! intervalsMs.empty())
            {
                double sum = 0.0, minI = 1.0e9, maxI = 0.0;
                for (double v : intervalsMs) { sum += v; minI = juce::jmin (minI, v); maxI = juce::jmax (maxI, v); }
                const double mean = sum / (double) intervalsMs.size();
                double sumSqDiff = 0.0;
                for (double v : intervalsMs) sumSqDiff += (v - mean) * (v - mean);
                const double stddev = std::sqrt (sumSqDiff / (double) intervalsMs.size());
                std::cout << "  Interval between consecutive clicks (ms): mean=" << mean << " stddev=" << stddev
                           << " min=" << minI << " max=" << maxI
                           << " (stddev/mean=" << (stddev / juce::jmax (1.0, mean)) << " - low = periodic/timer-like, high = irregular)\n";
            }
        }
        std::cout << "\n";
    }

    return 0;
}
