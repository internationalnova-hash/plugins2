#include "NotesCore.h"

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace NovaNotes
{
namespace
{
    constexpr float kPi = 3.14159265358979f;

    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.f;
        auto mid = v.begin() + (std::ptrdiff_t) (v.size() / 2);
        std::nth_element (v.begin(), mid, v.end());
        return *mid;
    }

    int pcOf (float midi) noexcept { return ((int) std::lround (midi) % 12 + 12) % 12; }

    // Centered moving average, window 2r+1, shrinking at the edges.
    std::vector<float> movingAverage (const std::vector<float>& v, int r)
    {
        const int n = (int) v.size();
        std::vector<double> cum ((size_t) n + 1, 0.0);
        for (int i = 0; i < n; ++i) cum[(size_t) i + 1] = cum[(size_t) i] + v[(size_t) i];
        std::vector<float> out ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const int a = std::max (0, i - r), b = std::min (n, i + r + 1);
            out[(size_t) i] = (float) ((cum[(size_t) b] - cum[(size_t) a]) / (b - a));
        }
        return out;
    }
}

bool Note::isEdited() const noexcept
{
    return std::abs (pitch - center) > 1.0e-4f || drift > 1.0e-4f || std::abs (vibrato - 1.f) > 1.0e-4f
        || std::abs (formant) > 0.01f || std::abs (gainDb) > 0.01f || std::abs (timeShift) > 1.0e-5f;
}

// ════════════════════════════════════════════════════════════════════════════
// Pitch tracking: YIN (cumulative mean normalised difference), FFT-based
// ════════════════════════════════════════════════════════════════════════════
PitchTrack trackPitch (const float* x, int n, double sr)
{
    PitchTrack t;
    t.sampleRate = sr;
    t.hop = std::max (1, (int) std::lround (sr * 0.005));

    const int tauMin = std::max (2, (int) std::floor (sr / 1100.0));
    const int tauMax = (int) std::ceil (sr / 65.0);
    const int W = (int) std::lround (sr * 0.025);
    const int span = W + tauMax + 1;
    int order = 1;
    while ((1 << order) < span + W) ++order;
    const int N = 1 << order;
    juce::dsp::FFT fft (order);

    const int frames = n / t.hop + 1;
    t.midi.assign ((size_t) frames, 0.f);
    t.conf.assign ((size_t) frames, 0.f);
    t.db.assign ((size_t) frames, -120.f);

    std::vector<float> a ((size_t) N * 2), b ((size_t) N * 2), seg ((size_t) span);
    std::vector<double> e2 ((size_t) span + 1);
    std::vector<float> d ((size_t) tauMax + 2), dn ((size_t) tauMax + 2);

    float peakDb = -120.f;
    for (int f = 0; f < frames; ++f)
    {
        const int s = f * t.hop - span / 2;
        for (int i = 0; i < span; ++i)
        {
            const int k = s + i;
            seg[(size_t) i] = (k >= 0 && k < n) ? x[k] : 0.f;
        }
        e2[0] = 0.0;
        for (int i = 0; i < span; ++i) e2[(size_t) i + 1] = e2[(size_t) i] + (double) seg[(size_t) i] * seg[(size_t) i];

        const double e0 = e2[(size_t) W];
        const float lvl = (float) (10.0 * std::log10 (e0 / W + 1.0e-12));
        t.db[(size_t) f] = lvl;
        peakDb = std::max (peakDb, lvl);
        if (e0 < 1.0e-9) continue;

        std::fill (a.begin(), a.end(), 0.f);
        std::fill (b.begin(), b.end(), 0.f);
        std::copy (seg.begin(), seg.begin() + W, a.begin());
        std::copy (seg.begin(), seg.end(), b.begin());
        fft.performRealOnlyForwardTransform (a.data(), true);
        fft.performRealOnlyForwardTransform (b.data(), true);
        for (int k = 0; k <= N / 2; ++k)
        {
            const float ar = a[(size_t) 2 * k], ai = a[(size_t) 2 * k + 1];
            const float br = b[(size_t) 2 * k], bi = b[(size_t) 2 * k + 1];
            a[(size_t) 2 * k]     = ar * br + ai * bi;
            a[(size_t) 2 * k + 1] = ar * bi - ai * br;
        }
        fft.performRealOnlyInverseTransform (a.data());
        const double scale = a[0] != 0.f ? e0 / a[0] : 1.0;   // whatever the FFT's normalisation is

        d[0] = 0.f;
        double run = 0.0;
        dn[0] = 1.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const double r = a[(size_t) tau] * scale;
            const double et = e2[(size_t) tau + W] - e2[(size_t) tau];
            d[(size_t) tau] = (float) std::max (0.0, e0 + et - 2.0 * r);
            run += d[(size_t) tau];
            dn[(size_t) tau] = run > 0.0 ? (float) (d[(size_t) tau] * tau / run) : 1.f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (dn[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && dn[(size_t) tau + 1] < dn[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            best = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (dn[(size_t) tau] < dn[(size_t) best]) best = tau;
        }

        const float y0 = dn[(size_t) best - 1], y1 = dn[(size_t) best], y2 = dn[(size_t) best + 1];
        const float den = y0 - 2.f * y1 + y2;
        const float shift = std::abs (den) > 1.0e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (y0 - y2) / den) : 0.f;
        const float tauF = (float) best + shift;

        t.conf[(size_t) f] = juce::jlimit (0.f, 1.f, 1.f - y1);
        t.midi[(size_t) f] = (y1 < 0.3f) ? hzToMidi ((float) sr / tauF) : 0.f;
    }

    // Voicing: periodic AND loud enough relative to the take
    const float floorDb = std::max (-60.f, peakDb - 48.f);
    for (int f = 0; f < frames; ++f)
        if (t.db[(size_t) f] < floorDb) t.midi[(size_t) f] = 0.f;

    // Octave-error repair against the local median, then a 5-frame median
    std::vector<float> fixed = t.midi;
    for (int f = 0; f < frames; ++f)
    {
        if (t.midi[(size_t) f] <= 0.f) continue;
        std::vector<float> nb;
        for (int k = std::max (0, f - 12); k < std::min (frames, f + 13); ++k)
            if (t.midi[(size_t) k] > 0.f) nb.push_back (t.midi[(size_t) k]);
        const float med = median (nb);
        float m = t.midi[(size_t) f];
        if (m - med > 9.f) m -= 12.f;
        else if (med - m > 9.f) m += 12.f;
        fixed[(size_t) f] = m;
    }
    for (int f = 0; f < frames; ++f)
    {
        if (fixed[(size_t) f] <= 0.f) { t.midi[(size_t) f] = 0.f; continue; }
        std::vector<float> nb;
        for (int k = std::max (0, f - 2); k <= std::min (frames - 1, f + 2); ++k)
            if (fixed[(size_t) k] > 0.f) nb.push_back (fixed[(size_t) k]);
        t.midi[(size_t) f] = median (nb);
    }

    // Drop voiced blips shorter than 20 ms
    for (int f = 0; f < frames;)
    {
        if (t.midi[(size_t) f] <= 0.f) { ++f; continue; }
        int g = f;
        while (g < frames && t.midi[(size_t) g] > 0.f) ++g;
        if (g - f < 4) for (int k = f; k < g; ++k) t.midi[(size_t) k] = 0.f;
        f = g;
    }
    return t;
}

// ════════════════════════════════════════════════════════════════════════════
// Pitch marks: one per period in voiced regions (on the low-passed waveform
// peak), every 5 ms in unvoiced regions.
// ════════════════════════════════════════════════════════════════════════════
std::vector<int> findEpochs (const float* x, int n, const PitchTrack& t)
{
    std::vector<int> ep;
    if (n <= 0) return ep;

    // zero-phase one-pole low-pass (~900 Hz) to find stable peaks
    std::vector<float> lp ((size_t) n);
    const float c = std::exp (-2.f * kPi * 900.f / (float) t.sampleRate);
    float z = 0.f;
    for (int i = 0; i < n; ++i) { z = (1.f - c) * x[i] + c * z; lp[(size_t) i] = z; }
    z = 0.f;
    for (int i = n - 1; i >= 0; --i) { z = (1.f - c) * lp[(size_t) i] + c * z; lp[(size_t) i] = z; }

    const int uHop = t.hop;
    auto voicedAt = [&] (int pos) -> float
    {
        const int f = juce::jlimit (0, t.size() - 1, pos / t.hop);
        return t.midi[(size_t) f];
    };
    auto peakIn = [&] (int a, int b) -> int
    {
        a = std::max (0, a); b = std::min (n - 1, b);
        int best = a;
        for (int i = a + 1; i <= b; ++i) if (lp[(size_t) i] > lp[(size_t) best]) best = i;
        return best;
    };

    ep.push_back (0);
    bool prevVoiced = false;
    while (true)
    {
        const int last = ep.back();
        const float m = voicedAt (last);
        int next;
        if (m > 0.f)
        {
            const float P = (float) t.sampleRate / midiToHz (m);
            if (! prevVoiced)
                next = peakIn (last + 1, last + (int) P);
            else
            {
                const int expect = last + (int) std::lround (P);
                next = peakIn (expect - (int) (0.2f * P), expect + (int) (0.2f * P));
            }
            if (next <= last + (int) (0.5f * P)) next = last + (int) std::lround (P);
            prevVoiced = true;
        }
        else
        {
            next = last + uHop;
            prevVoiced = false;
        }
        if (next >= n) break;
        ep.push_back (next);
    }
    return ep;
}

// ════════════════════════════════════════════════════════════════════════════
// Notes
// ════════════════════════════════════════════════════════════════════════════
float measureCenter (const PitchTrack& t, int start, int end)
{
    const int len = end - start;
    int a = start, b = end;
    if (len >= 16) { a = start + len / 5; b = end - len / 5; }
    std::vector<std::pair<float, float>> v;   // pitch, weight
    for (int f = a; f < b; ++f)
        if (t.midi[(size_t) f] > 0.f)
            v.push_back ({ t.midi[(size_t) f], std::pow (10.f, t.db[(size_t) f] / 20.f) });
    if (v.empty())
        for (int f = start; f < end; ++f)
            if (t.midi[(size_t) f] > 0.f) v.push_back ({ t.midi[(size_t) f], 1.f });
    if (v.empty()) return 60.f;
    std::sort (v.begin(), v.end());
    float total = 0.f;
    for (auto& p : v) total += p.second;
    float acc = 0.f;
    for (auto& p : v) { acc += p.second; if (acc >= total * 0.5f) return p.first; }
    return v.back().first;
}

std::vector<Note> segmentNotes (const PitchTrack& t)
{
    const int F = t.size();
    std::vector<Note> notes;
    if (F == 0) return notes;

    // Smoothed pitch (105 ms over voiced frames) - vibrato averages out, real note changes don't
    std::vector<float> sm ((size_t) F, 0.f);
    for (int f = 0; f < F; ++f)
    {
        if (t.midi[(size_t) f] <= 0.f) continue;
        float s = 0.f; int c = 0;
        for (int k = std::max (0, f - 10); k <= std::min (F - 1, f + 10); ++k)
            if (t.midi[(size_t) k] > 0.f) { s += t.midi[(size_t) k]; ++c; }
        sm[(size_t) f] = s / (float) c;
    }

    // Voiced runs, bridging gaps up to 15 ms
    std::vector<std::pair<int, int>> runs;
    for (int f = 0; f < F;)
    {
        if (t.midi[(size_t) f] <= 0.f) { ++f; continue; }
        int g = f, lastV = f;
        while (g < F)
        {
            if (t.midi[(size_t) g] > 0.f) lastV = g;
            else if (g - lastV > 3) break;
            ++g;
        }
        runs.push_back ({ f, lastV + 1 });
        f = lastV + 1;
    }

    constexpr int minLen = 10;   // 50 ms (slow note changes)
    constexpr int minFast = 6;   // 30 ms (clean jumps inside fast runs)
    std::vector<std::pair<int, int>> segs;
    for (auto [a, b] : runs)
    {
        int noteStart = a;
        auto refOver = [&] (int from, int to)
        {
            float s = 0.f; int c = 0;
            for (int k = from; k < to; ++k) if (sm[(size_t) k] > 0.f) { s += sm[(size_t) k]; ++c; }
            return c > 0 ? s / (float) c : 0.f;
        };
        float ref = refOver (a, std::min (b, a + 8));
        int devCount = 0, devStart = -1;
        auto rawRefAt = [&] (int from)
        {
            std::vector<float> v;
            for (int k = from; k < std::min (b, from + 4); ++k) if (t.midi[(size_t) k] > 0.f) v.push_back (t.midi[(size_t) k]);
            return median (v);
        };
        float rawRef = rawRefAt (a);
        int jumpCount = 0, jumpStart = -1;
        for (int f = a; f < b; ++f)
        {
            // fast clean jump (runs): raw pitch leaves the note by > 1.2 st and settles
            if (t.midi[(size_t) f] > 0.f && rawRef > 0.f)
            {
                if (std::abs (t.midi[(size_t) f] - rawRef) > 1.2f)
                {
                    if (jumpCount == 0) jumpStart = f;
                    if (++jumpCount >= 4)
                    {
                        float lo = 1e9f, hi = -1e9f;
                        for (int k = jumpStart; k <= f; ++k) if (t.midi[(size_t) k] > 0.f) { lo = std::min (lo, t.midi[(size_t) k]); hi = std::max (hi, t.midi[(size_t) k]); }
                        if (hi - lo < 0.8f && jumpStart - noteStart >= minFast && b - jumpStart >= minFast)
                        {
                            segs.push_back ({ noteStart, jumpStart });
                            noteStart = jumpStart;
                            ref = refOver (jumpStart, std::min (b, jumpStart + 8));
                            rawRef = rawRefAt (jumpStart);
                            devCount = 0; jumpCount = 0;
                            continue;
                        }
                        if (hi - lo >= 0.8f) { jumpCount = 0; }   // still gliding, keep watching
                    }
                }
                else
                {
                    jumpCount = 0;
                    rawRef = rawRef * 0.9f + t.midi[(size_t) f] * 0.1f;
                }
            }
            // energy onset (re-attacked note on the same pitch)
            if (f - 4 >= noteStart + minLen && t.db[(size_t) f] - t.db[(size_t) f - 4] > 8.f
                && b - f >= minLen && t.midi[(size_t) f] > 0.f)
            {
                segs.push_back ({ noteStart, f });
                noteStart = f;
                ref = refOver (f, std::min (b, f + 8));
                rawRef = rawRefAt (f);
                devCount = 0; jumpCount = 0;
                continue;
            }
            if (sm[(size_t) f] <= 0.f) continue;
            if (std::abs (sm[(size_t) f] - ref) > 0.7f)
            {
                if (devCount == 0) devStart = f;
                if (++devCount >= 8)
                {
                    int split = devStart;
                    while (split > noteStart + 1 && sm[(size_t) split - 1] > 0.f
                           && std::abs (sm[(size_t) split - 1] - ref) > 0.35f) --split;
                    if (split - noteStart >= minLen)
                    {
                        segs.push_back ({ noteStart, split });
                        noteStart = split;
                        rawRef = rawRefAt (split);
                        jumpCount = 0;
                    }
                    ref = refOver (std::max (noteStart, f - 3), f + 1);
                    devCount = 0;
                }
            }
            else
            {
                devCount = 0;
                ref = ref * 0.97f + sm[(size_t) f] * 0.03f;
            }
        }
        segs.push_back ({ noteStart, b });
    }

    // Merge too-short pieces into the adjacent piece nearest in pitch
    bool merged = true;
    while (merged && segs.size() > 1)
    {
        merged = false;
        for (size_t i = 0; i < segs.size(); ++i)
        {
            if (segs[i].second - segs[i].first >= minFast) continue;
            const bool joinPrev = i > 0 && segs[i - 1].second == segs[i].first;
            const bool joinNext = i + 1 < segs.size() && segs[i + 1].first == segs[i].second;
            if (! joinPrev && ! joinNext) continue;
            const float c = measureCenter (t, segs[i].first, segs[i].second);
            bool toPrev = joinPrev;
            if (joinPrev && joinNext)
                toPrev = std::abs (measureCenter (t, segs[i - 1].first, segs[i - 1].second) - c)
                      <= std::abs (measureCenter (t, segs[i + 1].first, segs[i + 1].second) - c);
            if (toPrev) { segs[i - 1].second = segs[i].second; segs.erase (segs.begin() + (std::ptrdiff_t) i); }
            else        { segs[i + 1].first = segs[i].first;   segs.erase (segs.begin() + (std::ptrdiff_t) i); }
            merged = true;
            break;
        }
    }

    int id = 1;
    for (auto [a, b] : segs)
    {
        if (b - a < 8) continue;   // < 40 ms: a blip, not a note
        Note n;
        n.id = id++;
        n.start = a; n.end = b;
        n.center = n.pitch = measureCenter (t, a, b);
        notes.push_back (n);
    }
    return notes;
}

void decompose (const PitchTrack& t, const Note& n, std::vector<float>& slow, std::vector<float>& fast)
{
    const int len = std::max (0, n.end - n.start);
    std::vector<float> dev ((size_t) len, 0.f);
    float last = 0.f;
    bool any = false;
    for (int i = 0; i < len; ++i)
    {
        const float m = t.midi[(size_t) (n.start + i)];
        if (m > 0.f) { last = m - n.center; any = true; }
        dev[(size_t) i] = last;
    }
    if (any)   // back-fill leading unvoiced frames
    {
        int i = 0;
        while (i < len && t.midi[(size_t) (n.start + i)] <= 0.f) ++i;
        for (int k = 0; k < i && i < len; ++k) dev[(size_t) k] = dev[(size_t) i];
    }
    // two passes of a 155 ms box = ~300 ms triangle: keeps scoops and drift, removes vibrato
    slow = movingAverage (movingAverage (dev, 15), 15);
    fast.resize ((size_t) len);
    for (int i = 0; i < len; ++i) fast[(size_t) i] = dev[(size_t) i] - slow[(size_t) i];
}

// ════════════════════════════════════════════════════════════════════════════
// Key & scale
// ════════════════════════════════════════════════════════════════════════════
Key detectKey (const std::vector<Note>& notes, const PitchTrack& t)
{
    static const float major[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    std::array<float, 12> w {};
    for (const auto& n : notes)
    {
        // long notes sung close to a semitone count most; smeared/out-of-tune ones barely count
        const float dur = (float) (n.end - n.start) * (float) t.hop / (float) t.sampleRate;
        const float off = std::abs (n.center - std::round (n.center));
        w[(size_t) pcOf (n.center)] += dur * std::max (0.05f, 1.f - off / 0.5f);
    }
    auto corr = [] (const std::array<float, 12>& a, const float* p, int rot)
    {
        float ma = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { ma += a[(size_t) i]; mp += p[i]; }
        ma /= 12; mp /= 12;
        float num = 0, da = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float x = a[(size_t) ((i + rot) % 12)] - ma, y = p[i] - mp;
            num += x * y; da += x * x; dp += y * y;
        }
        return (da > 0 && dp > 0) ? num / std::sqrt (da * dp) : 0.f;
    };
    Key best;
    float bestC = -2.f;
    for (int r = 0; r < 12; ++r)
    {
        const float cM = corr (w, major, r), cm = corr (w, minor, r);
        if (cM > bestC) { bestC = cM; best = { r, false, cM }; }
        if (cm > bestC) { bestC = cm; best = { r, true, cm }; }
    }
    // A phrase that only uses a few notes fits several keys equally well
    int distinct = 0;
    float total = 0.f;
    for (float v : w) total += v;
    for (float v : w) if (total > 0.f && v / total > 0.04f) ++distinct;
    best.confidence = juce::jlimit (0.f, 1.f, bestC * std::min (1.f, (float) distinct / 6.f));
    return best;
}

int scaleMask (Key k)
{
    static const int majorSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minorSteps[7] = { 0, 2, 3, 5, 7, 8, 10 };
    int mask = 0;
    for (int s : (k.minor ? minorSteps : majorSteps)) mask |= 1 << ((k.tonic + s) % 12);
    return mask;
}

float snapToMask (float midi, int mask)
{
    if (mask == 0 || mask == 0xFFF) return (float) std::lround (midi);
    float best = (float) std::lround (midi), bestD = 99.f;
    for (int k = (int) std::floor (midi) - 6; k <= (int) std::ceil (midi) + 6; ++k)
    {
        if (! (mask & (1 << ((k % 12 + 12) % 12)))) continue;
        const float dd = std::abs ((float) k - midi);
        if (dd < bestD) { bestD = dd; best = (float) k; }
    }
    return best;
}

// ════════════════════════════════════════════════════════════════════════════
// Macros & edits
// ════════════════════════════════════════════════════════════════════════════
namespace
{
    bool wanted (const std::vector<int>& ids, int id)
    {
        return ids.empty() || std::find (ids.begin(), ids.end(), id) != ids.end();
    }
}

void correctPitch (std::vector<Note>& notes, const std::vector<int>& ids, float centerAmt, float driftAmt, int mask)
{
    for (auto& n : notes)
    {
        if (! wanted (ids, n.id)) continue;
        const float target = snapToMask (n.center, mask);
        n.pitch = n.center + (target - n.center) * juce::jlimit (0.f, 1.f, centerAmt);
        n.drift = juce::jlimit (0.f, 1.f, driftAmt);
    }
}

void quantizeTime (std::vector<Note>& notes, const std::vector<int>& ids, const PitchTrack& t,
                   double secondsPerBeat, double firstBeatSec, float amount)
{
    if (secondsPerBeat <= 0.0) return;
    const double grid = secondsPerBeat / 4.0;
    for (auto& n : notes)
    {
        if (! wanted (ids, n.id)) continue;
        const double s = n.start * (double) t.hop / t.sampleRate;
        const double k = std::round ((s - firstBeatSec) / grid);
        const double g = firstBeatSec + k * grid;
        const double delta = g - s;
        n.timeShift = std::abs (delta) < grid * 0.45 ? (float) (delta * amount) : 0.f;
    }
}

int nextNoteId (const std::vector<Note>& notes)
{
    int m = 0;
    for (auto& n : notes) m = std::max (m, n.id);
    return m + 1;
}

int splitNote (std::vector<Note>& notes, int id, int frame, const PitchTrack& t)
{
    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto& n = notes[i];
        if (n.id != id) continue;
        if (frame - n.start < 4 || n.end - frame < 4) return -1;
        Note right = n;
        right.id = nextNoteId (notes);
        right.start = frame;
        n.end = frame;
        const float shiftL = n.pitch - n.center, shiftR = right.pitch - right.center;
        n.center = measureCenter (t, n.start, n.end);
        right.center = measureCenter (t, right.start, right.end);
        n.pitch = n.center + shiftL;
        right.pitch = right.center + shiftR;
        notes.insert (notes.begin() + (std::ptrdiff_t) i + 1, right);
        return right.id;
    }
    return -1;
}

// ════════════════════════════════════════════════════════════════════════════
// AI Assist: decides per note what a careful engineer would do
// ════════════════════════════════════════════════════════════════════════════
AssistReport aiAssist (const PitchTrack& t, std::vector<Note>& notes, const std::vector<int>& ids,
                       AssistStyle style, Key key)
{
    AssistReport rep;
    const int mask = scaleMask (key);
    const double fps = t.sampleRate / t.hop;

    // Out-of-key notes the singer lands on cleanly and often are intentional (blue notes etc.)
    std::array<float, 12> clean {}, total {};
    float all = 0.f;
    for (const auto& n : notes)
    {
        const float dur = (float) (n.end - n.start);
        total[(size_t) pcOf (n.center)] += dur;
        all += dur;
        if (std::abs (n.center - std::round (n.center)) < 0.2f) clean[(size_t) pcOf (n.center)] += dur;
    }
    int allowed = mask;
    // Only Natural keeps them - Tight / Tune-ready put every note in the key
    if (style == AssistStyle::natural)
        for (int pc = 0; pc < 12; ++pc)
            if (! (mask & (1 << pc)) && all > 0.f && clean[(size_t) pc] / all > 0.05f) allowed |= 1 << pc;

    float baseAmt, baseDrift, vib, trans;
    switch (style)
    {
        case AssistStyle::natural:   baseAmt = 0.85f; baseDrift = 0.20f; vib = 1.00f; trans = 0.55f; break;
        case AssistStyle::tight:     baseAmt = 1.00f; baseDrift = 0.45f; vib = 1.00f; trans = 0.45f; break;
        default:                     baseAmt = 1.00f; baseDrift = 0.70f; vib = 0.85f; trans = 0.35f; break;
    }

    float prevTarget = -1.f, prevCenter = -1.f;
    for (auto& n : notes)
    {
        if (! wanted (ids, n.id)) { prevTarget = n.pitch; prevCenter = n.center; continue; }

        const float secs = (float) ((n.end - n.start) / fps);
        std::vector<float> slow, fast;
        decompose (t, n, slow, fast);
        float lo = 0.f, hi = 0.f, fastE = 0.f, confSum = 0.f;
        int voiced = 0;
        const int len = n.end - n.start;
        for (int i = len / 6; i < len - len / 6; ++i) { lo = std::min (lo, slow[(size_t) i]); hi = std::max (hi, slow[(size_t) i]); }
        for (int i = 0; i < len; ++i)
        {
            fastE += fast[(size_t) i] * fast[(size_t) i];
            if (t.midi[(size_t) (n.start + i)] > 0.f) { confSum += t.conf[(size_t) (n.start + i)]; ++voiced; }
        }
        const float glide = hi - lo;
        const float vibRms = len > 0 ? std::sqrt (fastE / (float) len) : 0.f;
        const float conf = voiced > 0 ? confSum / (float) voiced : 0.f;

        // The note the singer meant
        float target = snapToMask (n.center, allowed);
        const float lower = std::floor (n.center), upper = std::ceil (n.center);
        const bool lowerOk = allowed & (1 << pcOf (lower)), upperOk = allowed & (1 << pcOf (upper));
        if (lowerOk && upperOk && std::abs ((n.center - lower) - 0.5f) < 0.12f && prevTarget > 0.f)
            target = (n.center > prevCenter) == (upper > prevTarget) ? upper : lower;   // follow the melody's direction
        const float dist = std::abs (target - n.center);

        float amt = baseAmt, drift = baseDrift, vibrato = 1.f;
        if (secs < 0.09f)   // runs / passing notes: eased in Natural, fully in key for Tune-ready
        {
            amt *= style == AssistStyle::natural ? 0.5f : style == AssistStyle::tight ? 0.8f : 1.f;
            drift = 0.f;
            ++rep.runs;
        }
        if (glide > 1.5f)                { amt *= 0.7f; drift = 0.f; ++rep.keptSlides; }  // intentional slide
        if (conf < 0.6f)                 { amt *= 0.6f; drift *= 0.5f; }                   // breathy / unsure pitch
        if (vibRms > 0.12f && secs > 0.25f) { vibrato = vib; ++rep.keptVibrato; }
        if (style == AssistStyle::natural && dist < 0.06f && glide < 0.6f) { amt = 0.f; drift = 0.f; }

        n.pitch = n.center + (target - n.center) * amt;
        n.drift = drift;
        n.vibrato = vibrato;
        n.transition = trans;
        if (amt > 0.f && dist >= 0.06f) ++rep.fixed; else ++rep.leftAlone;

        prevTarget = target;
        prevCenter = n.center;
    }
    return rep;
}

// ════════════════════════════════════════════════════════════════════════════
// Rendering: TD-PSOLA with per-frame pitch offset, formant and gain curves
// and a piecewise-linear time map (note moves).
// ════════════════════════════════════════════════════════════════════════════
bool anyEdits (const std::vector<Note>& notes)
{
    return std::any_of (notes.begin(), notes.end(), [] (const Note& n) { return n.isEdited(); });
}

namespace
{
    struct Curves
    {
        std::vector<float> offset, formant, gainDb;   // per analysis frame
    };

    Curves buildCurves (const PitchTrack& t, const std::vector<Note>& notes)
    {
        const int F = t.size();
        Curves c;
        c.offset.assign ((size_t) F, 0.f);
        c.formant.assign ((size_t) F, 0.f);
        c.gainDb.assign ((size_t) F, 0.f);
        std::vector<int> owner ((size_t) F, -1);

        for (size_t i = 0; i < notes.size(); ++i)
        {
            const auto& n = notes[i];
            std::vector<float> slow, fast;
            decompose (t, n, slow, fast);
            for (int f = std::max (0, n.start); f < std::min (F, n.end); ++f)
            {
                const size_t k = (size_t) (f - n.start);
                c.offset[(size_t) f] = (n.pitch - n.center) - n.drift * slow[k] + (n.vibrato - 1.f) * fast[k];
                c.formant[(size_t) f] = n.formant;
                c.gainDb[(size_t) f] = n.gainDb;
                owner[(size_t) f] = (int) i;
            }
        }
        // Gaps: pitch offset from the nearest note (only matters where voiced),
        // formant/gain only right next to a note
        int prev = -1;
        for (int f = 0; f < F; ++f)
        {
            if (owner[(size_t) f] >= 0) { prev = f; continue; }
            int next = f;
            while (next < F && owner[(size_t) next] < 0) ++next;
            for (int g = f; g < next; ++g)
            {
                const int dPrev = prev >= 0 ? g - prev : 1 << 30;
                const int dNext = next < F ? next - g : 1 << 30;
                const int src = dPrev <= dNext ? prev : next;
                if (src < 0 || src >= F) continue;
                c.offset[(size_t) g] = c.offset[(size_t) src];
                if (std::min (dPrev, dNext) <= 6)
                {
                    c.formant[(size_t) g] = c.formant[(size_t) src];
                    c.gainDb[(size_t) g] = c.gainDb[(size_t) src];
                }
            }
            f = next - 1;
        }
        // Transitions: interpolate the offset across each legato boundary
        for (size_t i = 1; i < notes.size(); ++i)
        {
            const auto& a = notes[i - 1];
            const auto& b = notes[i];
            if (b.start - a.end > 12) continue;
            // glide length, never more than 40% of the shorter note (fast runs stay on pitch)
            const int shorter = std::min (a.end - a.start, b.end - b.start);
            const int w = std::min (2 + (int) std::lround (b.transition * 30.f), std::max (2, (int) (0.4f * (float) shorter)));
            const int mid = (a.end + b.start) / 2;
            const int L = std::max (a.start + (a.end - a.start) / 3, mid - w / 2);
            const int R = std::min (b.end - (b.end - b.start) / 3, mid + w / 2);
            if (R - L < 2 || L < 0 || R >= F) continue;
            const float vL = c.offset[(size_t) L], vR = c.offset[(size_t) R];
            for (int f = L + 1; f < R; ++f)
            {
                const float u = (float) (f - L) / (float) (R - L);
                const float s = u * u * (3.f - 2.f * u);
                c.offset[(size_t) f] = vL + (vR - vL) * s;
            }
        }
        c.formant = movingAverage (c.formant, 2);
        c.gainDb = movingAverage (c.gainDb, 2);
        return c;
    }

    // Monotonic out->in time map from note moves
    struct TimeMap
    {
        std::vector<double> in, out;
        bool identity = true;

        double outToIn (double o) const
        {
            if (identity) return o;
            auto it = std::upper_bound (out.begin(), out.end(), o);
            if (it == out.begin()) return in.front();
            if (it == out.end()) return in.back();
            const size_t k = (size_t) (it - out.begin());
            const double u = (o - out[k - 1]) / std::max (1.0e-9, out[k] - out[k - 1]);
            return in[k - 1] + u * (in[k] - in[k - 1]);
        }
    };

    TimeMap buildTimeMap (const PitchTrack& t, const std::vector<Note>& notes, int n)
    {
        TimeMap m;
        m.in.push_back (0.0); m.out.push_back (0.0);
        for (const auto& note : notes)
        {
            const double s = (double) note.start * t.hop, e = (double) note.end * t.hop;
            const double d = note.timeShift * t.sampleRate;
            if (std::abs (d) > 0.5) m.identity = false;
            m.in.push_back (s); m.out.push_back (s + d);
            m.in.push_back (e); m.out.push_back (e + d);
        }
        m.in.push_back (n); m.out.push_back (n);
        // enforce strictly increasing in both
        for (size_t i = 1; i < m.in.size(); ++i)
        {
            m.in[i]  = std::max (m.in[i],  m.in[i - 1] + 1.0);
            m.out[i] = std::max (m.out[i], m.out[i - 1] + 1.0);
        }
        for (size_t i = m.out.size() - 1; i > 0; --i)   // keep the end pinned
            if (m.out[i - 1] >= m.out[i]) m.out[i - 1] = m.out[i] - 1.0;
        return m;
    }
}

void render (const float* x, int n, const PitchTrack& t, const std::vector<int>& epochs,
             const std::vector<Note>& notes, float* out)
{
    if (n <= 0) return;
    if (! anyEdits (notes) || epochs.size() < 3)
    {
        std::copy (x, x + n, out);
        return;
    }

    const Curves cv = buildCurves (t, notes);
    const TimeMap tm = buildTimeMap (t, notes, n);
    const int F = t.size();
    const int E = (int) epochs.size();

    std::vector<float> acc ((size_t) n, 0.f), wsum ((size_t) n, 0.f);
    auto frameOf = [&] (double pos) { return juce::jlimit (0, F - 1, (int) (pos / t.hop)); };
    auto sampleAt = [&] (double pos) -> float
    {
        const int i = (int) std::floor (pos);
        if (i < 0 || i + 1 >= n) return 0.f;
        const float u = (float) (pos - i);
        return x[i] + (x[i + 1] - x[i]) * u;
    };

    double to = 0.0;
    while (to < n)
    {
        const double ti = tm.outToIn (to);
        int k = (int) (std::lower_bound (epochs.begin(), epochs.end(), (int) ti) - epochs.begin());
        if (k >= E) k = E - 1;
        if (k > 0 && std::abs (epochs[(size_t) k - 1] - ti) < std::abs (epochs[(size_t) k] - ti)) --k;
        const int e = epochs[(size_t) k];
        const int left  = k > 0 ? e - epochs[(size_t) k - 1] : (k + 1 < E ? epochs[(size_t) k + 1] - e : t.hop);
        const int right = k + 1 < E ? epochs[(size_t) k + 1] - e : left;

        const int f = frameOf (e);
        const bool voiced = t.midi[(size_t) f] > 0.f;
        const double ratio = voiced ? std::exp2 (cv.offset[(size_t) f] / 12.0) : 1.0;
        const double fr = std::exp2 (cv.formant[(size_t) f] / 1200.0);
        const float g = juce::Decibels::decibelsToGain (cv.gainDb[(size_t) f]);
        const double pin = 0.5 * (left + right);
        double pout = pin / ratio;

        // Untouched spots: lock onto the original marks so they come out identical
        if (std::abs (ratio - 1.0) < 1.0e-6 && std::abs (fr - 1.0) < 1.0e-6 && std::abs (ti - to) < 0.5)
        {
            to = e;
            pout = right;
        }

        const double L = std::min (std::max ((double) left, pout), 2.5 * pin);
        const double R = std::min (std::max ((double) right, pout), 2.5 * pin);
        const int i0 = std::max (0, (int) std::ceil (to - L / fr));
        const int i1 = std::min (n - 1, (int) std::floor (to + R / fr));
        for (int i = i0; i <= i1; ++i)
        {
            const double j = (i - to) * fr;
            const double w = j < 0 ? 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * j / L))
                                   : 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * j / R));
            const float s = sampleAt (e + j);
            acc[(size_t) i] += (float) (w * s) * g;
            wsum[(size_t) i] += (float) w;
        }
        to += std::max (1.0, pout);
    }

    for (int i = 0; i < n; ++i)
    {
        const float v = acc[(size_t) i] / std::max (wsum[(size_t) i], 0.2f);
        out[i] = std::isfinite (v) ? v : 0.f;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// JSON
// ════════════════════════════════════════════════════════════════════════════
juce::String notesToJson (const std::vector<Note>& notes)
{
    juce::String s;
    s.preallocateBytes (notes.size() * 140 + 4);
    s << "[";
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        if (i) s << ",";
        s << "{\"id\":" << n.id << ",\"s\":" << n.start << ",\"e\":" << n.end
          << ",\"c\":" << juce::String (n.center, 4) << ",\"p\":" << juce::String (n.pitch, 4)
          << ",\"dr\":" << juce::String (n.drift, 3) << ",\"vb\":" << juce::String (n.vibrato, 3)
          << ",\"tr\":" << juce::String (n.transition, 3) << ",\"fm\":" << juce::String (n.formant, 1)
          << ",\"g\":" << juce::String (n.gainDb, 2) << ",\"ts\":" << juce::String (n.timeShift, 5) << "}";
    }
    s << "]";
    return s;
}

std::vector<Note> notesFromJson (const juce::String& json)
{
    std::vector<Note> out;
    const auto v = juce::JSON::parse (json);
    if (auto* arr = v.getArray())
        for (const auto& o : *arr)
        {
            Note n;
            n.id = (int) o["id"];
            n.start = (int) o["s"];
            n.end = (int) o["e"];
            n.center = (float) (double) o["c"];
            n.pitch = (float) (double) o["p"];
            n.drift = (float) (double) o["dr"];
            n.vibrato = o.hasProperty ("vb") ? (float) (double) o["vb"] : 1.f;
            n.transition = o.hasProperty ("tr") ? (float) (double) o["tr"] : 0.5f;
            n.formant = (float) (double) o["fm"];
            n.gainDb = (float) (double) o["g"];
            n.timeShift = (float) (double) o["ts"];
            if (n.end > n.start) out.push_back (n);
        }
    return out;
}

juce::String trackToJson (const PitchTrack& t, int decimate)
{
    decimate = std::max (1, decimate);
    juce::String m, d;
    const int F = t.size();
    m.preallocateBytes ((size_t) (F / decimate) * 7);
    d.preallocateBytes ((size_t) (F / decimate) * 4);
    for (int f = 0; f < F; f += decimate)
    {
        if (f) { m << ","; d << ","; }
        const float v = t.midi[(size_t) f];
        m << (v > 0.f ? juce::String (v, 2) : juce::String ("0"));
        d << juce::String ((int) std::lround (std::max (-90.f, t.db[(size_t) f])));
    }
    return "{\"hop\":" + juce::String (t.hop) + ",\"sr\":" + juce::String (t.sampleRate, 1)
         + ",\"dec\":" + juce::String (decimate) + ",\"frames\":" + juce::String (F)
         + ",\"midi\":[" + m + "],\"db\":[" + d + "]}";
}

void applyPatch (Note& n, const juce::var& p)
{
    if (p.hasProperty ("p"))  n.pitch      = juce::jlimit (n.center - 24.f, n.center + 24.f, (float) (double) p["p"]);
    if (p.hasProperty ("dr")) n.drift      = juce::jlimit (0.f, 1.f, (float) (double) p["dr"]);
    if (p.hasProperty ("vb")) n.vibrato    = juce::jlimit (0.f, 2.f, (float) (double) p["vb"]);
    if (p.hasProperty ("tr")) n.transition = juce::jlimit (0.f, 1.f, (float) (double) p["tr"]);
    if (p.hasProperty ("fm")) n.formant    = juce::jlimit (-1200.f, 1200.f, (float) (double) p["fm"]);
    if (p.hasProperty ("g"))  n.gainDb     = juce::jlimit (-48.f, 12.f, (float) (double) p["g"]);
    if (p.hasProperty ("ts")) n.timeShift  = juce::jlimit (-2.f, 2.f, (float) (double) p["ts"]);
}
}
