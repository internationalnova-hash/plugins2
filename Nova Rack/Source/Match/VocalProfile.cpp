#include "VocalProfile.h"
#include "NovaVocalTuner.h"   // NovaPitchTracker (shared tuner core)

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace nr::match
{

namespace
{
    float toDb (double power) noexcept { return (float) (10.0 * std::log10 (power + 1.0e-12)); }

    float percentile (std::vector<float> v, float p)
    {
        if (v.empty()) return 0.f;
        const auto k = (size_t) juce::jlimit (0, (int) v.size() - 1, (int) std::round (p * (float) (v.size() - 1)));
        std::nth_element (v.begin(), v.begin() + (std::ptrdiff_t) k, v.end());
        return v[k];
    }

    float mean (const std::vector<float>& v)
    {
        if (v.empty()) return 0.f;
        double s = 0.0;
        for (auto x : v) s += x;
        return (float) (s / (double) v.size());
    }
}

float VocalProfile::bandCentreHz (int band) noexcept
{
    return 80.f * std::pow (2.f, (float) band / 3.f);   // 80 Hz .. ~16 kHz over 24 bands
}

VocalProfile analyseVocal (const juce::AudioBuffer<float>& audio, double sr)
{
    VocalProfile pr;
    const int numCh = audio.getNumChannels();
    const int n = juce::jmin (audio.getNumSamples(), (int) (40.0 * sr));
    if (numCh == 0 || n < (int) (1.0 * sr) || sr < 8000.0) return pr;

    // Mid (mono) and side
    std::vector<float> mid ((size_t) n), side ((size_t) n, 0.f);
    for (int i = 0; i < n; ++i)
    {
        if (numCh >= 2)
        {
            const float l = audio.getSample (0, i), r = audio.getSample (1, i);
            mid[(size_t) i] = 0.5f * (l + r);
            side[(size_t) i] = 0.5f * (l - r);
        }
        else
        {
            mid[(size_t) i] = audio.getSample (0, i);
        }
    }

    // ── 10 ms envelope (levels, peaks) ───────────────────────────────────
    const int hop = juce::jmax (1, (int) std::round (sr * 0.01));
    const int numHops = n / hop;
    std::vector<float> env ((size_t) numHops), envPeak ((size_t) numHops);
    for (int h = 0; h < numHops; ++h)
    {
        double s = 0.0; float pk = 0.f;
        for (int i = h * hop; i < (h + 1) * hop; ++i)
        {
            const float x = mid[(size_t) i];
            s += (double) x * x;
            pk = juce::jmax (pk, std::abs (x));
        }
        env[(size_t) h] = toDb (s / hop);
        envPeak[(size_t) h] = 20.f * std::log10 (pk + 1.0e-9f);
    }
    const float loud = percentile (env, 0.95f);
    const float activeFloor = juce::jmax (loud - 30.f, -70.f);
    auto isActive = [&] (int h) { return env[(size_t) h] > activeFloor; };

    int activeHops = 0;
    for (int h = 0; h < numHops; ++h) if (isActive (h)) ++activeHops;
    pr.activeSeconds = (float) activeHops * 0.01f;
    if (pr.activeSeconds < 0.5f) return pr;

    // ── Spectrum (2048-point FFT every 10 ms) ───────────────────────────
    constexpr int fftOrder = 11, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft (fftOrder);
    std::vector<float> window ((size_t) fftSize), buf ((size_t) fftSize * 2);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));

    std::array<std::pair<int, int>, VocalProfile::kBands> bins {};
    for (int b = 0; b < VocalProfile::kBands; ++b)
    {
        const float c = VocalProfile::bandCentreHz (b);
        const int lo = juce::jmax (1, (int) std::floor (c * std::pow (2.f, -1.f / 6.f) * fftSize / sr));
        const int hi = juce::jmin (fftSize / 2 - 1, juce::jmax (lo, (int) std::ceil (c * std::pow (2.f, 1.f / 6.f) * fftSize / sr)));
        bins[(size_t) b] = { lo, hi };
    }
    auto bandPower = [&] (float fLo, float fHi)
    {
        const int lo = juce::jmax (1, (int) (fLo * fftSize / sr)), hi = juce::jmin (fftSize / 2 - 1, (int) (fHi * fftSize / sr));
        double s = 0.0;
        for (int k = lo; k <= hi; ++k) s += (double) buf[(size_t) k];
        return s;
    };

    std::array<double, VocalProfile::kBands> bandSum {};
    std::vector<float> sibRatio;   // S-band power (dB) of each active frame
    std::vector<double> sibPower;
    for (int h = 0; h < numHops; ++h)
    {
        if (! isActive (h)) continue;
        const int start = h * hop + hop / 2 - fftSize / 2;
        std::fill (buf.begin(), buf.end(), 0.f);
        for (int i = 0; i < fftSize; ++i)
        {
            const int j = start + i;
            if (j >= 0 && j < n) buf[(size_t) i] = mid[(size_t) j] * window[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = 0; k <= fftSize / 2; ++k) buf[(size_t) k] = buf[(size_t) k] * buf[(size_t) k];   // power

        for (int b = 0; b < VocalProfile::kBands; ++b)
        {
            double s = 0.0;
            for (int k = bins[(size_t) b].first; k <= bins[(size_t) b].second; ++k) s += (double) buf[(size_t) k];
            bandSum[(size_t) b] += s / (double) (bins[(size_t) b].second - bins[(size_t) b].first + 1);
        }
        const double sp = bandPower (5000.f, 10000.f) / juce::jmax (1.0e-12, bandPower (300.f, 3000.f));
        sibRatio.push_back (toDb (sp));
        sibPower.push_back (sp);
    }
    double refLevel = 0.0; int refCount = 0;
    for (int b = 0; b < VocalProfile::kBands; ++b)
    {
        pr.spectrumDb[(size_t) b] = toDb (bandSum[(size_t) b] / activeHops);
        const float c = VocalProfile::bandCentreHz (b);
        if (c >= 250.f && c <= 4000.f) { refLevel += pr.spectrumDb[(size_t) b]; ++refCount; }
    }
    for (auto& v : pr.spectrumDb) v -= (float) (refLevel / juce::jmax (1, refCount));

    // Sibilance: how far the loudest S's stick out of the S band's own average
    // (a de-esser lowers the peaks; an EQ moves the whole band, so it doesn't count)
    {
        std::vector<float> sorted = sibRatio;
        std::sort (sorted.begin(), sorted.end());
        const size_t top = juce::jmax ((size_t) 1, sorted.size() / 20);
        double peak = 0.0, all = 0.0;
        for (size_t i = sorted.size() - top; i < sorted.size(); ++i) peak += std::pow (10.0, sorted[i] / 10.0);
        for (auto v : sibPower) all += v;
        pr.sibilanceDb = toDb (peak / (double) top) - toDb (all / juce::jmax ((size_t) 1, sibPower.size()));
    }

    // ── Dynamics: 100 ms windows ────────────────────────────────────────
    {
        std::vector<float> lv, crest, dens;
        for (int h = 0; h + 10 <= numHops; h += 10)
        {
            double s = 0.0; float pk = -200.f; int act = 0;
            for (int k = h; k < h + 10; ++k)
            {
                s += std::pow (10.0, env[(size_t) k] / 10.0);
                pk = juce::jmax (pk, envPeak[(size_t) k]);
                if (isActive (k)) ++act;
            }
            if (act < 8) continue;
            const float rms = toDb (s / 10.0);
            if (rms < loud - 15.f) continue;           // tails and breaths aren't the voice's dynamics
            lv.push_back (rms);
            crest.push_back (pk - rms);
        }
        for (int h = 0; h < numHops; ++h)
            if (isActive (h) && env[(size_t) h] > loud - 20.f)
                dens.push_back (envPeak[(size_t) h] - env[(size_t) h]);
        pr.rangeDb = percentile (lv, 0.9f) - percentile (lv, 0.1f);
        pr.crestDb = mean (crest);
        pr.densityDb = mean (dens);
    }

    // ── Tuning tightness ────────────────────────────────────────────────
    {
        NovaPitchTracker tracker;
        tracker.prepare (sr);
        std::vector<float> dev;
        for (int i = 0; i < n; ++i)
            if (tracker.push (mid[(size_t) i]) && tracker.getHz() > 0.f)
            {
                const float m = 69.f + 12.f * std::log2 (tracker.getHz() / 440.f);
                dev.push_back (100.f * std::abs (m - std::round (m)));
            }
        pr.tuningCents = dev.size() >= 20 ? percentile (dev, 0.5f) : 0.f;
    }

    // ── Width ───────────────────────────────────────────────────────────
    if (numCh >= 2)
    {
        double sm = 0.0, ss = 0.0;
        for (int h = 0; h < numHops; ++h)
        {
            if (! isActive (h)) continue;
            for (int i = h * hop; i < (h + 1) * hop; ++i)
            {
                sm += (double) mid[(size_t) i] * mid[(size_t) i];
                ss += (double) side[(size_t) i] * side[(size_t) i];
            }
        }
        pr.widthDb = juce::jmax (-60.f, toDb (ss) - toDb (sm));
    }

    // ── What happens after phrase ends: reverb tail and echoes ──────────
    {
        constexpr int kCurve = 90;                       // 900 ms
        std::array<double, kCurve> sum {};
        std::array<int, kCurve> cnt {};
        for (float minDrop : { 10.f, 7.f })
        {
            pr.phraseEnds = 0;
            sum.fill (0.0); cnt.fill (0);
            for (int t = 10; t + 6 < numHops; ++t)
            {
                double pre = 0.0;
                for (int k = t - 10; k < t; ++k) pre += env[(size_t) k];
                pre /= 10.0;
                double post = 0.0;
                for (int k = t + 2; k < t + 6; ++k) post += env[(size_t) k];
                post /= 4.0;
                if (pre < loud - 20.f || pre - post < minDrop) continue;

                ++pr.phraseEnds;
                for (int k = 0; k < kCurve && t + k < numHops; ++k)
                {
                    const float v = env[(size_t) (t + k)] - (float) pre;
                    if (k > 12 && v > -6.f) break;          // the next phrase started
                    sum[(size_t) k] += juce::jmax (-70.f, v);
                    ++cnt[(size_t) k];
                }
                t += 30;                                     // one end per 300 ms at most
            }
            if (pr.phraseEnds >= 3) break;
        }

        if (pr.phraseEnds >= 2)
        {
            std::array<float, kCurve> c {};
            for (int k = 0; k < kCurve; ++k) c[(size_t) k] = cnt[(size_t) k] > 0 ? (float) (sum[(size_t) k] / cnt[(size_t) k]) : -70.f;

            double s = 0.0; int m = 0;
            for (int k = 12; k <= 30; ++k) if (cnt[(size_t) k] > 0) { s += c[(size_t) k]; ++m; }
            pr.tailDb = m > 0 ? (float) (s / m) : -60.f;
            s = 0.0; m = 0;
            for (int k = 5; k <= 15; ++k) if (cnt[(size_t) k] > 0) { s += c[(size_t) k]; ++m; }
            pr.earlyTailDb = m > 0 ? (float) (s / m) : -60.f;

            // Slope by least squares over 120-450 ms
            double sx = 0, sy = 0, sxx = 0, sxy = 0; int q = 0;
            for (int k = 12; k <= 45; ++k)
                if (cnt[(size_t) k] > 0 && c[(size_t) k] > -65.f)
                {
                    const double x = k * 0.01;
                    sx += x; sy += c[(size_t) k]; sxx += x * x; sxy += x * c[(size_t) k]; ++q;
                }
            pr.tailSlope = q >= 5 ? (float) ((q * sxy - sx * sy) / juce::jmax (1e-9, q * sxx - sx * sx)) : 0.f;

        }
    }

    // ── Echo: in the gaps, does the level follow what was sung L ms earlier? ──
    {
        std::vector<float> r;
        for (int L = 8; L <= 95; ++L)
        {
            double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0; int q = 0;
            for (int t = L; t < numHops; ++t)
            {
                const float y = env[(size_t) t], x = env[(size_t) (t - L)];
                if (y > loud - 18.f || x < loud - 12.f) continue;   // gap now, singing then
                sx += x; sy += y; sxx += (double) x * x; syy += (double) y * y; sxy += (double) x * y; ++q;
            }
            const double vx = q * sxx - sx * sx, vy = q * syy - sy * sy;
            r.push_back (q >= 20 && vx > 0 && vy > 0 ? (float) ((q * sxy - sx * sy) / std::sqrt (vx * vy)) : 0.f);
        }
        float best = 0.f; int bestL = 0;
        for (int i = 4; i < (int) r.size(); ++i)   // from 120 ms
        {
            std::vector<float> around;
            for (int j = i - 25; j <= i + 25; ++j)
                if (j >= 0 && j < (int) r.size() && std::abs (j - i) > 5) around.push_back (r[(size_t) j]);
            const float contrast = r[(size_t) i] - percentile (around, 0.5f);
            if (contrast > best) { best = contrast; bestL = i + 8; }
        }
        pr.echoDb = 20.f * best;                     // correlation contrast, scaled to a dB-like number
        pr.echoMs = (float) bestL * 10.f;
    }

    pr.valid = true;
    return pr;
}

const char* stageName (int stage) noexcept
{
    static const char* names[kNumStages] = { "Tuning", "Tone (EQ)", "Dynamics", "Air & S's", "Warmth", "Width & doubles", "Delay", "Space (reverb)" };
    return names[juce::jlimit (0, kNumStages - 1, stage)];
}

float stageDistance (int stage, const VocalProfile& r, const VocalProfile& t) noexcept
{
    switch (stage)
    {
        case Tuning:   return std::abs (r.tuningCents - t.tuningCents) / 6.f;
        case Tone:
        {
            double s = 0.0;
            for (int b = 0; b < VocalProfile::kBands; ++b)
            {
                const float d = juce::jlimit (-30.f, 30.f, r.spectrumDb[(size_t) b] - t.spectrumDb[(size_t) b]);
                s += (double) d * d;
            }
            return (float) std::sqrt (s / VocalProfile::kBands) / 3.f;
        }
        case Dynamics: return std::abs (r.rangeDb - t.rangeDb) / 5.f + std::abs (r.crestDb - t.crestDb) / 1.5f;
        case Air:      return std::abs (r.sibilanceDb - t.sibilanceDb) / 2.5f;
        case Warmth:   return std::abs (r.densityDb - t.densityDb) / 1.5f;
        case Width:    return (r.widthDb < -40.f && t.widthDb < -40.f) ? 0.f
                                : std::abs (juce::jmax (-40.f, r.widthDb) - juce::jmax (-40.f, t.widthDb)) / 6.f;
        case Delay:
        {
            float d = std::abs (r.echoDb - t.echoDb) / 2.5f;
            if (r.echoDb > 3.f && t.echoDb > 3.f) d += std::abs (r.echoMs - t.echoMs) / 120.f;
            return d;
        }
        case Space:
        {
            if (r.phraseEnds < 2 || t.phraseEnds < 2) return 0.f;
            return std::abs (juce::jmax (-55.f, r.tailDb) - juce::jmax (-55.f, t.tailDb)) / 5.f
                 + std::abs (juce::jmax (-55.f, r.earlyTailDb) - juce::jmax (-55.f, t.earlyTailDb)) / 4.f
                 + std::abs (r.tailSlope - t.tailSlope) / 40.f;
        }
        default: return 0.f;
    }
}

std::array<float, kNumStages> stageScores (const VocalProfile& ref, const VocalProfile& test) noexcept
{
    std::array<float, kNumStages> s {};
    for (int i = 0; i < kNumStages; ++i)
    {
        const float d = stageDistance (i, ref, test);
        s[(size_t) i] = 100.f / (1.f + d * d);
    }
    return s;
}

float overallScore (const std::array<float, kNumStages>& s) noexcept
{
    static const float w[kNumStages] = { 0.10f, 0.28f, 0.14f, 0.08f, 0.08f, 0.08f, 0.10f, 0.14f };
    float sum = 0.f;
    for (int i = 0; i < kNumStages; ++i) sum += w[i] * s[(size_t) i];
    return sum;
}

} // namespace nr::match
