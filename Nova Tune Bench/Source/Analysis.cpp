#include "Analysis.h"

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>

namespace
{
    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.f;
        const size_t m = v.size() / 2;
        std::nth_element (v.begin(), v.begin() + (long) m, v.end());
        return v[m];
    }

    std::vector<float> toMono (const juce::AudioBuffer<float>& b, int offset = 0)
    {
        const int n = juce::jmax (0, b.getNumSamples() - offset);
        std::vector<float> m ((size_t) n, 0.f);
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            const float* x = b.getReadPointer (ch) + offset;
            for (int i = 0; i < n; ++i) m[(size_t) i] += x[i] / (float) b.getNumChannels();
        }
        return m;
    }
}

int scaleMaskFromName (const juce::String& name)
{
    const auto n = name.toLowerCase();
    if (n.startsWith ("maj")) return (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);
    if (n.startsWith ("min")) return (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10);
    return 0xFFF;
}

// ── Reference pitch tracker: YIN with an FFT difference function ─────────

PitchTrack trackPitch (const float* x, int numSamples, double sr)
{
    PitchTrack t;
    const int W      = (int) std::round (0.025 * sr);
    const int tauMax = (int) std::ceil (sr / 60.0);
    const int tauMin = (int) std::floor (sr / 1100.0);
    const int hop    = (int) std::round (0.005 * sr);
    t.hopSeconds = hop / sr;

    int order = 1;
    while ((1 << order) < W + tauMax) ++order;
    const int N = 1 << order;
    juce::dsp::FFT fft (order);

    std::vector<double> P ((size_t) numSamples + 1, 0.0);
    for (int i = 0; i < numSamples; ++i) P[(size_t) i + 1] = P[(size_t) i] + (double) x[i] * x[i];

    std::vector<std::complex<float>> a ((size_t) N), b ((size_t) N), A ((size_t) N), B ((size_t) N), r ((size_t) N);
    std::vector<float> dp ((size_t) tauMax + 2);

    float maxRms = -200.f;
    for (int s = 0; s + W + tauMax <= numSamples; s += hop)
    {
        PitchFrame f;
        const double E1 = P[(size_t) (s + W)] - P[(size_t) s];
        f.rmsDb = (float) (10.0 * std::log10 (juce::jmax (E1 / W, 1e-14)));
        maxRms = juce::jmax (maxRms, f.rmsDb);

        if (f.rmsDb > -70.f)
        {
            std::fill (a.begin(), a.end(), std::complex<float> {});
            std::fill (b.begin(), b.end(), std::complex<float> {});
            for (int j = 0; j < W; ++j) a[(size_t) j] = x[s + j];
            for (int j = 0; j < W + tauMax; ++j) b[(size_t) j] = x[s + j];
            fft.perform (a.data(), A.data(), false);
            fft.perform (b.data(), B.data(), false);
            for (int k = 0; k < N; ++k) A[(size_t) k] = std::conj (A[(size_t) k]) * B[(size_t) k];
            fft.perform (A.data(), r.data(), true);

            const double scale = std::abs (r[0].real()) > 1e-20 ? E1 / r[0].real() : 1.0;

            dp[0] = 1.f;
            double cum = 0;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                const double E2 = P[(size_t) (s + tau + W)] - P[(size_t) (s + tau)];
                const double d  = juce::jmax (0.0, E1 + E2 - 2.0 * r[(size_t) tau].real() * scale);
                cum += d;
                dp[(size_t) tau] = cum > 0 ? (float) (d * tau / cum) : 1.f;
            }

            int best = -1;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (dp[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 < tauMax && dp[(size_t) tau + 1] < dp[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            if (best < 0)
            {
                best = tauMin;
                for (int tau = tauMin; tau < tauMax; ++tau)
                    if (dp[(size_t) tau] < dp[(size_t) best]) best = tau;
            }

            double tauF = best;
            if (best > tauMin && best + 1 < tauMax)
            {
                const double y0 = dp[(size_t) best - 1], y1 = dp[(size_t) best], y2 = dp[(size_t) best + 1];
                const double den = y0 - 2 * y1 + y2;
                if (std::abs (den) > 1e-12) tauF += 0.5 * (y0 - y2) / den;
            }

            f.aperiodicity = dp[(size_t) best];
            f.midi = (float) (69.0 + 12.0 * std::log2 ((sr / tauF) / 440.0));
        }
        t.frames.push_back (f);
    }

    const float gate = juce::jmax (-50.f, maxRms - 40.f);
    for (auto& f : t.frames)
        f.voiced = f.rmsDb > gate && f.aperiodicity < 0.25f && f.midi > 30.f && f.midi < 90.f;

    // 5-frame median on voiced runs (removes isolated octave blips)
    std::vector<float> sm (t.frames.size());
    for (size_t i = 0; i < t.frames.size(); ++i)
    {
        if (! t.frames[i].voiced) { sm[i] = 0.f; continue; }
        std::vector<float> w;
        for (long k = (long) i - 2; k <= (long) i + 2; ++k)
            if (k >= 0 && k < (long) t.frames.size() && t.frames[(size_t) k].voiced) w.push_back (t.frames[(size_t) k].midi);
        sm[i] = median (w);
    }
    for (size_t i = 0; i < t.frames.size(); ++i) t.frames[i].midi = sm[i];

    return t;
}

// ── Latency ──────────────────────────────────────────────────────────────

int measureLatencySamples (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output, double sr)
{
    const auto in = toMono (input), out = toMono (output);
    const int blk = juce::jmax (1, (int) std::round (sr / 2000.0)); // 0.5 ms resolution
    auto env = [blk] (const std::vector<float>& v)
    {
        std::vector<float> e (v.size() / (size_t) blk);
        for (size_t i = 0; i < e.size(); ++i)
        {
            double s = 0;
            for (int j = 0; j < blk; ++j) s += std::abs (v[i * (size_t) blk + (size_t) j]);
            e[i] = (float) (s / blk);
        }
        double mean = 0; for (auto f : e) mean += f; mean /= juce::jmax ((size_t) 1, e.size());
        for (auto& f : e) f -= (float) mean;
        return e;
    };
    const auto ei = env (in), eo = env (out);
    const int maxLag = (int) (0.4 * sr / blk);
    int bestLag = 0; double best = -1e30;
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double s = 0;
        const size_t n = std::min (ei.size(), eo.size() > (size_t) lag ? eo.size() - (size_t) lag : 0);
        for (size_t i = 0; i < n; ++i) s += (double) ei[i] * eo[i + (size_t) lag];
        if (s > best) { best = s; bestLag = lag; }
    }
    return bestLag * blk;
}

// ── Scoring ──────────────────────────────────────────────────────────────

TuneScore scoreTuning (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output,
                       double sr, int key, int mask, int reportedLatency, bool alignByMeasured, int knownLatency,
                       float expectShift, int expectFixed)
{
    TuneScore sc;
    const int measured = knownLatency >= 0 ? knownLatency : measureLatencySamples (input, output, sr);
    sc.reportedLatencyMs = (float) (1000.0 * reportedLatency / sr);
    sc.measuredLatencyMs = (float) (1000.0 * measured / sr);
    const int lag = alignByMeasured ? measured : reportedLatency;

    const auto in  = toMono (input);
    const auto out = toMono (output, lag);
    const int n = (int) std::min (in.size(), out.size());
    if (n <= 0) return sc;

    // Level
    double ein = 0, eout = 0;
    for (int i = 0; i < n; ++i) { ein += (double) in[(size_t) i] * in[(size_t) i]; eout += (double) out[(size_t) i] * out[(size_t) i]; }
    sc.levelDb = (float) (10.0 * std::log10 (juce::jmax (eout, 1e-20) / juce::jmax (ein, 1e-20)));

    const auto ti = trackPitch (in.data(), n, sr);
    const auto to = trackPitch (out.data(), n, sr);
    const size_t F = std::min (ti.frames.size(), to.frames.size());
    const double hopMs = ti.hopSeconds * 1000.0;
    const int settleFrames = (int) std::ceil (60.0 / hopMs);
    const int maxGapFrames = (int) std::ceil (120.0 / hopMs); // slides blur pitch briefly; still the same phrase

    auto nearestTarget = [&] (float midi, bool& ambiguous)
    {
        int best = -1, second = -1;
        float bd = 1e9f, sd = 1e9f;
        for (int m = (int) std::floor (midi) - 7; m <= (int) std::ceil (midi) + 7; ++m)
        {
            if (! ((mask >> (((m - key) % 12 + 12) % 12)) & 1)) continue;
            const float d = std::abs (midi - (float) m);
            if (d < bd) { second = best; sd = bd; best = m; bd = d; }
            else if (d < sd) { second = m; sd = d; }
        }
        ambiguous = (sd - bd) < 0.3f;
        juce::ignoreUnused (second);
        return best;
    };

    std::vector<float> errs, rough, segStd, snaps, segVals;
    int inTune = 0, settled = 0, wrong = 0, octave = 0, judged = 0, sung = 0, lost = 0;

    int segTarget = -1000, segStart = 0, lastVoiced = -100;
    bool snapPending = false; int snapStart = 0, snapRun = 0;

    auto closeSegment = [&]
    {
        if ((int) segVals.size() >= (int) (150.0 / hopMs))
        {
            double m = 0; for (auto v : segVals) m += v; m /= (double) segVals.size();
            double var = 0; for (auto v : segVals) var += (v - m) * (v - m);
            segStd.push_back ((float) std::sqrt (var / (double) segVals.size()));
        }
        segVals.clear();
    };

    for (size_t i = 0; i < F; ++i)
    {
        const auto& fi = ti.frames[i];
        const auto& fo = to.frames[i];
        if (! fi.voiced || fi.aperiodicity > 0.15f) continue;

        ++sung;
        if (! fo.voiced) { ++lost; continue; }
        rough.push_back (fo.aperiodicity - fi.aperiodicity);

        bool amb = false;
        float targetF;
        if (expectFixed >= 0)            targetF = (float) expectFixed;
        else if (expectShift != 0.f)     targetF = fi.midi + expectShift;
        else { targetF = (float) nearestTarget (fi.midi, amb); if (amb) continue; }
        const int target = (int) std::round (targetF);

        // Segment bookkeeping (a held target note)
        if (target != segTarget || (int) i - lastVoiced > maxGapFrames)
        {
            // An unresolved snap only counts if the note was held long
            // enough to judge - the in-between notes a slide passes through
            // are not notes the singer meant.
            if (snapPending && (lastVoiced - segStart) * hopMs >= 100.0) { snaps.push_back (300.f); ++sc.noteChanges; }
            closeSegment();
            const bool isChange = segTarget != -1000 && (int) i - lastVoiced <= maxGapFrames;
            segTarget = target; segStart = (int) i;
            snapPending = isChange; snapStart = (int) i; snapRun = 0;
        }
        lastVoiced = (int) i;

        const float diff = fo.midi - targetF;
        const float cents = diff * 100.f;

        // Snap speed: time from entering a new note until the output sits
        // within 15 cents of it for 30 ms
        if (snapPending)
        {
            snapRun = std::abs (cents) < 15.f ? snapRun + 1 : 0;
            if (snapRun >= 6)
            {
                snaps.push_back ((float) (((int) i - 5 - snapStart) * hopMs));
                snapPending = false; ++sc.noteChanges;
            }
            else if (((int) i - snapStart) * hopMs > 300.0)
            {
                snaps.push_back (300.f);
                snapPending = false; ++sc.noteChanges;
            }
        }

        // Accuracy and wrong notes are judged only once a note is settled,
        // so transition timing doesn't count against a tuner twice.
        if ((int) i - segStart < settleFrames) continue;
        ++judged;

        if (std::abs (diff) > 0.5f)
        {
            ++wrong;
            const float oct = std::round (diff / 12.f);
            if (oct != 0.f && std::abs (diff - 12.f * oct) < 0.5f) ++octave;
            continue;
        }

        ++settled;
        errs.push_back (std::abs (cents));
        if (std::abs (cents) < 10.f) ++inTune;
        segVals.push_back (cents);
    }
    closeSegment();

    sc.scoredFrames   = judged;
    sc.inTunePct      = settled > 0 ? 100.f * inTune / settled : 0.f;
    sc.medianErrCents = median (errs);
    sc.warbleCents    = median (segStd);
    sc.wrongNotePct   = judged > 0 ? 100.f * wrong / judged : 0.f;
    sc.octaveErrPct   = judged > 0 ? 100.f * octave / judged : 0.f;
    if (std::getenv ("TUNEBENCH_DEBUG") != nullptr)
    {
        std::printf ("  [debug] %d snaps:", (int) snaps.size());
        for (auto v : snaps) std::printf (" %.0f", v);
        std::printf ("\n");
    }
    sc.snapMs         = median (snaps);
    sc.voicedLossPct  = sung > 0 ? 100.f * lost / sung : 0.f;
    sc.roughness      = median (rough);

    // Clicks: 2.5 ms frames where the output's high-frequency energy jumps
    // far above both the input's and the output's own local level.
    const int cf = juce::jmax (8, (int) std::round (0.0025 * sr));
    const int nf = n / cf;
    std::vector<float> hin ((size_t) nf), hout ((size_t) nf);
    for (int k = 0; k < nf; ++k)
    {
        double a = 0, b = 0;
        for (int j = 1; j < cf; ++j)
        {
            const int s = k * cf + j;
            const double di = in[(size_t) s] - in[(size_t) s - 1];
            const double dout = out[(size_t) s] - out[(size_t) s - 1];
            a += di * di; b += dout * dout;
        }
        hin[(size_t) k] = (float) a; hout[(size_t) k] = (float) b;
    }
    int clicks = 0; bool inClick = false;
    for (int k = 0; k < nf; ++k)
    {
        std::vector<float> w;
        for (int j = juce::jmax (0, k - 40); j < juce::jmin (nf, k + 41); ++j) if (j != k) w.push_back (hout[(size_t) j]);
        const float local = median (w);
        // Compare against the loudest input HF within +-10 ms, so a consonant
        // the tuner merely moved by a few ms doesn't count as a new click.
        float inNear = 0.f;
        for (int j = juce::jmax (0, k - 4); j < juce::jmin (nf, k + 5); ++j) inNear = juce::jmax (inNear, hin[(size_t) j]);
        const bool c = hout[(size_t) k] > 31.6f * juce::jmax (inNear, 1e-12f)
                    && hout[(size_t) k] > 10.f * juce::jmax (local, 1e-12f)
                    && hout[(size_t) k] > 1e-7f;
        if (c && ! inClick)
        {
            ++clicks;
            if (std::getenv ("TUNEBENCH_DEBUG") != nullptr) std::printf ("  [debug] click at %.3fs (+%.0f dB vs input)\n", k * cf / sr, 10.0 * std::log10 (hout[(size_t) k] / juce::jmax (hin[(size_t) k], 1e-12f)));
        }
        inClick = c;
    }
    sc.clicksPerMin = (float) (clicks / juce::jmax (1e-6, n / sr / 60.0));

    return sc;
}
