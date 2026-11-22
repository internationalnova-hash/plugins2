#include "AlignEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace NovaAlign
{
    static bool loadAudioFileToBuffer (const juce::File& file, juce::AudioBuffer<float>& buffer, double& sampleRate)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr)
            return false;

        sampleRate = reader->sampleRate;
        const int numChannels = static_cast<int> (reader->numChannels);
        const int64_t numSamples = reader->lengthInSamples;

        if (numSamples <= 0)
            return false;

        buffer.setSize (numChannels, static_cast<int> (numSamples));
        buffer.clear();
        reader->read (&buffer, 0, static_cast<int> (numSamples), 0, true, true);
        return true;
    }

    static juce::AudioBuffer<float> createMonoMix (const juce::AudioBuffer<float>& source)
    {
        const int numSamples = source.getNumSamples();
        const int numChannels = source.getNumChannels();
        juce::AudioBuffer<float> mono (1, numSamples);
        mono.clear();

        for (int channel = 0; channel < numChannels; ++channel)
            mono.addFrom (0, 0, source, channel, 0, numSamples, 1.0f / static_cast<float> (numChannels));

        return mono;
    }

    static juce::AudioBuffer<float> createEnvelope (const juce::AudioBuffer<float>& source,
                                                    int frameSize = 512,
                                                    int hopSize = 256)
    {
        const int numSamples = source.getNumSamples();
        const int numFrames = juce::jmax (1, 1 + ((numSamples - frameSize) / hopSize));
        juce::AudioBuffer<float> envelope (1, numFrames);
        envelope.clear();

        for (int frame = 0; frame < numFrames; ++frame)
        {
            const int start = frame * hopSize;
            const int length = juce::jmin (frameSize, numSamples - start);
            float energy = 0.0f;

            for (int channel = 0; channel < source.getNumChannels(); ++channel)
            {
                const float* data = source.getReadPointer (channel);
                for (int i = 0; i < length; ++i)
                    energy += data[start + i] * data[start + i];
            }

            envelope.setSample (0, frame, std::sqrt (energy / static_cast<float> (length + 1)));
        }

        return envelope;
    }

    static juce::Array<AlignEngine::AnchorPoint> detectAnchorPoints (const juce::AudioBuffer<float>& envelope,
                                                                      float sensitivity)
    {
        juce::Array<AlignEngine::AnchorPoint> anchors;
        const int numFrames = envelope.getNumSamples();
        const float threshold = juce::jmax (0.001f, sensitivity * 0.08f);

        for (int i = 2; i < numFrames - 2; ++i)
        {
            const float current = envelope.getSample (0, i);
            if (current < threshold)
                continue;

            const float previous = envelope.getSample (0, i - 1);
            const float next = envelope.getSample (0, i + 1);
            if (current >= previous && current >= next)
            {
                AlignEngine::AnchorPoint anchor;
                anchor.samplePosition = static_cast<int64_t> (i);
                anchor.strength = current;
                anchors.add (anchor);
            }
        }

        if (anchors.size() > 24)
        {
            anchors.removeRange (24, anchors.size() - 24);
        }

        return anchors;
    }

    static int64_t crossCorrelateRange (const float* guideData,
                                        const float* targetData,
                                        int64_t length,
                                        int64_t maxShift)
    {
        int64_t bestLag = 0;
        double bestScore = -1.0;

        const int64_t step = juce::jmax<int64_t> (1, maxShift / 16);

        for (int64_t lag = -maxShift; lag <= maxShift; lag += step)
        {
            int64_t guideStart = lag < 0 ? -lag : 0;
            int64_t targetStart = lag > 0 ? lag : 0;
            const int64_t overlap = length - juce::jmax (guideStart, targetStart);
            if (overlap < 32)
                continue;

            double sumGT = 0.0;
            double energyG = 0.0;
            double energyT = 0.0;

            for (int64_t i = 0; i < overlap; ++i)
            {
                const double g = guideData[guideStart + i];
                const double t = targetData[targetStart + i];
                sumGT += g * t;
                energyG += g * g;
                energyT += t * t;
            }

            const double denom = std::sqrt (energyG * energyT) + 1e-12;
            const double score = sumGT / denom;
            if (score > bestScore)
            {
                bestScore = score;
                bestLag = lag;
            }
        }

        return bestLag;
    }

    static juce::Array<AlignEngine::WarpPoint> estimateWarpPoints (const juce::AudioBuffer<float>& guideEnvelope,
                                                                    const juce::AudioBuffer<float>& targetEnvelope,
                                                                    const juce::AudioBuffer<float>& monoGuide,
                                                                    const juce::AudioBuffer<float>& monoTarget,
                                                                    const AlignEngine::AlignSettings& settings,
                                                                    int64_t sampleRate)
    {
        const auto guideAnchors = detectAnchorPoints (guideEnvelope, settings.phraseSensitivity);
        const auto targetAnchors = detectAnchorPoints (targetEnvelope, settings.phraseSensitivity);
        juce::Array<AlignEngine::WarpPoint> warpPoints;

        if (guideAnchors.isEmpty() || targetAnchors.isEmpty())
        {
            AlignEngine::WarpPoint fallback;
            fallback.samplePosition = 0;
            fallback.shiftSamples = 0;
            warpPoints.add (fallback);
            return warpPoints;
        }

        const int anchorCount = juce::jmin (guideAnchors.size(), targetAnchors.size());
        const int64_t maxShiftSamples = static_cast<int64_t> ((sampleRate * settings.maxShiftMs) / 1000.0);
        const int64_t windowHalf = static_cast<int64_t> (juce::jmax<double> (256.0, sampleRate * 0.12));

        for (int i = 0; i < anchorCount; ++i)
        {
            const int64_t guidePos = guideAnchors[i].samplePosition * 512;
            const int64_t targetPos = targetAnchors[i].samplePosition * 512;
            const int64_t guideStart = juce::jmax<int64_t> (0, guidePos - windowHalf);
            const int64_t targetStart = juce::jmax<int64_t> (0, targetPos - windowHalf);
            const int64_t guideLen = juce::jmin<int64_t> (windowHalf * 2, monoGuide.getNumSamples() - guideStart);
            const int64_t targetLen = juce::jmin<int64_t> (windowHalf * 2, monoTarget.getNumSamples() - targetStart);
            const int64_t effectiveLen = juce::jmin (guideLen, targetLen);

            if (effectiveLen < 64)
                continue;

            const float* guideData = monoGuide.getReadPointer (0) + guideStart;
            const float* targetData = monoTarget.getReadPointer (0) + targetStart;
            const int64_t step = juce::jmax<int64_t> (1, maxShiftSamples / 32);

            int64_t bestLag = 0;
            double bestScore = -1.0;
            for (int64_t lag = -maxShiftSamples; lag <= maxShiftSamples; lag += step)
            {
                const int64_t guideOffset = lag < 0 ? -lag : 0;
                const int64_t targetOffset = lag > 0 ? lag : 0;
                const int64_t overlap = effectiveLen - juce::jmax (guideOffset, targetOffset);
                if (overlap < 32)
                    continue;

                double sumGT = 0.0;
                double energyG = 0.0;
                double energyT = 0.0;
                for (int64_t j = 0; j < overlap; ++j)
                {
                    const double g = guideData[guideOffset + j];
                    const double t = targetData[targetOffset + j];
                    sumGT += g * t;
                    energyG += g * g;
                    energyT += t * t;
                }

                const double denom = std::sqrt (energyG * energyT) + 1e-12;
                const double score = sumGT / denom;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestLag = lag;
                }
            }

            const int64_t rawShift = (guidePos - targetPos) + bestLag;
            const double weightedShift = rawShift * (0.4 + settings.consonantPriority * 0.6);
            const int64_t boundedShift = static_cast<int64_t> (juce::jlimit<double> (-maxShiftSamples, maxShiftSamples, weightedShift));

            AlignEngine::WarpPoint warp;
            warp.samplePosition = targetPos;
            warp.shiftSamples = boundedShift;
            warpPoints.add (warp);
        }

        if (! warpPoints.isEmpty() && warpPoints.size() > 1)
        {
            AlignEngine::WarpPoint first;
            first.samplePosition = 0;
            first.shiftSamples = warpPoints.getFirst().shiftSamples;
            AlignEngine::WarpPoint last;
            last.samplePosition = monoTarget.getNumSamples() - 1;
            last.shiftSamples = warpPoints.getLast().shiftSamples;
            warpPoints.insert (0, first);
            warpPoints.add (last);
        }

        return warpPoints;
    }

    static int64_t interpolateWarpShift (int64_t samplePosition,
                                        const juce::Array<AlignEngine::WarpPoint>& warpPoints)
    {
        if (warpPoints.isEmpty())
            return 0;

        if (samplePosition <= warpPoints.getFirst().samplePosition)
            return warpPoints.getFirst().shiftSamples;

        if (samplePosition >= warpPoints.getLast().samplePosition)
            return warpPoints.getLast().shiftSamples;

        for (int i = 1; i < warpPoints.size(); ++i)
        {
            const auto& previous = warpPoints.getReference (i - 1);
            const auto& current = warpPoints.getReference (i);
            if (samplePosition <= current.samplePosition)
            {
                const double ratio = static_cast<double> (samplePosition - previous.samplePosition) / static_cast<double> (current.samplePosition - previous.samplePosition);
                return static_cast<int64_t> (std::round (previous.shiftSamples + ratio * (current.shiftSamples - previous.shiftSamples)));
            }
        }

        return warpPoints.getLast().shiftSamples;
    }

    static juce::AudioBuffer<float> computeWarpedBuffer (const juce::AudioBuffer<float>& input,
                                                         const juce::Array<AlignEngine::WarpPoint>& warpPoints)
    {
        const int numChannels = input.getNumChannels();
        const int numSamples = input.getNumSamples();
        juce::AudioBuffer<float> output;
        output.setSize (numChannels, numSamples);
        output.clear();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* source = input.getReadPointer (channel);
            auto* dest = output.getWritePointer (channel);

            for (int64_t sample = 0; sample < numSamples; ++sample)
            {
                int64_t shift = interpolateWarpShift (sample, warpPoints);
                const double sourceIndex = sample - static_cast<double> (shift);
                const int64_t sourceFloor = static_cast<int64_t> (std::floor (sourceIndex));
                const double frac = sourceIndex - static_cast<double> (sourceFloor);

                if (sourceFloor >= 0 && sourceFloor + 1 < numSamples)
                {
                    const float a = source[sourceFloor];
                    const float b = source[sourceFloor + 1];
                    dest[sample] = static_cast<float> (a + (b - a) * frac);
                }
                else if (sourceFloor >= 0 && sourceFloor < numSamples)
                {
                    dest[sample] = source[sourceFloor];
                }
            }
        }

        return output;
    }

    static bool writeBufferToWavFile (const juce::AudioBuffer<float>& buffer,
                                      const juce::File& destinationFile,
                                      double sampleRate)
    {
        std::unique_ptr<juce::FileOutputStream> fileStream (destinationFile.createOutputStream());
        if (fileStream == nullptr || ! fileStream->openedOk())
            return false;

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (fileStream.release(), sampleRate, static_cast<unsigned int> (buffer.getNumChannels()), 32, {}, 0));
        if (writer == nullptr)
            return false;

        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        return true;
    }

    // Shared core: loads both files, computes the warp curve, and renders the
    // warped Target into outBuffer/outSampleRate. Both public entry points
    // delegate here so file-writing and in-memory use share one code path.
    static bool alignTargetClipCore (const juce::File& guideFile,
                                     const juce::File& targetFile,
                                     const AlignEngine::AlignSettings& settings,
                                     juce::AudioBuffer<float>& outBuffer,
                                     double& outSampleRate,
                                     int64_t& outShiftSamples)
    {
        juce::AudioBuffer<float> guideBuffer;
        juce::AudioBuffer<float> targetBuffer;
        double guideSampleRate = 0.0;
        double targetSampleRate = 0.0;

        if (! loadAudioFileToBuffer (guideFile, guideBuffer, guideSampleRate))
            return false;
        if (! loadAudioFileToBuffer (targetFile, targetBuffer, targetSampleRate))
            return false;

        if (std::abs (guideSampleRate - targetSampleRate) > 1e-3)
            return false;

        const double sampleRate = targetSampleRate;
        const auto monoGuide = createMonoMix (guideBuffer);
        const auto monoTarget = createMonoMix (targetBuffer);
        const auto guideEnvelope = createEnvelope (monoGuide);
        const auto targetEnvelope = createEnvelope (monoTarget);
        const int64_t maxLagSamples = static_cast<int64_t> ((sampleRate * settings.maxShiftMs) / 1000.0);

        auto warpPoints = estimateWarpPoints (guideEnvelope, targetEnvelope, monoGuide, monoTarget, settings, static_cast<int64_t> (sampleRate));
        outSampleRate = sampleRate;

        if (warpPoints.isEmpty())
        {
            const int64_t globalShift = crossCorrelateRange (monoGuide.getReadPointer (0), monoTarget.getReadPointer (0), juce::jmin<int64_t> (monoGuide.getNumSamples(), monoTarget.getNumSamples()), maxLagSamples);
            outShiftSamples = globalShift;

            juce::Array<AlignEngine::WarpPoint> simpleWarp;
            simpleWarp.add ({ 0, globalShift });
            outBuffer = computeWarpedBuffer (targetBuffer, simpleWarp);
            return true;
        }

        float averageShift = 0.0f;
        for (const auto& warpPoint : warpPoints)
            averageShift += static_cast<float> (warpPoint.shiftSamples);
        averageShift /= static_cast<float> (warpPoints.size());

        for (auto& warpPoint : warpPoints)
        {
            const double adjusted = warpPoint.shiftSamples * settings.amount * settings.tightness * (1.0 - settings.naturalness * 0.45);
            warpPoint.shiftSamples = static_cast<int64_t> (juce::jlimit<double> (-maxLagSamples, maxLagSamples, adjusted));
        }

        outShiftSamples = static_cast<int64_t> (averageShift);
        outBuffer = computeWarpedBuffer (targetBuffer, warpPoints);
        return true;
    }

    bool AlignEngine::alignTargetClip (const juce::File& guideFile,
                                       const juce::File& targetFile,
                                       const juce::File& destinationFile,
                                       const AlignSettings& settings,
                                       int64_t& outShiftSamples)
    {
        juce::AudioBuffer<float> renderedBuffer;
        double sampleRate = 0.0;

        if (! alignTargetClipCore (guideFile, targetFile, settings, renderedBuffer, sampleRate, outShiftSamples))
            return false;

        return writeBufferToWavFile (renderedBuffer, destinationFile, sampleRate);
    }

    bool AlignEngine::alignTargetClipToBuffer (const juce::File& guideFile,
                                               const juce::File& targetFile,
                                               const AlignSettings& settings,
                                               juce::AudioBuffer<float>& outBuffer,
                                               double& outSampleRate,
                                               int64_t& outShiftSamples)
    {
        return alignTargetClipCore (guideFile, targetFile, settings, outBuffer, outSampleRate, outShiftSamples);
    }
}
