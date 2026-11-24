#include "NovaPitchHarmony.h"

#include <cmath>

namespace
{
    // Interval choices (index -> scale steps; +-99 = a whole octave).
    constexpr int kSteps[NovaPitchHarmony::kNumIntervals] = { 0, 1, 2, 3, 4, 5, 99, -2, -3, -4, -5, -99 };

    bool inScale (int note, int keyPc, int mask) noexcept
    {
        const int rel = ((note - keyPc) % 12 + 12) % 12;
        return ((mask >> rel) & 1) != 0;
    }
}

int NovaPitchHarmony::intervalShift (int note, int intervalIndex, int keyPc, int stepMask) noexcept
{
    const int steps = kSteps[juce::jlimit (0, kNumIntervals - 1, intervalIndex)];
    if (steps == 99)  return 12;
    if (steps == -99) return -12;
    if (steps == 0 || (stepMask & 0xFFF) == 0) return 0;

    const int dir = steps > 0 ? 1 : -1;
    int n = note;
    for (int k = 0; k < std::abs (steps); ++k)
    {
        n += dir;
        int guard = 0;
        while (! inScale (n, keyPc, stepMask) && ++guard < 12) n += dir;
    }
    return n - note;
}

void NovaPitchHarmony::prepare (double sampleRate, int maxBlockSize, bool lowLatency)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    maxBlock = juce::jmax (64, maxBlockSize);

    ringSize = 1;
    while (ringSize < (int) (0.09 * sr) + maxBlock) ringSize <<= 1;
    ringMask = ringSize - 1;

    for (auto& p : parts)
    {
        p.tuner.prepare (sr, maxBlock, lowLatency);
        p.formant.prepare (sr);
        p.formant.setFormantAmount (1.f);
        p.formant.setFormantWarp (0.f);
        p.formantPost.prepare (sr);
        p.formantPost.setFormantAmount (1.f);
        p.formantPost.setFormantWarp (0.f);
        p.ring.assign ((size_t) ringSize, 0.f);
        p.gate.reset (sr, 0.02);
    }
    mono.assign ((size_t) maxBlock, 0.f);
    delayed.assign ((size_t) maxBlock, 0.f);
    int dsz = 1;
    while (dsz < parts[0].tuner.getLatencySamples() + maxBlock + 1) dsz <<= 1;
    inDelay.assign ((size_t) dsz, 0.f);
    inDelayMask = dsz - 1;
    work.assign ((size_t) maxBlock, 0.f);

    onSm.reset (sr, 0.03);
    leadSm.reset (sr, 0.03);
    mixSm.reset (sr, 0.03);

    // Fixed per-voice character, so the stack sounds the same every time.
    juce::Random rnd (0x4e6f7661);
    for (int v = 0; v < kMaxVoices; ++v)
    {
        auto& vc = voices[(size_t) v];
        vc.phase  = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
        vc.rate   = 0.12f + 0.4f * rnd.nextFloat();
        vc.baseMs = 7.f + 21.f * rnd.nextFloat();
        vc.depthMs = 0.5f + 1.2f * rnd.nextFloat();
        vc.jitter = 0.85f + 0.25f * rnd.nextFloat();
        vc.pan = (v % 2 == 0 ? -1.f : 1.f) * (0.35f + 0.65f * rnd.nextFloat());
    }

    active = false;
    reset();
}

void NovaPitchHarmony::resetPart (Part& p) noexcept
{
    p.tuner.reset();
    p.formant.reset();
    p.formantPost.reset();
    // The corrector refreshes its envelope every 256 samples. Offset the
    // re-colouring stage's refresh points by the shifter latency, so at every
    // moment it uses the very envelope the whitening stage used on that audio.
    constexpr int kFormantHop = 256;
    const int prime = (kFormantHop - p.tuner.getLatencySamples() % kFormantHop) % kFormantHop;
    for (int i = 0; i < prime; ++i) p.formantPost.processPre (0.f);
}

void NovaPitchHarmony::reset() noexcept
{
    for (auto& p : parts)
    {
        resetPart (p);
        std::fill (p.ring.begin(), p.ring.end(), 0.f);
        p.gate.setCurrentAndTargetValue (0.f);
        p.shift = 0;
        p.running = false;
    }
    ringWrite = 0;
    std::fill (inDelay.begin(), inDelay.end(), 0.f);
    inDelayWrite = 0;
    onSm.setCurrentAndTargetValue (0.f);
    leadSm.setCurrentAndTargetValue (1.f);
    mixSm.setCurrentAndTargetValue (0.f);
    lastLeadNote = 0.f;
    unvoicedSamples = 1 << 24;
    for (auto& m : partMidiMeter) m.store (0.f);
    for (auto& m : voiceMeter) m.store (0.f);
    activeVoiceMeter.store (0);
}

void NovaPitchHarmony::noteOn (int note) noexcept
{
    noteOff (note);
    if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note;
}

void NovaPitchHarmony::noteOff (int note) noexcept
{
    for (int i = 0; i < numHeld; ++i)
        if (held[(size_t) i] == note)
        {
            for (int j = i + 1; j < numHeld; ++j) held[(size_t) (j - 1)] = held[(size_t) j];
            --numHeld;
            return;
        }
}

void NovaPitchHarmony::process (const float* dryL, const float* dryR, float* outL, float* outR,
                                int numSamples, float leadTargetMidi) noexcept
{
    const bool wantOn = params.on && params.lead.enabled;

    if (! active)
    {
        if (! wantOn) return;               // harmony off: the lead passes untouched
        reset();
        active = true;
    }

    onSm.setTargetValue (wantOn ? 1.f : 0.f);
    leadSm.setTargetValue (wantOn ? juce::Decibels::decibelsToGain (params.leadGainDb, -60.f) : 1.f);
    mixSm.setTargetValue (juce::jlimit (0.f, 1.f, params.mix) * juce::Decibels::decibelsToGain (params.outGainDb));

    if (leadTargetMidi > 0.f) { lastLeadNote = leadTargetMidi; unvoicedSamples = 0; }
    else if (unvoicedSamples < (1 << 24)) unvoicedSamples += numSamples;
    const int leadNote = (int) std::round (lastLeadNote);
    // Parts sound while the lead is singing, plus a short hold for word endings.
    // Outside that they are muted, so a new phrase doesn't start with the shifter
    // re-locking at full level.
    const bool leadSinging = leadNote > 0 && unvoicedSamples < (int) (0.12 * sr);

    // Which parts are enabled, and what each one plays
    std::array<int, kParts> enabled {};
    int numEnabled = 0;
    for (int p = 0; p < kParts; ++p)
        if (params.partOn[(size_t) p]) enabled[(size_t) numEnabled++] = p;

    for (int p = 0; p < kParts; ++p)
    {
        auto& part = parts[(size_t) p];
        NovaVocalTuner::Params tp = params.lead;
        tp.enabled = true;
        tp.amount = 1.f;
        tp.transientHold = 0.f;   // it would also scale back the harmony interval itself
        bool sounding = false;
        float shownMidi = 0.f;

        if (params.partOn[(size_t) p])
        {
            if (params.midiMode)
            {
                int k = 0;
                while (k < numEnabled && enabled[(size_t) k] != p) ++k;
                if (k < numHeld && leadSinging)
                {
                    tp.flattenMidi = held[(size_t) k];
                    sounding = true;
                    shownMidi = leadTargetMidi > 0.f ? (float) held[(size_t) k] : 0.f;
                }
            }
            else
            {
                if (leadNote > 0)
                    part.shift = intervalShift (leadNote, params.interval[(size_t) p], params.keyPc, params.stepMask);
                tp.fixedShiftSemis = (float) part.shift;
                sounding = leadSinging;
                shownMidi = leadTargetMidi > 0.f ? (float) (leadNote + part.shift) : 0.f;
            }
        }

        // A part that was switched off starts from a clean tuner
        const bool enabledNow = params.partOn[(size_t) p];
        if (enabledNow && ! part.running)
            resetPart (part);
        part.running = enabledNow || part.gate.getCurrentValue() > 0.f;
        part.gate.setTargetValue (sounding ? 1.f : 0.f);
        part.gain = juce::Decibels::decibelsToGain (params.partGainDb[(size_t) p], -60.f);
        part.tuner.setParams (tp);
        partMidiMeter[(size_t) p].store (shownMidi, std::memory_order_relaxed);
    }

    // Voice stack layout
    const int numVoices = numEnabled > 0 ? juce::jlimit (numEnabled, kMaxVoices, params.voices) : 0;
    std::array<int, kParts> perPart {};
    for (int v = 0; v < numVoices; ++v) ++perPart[(size_t) enabled[(size_t) (v % numEnabled)]];

    const float width = juce::jlimit (0.f, 2.f, params.width);
    const float human = juce::jlimit (0.f, 1.f, params.humanize);
    for (int v = 0; v < numVoices; ++v)
    {
        auto& vc = voices[(size_t) v];
        const int rank = v / numEnabled;
        float pan;
        if (rank == 0)
        {
            const int k = v % numEnabled;
            pan = numEnabled > 1 ? ((float) k / (float) (numEnabled - 1) * 2.f - 1.f) * 0.3f * width : 0.f;
        }
        else
        {
            pan = vc.pan * width;
        }
        pan = juce::jlimit (-1.f, 1.f, pan);
        vc.panL = std::sqrt (0.5f * (1.f - pan));
        vc.panR = std::sqrt (0.5f * (1.f + pan));
        vc.sumSq = 0.0;
    }

    const float msToSamples = (float) (sr * 0.001);
    const bool monoOut = outR == outL;

    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - start);

        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = 0.5f * (dryL[start + i] + dryR[start + i]);

        // The same input, delayed by the shifter's latency (for the re-colouring stage)
        const int lat = parts[0].tuner.getLatencySamples();
        for (int i = 0; i < n; ++i)
        {
            inDelay[(size_t) ((inDelayWrite + i) & inDelayMask)] = mono[(size_t) i];
            delayed[(size_t) i] = inDelay[(size_t) ((inDelayWrite + i - lat) & inDelayMask)];
        }
        inDelayWrite = (inDelayWrite + n) & inDelayMask;

        // Parts -> rings
        for (auto& part : parts)
        {
            // Enabled parts keep running even while silent, so they are already
            // locked onto the voice when a note starts (no warm-up burst).
            if (! part.running)
            {
                for (int i = 0; i < n; ++i) part.ring[(size_t) ((ringWrite + i) & ringMask)] = 0.f;
                continue;
            }
            for (int i = 0; i < n; ++i) work[(size_t) i] = part.formant.processPre (mono[(size_t) i]);
            part.tuner.process (work.data(), work.data(), n);
            for (int i = 0; i < n; ++i)
            {
                part.formantPost.processPre (delayed[(size_t) i]); // keeps its envelope in step
                float y = part.formantPost.processPost (work[(size_t) i]) * part.gain * part.gate.getNextValue();
                if (! std::isfinite (y)) y = 0.f;
                part.ring[(size_t) ((ringWrite + i) & ringMask)] = y;
            }
        }

        // Voices: per-chunk delay ramps (cheap, smooth drift)
        std::array<float, kMaxVoices> d0 {}, d1 {}, g {};
        for (int v = 0; v < numVoices; ++v)
        {
            auto& vc = voices[(size_t) v];
            const int p = enabled[(size_t) (v % numEnabled)];
            const int rank = v / numEnabled;
            // Each part's stack sums to about the part's own level, and the parts
            // together to about one voice, so bigger stacks get wider, not louder.
            g[(size_t) v] = (rank == 0 ? 1.f : vc.jitter) / std::sqrt ((float) juce::jmax (1, perPart[(size_t) p]))
                          / std::sqrt ((float) numEnabled);
            if (rank == 0) { d0[(size_t) v] = d1[(size_t) v] = 0.f; continue; }

            const float base = vc.baseMs * (0.4f + 0.8f * human);
            const float depth = vc.depthMs * (0.25f + 1.5f * human);
            d0[(size_t) v] = (base + depth * (1.f + std::sin (vc.phase))) * msToSamples;
            vc.phase += juce::MathConstants<float>::twoPi * vc.rate * (float) n / (float) sr;
            if (vc.phase > juce::MathConstants<float>::twoPi) vc.phase -= juce::MathConstants<float>::twoPi;
            d1[(size_t) v] = (base + depth * (1.f + std::sin (vc.phase))) * msToSamples;
        }

        const float invN = 1.f / (float) n;
        for (int i = 0; i < n; ++i)
        {
            float hL = 0.f, hR = 0.f;
            for (int v = 0; v < numVoices; ++v)
            {
                const auto& ring = parts[(size_t) enabled[(size_t) (v % numEnabled)]].ring;
                float x;
                if (d0[(size_t) v] == 0.f && d1[(size_t) v] == 0.f)
                {
                    x = ring[(size_t) ((ringWrite + i) & ringMask)];
                }
                else
                {
                    const float d = d0[(size_t) v] + (d1[(size_t) v] - d0[(size_t) v]) * (float) i * invN;
                    const double pos = (double) (ringWrite + i) - (double) d;
                    const double fl = std::floor (pos);
                    const float frac = (float) (pos - fl);
                    const int i0 = (int) ((juce::int64) fl & ringMask);
                    const float a = ring[(size_t) i0], b = ring[(size_t) ((i0 + 1) & ringMask)];
                    x = a + (b - a) * frac;
                }
                x *= g[(size_t) v];
                auto& vc = voices[(size_t) v];
                vc.sumSq += (double) x * x;
                hL += x * vc.panL;
                hR += x * vc.panR;
            }

            const float on = onSm.getNextValue();
            const float lead = leadSm.getNextValue();
            const float wet = mixSm.getNextValue() * on;
            if (! std::isfinite (hL)) hL = 0.f;
            if (! std::isfinite (hR)) hR = 0.f;

            const int o = start + i;
            if (monoOut)
            {
                outL[o] = outL[o] * lead + 0.5f * (hL + hR) * wet;
            }
            else
            {
                outL[o] = outL[o] * lead + hL * wet;
                outR[o] = outR[o] * lead + hR * wet;
            }
        }

        ringWrite = (ringWrite + n) & ringMask;
    }

    for (int v = 0; v < kMaxVoices; ++v)
        voiceMeter[(size_t) v].store (v < numVoices && numSamples > 0
                                        ? (float) std::sqrt (voices[(size_t) v].sumSq / numSamples) : 0.f,
                                      std::memory_order_relaxed);
    activeVoiceMeter.store (numVoices, std::memory_order_relaxed);

    // Fully faded out: stop, so the lead is bit-for-bit the tuner's output again
    if (! wantOn && onSm.getCurrentValue() <= 0.f && ! onSm.isSmoothing()
        && ! leadSm.isSmoothing() && leadSm.getCurrentValue() == 1.f)
    {
        active = false;
        for (auto& m : partMidiMeter) m.store (0.f);
        for (auto& m : voiceMeter) m.store (0.f);
        activeVoiceMeter.store (0);
    }
}
