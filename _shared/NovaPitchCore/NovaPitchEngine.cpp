#include "NovaPitchEngine.h"
#include <cmath>

namespace nova::pitch
{
    void NovaPitchEngine::prepare (double sampleRate, int maxBlockSize, bool lowLatencyMode) noexcept
    {
        detector.prepare (sampleRate, lowLatencyMode);
        appliedLowLatencyMode = lowLatencyMode;

        quantizer.prepare (sampleRate);
        shifterL.prepare (sampleRate);
        shifterR.prepare (sampleRate);
        formantL.prepare (sampleRate);
        formantR.prepare (sampleRate);

        latencySamples = shifterL.getLatencySamples();

        monoScratch.assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);

        const int dryDelaySize = juce::jmax (1, latencySamples + 4);
        dryDelayL.assign ((size_t) dryDelaySize, 0.0f);
        dryDelayR.assign ((size_t) dryDelaySize, 0.0f);

        mixSmoothed.reset (sampleRate, 0.02);
        outputGainSmoothed.reset (sampleRate, 0.02);
        transientsSmoothed.reset (sampleRate, 0.02);
        formantAmountSmoothed.reset (sampleRate, 0.02);
        formantWarpSmoothed.reset (sampleRate, 0.02);
        bypassSmoothed.reset (sampleRate, 0.03); // slightly longer - an on/off toggle, not a continuous knob

        mixSmoothed.setCurrentAndTargetValue (1.0f);
        outputGainSmoothed.setCurrentAndTargetValue (1.0f);
        transientsSmoothed.setCurrentAndTargetValue (0.3f);
        formantAmountSmoothed.setCurrentAndTargetValue (1.0f);
        formantWarpSmoothed.setCurrentAndTargetValue (0.0f);
        bypassSmoothed.setCurrentAndTargetValue (1.0f); // 1 = active, 0 = bypassed

        // ~3ms fast envelope, ~50ms slow envelope - fast rising sharply
        // above slow is what flags a transient/onset.
        fastEnvAlpha = 1.0f - std::exp (-1.0f / (0.003f * (float) sampleRate));
        slowEnvAlpha = 1.0f - std::exp (-1.0f / (0.050f * (float) sampleRate));

        releaseAlpha = 1.0f - std::exp (-1.0f / (0.12f * (float) sampleRate));

        reset();
    }

    void NovaPitchEngine::reset() noexcept
    {
        detector.reset();
        quantizer.reset();
        shifterL.reset();
        shifterR.reset();
        formantL.reset();
        formantR.reset();

        std::fill (dryDelayL.begin(), dryDelayL.end(), 0.0f);
        std::fill (dryDelayR.begin(), dryDelayR.end(), 0.0f);
        dryDelayWrite = 0;

        fastEnv = 0.0f;
        slowEnv = 0.0f;

        lastDetectedHz = 0.0f;
        lastCorrectedHz = 0.0f;
        lastConfidence = 0.0f;

        releasedOutHz = 0.0f;
    }

    void NovaPitchEngine::reconfigureIfNeeded (double sampleRate, bool desiredLowLatencyMode) noexcept
    {
        if (desiredLowLatencyMode == appliedLowLatencyMode)
            return;

        detector.prepare (sampleRate, desiredLowLatencyMode);
        appliedLowLatencyMode = desiredLowLatencyMode;
    }

    void NovaPitchEngine::setParams (const Params& p) noexcept
    {
        quantizer.setKeyAndScale (p.keyPitchClass, p.scale);
        quantizer.setAmount (p.amountPercent / 100.0f);
        quantizer.setRetuneSpeed (p.retuneSpeedPercent / 100.0f);
        quantizer.setVibratoPreserve (p.vibratoPreservePercent / 100.0f);
        quantizer.setVibratoRateHz (p.vibratoRateHz);

        detector.setConfidenceGate (p.confidenceThresholdPercent / 100.0f);
        const float rangeNorm = juce::jlimit (0.0f, 1.0f, p.rangePercent / 100.0f);
        detector.setSearchRange (70.0f, 350.0f + rangeNorm * 650.0f); // ceiling: 350..1000Hz

        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mixPercent / 100.0f));
        outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (p.outputGainDb));
        transientsSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.transientsPercent / 100.0f));
        formantAmountSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.formantAmountPercent / 100.0f));
        formantWarpSmoothed.setTargetValue (juce::jlimit (-1.0f, 1.0f, p.formantWarpPercent / 100.0f));
        bypassSmoothed.setTargetValue (p.bypassed ? 0.0f : 1.0f);
    }

    void NovaPitchEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            monoScratch[(size_t) i] = 0.5f * (inL[i] + inR[i]);

        const int dryDelaySize = (int) dryDelayL.size();

        for (int i = 0; i < numSamples; ++i)
        {
            const float mono = monoScratch[(size_t) i];

            // Feed the detector ONE SAMPLE AT A TIME, interleaved with the
            // per-sample correction loop below, rather than running the
            // WHOLE block through it upfront - a block-level call lets any
            // hop boundary crossed partway through this block update
            // detector.getCurrentHz() for EVERY sample in the block,
            // including ones that occur BEFORE that hop's true sample
            // position, since the corrector loop only starts reading it
            // after the whole block has already been analyzed. That
            // leaks a few samples up to tens of milliseconds of "future"
            // pitch information backward into earlier samples of the same
            // block - the leak's size scales directly with the host's
            // block size, which is exactly why a plugin that measured
            // clean in a 512-sample offline test sounded measurably worse
            // in a real host: confirmed directly by re-rendering the same
            // material at several block sizes and finding the output
            // diverges by tens of milliseconds specifically at moments
            // where a hop boundary would fall mid-block, worst at exactly
            // the fast-transition passages this session spent the most
            // effort on. Sample-at-a-time keeps this genuinely causal and
            // block-size-invariant, matching how any correctly-written
            // real-time audio plugin has to behave regardless of the
            // host's chosen buffer size.
            detector.pushBlock (&monoScratch[(size_t) i], 1);

            fastEnv += fastEnvAlpha * (std::abs (mono) - fastEnv);
            slowEnv += slowEnvAlpha * (std::abs (mono) - slowEnv);
            const float baseline = slowEnv * 1.5f + 1.0e-6f;
            const float onset = juce::jlimit (0.0f, 1.0f, (fastEnv - baseline) / baseline);

            const float detectedHz = detector.getCurrentHz();
            // Only feed the quantizer while the detector is genuinely
            // voiced - detectedHz alone isn't enough to gate on, since
            // getCurrentHz() deliberately HOLDS the last voiced value
            // across unvoiced gaps (that's the point of the confidence
            // gate). Without this, real vocal material's frequent brief
            // unvoiced stretches (breaths, consonants, pauses) kept
            // feeding that stale held value into the quantizer's glide
            // every single sample, letting its internal state keep
            // drifting toward a target with no real pitch behind it -
            // measured directly on real vocal material: correctedHz
            // sweeping continuously by hundreds of cents during passages
            // with no reliable detected pitch at all. Freezing the
            // quantizer (targetHz holds at its last value) during
            // unvoiced stretches is far more stable and honest.
            const float targetHz = detector.isVoiced() ? quantizer.process (detectedHz) : quantizer.getTargetHz();

            const float safeInHz  = detectedHz > 0.0f ? detectedHz : 220.0f;
            const float rawSafeOutHz = targetHz > 0.0f ? targetHz : safeInHz;

            // While genuinely voiced, track the target directly - no extra
            // lag beyond what the quantizer's own RetuneSpeed-controlled
            // glide already provides. While unvoiced, let the shifter's
            // ratio release back toward unity (safeInHz, i.e. "stop
            // actively shifting") instead of holding whatever correction
            // was last active indefinitely. Without this, a real vocal
            // take's natural decaying tail - breath, room tone, reverb,
            // never truly digital-silent the way a hard cut is - kept
            // getting actively pitch-shifted by whatever ratio was active
            // the moment real voicing stopped, audible as a quiet but
            // clearly-pitched ringing tail riding on top of what should
            // just sound like the singer's own natural decay (confirmed
            // directly: a hard digital silence cuts the shifter's output
            // to exact zero within its normal latency with no ringing at
            // all, so this isn't the filters' own resonance - it's this
            // held ratio still being applied to genuine, if quiet, real
            // signal). A short release keeps ordinary brief unvoiced gaps
            // (breaths, consonants between sung notes) from causing an
            // audible "un-correction" glitch, while still fully relaxing
            // within a couple hundred ms for a real ending.
            if (releasedOutHz <= 0.0f)
                releasedOutHz = safeInHz; // not primed yet (very first sample(s)) - start from a sane value, not zero

            if (detector.isVoiced())
                releasedOutHz = rawSafeOutHz;
            else
                releasedOutHz += releaseAlpha * (safeInHz - releasedOutHz);

            const float safeOutHz = releasedOutHz;

            const float fAmt  = formantAmountSmoothed.getNextValue();
            const float fWarp = formantWarpSmoothed.getNextValue();
            formantL.setFormantAmount (fAmt);
            formantR.setFormantAmount (fAmt);
            formantL.setFormantWarp (fWarp);
            formantR.setFormantWarp (fWarp);

            const float preL = formantL.processPre (inL[i]);
            const float preR = formantR.processPre (inR[i]);

            const float shiftedL = shifterL.processSample (preL, safeInHz, safeOutHz);
            const float shiftedR = shifterR.processSample (preR, safeInHz, safeOutHz);

            const float postL = formantL.processPost (shiftedL);
            const float postR = formantR.processPost (shiftedR);

            dryDelayL[(size_t) dryDelayWrite] = inL[i];
            dryDelayR[(size_t) dryDelayWrite] = inR[i];
            const int dryReadIdx = (dryDelayWrite - latencySamples + dryDelaySize) % dryDelaySize;
            const float dryL = dryDelayL[(size_t) dryReadIdx];
            const float dryR = dryDelayR[(size_t) dryReadIdx];
            dryDelayWrite = (dryDelayWrite + 1) % dryDelaySize;

            const float transAmt = transientsSmoothed.getNextValue();
            // Scaled by how UNVOICED/noisy the detector actually thinks
            // this moment is (1 - confidence), not just by amplitude onset
            // alone - an onset by itself doesn't distinguish a genuinely
            // noisy consonant (the intended use case: don't pitch-correct
            // noise) from the confident, clearly-pitched START of a sung
            // syllable, which an amplitude-only onset detector reads
            // identically. Confidence naturally rises fast once genuine
            // voicing stabilizes, so this still fully engages for its
            // actual purpose (a truly noisy, low-confidence consonant)
            // while backing off quickly once a real note is underway.
            const float noisiness = 1.0f - detector.getConfidence();
            const float holdBlend = juce::jlimit (0.0f, 1.0f, transAmt * onset * noisiness);
            const float postTL = postL * (1.0f - holdBlend) + dryL * holdBlend;
            const float postTR = postR * (1.0f - holdBlend) + dryR * holdBlend;

            const float mix = mixSmoothed.getNextValue();
            const float mixedL = dryL * (1.0f - mix) + postTL * mix;
            const float mixedR = dryR * (1.0f - mix) + postTR * mix;

            const float gain = outputGainSmoothed.getNextValue();

            // Bypass: click-free linear ramp between the fully dry signal
            // and the fully processed (mix+gain-applied) signal - crosses
            // over the WHOLE chain at once, not just the mix control, so
            // toggling bypass never jumps whatever the Mix knob is set to.
            const float bypassAmt = bypassSmoothed.getNextValue();
            outL[i] = inL[i] * (1.0f - bypassAmt) + (mixedL * gain) * bypassAmt;
            outR[i] = inR[i] * (1.0f - bypassAmt) + (mixedR * gain) * bypassAmt;
        }

        lastDetectedHz = detector.getCurrentHz();
        lastCorrectedHz = quantizer.getTargetHz();
        lastConfidence = detector.getConfidence();
    }
}
