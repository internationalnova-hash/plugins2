#include "NovaBendEngine.h"
#include <cmath>

namespace nova::bend
{
    void NovaBendEngine::prepare (double sampleRate, int maxBlockSize, bool highQuality) noexcept
    {
        detector.prepare (sampleRate, ! highQuality); // detector's own flag is "lowLatencyMode" (inverse sense)
        appliedHighQuality = highQuality;

        flattenTarget.prepare (sampleRate);
        shifterL.prepare (sampleRate, highQuality);
        shifterR.prepare (sampleRate, highQuality);
        formantL.prepare (sampleRate);
        formantR.prepare (sampleRate);
        formantL.setFormantAmount (1.0f); // always fully engaged - Nova Bend has no separate Formant "amount" knob, warp=0 is already a clean passthrough
        formantR.setFormantAmount (1.0f);

        // See NovaBendEngine.h class comment: latency is the shifter's own
        // fixed algorithmic delay only. The detector's own reported latency
        // (see NovaBendPitchDetector::getLatencySamples()) is a CONTROL-rate
        // tracking lag, not additional audio-path buffering - every sample
        // is still fed through the shifter immediately as it arrives, using
        // whatever pitch estimate is currently available, so it must not be
        // added on top here (doing so would over-delay the dry tap against
        // the wet path's actual audio-domain delay for no real benefit).
        latencySamples = shifterL.getLatencySamples();

        monoScratch.assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);

        const int dryDelaySize = juce::jmax (1, latencySamples + 4);
        dryDelayL.assign ((size_t) dryDelaySize, 0.0f);
        dryDelayR.assign ((size_t) dryDelaySize, 0.0f);

        mixSmoothed.reset (sampleRate, 0.02);
        bypassSmoothed.reset (sampleRate, 0.03);
        neutralSmoothed.reset (sampleRate, 0.02);
        mixSmoothed.setCurrentAndTargetValue (1.0f);
        bypassSmoothed.setCurrentAndTargetValue (1.0f); // 1 = active, 0 = bypassed
        neutralSmoothed.setCurrentAndTargetValue (0.0f); // 1 = neutral passthrough engaged

        releaseAlpha = 1.0f - std::exp (-1.0f / (0.12f * (float) sampleRate));

        reset();
    }

    void NovaBendEngine::reset() noexcept
    {
        detector.reset();
        flattenTarget.reset();
        shifterL.reset();
        shifterR.reset();
        formantL.reset();
        formantR.reset();

        std::fill (dryDelayL.begin(), dryDelayL.end(), 0.0f);
        std::fill (dryDelayR.begin(), dryDelayR.end(), 0.0f);
        dryDelayWrite = 0;

        lastDetectedHz = 0.0f;
        lastEffectivePitchSemitones = 0.0f;
        releasedOutHz = 0.0f;
    }

    void NovaBendEngine::reconfigureIfNeeded (double sampleRate, bool desiredHighQuality) noexcept
    {
        if (desiredHighQuality == appliedHighQuality)
            return;

        detector.prepare (sampleRate, ! desiredHighQuality);
        shifterL.prepare (sampleRate, desiredHighQuality);
        shifterR.prepare (sampleRate, desiredHighQuality);
        appliedHighQuality = desiredHighQuality;

        latencySamples = shifterL.getLatencySamples();
        const int dryDelaySize = juce::jmax (1, latencySamples + 4);
        dryDelayL.assign ((size_t) dryDelaySize, 0.0f);
        dryDelayR.assign ((size_t) dryDelaySize, 0.0f);
        dryDelayWrite = 0;
    }

    void NovaBendEngine::setParams (const Params& p) noexcept
    {
        params = p;
        flattenTarget.setTargetHz (noteOctaveToHz (p.flattenNoteIndex, p.flattenOctave));
        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mixPercent / 100.0f));
        bypassSmoothed.setTargetValue (p.bypassed ? 0.0f : 1.0f);
        const bool isNeutral = ! p.flatten && p.pitchSemitones == 0.0f && p.formantSemitones == 0.0f;
        neutralSmoothed.setTargetValue (isNeutral ? 1.0f : 0.0f);
    }

    void NovaBendEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            monoScratch[(size_t) i] = 0.5f * (inL[i] + inR[i]);

        const int dryDelaySize = (int) dryDelayL.size();
        const float pitchRatio = semitoneToRatio (params.pitchSemitones);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mono = monoScratch[(size_t) i];

            // Sample-at-a-time, interleaved with the correction loop below -
            // NOT a block-level call upfront. See NovaBendPitchDetector's
            // ancestor (NovaPitchDetector)'s own hard-won lesson: feeding a
            // whole block first lets a hop boundary crossed mid-block leak
            // "future" pitch info backward into earlier samples of the SAME
            // block, an effect that scales with host buffer size and is
            // invisible in an offline test fixed at one buffer size.
            detector.pushBlock (&monoScratch[(size_t) i], 1);

            const float detectedHz = detector.getCurrentHz();
            const bool voiced = detector.isVoiced(); // never gate on detectedHz>0 alone - see header comment
            const float safeInHz = detectedHz > 0.0f ? detectedHz : 220.0f;

            // ratio is computed detector-independently in plain mode (the
            // fix for this class's own artifact history - see
            // NovaBendShifter's header comment) - detectedHz/safeInHz are
            // still tracked here only to keep releasedOutHz primed for a
            // mid-stream Flatten toggle, and because Flatten itself
            // genuinely needs to know the sung note.
            float ratio;
            if (! params.flatten)
            {
                ratio = pitchRatio;
                releasedOutHz = safeInHz * pitchRatio;
            }
            else if (voiced)
            {
                const float outHz = flattenTarget.process (detectedHz);
                releasedOutHz = outHz;
                ratio = outHz / safeInHz;
            }
            else
            {
                releasedOutHz += releaseAlpha * (safeInHz - releasedOutHz);
                ratio = releasedOutHz / safeInHz;
            }

            const float effectivePitchSemitones = ratioToSemitone (ratio);

            // LINK: ONE authoritative formula, computed only here - no
            // APVTS parameter ever writes to another (see spec's explicit
            // "no feedback loops" requirement). The UI independently
            // derives the same sum in JS purely for the Formant knob's
            // on-screen position; that's a display-only computation with
            // no bearing on this engine's own math.
            const float formantWarpSemitones = params.link
                ? (params.formantSemitones + effectivePitchSemitones)
                : params.formantSemitones;

            formantL.setFormantWarpSemitones (formantWarpSemitones);
            formantR.setFormantWarpSemitones (formantWarpSemitones);

            const float preL = formantL.processPre (inL[i]);
            const float preR = formantR.processPre (inR[i]);

            const float shiftedL = shifterL.processSample (preL, ratio, safeInHz);
            const float shiftedR = shifterR.processSample (preR, ratio, safeInHz);

            const float postL = formantL.processPost (shiftedL);
            const float postR = formantR.processPost (shiftedR);

            dryDelayL[(size_t) dryDelayWrite] = inL[i];
            dryDelayR[(size_t) dryDelayWrite] = inR[i];
            const int dryReadIdx = (dryDelayWrite - latencySamples + dryDelaySize) % dryDelaySize;
            const float dryL = dryDelayL[(size_t) dryReadIdx];
            const float dryR = dryDelayR[(size_t) dryReadIdx];
            dryDelayWrite = (dryDelayWrite + 1) % dryDelaySize;

            // See NovaBendEngine.h's NEUTRAL PASSTHROUGH comment: postL/R
            // are a full analysis-resynthesis reconstruction even at
            // Pitch=0/Formant=0, not a literal passthrough - crossfade
            // toward the same delay-matched dry tap Mix uses below,
            // independent of the Mix knob itself (nothing to mix when
            // there's nothing being shifted).
            const float neutralAmt = neutralSmoothed.getNextValue();
            const float effectivePostL = postL + neutralAmt * (dryL - postL);
            const float effectivePostR = postR + neutralAmt * (dryR - postR);

            const float mix = mixSmoothed.getNextValue();
            const float mixedL = dryL * (1.0f - mix) + effectivePostL * mix;
            const float mixedR = dryR * (1.0f - mix) + effectivePostR * mix;

            // Bypass: click-free crossfade over the WHOLE chain at once
            // (including Mix), so toggling bypass never jumps whatever the
            // Mix knob happens to be set to.
            const float bypassAmt = bypassSmoothed.getNextValue();
            outL[i] = inL[i] * (1.0f - bypassAmt) + mixedL * bypassAmt;
            outR[i] = inR[i] * (1.0f - bypassAmt) + mixedR * bypassAmt;

            lastEffectivePitchSemitones = effectivePitchSemitones;
        }

        lastDetectedHz = detector.getCurrentHz();
    }
}
