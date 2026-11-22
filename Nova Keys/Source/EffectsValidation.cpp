// Headless validation of NovaEffectsChain (spec sections 13-14): each
// stage's measurable effect, the four macros' derived parameter mappings,
// and overall stability under aggressive settings.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Effects/NovaEffectsChain.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <random>
#include <functional>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    constexpr double sr = 44100.0;
    constexpr int blockSize = 512;

    juce::AudioBuffer<float> makeSineBuffer (double freqHz, int numSamples, float amplitude = 0.5f, int numChannels = 2)
    {
        juce::AudioBuffer<float> buf (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) i / sr);
        }
        return buf;
    }

    float rms (const juce::AudioBuffer<float>& b, int channel = 0)
    {
        const auto* d = b.getReadPointer (channel);
        double sum = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i) sum += (double) d[i] * (double) d[i];
        return (float) std::sqrt (sum / juce::jmax (1, b.getNumSamples()));
    }

    float peak (const juce::AudioBuffer<float>& b, int channel = 0)
    {
        const auto* d = b.getReadPointer (channel);
        float p = 0.f;
        for (int i = 0; i < b.getNumSamples(); ++i) p = juce::jmax (p, std::abs (d[i]));
        return p;
    }

    float crestFactor (const juce::AudioBuffer<float>& b, int channel = 0)
    {
        const float p = peak (b, channel);
        const float r = rms (b, channel);
        return r > 1.0e-9f ? p / r : 0.f;
    }

    void processInChunks (NovaEffectsChain& chain, juce::AudioBuffer<float>& buffer)
    {
        int pos = 0;
        const int total = buffer.getNumSamples();
        while (pos < total)
        {
            const int n = juce::jmin (blockSize, total - pos);
            juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), pos, n);
            chain.process (chunk);
            pos += n;
        }
    }
}

int main()
{
    std::cout << "\n[Effects] EQ: boosting a band measurably raises energy at that frequency\n";
    {
        NovaEffectsParams flat;
        flat.saturationActive = false; flat.compressionActive = false;
        flat.chorusActive = false; flat.delayActive = false; flat.reverbActive = false;
        flat.outputCeilingDb = 12.f; // effectively out of the way for this test

        auto highTone = makeSineBuffer (8000.0, 8192);
        NovaEffectsChain flatChain;
        flatChain.prepare (sr, blockSize);
        flatChain.setParams (flat);
        auto flatOut = highTone;
        processInChunks (flatChain, flatOut);

        NovaEffectsParams boosted = flat;
        boosted.eqHighGainDb = 12.f;
        NovaEffectsChain boostChain;
        boostChain.prepare (sr, blockSize);
        boostChain.setParams (boosted);
        auto boostedHighOut = highTone;
        processInChunks (boostChain, boostedHighOut);

        std::cout << "    rms flat=" << rms (flatOut) << " boosted=" << rms (boostedHighOut) << "\n";
        check (rms (boostedHighOut) > rms (flatOut) * 1.3f, "+12dB high-shelf measurably raises RMS of an 8kHz tone");

        auto lowTone = makeSineBuffer (80.0, 8192);
        NovaEffectsParams boostedLow = flat;
        boostedLow.eqLowGainDb = 12.f;
        NovaEffectsChain lowChain;
        lowChain.prepare (sr, blockSize);
        lowChain.setParams (boostedLow);
        auto boostedLowOut = lowTone;
        processInChunks (lowChain, boostedLowOut);
        auto flatLowOut = lowTone;
        processInChunks (flatChain, flatLowOut); // reuse the flat chain, still flat

        std::cout << "    rms flatLow=" << rms (flatLowOut) << " boostedLow=" << rms (boostedLowOut) << "\n";
        check (rms (boostedLowOut) > rms (flatLowOut) * 1.3f, "+12dB low-shelf measurably raises RMS of an 80Hz tone");
    }

    std::cout << "\n[Effects] Saturation: increasing drive/mix measurably flattens peaks (lower crest factor)\n";
    {
        auto input = makeSineBuffer (440.0, 4096, 0.9f);

        NovaEffectsParams off;
        off.eqActive = false; off.compressionActive = false; off.chorusActive = false; off.delayActive = false; off.reverbActive = false;
        off.saturationActive = false;
        off.outputCeilingDb = 12.f;
        NovaEffectsChain offChain;
        offChain.prepare (sr, blockSize);
        offChain.setParams (off);
        auto dryOut = input;
        processInChunks (offChain, dryOut);
        const float dryCrest = crestFactor (dryOut);

        NovaEffectsParams driven = off;
        driven.saturationActive = true;
        driven.saturationDrive = 1.f;
        driven.saturationMix = 1.f;
        NovaEffectsChain drivenChain;
        drivenChain.prepare (sr, blockSize);
        drivenChain.setParams (driven);
        auto drivenOut = input;
        processInChunks (drivenChain, drivenOut);
        const float drivenCrest = crestFactor (drivenOut);

        std::cout << "    dryCrest=" << dryCrest << " drivenCrest=" << drivenCrest << "\n";
        check (drivenCrest < dryCrest * 0.9f, "full-drive saturation measurably lowers crest factor (peaks flattened by waveshaping)");
    }

    std::cout << "\n[Effects] Compression: a loud signal is measurably gain-reduced relative to uncompressed\n";
    {
        auto input = makeSineBuffer (440.0, 8192, 0.9f); // loud, well above a -18dB threshold

        NovaEffectsParams off;
        off.eqActive = false; off.saturationActive = false; off.chorusActive = false; off.delayActive = false; off.reverbActive = false;
        off.compressionActive = false;
        off.outputCeilingDb = 12.f;
        NovaEffectsChain offChain;
        offChain.prepare (sr, blockSize);
        offChain.setParams (off);
        auto uncompressed = input;
        processInChunks (offChain, uncompressed);

        NovaEffectsParams comp = off;
        comp.compressionActive = true;
        comp.compThresholdDb = -18.f;
        comp.compRatio = 4.f;
        comp.compAttackMs = 2.f;
        comp.compReleaseMs = 50.f;
        NovaEffectsChain compChain;
        compChain.prepare (sr, blockSize);
        compChain.setParams (comp);
        auto compressed = input;
        processInChunks (compChain, compressed);

        std::cout << "    rms uncompressed=" << rms (uncompressed) << " compressed=" << rms (compressed) << "\n";
        check (rms (compressed) < rms (uncompressed) * 0.85f, "a 4:1 compressor on a loud tone measurably reduces output level");
    }

    std::cout << "\n[Effects] Chorus/Delay/Reverb each measurably change the output when active with nonzero mix\n";
    {
        auto input = makeSineBuffer (300.0, 8192, 0.4f);

        auto renderWith = [&] (std::function<void (NovaEffectsParams&)> configure) -> juce::AudioBuffer<float>
        {
            NovaEffectsParams p;
            p.eqActive = false; p.saturationActive = false; p.compressionActive = false;
            p.chorusActive = false; p.delayActive = false; p.reverbActive = false;
            p.outputCeilingDb = 12.f;
            configure (p);
            NovaEffectsChain chain;
            chain.prepare (sr, blockSize);
            chain.setParams (p);
            auto out = input;
            processInChunks (chain, out);
            return out;
        };

        auto bypassed = renderWith ([] (NovaEffectsParams&) {});

        auto withChorus = renderWith ([] (NovaEffectsParams& p) { p.chorusActive = true; p.chorusRateHz = 2.f; p.chorusDepth = 0.6f; p.chorusMix = 0.5f; });
        bool chorusDiffers = false;
        for (int i = 0; i < withChorus.getNumSamples(); ++i)
            if (std::abs (withChorus.getSample (0, i) - bypassed.getSample (0, i)) > 0.01f) { chorusDiffers = true; break; }
        check (chorusDiffers, "active chorus with 50% mix audibly changes the output");

        // delayTimeMs must be well under the 8192-sample (~186ms @44.1kHz)
        // test buffer, or the delayed signal never arrives within the
        // window being compared - that's a test-timing bug, not an engine
        // one (caught by an earlier run using 200ms here).
        auto withDelay = renderWith ([] (NovaEffectsParams& p) { p.delayActive = true; p.delayTimeMs = 40.f; p.delayFeedback = 0.4f; p.delayMix = 0.5f; });
        bool delayDiffers = false;
        for (int i = 0; i < withDelay.getNumSamples(); ++i)
            if (std::abs (withDelay.getSample (0, i) - bypassed.getSample (0, i)) > 0.01f) { delayDiffers = true; break; }
        check (delayDiffers, "active delay with 50% mix audibly changes the output");

        auto withReverb = renderWith ([] (NovaEffectsParams& p) { p.reverbActive = true; p.reverbRoomSize = 0.8f; p.reverbMix = 0.6f; });
        bool reverbDiffers = false;
        for (int i = 0; i < withReverb.getNumSamples(); ++i)
            if (std::abs (withReverb.getSample (0, i) - bypassed.getSample (0, i)) > 0.01f) { reverbDiffers = true; break; }
        check (reverbDiffers, "active reverb with 60% mix audibly changes the output");
    }

    std::cout << "\n[Effects] Output ceiling clamps an over-loud signal\n";
    {
        auto loud = makeSineBuffer (440.0, 4096, 2.0f); // intentionally way over unity

        NovaEffectsParams p;
        p.eqActive = false; p.saturationActive = false; p.compressionActive = false;
        p.chorusActive = false; p.delayActive = false; p.reverbActive = false;
        p.outputCeilingDb = -1.f; // ~0.89 linear
        NovaEffectsChain chain;
        chain.prepare (sr, blockSize);
        chain.setParams (p);
        processInChunks (chain, loud);

        const float ceilingLinear = juce::Decibels::decibelsToGain (-1.f);
        std::cout << "    peak after ceiling=" << peak (loud) << " (ceiling=" << ceilingLinear << ")\n";
        check (peak (loud) < ceilingLinear * 1.15f, "a 2.0-amplitude input is clamped down close to the output ceiling");
    }

    std::cout << "\n[Effects] Macros derive sensible parameter mappings\n";
    {
        NovaEffectsChain chain;
        chain.prepare (sr, blockSize);

        NovaMacroValues bright; bright.tone = 1.f; bright.motion = 0.5f; bright.space = 0.5f; bright.character = 0.5f;
        chain.setMacros (bright);
        check (chain.getParams().eqHighGainDb > 0.f && chain.getParams().eqLowGainDb < 0.f, "tone=1 (bright) boosts highs and cuts lows");

        NovaMacroValues dark; dark.tone = 0.f; dark.motion = 0.5f; dark.space = 0.5f; dark.character = 0.5f;
        chain.setMacros (dark);
        check (chain.getParams().eqHighGainDb < 0.f && chain.getParams().eqLowGainDb > 0.f, "tone=0 (dark) cuts highs and boosts lows");

        NovaMacroValues moving; moving.tone = 0.5f; moving.motion = 1.f; moving.space = 0.5f; moving.character = 0.5f;
        chain.setMacros (moving);
        const float movingChorusMix = chain.getParams().chorusMix;
        NovaMacroValues still = moving; still.motion = 0.f;
        chain.setMacros (still);
        check (movingChorusMix > chain.getParams().chorusMix, "motion=1 produces more chorus mix than motion=0");

        NovaMacroValues huge; huge.tone = 0.5f; huge.motion = 0.5f; huge.space = 1.f; huge.character = 0.5f;
        chain.setMacros (huge);
        const float hugeReverbMix = chain.getParams().reverbMix;
        NovaMacroValues dry = huge; dry.space = 0.f;
        chain.setMacros (dry);
        check (hugeReverbMix > chain.getParams().reverbMix, "space=1 (huge) produces more reverb mix than space=0 (dry)");

        NovaMacroValues rich; rich.tone = 0.5f; rich.motion = 0.5f; rich.space = 0.5f; rich.character = 1.f;
        chain.setMacros (rich);
        const float richDrive = chain.getParams().saturationDrive;
        const float richRatio = chain.getParams().compRatio;
        NovaMacroValues clean = rich; clean.character = 0.f;
        chain.setMacros (clean);
        check (richDrive > chain.getParams().saturationDrive, "character=1 (rich) drives more saturation than character=0 (clean)");
        check (richRatio > chain.getParams().compRatio, "character=1 (rich) uses a higher compression ratio than character=0 (clean)");
    }

    std::cout << "\n[Effects] Full chain at aggressive settings stays finite and bounded\n";
    {
        std::mt19937 rng (42);
        std::uniform_real_distribution<float> dist (-1.f, 1.f);
        juce::AudioBuffer<float> noise (2, 16384);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = noise.getWritePointer (ch);
            for (int i = 0; i < noise.getNumSamples(); ++i) d[i] = dist (rng);
        }

        NovaEffectsParams p;
        p.eqActive = true; p.eqLowGainDb = 12.f; p.eqHighGainDb = 12.f;
        p.saturationActive = true; p.saturationDrive = 1.f; p.saturationMix = 1.f;
        p.compressionActive = true; p.compThresholdDb = -30.f; p.compRatio = 10.f;
        p.chorusActive = true; p.chorusDepth = 1.f; p.chorusMix = 1.f;
        p.delayActive = true; p.delayFeedback = 0.9f; p.delayMix = 0.8f;
        p.reverbActive = true; p.reverbRoomSize = 1.f; p.reverbMix = 1.f;
        p.outputCeilingDb = -0.1f;

        NovaEffectsChain chain;
        chain.prepare (sr, blockSize);
        chain.setParams (p);
        processInChunks (chain, noise);

        bool allFinite = true;
        for (int ch = 0; ch < noise.getNumChannels() && allFinite; ++ch)
        {
            const auto* d = noise.getReadPointer (ch);
            for (int i = 0; i < noise.getNumSamples(); ++i)
                if (! std::isfinite (d[i])) { allFinite = false; break; }
        }
        check (allFinite, "full chain at aggressive settings on white noise stays finite (no NaN/Inf)");
        check (peak (noise) < 2.0f, "output ceiling keeps the fully-loaded chain's peak bounded");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL EFFECTS VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
