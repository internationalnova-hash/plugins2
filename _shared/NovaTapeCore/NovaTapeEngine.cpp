#include "NovaTapeEngine.h"
#include <cmath>

namespace nova::tape
{
    void NovaTapeEngine::prepare (double sampleRate, int maxBlockSize) noexcept
    {
        sr = sampleRate;
        preparedMaxBlockSize = maxBlockSize;

        saturator.prepare (sampleRate);
        headFilter.prepare (sampleRate);
        wowFlutter.prepare (sampleRate);
        noiseGen.prepare (sampleRate);

        dryBuffer.setSize (2, maxBlockSize);
        workBuffer.setSize (2, maxBlockSize);

        inputGainSmoothed.reset (sampleRate, 0.02);
        outputGainSmoothed.reset (sampleRate, 0.02);
        mixSmoothed.reset (sampleRate, 0.02);
        trimSmoothed.reset (sampleRate, 0.02);
        bypassSmoothed.reset (sampleRate, 0.03);

        inputGainSmoothed.setCurrentAndTargetValue (1.0f);
        outputGainSmoothed.setCurrentAndTargetValue (1.0f);
        mixSmoothed.setCurrentAndTargetValue (0.5f);
        trimSmoothed.setCurrentAndTargetValue (1.0f);
        bypassSmoothed.setCurrentAndTargetValue (1.0f);

        appliedOversamplingStages = -1;
        reconfigureOversamplingIfNeeded (sampleRate, maxBlockSize, 1);

        reset();
    }

    void NovaTapeEngine::reset() noexcept
    {
        saturator.reset();
        headFilter.reset();
        wowFlutter.reset();
        noiseGen.reset();

        preEmphasisL.reset(); preEmphasisR.reset();
        deEmphasisL.reset(); deEmphasisR.reset();
        lowCutL.reset(); lowCutR.reset();
        highCutL.reset(); highCutR.reset();

        if (oversampler != nullptr)
            oversampler->reset();

        inputLevelDebug = 0.0f;
        outputLevelDebug = 0.0f;
    }

    void NovaTapeEngine::reconfigureOversamplingIfNeeded (double sampleRate, int maxBlockSize, int desiredStages) noexcept
    {
        if (desiredStages == appliedOversamplingStages)
            return;

        if (desiredStages <= 0)
        {
            oversampler.reset();
        }
        else
        {
            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                2, desiredStages, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
            oversampler->initProcessing ((size_t) maxBlockSize);
            oversampler->reset();
        }

        appliedOversamplingStages = desiredStages;
        juce::ignoreUnused (sampleRate);
    }

    void NovaTapeEngine::setParams (const Params& p) noexcept
    {
        inputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (p.inputDb));
        outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb));
        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mixPercent / 100.0f));
        trimSmoothed.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-6.0f, 6.0f, p.trimDb)));
        bypassSmoothed.setTargetValue (p.bypassed ? 0.0f : 1.0f);

        const auto typeProfile = getTapeTypeProfile (p.tapeType);
        const float driveScale = juce::jlimit (0.0f, 1.0f, p.tapeAmountPercent / 100.0f);

        TapeTypeProfile scaled = typeProfile;
        scaled.driveAmount *= juce::jmap (driveScale, 0.0f, 1.0f, 0.3f, 1.15f);
        scaled.compressionAmount *= driveScale;
        scaled.headBumpGainDb *= juce::jmap (driveScale, 0.0f, 1.0f, 0.4f, 1.0f);
        scaled.hfRolloffTiltDb *= juce::jmap (driveScale, 0.0f, 1.0f, 0.4f, 1.0f);

        saturator.setProfile (scaled);
        saturator.setUserBias (juce::jlimit (-1.0f, 1.0f, p.biasMinus100to100 / 100.0f));

        headFilter.setParams (p.ips, scaled);

        wowFlutter.setAmount (p.wowFlutter0to10);
        noiseGen.setLevel (p.noise0to10, typeProfile.noiseCharacterGain, getIpsProfile (p.ips).noiseFloorGain);

        currentTapeType = p.tapeType;
        currentIps = p.ips;

        if (! juce::approximatelyEqual (p.lowCutHz, currentLowCutHz))
        {
            currentLowCutHz = p.lowCutHz;
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, juce::jlimit (10.0f, 500.0f, currentLowCutHz), 0.707f);
            *lowCutL.coefficients = *coeffs; *lowCutR.coefficients = *coeffs;
        }
        if (! juce::approximatelyEqual (p.highCutHz, currentHighCutHz))
        {
            currentHighCutHz = p.highCutHz;
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, juce::jlimit (1000.0f, 22000.0f, currentHighCutHz), 0.707f);
            *highCutL.coefficients = *coeffs; *highCutR.coefficients = *coeffs;
        }

        const float preDb = typeProfile.preEmphasisDb * juce::jmap (driveScale, 0.0f, 1.0f, 0.3f, 1.0f);
        const float deDb = typeProfile.deEmphasisDb * juce::jmap (driveScale, 0.0f, 1.0f, 0.3f, 1.0f);
        if (! juce::approximatelyEqual (preDb, currentPreEmphasisDb))
        {
            currentPreEmphasisDb = preDb;
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 6000.0f, 0.707f, juce::Decibels::decibelsToGain (preDb));
            *preEmphasisL.coefficients = *coeffs; *preEmphasisR.coefficients = *coeffs;
        }
        if (! juce::approximatelyEqual (deDb, currentDeEmphasisDb))
        {
            currentDeEmphasisDb = deDb;
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 6000.0f, 0.707f, juce::Decibels::decibelsToGain (-deDb));
            *deEmphasisL.coefficients = *coeffs; *deEmphasisR.coefficients = *coeffs;
        }
    }

    void NovaTapeEngine::applyTapeStage (juce::dsp::AudioBlock<float>& block) noexcept
    {
        auto* l = block.getChannelPointer (0);
        auto* r = block.getNumChannels() > 1 ? block.getChannelPointer (1) : l;
        const int n = (int) block.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            l[i] = saturator.dcBlockL (saturator.processSampleL (l[i]));
            if (r != l)
                r[i] = saturator.dcBlockR (saturator.processSampleR (r[i]));
        }
    }

    void NovaTapeEngine::process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const bool isMono = buffer.getNumChannels() < 2;

        // Copy the caller's buffer (possibly genuinely 1-channel) into a
        // 2-channel work buffer at the boundary - everything below always
        // assumes exactly 2 channels, matching how the oversampler is
        // constructed, instead of scattering mono/stereo branches through
        // every stage.
        workBuffer.setSize (2, numSamples, false, false, true);
        workBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        workBuffer.copyFrom (1, 0, buffer, isMono ? 0 : 1, 0, numSamples);

        dryBuffer.setSize (2, numSamples, false, false, true);
        dryBuffer.copyFrom (0, 0, workBuffer, 0, 0, numSamples);
        dryBuffer.copyFrom (1, 0, workBuffer, 1, 0, numSamples);

        inputLevelDebug = workBuffer.getMagnitude (0, numSamples);

        auto* chL = workBuffer.getWritePointer (0);
        auto* chR = workBuffer.getWritePointer (1);

        // Input gain + pre-emphasis (base rate).
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = inputGainSmoothed.getNextValue();
            chL[i] = preEmphasisL.processSample (chL[i] * g);
            chR[i] = preEmphasisR.processSample (chR[i] * g);
        }

        // Nonlinear tape stage - oversampled (Off/2x/4x). Linear filters
        // (head bump/rolloff, de-emphasis, low/high cut) deliberately run
        // OUTSIDE this block at base rate - they don't need oversampling.
        if (oversampler != nullptr)
        {
            juce::dsp::AudioBlock<float> block (workBuffer);
            auto oversampledBlock = oversampler->processSamplesUp (block);
            applyTapeStage (oversampledBlock);
            oversampler->processSamplesDown (block);
        }
        else
        {
            juce::dsp::AudioBlock<float> block (workBuffer);
            applyTapeStage (block);
        }

        // Head bump / HF rolloff, de-emphasis, wow/flutter, noise, tone
        // filters - all base rate.
        for (int i = 0; i < numSamples; ++i)
        {
            chL[i] = deEmphasisL.processSample (headFilter.processL (chL[i]));
            chR[i] = deEmphasisR.processSample (headFilter.processR (chR[i]));
        }

        wowFlutter.processBlockInPlace (chL, chR, numSamples);
        noiseGen.processAdd (chL, chR, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            chL[i] = highCutL.processSample (lowCutL.processSample (chL[i]));
            chR[i] = highCutR.processSample (lowCutR.processSample (chR[i]));

            const float outGain = outputGainSmoothed.getNextValue();
            chL[i] *= outGain;
            chR[i] *= outGain;

            const float mix = mixSmoothed.getNextValue();
            const float wetL = chL[i];
            const float wetR = chR[i];
            const float mixedL = dryBuffer.getSample (0, i) * (1.0f - mix) + wetL * mix;
            const float mixedR = dryBuffer.getSample (1, i) * (1.0f - mix) + wetR * mix;

            const float trim = trimSmoothed.getNextValue();
            const float trimmedL = mixedL * trim;
            const float trimmedR = mixedR * trim;

            const float bypassAmt = bypassSmoothed.getNextValue();
            chL[i] = dryBuffer.getSample (0, i) * (1.0f - bypassAmt) + trimmedL * bypassAmt;
            chR[i] = dryBuffer.getSample (1, i) * (1.0f - bypassAmt) + trimmedR * bypassAmt;
        }

        outputLevelDebug = workBuffer.getMagnitude (0, numSamples);

        // Copy back down to the caller's actual channel count.
        buffer.copyFrom (0, 0, workBuffer, 0, 0, numSamples);
        if (! isMono)
            buffer.copyFrom (1, 0, workBuffer, 1, 0, numSamples);
    }
}
