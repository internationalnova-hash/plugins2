#include "NovaVocalTuner.h"

#include <algorithm>
#include <cmath>

// ── Pitch tracker ────────────────────────────────────────────────────────

void NovaPitchTracker::prepare (double sampleRate, bool lowLatency)
{
    sr     = sampleRate > 0 ? sampleRate : 48000.0;
    // Low-latency mode: shorter window and an 80 Hz floor (~13 ms instead of ~20 ms)
    W      = (int) std::round ((lowLatency ? 0.010 : 0.016) * sr); // integration window
    tauMax = (int) std::ceil (sr / (lowLatency ? 80.0 : 70.0));    // lowest note
    tauMin = tauMinFloor = (int) std::floor (sr / 1100.0);         // highest ~1100 Hz
    L      = W + tauMax;
    hop    = juce::jmax (32, (int) std::round (sr * 0.00267)); // ~2.7 ms

    int order = 1;
    while ((1 << order) < L) ++order;
    N = 1 << order;
    fft = std::make_unique<juce::dsp::FFT> (order);

    // Estimate is centred on the frame middle; the 3-hop median adds one
    // hop, and the tuner interpolates from the previous estimate to the
    // newest across the following hop, which adds one more.
    delay = L / 2 + 2 * hop;

    ring.assign ((size_t) L, 0.f);
    frame.assign ((size_t) L, 0.f);
    prefix.assign ((size_t) L + 1, 0.0);
    a.assign ((size_t) N, {}); b.assign ((size_t) N, {});
    A.assign ((size_t) N, {}); B.assign ((size_t) N, {}); r.assign ((size_t) N, {});
    dp.assign ((size_t) tauMax + 2, 1.f);
    reset();
}

void NovaPitchTracker::reset() noexcept
{
    std::fill (ring.begin(), ring.end(), 0.f);
    ringPos = hopCount = filled = 0;
    raw[0] = raw[1] = raw[2] = 0.f;
    rawIdx = 0;
    lastTau = 0.f;
    unvoicedHops = 0;
    outHz = 0.f;
}

bool NovaPitchTracker::push (float x) noexcept
{
    ring[(size_t) ringPos] = x;
    if (++ringPos >= L) ringPos = 0;
    if (filled < L) ++filled;

    if (++hopCount < hop) return false;
    hopCount = 0;
    if (filled == L) analyse();
    return true;
}

void NovaPitchTracker::analyse() noexcept
{
    for (int i = 0; i < L; ++i)
        frame[(size_t) i] = ring[(size_t) ((ringPos + i) % L)];

    prefix[0] = 0;
    for (int i = 0; i < L; ++i) prefix[(size_t) i + 1] = prefix[(size_t) i] + (double) frame[(size_t) i] * frame[(size_t) i];

    const double E1 = prefix[(size_t) W];
    float hz = 0.f;

    if (E1 / W > 1.0e-6) // about -60 dBFS
    {
        for (int i = 0; i < N; ++i)
        {
            a[(size_t) i] = i < W ? frame[(size_t) i] : 0.f;
            b[(size_t) i] = i < L ? frame[(size_t) i] : 0.f;
        }
        fft->perform (a.data(), A.data(), false);
        fft->perform (b.data(), B.data(), false);
        for (int k = 0; k < N; ++k) A[(size_t) k] = std::conj (A[(size_t) k]) * B[(size_t) k];
        fft->perform (A.data(), r.data(), true);

        const double scale = std::abs (r[0].real()) > 1e-20 ? E1 / r[0].real() : 1.0;
        double cum = 0;
        dp[0] = 1.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const double E2 = prefix[(size_t) (tau + W)] - prefix[(size_t) tau];
            const double d  = juce::jmax (0.0, E1 + E2 - 2.0 * r[(size_t) tau].real() * scale);
            cum += d;
            dp[(size_t) tau] = cum > 0 ? (float) (d * tau / cum) : 1.f;
        }

        auto localMin = [this] (int t)
        {
            t = juce::jlimit (tauMin, tauMax - 1, t);
            while (t + 1 < tauMax && dp[(size_t) t + 1] < dp[(size_t) t]) ++t;
            while (t - 1 > tauMin && dp[(size_t) t - 1] < dp[(size_t) t]) --t;
            return t;
        };

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (dp[(size_t) tau] < 0.15f) { best = localMin (tau); break; }

        if (best < 0)
        {
            best = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (dp[(size_t) tau] < dp[(size_t) best]) best = tau;
        }

        // Octave continuity: prefer the candidate that matches the note we
        // were just on, if it is nearly as periodic.
        if (lastTau > 0.f)
        {
            const float dbl = (float) best * 2.f, half = (float) best * 0.5f;
            if (std::abs (dbl - lastTau) < 0.08f * lastTau && dbl < (float) tauMax)
            {
                const int c = localMin ((int) std::round (dbl));
                if (dp[(size_t) c] < 0.3f) best = c;
            }
            else if (std::abs (half - lastTau) < 0.08f * lastTau && half > (float) tauMin)
            {
                const int c = localMin ((int) std::round (half));
                if (dp[(size_t) c] < dp[(size_t) best] + 0.12f) best = c;
            }
        }

        if (dp[(size_t) best] < voicing)
        {
            double tauF = best;
            if (best > tauMin && best + 1 < tauMax)
            {
                const double y0 = dp[(size_t) best - 1], y1 = dp[(size_t) best], y2 = dp[(size_t) best + 1];
                const double den = y0 - 2.0 * y1 + y2;
                if (std::abs (den) > 1e-12) tauF += juce::jlimit (-1.0, 1.0, 0.5 * (y0 - y2) / den);
            }
            hz = (float) (sr / tauF);
            lastTau = (float) tauF;
            unvoicedHops = 0;
        }
    }

    if (hz <= 0.f && ++unvoicedHops * hop > (int) (0.05 * sr))
        lastTau = 0.f;

    raw[rawIdx] = hz;
    rawIdx = (rawIdx + 1) % 3;

    const float r0 = raw[(rawIdx + 2) % 3], r1 = raw[(rawIdx + 1) % 3], r2 = raw[rawIdx]; // newest..oldest
    const int voiced = (r0 > 0.f) + (r1 > 0.f) + (r2 > 0.f);

    if (voiced == 3)
        outHz = std::max (std::min (r0, r1), std::min (std::max (r0, r1), r2)); // median
    else if (voiced == 2)
        outHz = r0 > 0.f ? r0 : r1; // newest voiced value (bridges a single dropout)
    else
        outHz = 0.f;
}

// ── Tuner ────────────────────────────────────────────────────────────────

int NovaVocalTuner::scaleMaskFor (bool minor) noexcept
{
    return minor ? 0x5AD   // 0 2 3 5 7 8 10
                 : 0xAB5;  // 0 2 4 5 7 9 11
}

NovaVocalTuner::Params NovaVocalTuner::fromTuneKnob (float t, int keyPc, int scaleMask) noexcept
{
    Params p;
    p.enabled   = t > 0.001f;
    p.amount    = juce::jlimit (0.f, 1.f, t / 0.25f);
    const float speed = juce::jlimit (0.f, 1.f, (t - 0.25f) / 0.75f);
    p.retuneMs  = 200.f * (1.f - speed) * (1.f - speed);
    p.keyPc     = keyPc;
    p.scaleMask = scaleMask;
    return p;
}

void NovaVocalTuner::prepare (double sampleRate, int, bool lowLatency)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    tracker.prepare (sr, lowLatency);
    auto coef = [this] (double ms) { return (float) std::exp (-1.0 / (ms * 0.001 * sr)); };
    fastAtt = coef (1.0);  fastRel = coef (20.0);
    slowAtt = coef (40.0); slowRel = coef (200.0);
    latency = tracker.getDelaySamples();
    hopSize = tracker.getHopSize();
    dwellHops = juce::jmax (1, (int) std::round (0.02 * sr / hopSize)); // ~20 ms

    size = 1;
    while (size < latency + 4 * tracker.getMaxPeriodSamples() + 4096) size <<= 1;
    mask = size - 1;
    bufL.assign ((size_t) size, 0.f);
    bufR.assign ((size_t) size, 0.f);
    bufM.assign ((size_t) size, 0.f);
    reset();
}

void NovaVocalTuner::reset() noexcept
{
    tracker.reset();
    std::fill (bufL.begin(), bufL.end(), 0.f);
    std::fill (bufR.begin(), bufR.end(), 0.f);
    std::fill (bufM.begin(), bufM.end(), 0.f);
    w = 0;
    rp = (double) -latency;
    fadePos = -1;
    heldNote = -1000;
    pendingNote = -1000;
    pendingCount = 0;
    unvoicedSamples = 1 << 20;
    corr = 0.f;
    prevMidi = curMidi = 0.f;
    sinceEstimate = 0;
    period = 0.f;
    midiSlow = 0.f; slowNote = -1000.f;
    fastEnv = slowEnv = 0.f;
}

float NovaVocalTuner::readCubic (const std::vector<float>& buf, double pos) const noexcept
{
    const double fl = std::floor (pos);
    const juce::int64 i = (juce::int64) fl;
    const float f = (float) (pos - fl);
    const juce::int64 m = mask;
    const float x0 = buf[(size_t) ((i - 1) & m)], x1 = buf[(size_t) (i & m)];
    const float x2 = buf[(size_t) ((i + 1) & m)], x3 = buf[(size_t) ((i + 2) & m)];
    const float c1 = 0.5f * (x2 - x0);
    const float c2 = x0 - 2.5f * x1 + 2.f * x2 - 0.5f * x3;
    const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
    return ((c3 * f + c2) * f + c1) * f + x1;
}

// Finds the jump length near one period that best matches the waveform,
// so the splice lands on the same point of the next/previous cycle.
double NovaVocalTuner::refinePeriod (double fromPos, int dir, double P) const noexcept
{
    const juce::int64 base = (juce::int64) std::floor (fromPos);
    const juce::int64 m = mask;
    const int win = juce::jlimit (32, 128, (int) P);
    int lo = juce::jmax (8, (int) (P * 0.85)), hi = (int) (P * 1.15) + 1;
    if (dir > 0) hi = (int) juce::jmin ((juce::int64) hi, w - 3 - win - base);
    if (hi < lo) return P;

    double e1 = 0;
    for (int j = 0; j < win; ++j) { const float v = bufM[(size_t) ((base + j) & m)]; e1 += (double) v * v; }

    int bestQ = (int) std::round (P);
    double bestC = -2.0;
    for (int q = lo; q <= hi; ++q)
    {
        double c = 0, e2 = 0;
        const juce::int64 o = base + dir * q;
        for (int j = 0; j < win; ++j)
        {
            const float x = bufM[(size_t) ((base + j) & m)], y = bufM[(size_t) ((o + j) & m)];
            c += (double) x * y; e2 += (double) y * y;
        }
        const double nc = c / std::sqrt (e1 * e2 + 1e-20);
        if (nc > bestC) { bestC = nc; bestQ = q; }
    }
    return (double) bestQ;
}

void NovaVocalTuner::startJump (double newPos, int fade) noexcept
{
    rpB = newPos;
    fadeLen = juce::jmax (8, fade);
    fadePos = 0;
}

int NovaVocalTuner::chooseNote (float midi) noexcept
{
    auto inScale = [this] (int n) { return ((params.scaleMask >> (((n - params.keyPc) % 12 + 12) % 12)) & 1) != 0; };

    int nearest = (int) std::round (midi);
    float nd = 1e9f;
    for (int n = (int) std::floor (midi) - 7; n <= (int) std::ceil (midi) + 7; ++n)
        if (inScale (n) && std::abs (midi - (float) n) < nd) { nd = std::abs (midi - (float) n); nearest = n; }

    if (heldNote > -1000 && inScale (heldNote) && nearest != heldNote)
    {
        const float fromHeld = std::abs (midi - (float) heldNote);

        // Hysteresis: stay on the current note until the voice is clearly
        // past the halfway point. Neighbours a whole step apart (D-E in
        // G major) get a wider margin, otherwise a singer hovering on the
        // in-between note (D#) makes the output flutter D-E-D-E.
        const bool wideGap = std::abs (nearest - heldNote) >= 2;
        if (fromHeld - nd < (wideGap ? 0.55f : 0.24f))
        {
            pendingNote = -1000;
            return heldNote;
        }

        // Dwell: switching to a neighbour needs ~20 ms of consistent
        // evidence - unless the voice has moved more than a semitone, which
        // is a real melody move and snaps immediately.
        if (fromHeld < 1.0f)
        {
            if (nearest != pendingNote) { pendingNote = nearest; pendingCount = 0; }
            if (++pendingCount < dwellHops) return heldNote;
        }
    }

    pendingNote = -1000;
    heldNote = nearest;
    return nearest;
}

void NovaVocalTuner::process (float* left, float* right, int numSamples) noexcept
{
    const float aRet = params.retuneMs < 0.05f ? 0.f : (float) std::exp (-1.0 / (params.retuneMs * 0.001 * sr));
    const float aRel = (float) std::exp (-1.0 / (0.03 * sr));
    const int recentlyVoiced = (int) (0.05 * sr);
    tracker.setVoicingThreshold (params.voicingThreshold);
    tracker.setMaxHz (params.maxHz);
    const float vibCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi
                                            * juce::jmax (0.5, params.vibratoRateHz * 0.35) / sr);
    const float humanTol = params.humanize * 0.3f; // semitones
    double ratio = 1.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i], inR = right[i];
        const float mono = 0.5f * (inL + inR);
        const juce::int64 wi = w & (juce::int64) mask;
        bufL[(size_t) wi] = inL;
        bufR[(size_t) wi] = inR;
        bufM[(size_t) wi] = mono;
        ++w;

        if (tracker.push (mono))
        {
            const float hz = tracker.getHz();
            prevMidi = curMidi;
            sinceEstimate = 0;
            if (hz > 0.f)
            {
                curMidi = 69.f + 12.f * std::log2 (hz / 440.f);
                currentNote = params.flattenMidi >= 0 ? params.flattenMidi
                            : (params.enabled ? chooseNote (curMidi) : (int) std::round (curMidi));
                period = (float) (sr / hz);
                unvoicedSamples = 0;
                inMidiMeter.store (curMidi, std::memory_order_relaxed);
                targetMeter.store ((float) currentNote, std::memory_order_relaxed);
            }
            else
            {
                curMidi = 0.f;
                inMidiMeter.store (0.f, std::memory_order_relaxed);
                targetMeter.store (0.f, std::memory_order_relaxed);
            }
        }

        // Pitch at the read position: glide from the previous estimate to the
        // newest across one hop (no staircase error on fast vibrato/slides).
        float targetCorr = 0.f;
        if (params.flattenMidi >= 0 && curMidi > 0.f)
        {
            // Flatten: the whole correction is the distance to the one note
            float midiNow = curMidi;
            if (prevMidi > 0.f && std::abs (curMidi - prevMidi) < 1.f)
                midiNow = prevMidi + (curMidi - prevMidi) * juce::jmin (1.f, (float) sinceEstimate / (float) hopSize);
            targetCorr = juce::jlimit (-24.f, 24.f, (float) params.flattenMidi - midiNow);
        }
        else if (params.enabled && curMidi > 0.f)
        {
            float midiNow = curMidi;
            if (prevMidi > 0.f && std::abs (curMidi - prevMidi) < 1.f)
                midiNow = prevMidi + (curMidi - prevMidi) * juce::jmin (1.f, (float) sinceEstimate / (float) hopSize);

            // Natural vibrato: correct only the slow-moving note centre, so
            // the singer's own vibrato rides on top of the corrected note.
            if (midiSlow <= 0.f || slowNote != (float) currentNote) { midiSlow = midiNow; slowNote = (float) currentNote; }
            else midiSlow = vibCoef * midiSlow + (1.f - vibCoef) * midiNow;
            const float midiForCorr = midiNow + (midiSlow - midiNow) * params.vibratoPreserve;

            float dev = (float) currentNote - midiForCorr;

            // Humanize: notes already within the tolerance are only gently
            // nudged; anything further out is still fully corrected.
            if (humanTol > 0.f && std::abs (dev) < humanTol)
                dev *= std::abs (dev) / humanTol;

            targetCorr = dev * params.amount;
        }
        else
        {
            midiSlow = 0.f;
        }
        ++sinceEstimate;

        if (params.flattenMidi < 0)
            targetCorr += params.fixedShiftSemis;

        // Transient hold: ease off the correction while a note attack (a
        // consonant or onset) passes the read position.
        if (params.transientHold > 0.f)
        {
            const float x = std::abs (bufM[(size_t) ((w - latency) & (juce::int64) mask)]);
            fastEnv = x > fastEnv ? fastAtt * fastEnv + (1.f - fastAtt) * x : fastRel * fastEnv + (1.f - fastRel) * x;
            slowEnv = x > slowEnv ? slowAtt * slowEnv + (1.f - slowAtt) * x : slowRel * slowEnv + (1.f - slowRel) * x;
            const float onset = juce::jlimit (0.f, 1.f, (fastEnv / (slowEnv + 1e-5f) - 1.3f) / 1.5f);
            targetCorr *= 1.f - params.transientHold * onset;
        }

        const bool voiced = unvoicedSamples < recentlyVoiced;
        if (unvoicedSamples < (1 << 20)) ++unvoicedSamples;
        if (unvoicedSamples > (int) (0.08 * sr)) heldNote = -1000;

        const float coef = voiced ? aRet : aRel;
        corr = coef * corr + (1.f - coef) * targetCorr;
        ratio = std::exp2 ((double) corr / 12.0);

        // Read (with crossfade if a jump is in progress)
        float oL = readCubic (bufL, rp), oR = readCubic (bufR, rp);
        if (fadePos >= 0)
        {
            const float g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) fadePos / (float) fadeLen);
            oL += (readCubic (bufL, rpB) - oL) * g;
            oR += (readCubic (bufR, rpB) - oR) * g;
            rpB += ratio;
            if (++fadePos >= fadeLen) { rp = rpB; fadePos = -1; }
            else rp += ratio;
        }
        else
        {
            rp += ratio;
        }

        left[i] = oL;
        right[i] = oR;

        // Keep the read head within half a period of the fixed latency
        if (fadePos < 0)
        {
            const double d = (double) w - rp;
            if (voiced && period > 0.f)
            {
                const double half = 0.5 * period;
                const int fade = juce::jlimit (64, 256, (int) period);
                if (d < latency - half)      startJump (rp - refinePeriod (rp, -1, period), fade);
                else if (d > latency + half) startJump (rp + refinePeriod (rp, +1, period), fade);
            }
            else if (std::abs (d - latency) > 32.0)
            {
                startJump ((double) w - latency, 256);
            }
        }
    }

    ratioMeter.store ((float) ratio, std::memory_order_relaxed);
}

// ── Key estimate (offline, for Listen) ───────────────────────────────────

NovaKeyEstimate estimateNovaKey (const float* mono, int numSamples, double sampleRate)
{
    NovaKeyEstimate k;
    NovaPitchTracker t;
    t.prepare (sampleRate);

    double hist[12] = {};
    int voicedHops = 0, hops = 0;
    for (int i = 0; i < numSamples; ++i)
        if (t.push (mono[i]))
        {
            ++hops;
            const float hz = t.getHz();
            if (hz <= 0.f) continue;
            const int n = (int) std::round (69.f + 12.f * std::log2 (hz / 440.f));
            hist[(n % 12 + 12) % 12] += 1.0;
            ++voicedHops;
        }

    const double voicedSec = hops > 0 ? (double) voicedHops / hops * numSamples / sampleRate : 0.0;
    if (voicedSec < 2.0) return k;

    // A key needs evidence: a take that only uses two or three notes (e.g.
    // A and B) fits several keys equally well, so don't guess.
    int distinct = 0;
    for (double h : hist) if (h >= 0.05 * voicedHops) ++distinct;
    if (distinct < 4) return k;

    // Krumhansl-Kessler key profiles
    static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    auto corrWith = [&hist] (const double* prof, int key)
    {
        double mh = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mh += hist[i]; mp += prof[i]; }
        mh /= 12; mp /= 12;
        double c = 0, vh = 0, vp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double h = hist[(i + key) % 12] - mh, p = prof[i] - mp;
            c += h * p; vh += h * h; vp += p * p;
        }
        return c / std::sqrt (vh * vp + 1e-20);
    };

    double best = -2;
    for (int key = 0; key < 12; ++key)
        for (int m = 0; m < 2; ++m)
        {
            const double c = corrWith (m ? mnr : maj, key);
            if (c > best) { best = c; k.keyPc = key; k.minor = m == 1; }
        }

    k.confidence = (float) best;
    k.valid = best > 0.55;
    return k;
}
