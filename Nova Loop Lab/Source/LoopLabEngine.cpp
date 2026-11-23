#include "LoopLabEngine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ll
{
    namespace
    {
        constexpr double kMinBpm = 70.0, kMaxBpm = 175.0;

        std::vector<float> monoOf (const Sample& s)
        {
            const int n = s.length();
            std::vector<float> m ((size_t) n);
            const int chans = s.audio.getNumChannels();
            for (int i = 0; i < n; ++i)
            {
                float v = 0.0f;
                for (int ch = 0; ch < chans; ++ch) v += s.audio.getSample (ch, i);
                m[(size_t) i] = chans > 0 ? v / (float) chans : 0.0f;
            }
            return m;
        }

        // Spectral-flux onset strength, one value per hop.
        std::vector<float> onsetStrength (const std::vector<float>& mono, int hop)
        {
            constexpr int order = 10, size = 1 << order;
            juce::dsp::FFT fft (order);
            juce::dsp::WindowingFunction<float> win ((size_t) size, juce::dsp::WindowingFunction<float>::hann, false);
            std::vector<float> buf ((size_t) size * 2), prev ((size_t) size / 2 + 1, 0.0f);
            const int frames = std::max (0, ((int) mono.size() - size) / hop + 1);
            std::vector<float> out ((size_t) std::max (1, frames), 0.0f), energyDb (out.size(), -100.0f);

            for (int f = 0; f < frames; ++f)
            {
                std::fill (buf.begin(), buf.end(), 0.0f);
                std::copy_n (mono.begin() + f * hop, size, buf.begin());
                win.multiplyWithWindowingTable (buf.data(), (size_t) size);
                fft.performFrequencyOnlyForwardTransform (buf.data(), true);

                float flux = 0.0f;
                for (int k = 1; k <= size / 2; ++k)
                {
                    const float mag = std::log1p (10.0f * buf[(size_t) k]);
                    flux += std::max (0.0f, mag - prev[(size_t) k]);
                    prev[(size_t) k] = mag;
                }
                out[(size_t) f] = f == 0 ? 0.0f : flux;

                double e = 1.0e-10;
                for (int i = 0; i < size; ++i) e += (double) mono[(size_t) (f * hop + i)] * mono[(size_t) (f * hop + i)];
                energyDb[(size_t) f] = (float) (10.0 * std::log10 (e / size));
            }

            // Energy rise in dB, floored so silence-to-noise doesn't dominate.
            std::vector<float> rise (out.size(), 0.0f);
            for (size_t f = 1; f < out.size(); ++f)
                rise[f] = energyDb[f] > -60.0f ? std::max (0.0f, energyDb[f] - energyDb[f - 1]) : 0.0f;

            auto normalise = [] (std::vector<float>& v)
            {
                const float mx = v.empty() ? 0.0f : *std::max_element (v.begin(), v.end());
                if (mx > 0.0f) for (auto& x : v) x /= mx;
            };
            normalise (out);
            normalise (rise);
            for (size_t f = 0; f < out.size(); ++f) out[f] = 0.5f * out[f] + 0.5f * rise[f];
            normalise (out);
            return out;
        }

        // Normalised autocorrelation of the onset curve at a fractional lag.
        struct Acf
        {
            std::vector<float> x;
            float energy = 1.0f;

            explicit Acf (const std::vector<float>& onset) : x (onset)
            {
                const float mean = x.empty() ? 0.0f : std::accumulate (x.begin(), x.end(), 0.0f) / (float) x.size();
                for (auto& v : x) v -= mean;
                energy = std::max (1.0e-9f, std::inner_product (x.begin(), x.end(), x.begin(), 0.0f));
            }

            float at (double lag) const
            {
                const int l0 = (int) std::floor (lag);
                const float frac = (float) (lag - l0);
                auto r = [this] (int l)
                {
                    if (l <= 0 || l >= (int) x.size()) return 0.0f;
                    float s = 0.0f;
                    for (size_t i = (size_t) l; i < x.size(); ++i) s += x[i] * x[i - (size_t) l];
                    return s / energy;
                };
                return r (l0) * (1.0f - frac) + r (l0 + 1) * frac;
            }
        };

        // Periodicity at the beat, half bar and bar, weighted toward the tempos
        // most music sits at (so 140 wins over 70 when both fit equally).
        double bpmScore (const Acf& acf, double bpm, double hopsPerSecond)
        {
            const double lag = 60.0 / bpm * hopsPerSecond;
            const double raw = acf.at (lag) + 0.5 * acf.at (lag * 2.0) + 0.5 * acf.at (lag * 4.0);
            const double oct = std::log2 (bpm / 115.0) / 0.6;
            return std::max (0.0, raw) * std::exp (-0.5 * oct * oct);
        }

        std::array<float, 12> chromaOf (const std::vector<float>& mono, int from, int to, double sr)
        {
            constexpr int order = 12, size = 1 << order;
            std::array<float, 12> chroma {};
            juce::dsp::FFT fft (order);
            juce::dsp::WindowingFunction<float> win ((size_t) size, juce::dsp::WindowingFunction<float>::hann, false);
            std::vector<float> buf ((size_t) size * 2);

            from = std::max (0, from);
            to = std::min ((int) mono.size(), to);
            const int hop = size / 2;
            for (int p = from; p + size / 4 <= to; p += hop)
            {
                std::fill (buf.begin(), buf.end(), 0.0f);
                const int n = std::min (size, (int) mono.size() - p);
                std::copy_n (mono.begin() + p, n, buf.begin());
                win.multiplyWithWindowingTable (buf.data(), (size_t) size);
                fft.performFrequencyOnlyForwardTransform (buf.data(), true);
                for (int k = 1; k < size / 2; ++k)
                {
                    const double f = k * sr / size;
                    if (f < 55.0 || f > 5000.0) continue;
                    const int pc = ((int) std::lround (12.0 * std::log2 (f / 440.0)) + 69 + 1200) % 12;
                    chroma[(size_t) pc] += buf[(size_t) k];
                }
                if (to - p < size) break;
            }
            return chroma;
        }

        float cosineDistance (const std::array<float, 12>& a, const std::array<float, 12>& b)
        {
            float ab = 0, aa = 0, bb = 0;
            for (int i = 0; i < 12; ++i) { ab += a[(size_t) i] * b[(size_t) i]; aa += a[(size_t) i] * a[(size_t) i]; bb += b[(size_t) i] * b[(size_t) i]; }
            if (aa <= 1.0e-12f || bb <= 1.0e-12f) return 0.0f;
            return 1.0f - ab / std::sqrt (aa * bb);
        }

        float rmsOf (const std::vector<float>& m, int from, int to)
        {
            from = std::max (0, from); to = std::min ((int) m.size(), to);
            if (to <= from) return 0.0f;
            double s = 0;
            for (int i = from; i < to; ++i) s += (double) m[(size_t) i] * m[(size_t) i];
            return (float) std::sqrt (s / (to - from));
        }

        // Moves a rough onset to where the attack actually starts, then back to
        // the zero crossing just before it, so chops start clean.
        int refineOnset (const std::vector<float>& m, int pos, double sr)
        {
            const int n = (int) m.size();
            const int win = 64;
            const int from = std::max (win, pos - 512), to = std::min (n - win, pos + 768);
            int best = pos;
            float bestRise = -1.0f;
            for (int p = from; p < to; p += 16)
            {
                const float rise = rmsOf (m, p, p + win) - rmsOf (m, p - win, p);
                if (rise > bestRise) { bestRise = rise; best = p; }
            }
            const int limit = std::max (0, best - (int) (0.004 * sr));
            for (int p = best; p > limit; --p)
                if ((m[(size_t) p - 1] <= 0.0f && m[(size_t) p] > 0.0f) || (m[(size_t) p - 1] >= 0.0f && m[(size_t) p] < 0.0f))
                    return p;
            return limit;
        }

        void finaliseChops (std::vector<int> starts, int length, double sr, std::vector<Chop>& out)
        {
            std::sort (starts.begin(), starts.end());
            starts.erase (std::unique (starts.begin(), starts.end()), starts.end());
            const int minLen = (int) (0.03 * sr);
            std::vector<int> kept;
            for (int s : starts)
                if (kept.empty() || s - kept.back() >= minLen)
                    kept.push_back (s);
            if (kept.empty() || kept.front() != 0) kept.insert (kept.begin(), 0);
            while ((int) kept.size() > kMaxChops) kept.pop_back();

            out.clear();
            for (size_t i = 0; i < kept.size(); ++i)
            {
                const int end = i + 1 < kept.size() ? kept[i + 1] : length;
                if (end - kept[i] >= minLen || out.empty())
                    out.push_back ({ kept[i], end });
                else
                    out.back().end = end;
            }
        }

        // RBJ biquad, direct form 1.
        struct Biquad
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;

            void set (bool highPass, double f, double sr, double q = 0.7071)
            {
                f = juce::jlimit (10.0, sr * 0.45, f);
                const double w = juce::MathConstants<double>::twoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
                const double a0 = 1.0 + al;
                if (highPass) { b0 = (float) ((1 + c) / 2 / a0); b1 = (float) (-(1 + c) / a0); b2 = b0; }
                else          { b0 = (float) ((1 - c) / 2 / a0); b1 = (float) ((1 - c) / a0);  b2 = b0; }
                a1 = (float) (-2 * c / a0); a2 = (float) ((1 - al) / a0);
            }
            float process (float x)
            {
                const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = x; y2 = y1; y1 = y;
                return y;
            }
        };

        void fade (juce::AudioBuffer<float>& b, int start, int n, bool in)
        {
            n = std::min (n, b.getNumSamples() - start);
            if (n <= 0) return;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
            {
                auto* d = b.getWritePointer (ch, start);
                for (int i = 0; i < n; ++i)
                {
                    const float g = (float) (i + 1) / (float) (n + 1);
                    d[i] *= in ? g : 1.0f - g;
                }
            }
        }

        // Sanitises a finished buffer: NaN/inf to zero, trims anything over -0.2 dBFS.
        void sanitise (juce::AudioBuffer<float>& b)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
            {
                auto* d = b.getWritePointer (ch);
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    if (! std::isfinite (d[i])) d[i] = 0.0f;
                    peak = std::max (peak, std::abs (d[i]));
                }
            }
            if (peak > 0.98f)
                b.applyGain (0.98f / peak); // clean trim, never distorts
        }
    }

    juce::String keyName (int key)
    {
        static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        key = juce::jlimit (0, 23, key);
        return juce::String (names[key % 12]) + (key >= 12 ? " MIN" : " MAJ");
    }

    // ── Analysis ────────────────────────────────────────────────────────────
    Analysis analyse (const Sample& s)
    {
        Analysis a;
        const auto mono = monoOf (s);
        const int n = (int) mono.size();
        if (n == 0) return a;

        // Waveform overview.
        a.peaks.assign (kPeakBins, 0.0f);
        float gmax = 1.0e-9f;
        for (int b = 0; b < kPeakBins; ++b)
        {
            const int from = (int) ((int64_t) b * n / kPeakBins), to = std::max (from + 1, (int) ((int64_t) (b + 1) * n / kPeakBins));
            float m = 0.0f;
            for (int i = from; i < std::min (to, n); ++i) m = std::max (m, std::abs (mono[(size_t) i]));
            a.peaks[(size_t) b] = m;
            gmax = std::max (gmax, m);
        }
        for (auto& p : a.peaks) p /= gmax;

        // Onsets.
        a.hop = 512;
        a.onset = onsetStrength (mono, a.hop);

        // Tempo: autocorrelation of the onset curve, preferring a tempo at
        // which the sample is a whole number of bars (true for most loops).
        const double hopsPerSecond = s.sampleRate / a.hop;
        const Acf acf (a.onset);
        double bestBpm = 120.0, bestScore = -1.0e9;
        for (double bpm = kMinBpm; bpm <= kMaxBpm; bpm += 0.5)
        {
            const double sc = bpmScore (acf, bpm, hopsPerSecond);
            if (sc > bestScore) { bestScore = sc; bestBpm = bpm; }
        }

        const double dur = s.seconds();
        double fitBpm = 0.0, fitScore = -1.0e9;
        for (int beats : { 4, 8, 12, 16, 24, 32, 48, 64 })
        {
            const double bpm = 60.0 * beats / dur;
            if (bpm < kMinBpm || bpm > kMaxBpm) continue;
            const double sc = bpmScore (acf, bpm, hopsPerSecond);
            if (sc > fitScore) { fitScore = sc; fitBpm = bpm; }
        }

        // A sample that is a whole number of bars at a sensible tempo is almost
        // always a loop cut on the bar line: trust that unless the rhythm clearly
        // says otherwise.
        if (fitBpm > 0.0 && (fitScore >= 0.5 * bestScore || bestScore <= 0.05))
        {
            a.bpm = fitBpm;
            a.loopFits = true;
        }
        else
        {
            a.bpm = bestBpm;
        }

        a.beats = dur * a.bpm / 60.0;
        a.bars = std::max (1, (int) std::lround (a.beats / 4.0));

        // Key: chroma over (up to) the first 60 s against Krumhansl profiles.
        static const float major[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float minor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        const auto chroma = chromaOf (mono, 0, std::min (n, (int) (60.0 * s.sampleRate)), s.sampleRate);
        auto corr = [&] (const float* prof, int root)
        {
            float mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += chroma[(size_t) ((i + root) % 12)]; my += prof[i]; }
            mx /= 12; my /= 12;
            float sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const float x = chroma[(size_t) ((i + root) % 12)] - mx, y = prof[i] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            return sxx > 0 && syy > 0 ? sxy / std::sqrt (sxx * syy) : 0.0f;
        };
        float best = -2.0f;
        for (int root = 0; root < 12; ++root)
        {
            const float cM = corr (major, root), cm = corr (minor, root);
            if (cM > best) { best = cM; a.key = root; }
            if (cm > best) { best = cm; a.key = 12 + root; }
        }
        a.keyConfidence = best;
        return a;
    }

    // ── Chopping ────────────────────────────────────────────────────────────
    ChopResult makeChops (const Sample& s, const Analysis& a, const ChopSettings& cs)
    {
        ChopResult r;
        const int n = s.length();
        if (n == 0) return r;
        const auto mono = monoOf (s);
        const double beatLen = s.sampleRate * 60.0 / a.bpm;

        auto transient = [&] (int want)
        {
            struct Cand { int i; float v; };
            std::vector<Cand> cands;
            const auto& o = a.onset;
            for (int i = 2; i + 2 < (int) o.size(); ++i)
                if (o[(size_t) i] > 0.05f && o[(size_t) i] >= o[(size_t) i - 1] && o[(size_t) i] >= o[(size_t) i + 1]
                    && o[(size_t) i] >= o[(size_t) i - 2] && o[(size_t) i] >= o[(size_t) i + 2])
                    cands.push_back ({ i, o[(size_t) i] });
            std::sort (cands.begin(), cands.end(), [] (const Cand& x, const Cand& y) { return x.v > y.v; });

            const int minGap = (int) std::max (0.06 * s.sampleRate, beatLen / 4.0 * 0.75);
            std::vector<int> starts { 0 };
            for (const auto& c : cands)
            {
                const int pos = refineOnset (mono, c.i * a.hop + a.hop / 2, s.sampleRate);
                const bool farEnough = std::all_of (starts.begin(), starts.end(), [&] (int p) { return std::abs (p - pos) >= minGap; });
                if (farEnough) starts.push_back (pos);
                if ((int) starts.size() >= want) break;
            }
            finaliseChops (starts, n, s.sampleRate, r.chops);
        };

        if (cs.mode == ChopMode::transient)
        {
            transient (juce::jlimit (2, kMaxChops, (int) std::lround (4 + cs.sensitivity * 12)));
        }
        else if (cs.mode == ChopMode::grid)
        {
            int div = cs.gridDiv == 4 || cs.gridDiv == 16 ? cs.gridDiv : 8;
            auto count = [&] (int d) { return std::max (1, (int) std::floor (n / (beatLen * 4.0 / d) + 0.01)); };
            while (count (div) > kMaxChops && div > 4) div /= 2;
            double slice = beatLen * 4.0 / div;
            if (count (div) > kMaxChops) slice = (double) n / kMaxChops; // very long sample: 16 equal pieces
            r.effectiveGridDiv = div;

            std::vector<int> starts;
            for (double p = 0.0; p < n - 0.03 * s.sampleRate && (int) starts.size() < kMaxChops; p += slice)
                starts.push_back ((int) std::lround (p));
            finaliseChops (starts, n, s.sampleRate, r.chops);
        }
        else
        {
            const int numBeats = (int) std::floor (n / beatLen + 0.01);
            if (numBeats < 4)
            {
                transient (4);
                return r;
            }

            std::vector<std::array<float, 12>> chroma ((size_t) numBeats);
            std::vector<float> rms ((size_t) numBeats);
            for (int b = 0; b < numBeats; ++b)
            {
                const int from = (int) (b * beatLen), to = (int) ((b + 1) * beatLen);
                chroma[(size_t) b] = chromaOf (mono, from, to, s.sampleRate);
                rms[(size_t) b] = rmsOf (mono, from, to);
            }

            struct Cand { int beat; float v; };
            std::vector<Cand> cands;
            for (int b = 1; b < numBeats; ++b)
            {
                std::array<float, 12> before {}, after {};
                for (int k = std::max (0, b - 2); k < b; ++k) for (int i = 0; i < 12; ++i) before[(size_t) i] += chroma[(size_t) k][(size_t) i];
                for (int k = b; k < std::min (numBeats, b + 2); ++k) for (int i = 0; i < 12; ++i) after[(size_t) i] += chroma[(size_t) k][(size_t) i];
                float v = cosineDistance (before, after)
                        + 0.5f * std::abs (std::log ((rms[(size_t) b] + 1.0e-4f) / (rms[(size_t) b - 1] + 1.0e-4f)));
                v *= b % 4 == 0 ? 1.6f : b % 2 == 0 ? 1.15f : 1.0f;
                cands.push_back ({ b, v });
            }
            std::sort (cands.begin(), cands.end(), [] (const Cand& x, const Cand& y) { return x.v > y.v; });

            const int want = juce::jlimit (2, 8, (int) std::lround (4 + cs.sensitivity * 4));
            const int minBeats = numBeats >= 8 ? 2 : 1;
            std::vector<int> beats { 0 };
            for (const auto& c : cands)
            {
                if (std::all_of (beats.begin(), beats.end(), [&] (int b) { return std::abs (b - c.beat) >= minBeats; }))
                    beats.push_back (c.beat);
                if ((int) beats.size() >= want) break;
            }

            std::vector<int> starts;
            for (int b : beats)
            {
                const int pos = (int) std::lround (b * beatLen);
                starts.push_back (b == 0 ? 0 : refineOnset (mono, pos, s.sampleRate));
            }
            finaliseChops (starts, n, s.sampleRate, r.chops);
        }
        return r;
    }

    // ── Generator ───────────────────────────────────────────────────────────
    std::vector<Step> generate (const std::vector<Chop>& chops, const std::vector<ChopFx>& fx,
                                const Sample& s, const Analysis& a, const GenSettings& g)
    {
        std::vector<Step> out;
        const int numChops = (int) chops.size();
        if (numChops == 0) return out;

        std::vector<int> usable;
        for (int i = 0; i < numChops; ++i)
            if (i >= (int) fx.size() || fx[(size_t) i].use)
                usable.push_back (i);
        if (usable.empty())
            for (int i = 0; i < numChops; ++i) usable.push_back (i);

        // Natural chop lengths in 16ths at the sample's own tempo.
        std::vector<int> natural ((size_t) numChops);
        for (int i = 0; i < numChops; ++i)
        {
            const double secs = (chops[(size_t) i].end - chops[(size_t) i].start) / s.sampleRate;
            natural[(size_t) i] = juce::jlimit (1, 16, (int) std::lround (secs * a.bpm / 60.0 * 4.0));
        }

        juce::Random rng ((juce::int64) g.seed * 7919 + 17);
        const float v = juce::jlimit (0.0f, 1.0f, g.variation);
        const float st = juce::jlimit (0.0f, 1.0f, g.stutter);

        auto nextUsable = [&] (int after)
        {
            for (int k : usable) if (k > after) return k;
            return usable.front();
        };
        auto randomUsable = [&] { return usable[(size_t) rng.nextInt ((int) usable.size())]; };

        int last = -1;
        auto makeBar = [&] (bool first, bool fill)
        {
            std::vector<Step> bar;
            int t = 0;
            while (t < 16)
            {
                const int rem = 16 - t;
                int chop;
                if (first && t == 0)
                    chop = usable.front();
                else
                {
                    const float r = rng.nextFloat();
                    const float keepOrder = (1.0f - v) * 0.75f, doubleUp = v * 0.18f;
                    if (last >= 0 && r < keepOrder)                 chop = nextUsable (last);
                    else if (last >= 0 && r < keepOrder + doubleUp) chop = last;
                    else                                            chop = randomUsable();
                }

                if (t > 0 && rng.nextFloat() < v * 0.08f)
                {
                    t += std::min (rem, rng.nextBool() ? 1 : 2); // a little space
                    continue;
                }

                int len = juce::jlimit (1, std::min (rem, 8), natural[(size_t) chop]);
                if (rng.nextFloat() < v * 0.5f)
                {
                    static const int lens[] = { 1, 2, 2, 3, 4, 4 };
                    len = std::min (rem, lens[rng.nextInt (6)]);
                }
                if ((t + len) % 2 == 1 && len > 1 && rng.nextFloat() < 0.7f) --len; // land on 8ths mostly

                const bool stut = fill && t >= 12 ? rng.nextFloat() < 0.35f + st * 0.6f
                                                  : rng.nextFloat() < st * 0.35f;
                bar.push_back ({ t, len, chop, stut });
                last = chop;
                t += len;
            }
            return bar;
        };

        auto mutate = [&] (std::vector<Step> bar, float amount)
        {
            for (auto& stp : bar)
                if (rng.nextFloat() < amount)
                    stp.chop = randomUsable();
            return bar;
        };

        std::vector<std::vector<Step>> bars;
        const auto A = makeBar (true, g.bars == 1 && st > 0.5f);
        bars.push_back (A);
        if (g.bars >= 2)
            bars.push_back (g.bars == 2 ? mutate (makeBar (false, true), 0.0f) : mutate (A, 0.25f + 0.5f * v));
        if (g.bars >= 4)
        {
            bars.push_back (mutate (A, 0.15f + 0.3f * v));
            bars.push_back (makeBar (false, true));
        }

        for (size_t b = 0; b < bars.size(); ++b)
            for (auto stp : bars[b])
            {
                stp.t += (int) b * 16;
                out.push_back (stp);
            }
        return out;
    }

    // ── DSP building blocks ─────────────────────────────────────────────────
    juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double ratio)
    {
        const int chans = in.getNumChannels(), n = in.getNumSamples();
        const int outLen = std::max (0, (int) std::floor ((n - 1) / ratio));
        juce::AudioBuffer<float> out (chans, outLen);
        if (outLen == 0) return out;

        juce::AudioBuffer<float> padded (chans, n + 8);
        padded.clear();
        for (int ch = 0; ch < chans; ++ch) padded.copyFrom (ch, 0, in, ch, 0, n);

        for (int ch = 0; ch < chans; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, padded.getReadPointer (ch), out.getWritePointer (ch), outLen);
        }
        return out;
    }

    // WSOLA time-stretch: output is ~factor x the input length, pitch kept.
    juce::AudioBuffer<float> timeStretch (const juce::AudioBuffer<float>& in, double factor)
    {
        const int chans = in.getNumChannels(), n = in.getNumSamples();
        const int outLen = std::max (1, (int) std::lround (n * factor));
        juce::AudioBuffer<float> out (chans, outLen);
        out.clear();

        constexpr int N = 1024, Hs = N / 2, tol = 256;
        if (n < 2 * N || std::abs (factor - 1.0) < 0.005)
        {
            // Too short to stretch meaningfully: keep the audio, pad or trim.
            for (int ch = 0; ch < chans; ++ch) out.copyFrom (ch, 0, in, ch, 0, std::min (n, outLen));
            if (outLen < n) fade (out, std::max (0, outLen - 256), 256, false);
            return out;
        }

        std::vector<float> window ((size_t) N), norm ((size_t) outLen + N, 0.0f);
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);

        std::vector<float> mono ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float v = 0;
            for (int ch = 0; ch < chans; ++ch) v += in.getSample (ch, i);
            mono[(size_t) i] = v;
        }

        const double Ha = Hs / factor;
        int prevPos = 0;
        for (int k = 0;; ++k)
        {
            const int outPos = k * Hs;
            if (outPos >= outLen) break;

            int pos = (int) std::lround (k * Ha);
            if (k > 0)
            {
                // Best match to the natural continuation of the previous grain.
                const int natural = prevPos + Hs;
                int best = pos;
                float bestC = -1.0e30f;
                for (int d = -tol; d <= tol; d += 4)
                {
                    const int p = pos + d;
                    if (p < 0 || p + N > n || natural + N > n) continue;
                    float c = 0;
                    for (int i = 0; i < N; i += 8) c += mono[(size_t) (p + i)] * mono[(size_t) (natural + i)];
                    if (c > bestC) { bestC = c; best = p; }
                }
                pos = best;
            }
            pos = juce::jlimit (0, std::max (0, n - N), pos);
            prevPos = pos;

            for (int ch = 0; ch < chans; ++ch)
            {
                const float* src = in.getReadPointer (ch, pos);
                for (int i = 0; i < N && outPos + i < outLen; ++i)
                    out.addSample (ch, outPos + i, src[i] * window[(size_t) i]);
            }
            for (int i = 0; i < N && outPos + i < outLen; ++i) norm[(size_t) (outPos + i)] += window[(size_t) i];
        }

        for (int i = 0; i < outLen; ++i)
        {
            const float g = norm[(size_t) i] > 0.1f ? 1.0f / norm[(size_t) i] : 1.0f;
            for (int ch = 0; ch < chans; ++ch) out.setSample (ch, i, out.getSample (ch, i) * g);
        }
        return out;
    }

    juce::AudioBuffer<float> renderChop (const Sample& s, const Chop& c, const ChopFx& fx, double outRate, double tempoRatio)
    {
        const int start = juce::jlimit (0, s.length(), c.start), end = juce::jlimit (start, s.length(), c.end);
        juce::AudioBuffer<float> x (2, std::max (1, end - start));
        x.clear();
        for (int ch = 0; ch < 2; ++ch)
            x.copyFrom (ch, 0, s.audio, std::min (ch, s.audio.getNumChannels() - 1), start, end - start);

        if (fx.reverse)
            for (int ch = 0; ch < 2; ++ch) x.reverse (ch, 0, x.getNumSamples());

        const double stretch = juce::jlimit (0.5, 2.0, fx.stretch / 100.0) * juce::jlimit (0.25, 4.0, tempoRatio);
        if (std::abs (stretch - 1.0) > 0.005)
            x = timeStretch (x, stretch);

        const double pitchRatio = std::pow (2.0, juce::jlimit (-12, 12, fx.pitch) / 12.0);
        const double ratio = (s.sampleRate / outRate) * pitchRatio;
        if (std::abs (ratio - 1.0) > 1.0e-6)
            x = resample (x, ratio);

        const int n = x.getNumSamples();
        if (n == 0) return x;

        // Filter: one knob, low-pass to the left, high-pass to the right.
        if (fx.filter != 0)
        {
            const double amt = std::abs (fx.filter) / 100.0;
            const bool hp = fx.filter > 0;
            const double f = hp ? 20.0 * std::pow (3000.0 / 20.0, amt) : 20000.0 * std::pow (300.0 / 20000.0, amt);
            for (int ch = 0; ch < 2; ++ch)
            {
                Biquad b1, b2;
                b1.set (hp, f, outRate); b2.set (hp, f, outRate);
                auto* d = x.getWritePointer (ch);
                for (int i = 0; i < n; ++i) d[i] = b2.process (b1.process (d[i]));
            }
        }

        // Lo-fi: sample-rate drop, bit crush, dulled top, tape-ish drive.
        if (fx.tone > 0)
        {
            const float t = fx.tone / 100.0f;
            const int hold = 1 + (int) std::lround (t * 5.0f);
            const float steps = std::pow (2.0f, 16.0f - t * 10.0f);
            const float drive = 1.0f + t * 3.0f, mix = juce::jlimit (0.0f, 1.0f, t * 1.4f);
            for (int ch = 0; ch < 2; ++ch)
            {
                Biquad lp;
                lp.set (false, 12000.0 - t * 8500.0, outRate);
                auto* d = x.getWritePointer (ch);
                float held = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    if (i % hold == 0) held = d[i];
                    float w = std::round (held * steps) / steps;
                    w = std::tanh (w * drive) / std::tanh (drive);
                    w = lp.process (w);
                    d[i] = d[i] * (1.0f - mix) + w * mix;
                }
            }
        }

        fade (x, 0, (int) (0.001 * outRate), true);
        fade (x, std::max (0, n - (int) (0.006 * outRate)), (int) (0.006 * outRate), false);
        sanitise (x);
        return x;
    }

    int loopLengthSamples (const LoopSpec& spec)
    {
        return std::max (1, (int) std::lround (spec.bars * 4 * 60.0 / spec.bpm * spec.sampleRate));
    }

    juce::AudioBuffer<float> renderLoop (const std::vector<juce::AudioBuffer<float>>& chops,
                                         const std::vector<Step>& steps, const LoopSpec& spec)
    {
        const int total = loopLengthSamples (spec);
        juce::AudioBuffer<float> out (2, total);
        out.clear();

        const double stepLen = total / (spec.bars * 16.0);
        const double swingOff = juce::jlimit (0.0f, 1.0f, spec.swing) * stepLen / 3.0;
        auto posOf = [&] (int t) { return t * stepLen + (t % 2 == 1 ? swingOff : 0.0); };
        const int fadeOut = (int) (0.004 * spec.sampleRate), fadeIn = (int) (0.001 * spec.sampleRate);

        auto place = [&] (const juce::AudioBuffer<float>& src, int srcLen, int at)
        {
            juce::AudioBuffer<float> seg (2, srcLen);
            for (int ch = 0; ch < 2; ++ch) seg.copyFrom (ch, 0, src, std::min (ch, src.getNumChannels() - 1), 0, srcLen);
            fade (seg, 0, fadeIn, true);
            fade (seg, std::max (0, srcLen - fadeOut), fadeOut, false);
            for (int i = 0; i < srcLen; ++i)
            {
                const int dst = (at + i) % total;
                for (int ch = 0; ch < 2; ++ch) out.addSample (ch, dst, seg.getSample (ch, i));
            }
        };

        for (const auto& stp : steps)
        {
            if (stp.chop < 0 || stp.chop >= (int) chops.size()) continue;
            const auto& src = chops[(size_t) stp.chop];
            if (src.getNumSamples() == 0) continue;

            const int at = (int) std::lround (posOf (stp.t));
            const int gate = std::max (1, (int) std::lround (posOf (stp.t + stp.len)) - at);

            if (stp.stutter)
            {
                const int sub = std::max (64, (int) std::lround (stp.len <= 2 ? stepLen / 2.0 : stepLen));
                for (int p = 0; p < gate; p += sub)
                    place (src, std::min ({ sub, gate - p, src.getNumSamples() }), at + p);
            }
            else
            {
                place (src, std::min (gate, src.getNumSamples()), at);
            }
        }

        sanitise (out);
        return out;
    }
}
