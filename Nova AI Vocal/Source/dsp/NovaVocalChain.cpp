#include "NovaVocalChain.h"

#include <cmath>

// ── Biquad (RBJ Audio EQ Cookbook) ───────────────────────────────────────

namespace
{
    constexpr double kTwoPi = 6.283185307179586;

    inline double clampHz (double sr, double hz) noexcept { return juce::jlimit (10.0, sr * 0.45, hz); }

    inline float onePoleCoef (double sr, double ms) noexcept
    {
        return (float) std::exp (-1.0 / (juce::jmax (0.01, ms) * 0.001 * sr));
    }

    inline float follow (float env, float in, float att, float rel) noexcept
    {
        const float c = in > env ? att : rel;
        return c * env + (1.f - c) * in;
    }

    inline float toDb (float lin) noexcept { return 20.f * std::log10 (juce::jmax (lin, 1.0e-6f)); }
    inline float fromDb (float db) noexcept { return std::pow (10.f, db * 0.05f); }
}

void NovaBiquad::set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
{
    const double inv = 1.0 / na0;
    b0 = (float) (nb0 * inv);
    b1 = (float) (nb1 * inv);
    b2 = (float) (nb2 * inv);
    a1 = (float) (na1 * inv);
    a2 = (float) (na2 * inv);
}

void NovaBiquad::setHighPass (double sr, double hz, double q) noexcept
{
    const double w0 = kTwoPi * clampHz (sr, hz) / sr, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

void NovaBiquad::setLowPass (double sr, double hz, double q) noexcept
{
    const double w0 = kTwoPi * clampHz (sr, hz) / sr, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

void NovaBiquad::setPeak (double sr, double hz, double q, double gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = kTwoPi * clampHz (sr, hz) / sr, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
}

void NovaBiquad::setHighShelf (double sr, double hz, double q, double gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = kTwoPi * clampHz (sr, hz) / sr, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double k = 2.0 * std::sqrt (A) * alpha;
    set (A * ((A + 1) + (A - 1) * c + k),
         -2 * A * ((A - 1) + (A + 1) * c),
         A * ((A + 1) + (A - 1) * c - k),
         (A + 1) - (A - 1) * c + k,
         2 * ((A - 1) - (A + 1) * c),
         (A + 1) - (A - 1) * c - k);
}

// ── Chain ────────────────────────────────────────────────────────────────

void NovaVocalChain::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;

    for (auto& s : macroSm) s.reset (sr, 0.05);
    for (auto* s : { &trimSm, &mudSm, &boxSm, &presSm, &airSm, &mixSm, &outSm })
        s->reset (sr, 0.05);
    delaySamplesSm.reset (sr, 0.2);

    sibAtt  = onePoleCoef (sr, 0.5);
    sibRel  = onePoleCoef (sr, 40.0);
    fullAtt = onePoleCoef (sr, 1.0);
    fullRel = onePoleCoef (sr, 60.0);
    grAtt   = onePoleCoef (sr, 1.0);
    grRel   = onePoleCoef (sr, 30.0);
    duckAtt = onePoleCoef (sr, 10.0);
    duckRel = onePoleCoef (sr, 300.0);
    lastCompAttMs = lastCompRelMs = -1.f;
    lastRoom = lastDamp = -1.f;

    // Longest delay: dotted quarter (1.5 beats) at 40 BPM = 2.25 s
    delaySize = (int) std::ceil (sr * 2.5) + 4;
    delayBufL.assign ((size_t) delaySize, 0.f);
    delayBufR.assign ((size_t) delaySize, 0.f);

    reverb.setSampleRate (sr);

    preDelaySize = (int) std::ceil (sr * 0.1) + 4;
    preDelayBufL.assign ((size_t) preDelaySize, 0.f);
    preDelayBufR.assign ((size_t) preDelaySize, 0.f);
    preDelaySm.reset (sr, 0.2);

    reset();
}

void NovaVocalChain::reset()
{
    for (auto* bank : { &hpf1, &hpf2, &mudEq, &boxEq, &presEq, &airEq, &deEssLp1, &deEssLp2, &deEssHp1, &deEssHp2,
                        &delayHp, &delayLp, &revHp })
        for (auto& f : *bank) f.reset();

    sibEnv = fullEnv = deEssGrState = 0.f;
    compGrState = 0.f;
    duckEnv = 0.f;
    std::fill (delayBufL.begin(), delayBufL.end(), 0.f);
    std::fill (delayBufR.begin(), delayBufR.end(), 0.f);
    delayWrite = 0;
    reverb.reset();
    std::fill (preDelayBufL.begin(), preDelayBufL.end(), 0.f);
    std::fill (preDelayBufR.begin(), preDelayBufR.end(), 0.f);
    preDelayWrite = 0;
    preDelaySm.setCurrentAndTargetValue (preDelayTargetSamples());
    lastRoom = lastDamp = lastTempoForRoom = -1.f;

    for (auto& s : macroSm) s.setCurrentAndTargetValue (s.getTargetValue());
    for (auto* s : { &trimSm, &mudSm, &boxSm, &presSm, &airSm, &mixSm, &outSm })
        s->setCurrentAndTargetValue (s->getTargetValue());

    const auto& V = activeVibe();
    delaySamplesSm.setCurrentAndTargetValue ((float) juce::jlimit (1.0, (double) delaySize - 4.0,
                                                                   V.delayBeats * 60.0 / tempoBpm * sr));
    updateCoefficients();
}

void NovaVocalChain::setMacros (const NovaVocalMacros& m) noexcept
{
    for (size_t i = 0; i < (size_t) kNumMacros; ++i)
        macroSm[i].setTargetValue (juce::jlimit (0.f, 1.f, m.v[i]));
}

void NovaVocalChain::setPlan (const NovaVocalPlan& p) noexcept
{
    const bool on = p.active;
    trimSm.setTargetValue (fromDb (on ? juce::jlimit (-24.f, 24.f, p.inputTrimDb) : 0.f));
    mudSm.setTargetValue  (on ? p.mudDb : 0.f);
    boxSm.setTargetValue  (on ? p.boxDb : 0.f);
    presSm.setTargetValue (on ? p.presenceDb : 0.f);
    airSm.setTargetValue  (on ? p.airDb : 0.f);
}

float NovaVocalChain::compGainDb (float levelDb, float thrDb, float ratio, float kneeDb) noexcept
{
    const float diff = levelDb - thrDb;
    const float slope = 1.f / ratio - 1.f;
    if (2.f * diff < -kneeDb) return 0.f;
    if (2.f * diff > kneeDb)  return slope * diff;
    const float t = diff + kneeDb * 0.5f;
    return slope * t * t / (2.f * kneeDb);
}

float NovaVocalChain::preDelayTargetSamples() const noexcept
{
    const double ms = juce::jlimit (5.0, 80.0, 60000.0 / juce::jmax (40.0, tempoBpm) / 16.0); // 1/64 note
    return (float) juce::jmin ((double) preDelaySize - 4.0, ms * 0.001 * sr);
}

void NovaVocalChain::updateCoefficients() noexcept
{
    const auto& V = activeVibe();

    const float clean  = macroSm[kClean].getCurrentValue();
    const float air    = macroSm[kAir].getCurrentValue();
    const float punch  = macroSm[kPunch].getCurrentValue();
    const float warmth = macroSm[kWarmth].getCurrentValue();
    const float space  = macroSm[kSpace].getCurrentValue();
    deEssAmount        = macroSm[kDeEss].getCurrentValue();

    // High-pass: 4th-order Butterworth (two biquads)
    const double hpfHz = 30.0 + clean * (V.hpfMaxHz - 30.0);
    for (int ch = 0; ch < 2; ++ch)
    {
        hpf1[(size_t) ch].setHighPass (sr, hpfHz, 0.5412);
        hpf2[(size_t) ch].setHighPass (sr, hpfHz, 1.3066);
    }

    // Tone EQ: Listen's corrective moves plus the vibe flavour from the macros
    const double mud  = juce::jlimit (-15.0, 6.0,  (double) mudSm.getCurrentValue() - clean * V.mudCutMaxDb);
    const double box  = juce::jlimit (-9.0,  4.0,  (double) boxSm.getCurrentValue());
    const double pres = juce::jlimit (-9.0,  12.0, (double) presSm.getCurrentValue() + air * V.presenceMaxDb);
    const double hi   = juce::jlimit (-9.0,  12.0, (double) airSm.getCurrentValue() + air * V.airMaxDb);

    for (size_t ch = 0; ch < 2; ++ch)
    {
        mudEq[ch].setPeak (sr, 300.0, 1.0, mud);
        boxEq[ch].setPeak (sr, 750.0, 1.4, box);
        presEq[ch].setPeak (sr, V.presenceHz, 0.8, pres);
        airEq[ch].setHighShelf (sr, V.airHz, 0.707, hi);
        deEssLp1[ch].setLowPass  (sr, V.deEssHz, 0.7071);
        deEssLp2[ch].setLowPass  (sr, V.deEssHz, 0.7071);
        deEssHp1[ch].setHighPass (sr, V.deEssHz, 0.7071);
        deEssHp2[ch].setHighPass (sr, V.deEssHz, 0.7071);
        delayHp[ch].setHighPass (sr, 300.0, 0.707);
        delayLp[ch].setLowPass (sr, V.delayLowpassHz, 0.707);
        revHp[ch].setHighPass (sr, 250.0, 0.707);
    }

    // Compressor. Listen trims the vocal to about -20 dBFS RMS, so these
    // thresholds land in a sensible place for any input level.
    compThr   = -8.f - 22.f * punch;
    compRatio = punch < 0.01f ? 1.f : 1.5f + punch * (V.compRatioMax - 1.5f);
    // Make-up gain: undo most of the reduction at a typical vocal peak
    // level (~-11 dBFS after Listen's trim) so the mix lands about as loud
    // as it came in.
    compMakeupDb = -compGainDb (-11.f, compThr, compRatio, 6.f) * 0.9f;
    if (V.compAttackMs != lastCompAttMs || V.compReleaseMs != lastCompRelMs)
    {
        compAttC = onePoleCoef (sr, V.compAttackMs);
        compRelC = onePoleCoef (sr, V.compReleaseMs);
        lastCompAttMs = V.compAttackMs;
        lastCompRelMs = V.compReleaseMs;
    }

    // Warmth
    satDrive = 1.f + V.satDriveMax * warmth;
    satMix   = juce::jlimit (0.f, 1.f, V.satMixMax * warmth);

    // Space
    delayFb     = V.delayFeedback;
    delayLevel  = space * V.delaySend;
    reverbLevel = space * V.reverbSend;
    delaySamplesSm.setTargetValue ((float) juce::jlimit (1.0, (double) delaySize - 4.0,
                                                         V.delayBeats * 60.0 / tempoBpm * sr));

    preDelaySm.setTargetValue (preDelayTargetSamples());

    // Reverb length follows tempo: slightly tighter on fast songs, longer on
    // slow ones (+-0.15 room per octave of tempo away from 120 BPM).
    const float tempoQ = (float) std::round (tempoBpm);
    if (V.reverbRoom != lastRoom || V.reverbDamping != lastDamp || tempoQ != lastTempoForRoom)
    {
        const float room = juce::jlimit (0.2f, 0.97f, V.reverbRoom + 0.15f * (float) std::log2 (120.0 / juce::jmax (40.0, tempoBpm)));
        juce::Reverb::Parameters p;
        p.roomSize   = room;
        p.damping    = V.reverbDamping;
        p.wetLevel   = 1.f;
        p.dryLevel   = 0.f;
        p.width      = 1.f;
        p.freezeMode = 0.f;
        reverb.setParameters (p);
        lastRoom = V.reverbRoom;
        lastDamp = V.reverbDamping;
        lastTempoForRoom = tempoQ;
    }
}

void NovaVocalChain::process (float* left, float* right, int numSamples) noexcept
{
    float revL[kSub], revR[kSub], dryL[kSub], dryR[kSub], duck[kSub];

    for (int start = 0; start < numSamples; start += kSub)
    {
        const int len = juce::jmin (kSub, numSamples - start);
        float* L = left + start;
        float* R = right + start;

        for (auto& s : macroSm) s.skip (len);
        for (auto* s : { &mudSm, &boxSm, &presSm, &airSm }) s->skip (len);
        updateCoefficients();

        for (int i = 0; i < len; ++i)
        {
            dryL[i] = L[i];
            dryR[i] = R[i];

            const float trim = trimSm.getNextValue();
            float xL = L[i] * trim;
            float xR = R[i] * trim;

            // High-pass + tone
            xL = airEq[0].process (presEq[0].process (boxEq[0].process (mudEq[0].process (hpf2[0].process (hpf1[0].process (xL))))));
            xR = airEq[1].process (presEq[1].process (boxEq[1].process (mudEq[1].process (hpf2[1].process (hpf1[1].process (xR))))));

            // De-esser: compare the sibilance band against the full signal
            // and turn down only that band when it jumps up toward it.
            const float lowL = deEssLp2[0].process (deEssLp1[0].process (xL));
            const float lowR = deEssLp2[1].process (deEssLp1[1].process (xR));
            const float bL   = deEssHp2[0].process (deEssHp1[0].process (xL));
            const float bR   = deEssHp2[1].process (deEssHp1[1].process (xR));
            sibEnv  = follow (sibEnv,  juce::jmax (std::abs (bL), std::abs (bR)), sibAtt, sibRel);
            fullEnv = follow (fullEnv, juce::jmax (std::abs (xL), std::abs (xR)), fullAtt, fullRel);

            float targetRed = 0.f;
            if (deEssAmount > 0.01f && sibEnv > 1.8e-3f)
            {
                const float relDb = toDb (sibEnv) - toDb (fullEnv);
                const float over  = relDb - (-2.f - 12.f * deEssAmount);
                if (over > 0.f)
                    targetRed = juce::jmin (over * 0.8f, 3.f + 15.f * deEssAmount);
            }
            deEssGrState = follow (deEssGrState, targetRed, grAtt, grRel);
            const float sibGain = fromDb (-deEssGrState);
            xL = lowL + bL * sibGain;
            xR = lowR + bR * sibGain;

            // Compressor (stereo-linked peak detector, soft knee)
            if (compRatio > 1.f)
            {
                const float levelDb = toDb (juce::jmax (std::abs (xL), std::abs (xR)));
                const float target  = compGainDb (levelDb, compThr, compRatio, 6.f);
                compGrState = target < compGrState ? compAttC * compGrState + (1.f - compAttC) * target
                                                   : compRelC * compGrState + (1.f - compRelC) * target;
                const float g = fromDb (compGrState + compMakeupDb);
                xL *= g;
                xR *= g;
            }
            else
            {
                compGrState = 0.f;
            }

            // Warmth: parallel soft saturation (unity gain at low levels)
            if (satMix > 0.001f)
            {
                xL += (std::tanh (satDrive * xL) / satDrive - xL) * satMix;
                xR += (std::tanh (satDrive * xR) / satDrive - xR) * satMix;
            }

            // Duck the effects while the vocal is loud
            duckEnv = follow (duckEnv, juce::jmax (std::abs (xL), std::abs (xR)), duckAtt, duckRel);
            duck[i] = 1.f - 0.5f * juce::jmin (1.f, duckEnv * 4.f);

            // Ping-pong delay
            const float dSamp = delaySamplesSm.getNextValue();
            float readPos = (float) delayWrite - dSamp;
            if (readPos < 0.f) readPos += (float) delaySize;
            const int   r0 = (int) readPos;
            const int   r1 = (r0 + 1) % delaySize;
            const float fr = readPos - (float) r0;
            const float dL = delayBufL[(size_t) r0] + fr * (delayBufL[(size_t) r1] - delayBufL[(size_t) r0]);
            const float dR = delayBufR[(size_t) r0] + fr * (delayBufR[(size_t) r1] - delayBufR[(size_t) r0]);
            const float fL = delayLp[0].process (delayHp[0].process (dL));
            const float fR = delayLp[1].process (delayHp[1].process (dR));

            delayBufL[(size_t) delayWrite] = 0.5f * (xL + xR) * delayLevel + fR * delayFb;
            delayBufR[(size_t) delayWrite] = fL * delayFb;
            if (++delayWrite >= delaySize) delayWrite = 0;

            // Reverb send, through the tempo pre-delay
            {
                preDelayBufL[(size_t) preDelayWrite] = xL;
                preDelayBufR[(size_t) preDelayWrite] = xR;
                float rp = (float) preDelayWrite - preDelaySm.getNextValue();
                if (rp < 0.f) rp += (float) preDelaySize;
                const int   p0 = (int) rp;
                const int   p1 = (p0 + 1) % preDelaySize;
                const float pf = rp - (float) p0;
                const float pL = preDelayBufL[(size_t) p0] + pf * (preDelayBufL[(size_t) p1] - preDelayBufL[(size_t) p0]);
                const float pR = preDelayBufR[(size_t) p0] + pf * (preDelayBufR[(size_t) p1] - preDelayBufR[(size_t) p0]);
                if (++preDelayWrite >= preDelaySize) preDelayWrite = 0;
                revL[i] = revHp[0].process (pL) * reverbLevel;
                revR[i] = revHp[1].process (pR) * reverbLevel;
            }

            L[i] = xL + fL * duck[i];
            R[i] = xR + fR * duck[i];
        }

        reverb.processStereo (revL, revR, len);

        for (int i = 0; i < len; ++i)
        {
            const float mix = mixSm.getNextValue();
            const float out = outSm.getNextValue();
            const float wetL = L[i] + revL[i] * duck[i];
            const float wetR = R[i] + revR[i] * duck[i];
            L[i] = (dryL[i] + (wetL - dryL[i]) * mix) * out;
            R[i] = (dryR[i] + (wetR - dryR[i]) * mix) * out;
        }
    }

    compGrMeter.store (-compGrState, std::memory_order_relaxed);
    deEssGrMeter.store (deEssGrState, std::memory_order_relaxed);
}
