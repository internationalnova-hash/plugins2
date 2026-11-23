#include "TimeStretcher.h"

namespace NovaStudio
{
namespace TimeStretcher
{
    namespace
    {
        constexpr double kTwoPi = juce::MathConstants<double>::twoPi;

        inline double wrapPhase(double p) noexcept
        {
            return p - kTwoPi * std::round(p / kTwoPi);
        }

        struct Spectrum
        {
            std::vector<float> mag, phase;
            void resize(int bins) { mag.assign((size_t) bins, 0.0f); phase.assign((size_t) bins, 0.0f); }
        };

        // Analyses one windowed frame of `src` centred at `centre` (zero outside the signal).
        struct Analyser
        {
            Analyser(int order) : fft(order), n(1 << order), window((size_t) n), work((size_t) n * 2)
            {
                for (int i = 0; i < n; ++i)
                    window[(size_t) i] = 0.5f - 0.5f * (float) std::cos(kTwoPi * i / n);
            }

            void analyse(const float* src, int64_t len, int64_t centre, Spectrum& out)
            {
                const int64_t start = centre - n / 2;
                for (int i = 0; i < n; ++i)
                {
                    const int64_t idx = start + i;
                    work[(size_t) i] = (idx >= 0 && idx < len) ? src[idx] * window[(size_t) i] : 0.0f;
                }
                std::fill(work.begin() + n, work.end(), 0.0f);
                fft.performRealOnlyForwardTransform(work.data(), true);
                const int bins = n / 2 + 1;
                for (int b = 0; b < bins; ++b)
                {
                    const float re = work[(size_t) (2 * b)], im = work[(size_t) (2 * b + 1)];
                    out.mag[(size_t) b] = std::sqrt(re * re + im * im);
                    out.phase[(size_t) b] = std::atan2(im, re);
                }
            }

            // Inverse-transforms (mag, phase), windows and overlap-adds into dst at `centre`.
            void synthesise(const std::vector<float>& mag, const std::vector<double>& phase, float* dst, int64_t len, int64_t centre, float gain)
            {
                const int bins = n / 2 + 1;
                std::fill(work.begin(), work.end(), 0.0f);
                for (int b = 0; b < bins; ++b)
                {
                    work[(size_t) (2 * b)]     = mag[(size_t) b] * (float) std::cos(phase[(size_t) b]);
                    work[(size_t) (2 * b + 1)] = mag[(size_t) b] * (float) std::sin(phase[(size_t) b]);
                }
                fft.performRealOnlyInverseTransform(work.data());
                const int64_t start = centre - n / 2;
                for (int i = 0; i < n; ++i)
                {
                    const int64_t idx = start + i;
                    if (idx >= 0 && idx < len)
                        dst[idx] += work[(size_t) i] * window[(size_t) i] * gain;
                }
            }

            juce::dsp::FFT fft;
            int n;
            std::vector<float> window;
            std::vector<float> work;
        };
    }

    juce::AudioBuffer<float> stretch(const juce::AudioBuffer<float>& input, double sampleRate, double ratio,
                                     Mode mode, std::function<bool(float)> progress)
    {
        ratio = juce::jlimit(0.25, 4.0, ratio);
        const int numCh = juce::jlimit(1, 2, input.getNumChannels());
        const int64_t inLen = input.getNumSamples();
        const int64_t outLen = (int64_t) std::llround((double) inLen * ratio);
        juce::AudioBuffer<float> out(numCh, (int) juce::jmax((int64_t) 1, outLen));
        out.clear();
        if (inLen < 16 || outLen < 16)
            return out;

        if (std::abs(ratio - 1.0) < 1.0e-6)
        {
            for (int c = 0; c < numCh; ++c) out.copyFrom(c, 0, input, c, 0, (int) inLen);
            return out;
        }

        // Window: ~93 ms for mixes (tonal clarity), ~46 ms for beats (tighter transients).
        int order = (mode == Mode::Beats ? 11 : 12) + (sampleRate > 60000.0 ? 1 : 0);
        Analyser an(order);
        const int N = an.n;
        const int bins = N / 2 + 1;
        const int Hs = N / 4;                     // synthesis hop (75% overlap)
        const double Ha = (double) Hs / ratio;    // analysis hop
        // Hann analysis * Hann synthesis at 75% overlap sums to 1.5; JUCE's inverse FFT is unscaled by 1/N... scaled below.
        const float olaGain = 1.0f / 1.5f;

        // Mid signal drives the phase advance; each channel keeps its offset from it.
        std::vector<float> mid((size_t) inLen);
        for (int64_t i = 0; i < inLen; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < numCh; ++c) v += input.getSample(c, (int) i);
            mid[(size_t) i] = v / (float) numCh;
        }

        Spectrum midNow, midPrev, chan[2];
        midNow.resize(bins); midPrev.resize(bins);
        for (auto& s : chan) s.resize(bins);
        std::vector<double> synthMid((size_t) bins, 0.0), outPhase((size_t) bins, 0.0), instFreq((size_t) bins, 0.0);
        std::vector<int> peaks; peaks.reserve((size_t) bins);
        std::vector<float> prevMag((size_t) bins, 0.0f);

        double fluxMean = 0.0, prevFlux = 0.0;
        int framesSinceTransient = 100;
        const double transientThresh = mode == Mode::Beats ? 1.6 : 2.2;

        const int64_t numFrames = outLen / Hs + 2;
        for (int64_t k = 0; k < numFrames; ++k)
        {
            const int64_t anaCentre = (int64_t) std::llround((double) k * Ha);
            const int64_t synCentre = k * Hs;

            an.analyse(mid.data(), inLen, anaCentre, midNow);
            an.analyse(mid.data(), inLen, anaCentre - Hs, midPrev);   // exact hop Hs for frequency estimate

            // Transient detection: positive spectral flux (log magnitude) vs its running mean.
            double flux = 0.0;
            for (int b = 1; b < bins; ++b)
            {
                const double d = std::log1p(midNow.mag[(size_t) b]) - std::log1p(prevMag[(size_t) b]);
                if (d > 0.0) flux += d;
            }
            prevMag = midNow.mag;
            const bool transient = k > 0 && framesSinceTransient > 2 && fluxMean > 1.0e-9
                                   && flux > fluxMean * transientThresh && flux > prevFlux;
            fluxMean = k == 0 ? flux : fluxMean * 0.9 + flux * 0.1;
            prevFlux = flux;
            framesSinceTransient = transient ? 0 : framesSinceTransient + 1;

            if (k == 0 || transient)
            {
                // Start, or a hit: take the analysis phases as they are (keeps attacks sharp).
                for (int b = 0; b < bins; ++b) synthMid[(size_t) b] = midNow.phase[(size_t) b];
            }
            else
            {
                // Instantaneous frequency per bin from the two frames one synthesis hop apart.
                for (int b = 0; b < bins; ++b)
                {
                    const double omega = kTwoPi * b / N;
                    const double delta = wrapPhase((double) midNow.phase[(size_t) b] - midPrev.phase[(size_t) b] - omega * Hs);
                    instFreq[(size_t) b] = omega + delta / Hs;
                }
                // Identity phase locking: advance spectral peaks, lock their neighbours to them.
                peaks.clear();
                for (int b = 2; b < bins - 2; ++b)
                {
                    const float m = midNow.mag[(size_t) b];
                    if (m > midNow.mag[(size_t) b - 1] && m >= midNow.mag[(size_t) b + 1]
                        && m > midNow.mag[(size_t) b - 2] && m >= midNow.mag[(size_t) b + 2])
                        peaks.push_back(b);
                }
                std::vector<double> newPhase((size_t) bins);
                if (peaks.empty())
                {
                    for (int b = 0; b < bins; ++b) newPhase[(size_t) b] = synthMid[(size_t) b] + instFreq[(size_t) b] * Hs;
                }
                else
                {
                    size_t pi = 0;
                    for (int b = 0; b < bins; ++b)
                    {
                        while (pi + 1 < peaks.size() && std::abs(peaks[pi + 1] - b) < std::abs(peaks[pi] - b)) ++pi;
                        const int p = peaks[pi];
                        const double peakPhase = synthMid[(size_t) p] + instFreq[(size_t) p] * Hs;
                        newPhase[(size_t) b] = peakPhase + ((double) midNow.phase[(size_t) b] - midNow.phase[(size_t) p]);
                    }
                }
                for (int b = 0; b < bins; ++b) synthMid[(size_t) b] = wrapPhase(newPhase[(size_t) b]);
            }

            for (int c = 0; c < numCh; ++c)
            {
                an.analyse(input.getReadPointer(c), inLen, anaCentre, chan[c]);
                for (int b = 0; b < bins; ++b)
                {
                    const bool midSilent = midNow.mag[(size_t) b] < 1.0e-7f;
                    outPhase[(size_t) b] = midSilent ? (double) chan[c].phase[(size_t) b]
                                                     : synthMid[(size_t) b] + ((double) chan[c].phase[(size_t) b] - midNow.phase[(size_t) b]);
                }
                an.synthesise(chan[c].mag, outPhase, out.getWritePointer(c), outLen, synCentre, olaGain);
            }

            if (progress && (k & 63) == 0 && !progress((float) k / (float) numFrames))
                return {};
        }
        return out;
    }

    double detectTempo(const juce::AudioBuffer<float>& input, double sampleRate)
    {
        const int64_t len = input.getNumSamples();
        if (len < sampleRate * 2.0) return 0.0;
        const int hop = 512;
        const int order = 10;
        Analyser an(order);
        const int bins = an.n / 2 + 1;
        std::vector<float> mono((size_t) len);
        for (int64_t i = 0; i < len; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < input.getNumChannels(); ++c) v += input.getSample(c, (int) i);
            mono[(size_t) i] = v;
        }
        // Onset envelope: positive log spectral flux.
        Spectrum s; s.resize(bins);
        std::vector<float> prev((size_t) bins, 0.0f);
        std::vector<double> env;
        for (int64_t c = 0; c < len; c += hop)
        {
            an.analyse(mono.data(), len, c, s);
            double f = 0.0;
            for (int b = 1; b < bins; ++b)
            {
                const double d = std::log1p(10.0 * s.mag[(size_t) b]) - std::log1p(10.0 * prev[(size_t) b]);
                if (d > 0.0) f += d;
            }
            prev = s.mag;
            env.push_back(f);
        }
        double mean = 0.0;
        for (auto v : env) mean += v;
        mean /= (double) env.size();
        for (auto& v : env) v = juce::jmax(0.0, v - mean);

        const double fps = sampleRate / hop;
        auto acf = [&] (double lag)
        {
            const int L = (int) std::round(lag);
            double sum = 0.0;
            for (size_t i = (size_t) L; i < env.size(); ++i) sum += env[i] * env[i - (size_t) L];
            return sum / (double) juce::jmax((size_t) 1, env.size() - (size_t) L);
        };

        double best = 0.0, bestBpm = 0.0, total = 0.0; int count = 0;
        for (double bpm = 60.0; bpm <= 200.0; bpm += 0.25)
        {
            const double lag = fps * 60.0 / bpm;
            // Beat, half bar and bar all agreeing makes a tempo believable.
            double score = acf(lag) + 0.5 * acf(lag * 2.0) + 0.25 * acf(lag * 4.0);
            const double prior = std::exp(-0.5 * std::pow(std::log2(bpm / 110.0) / 0.7, 2.0));
            score *= prior;
            total += score; ++count;
            if (score > best) { best = score; bestBpm = bpm; }
        }
        if (count == 0 || best < 1.3 * (total / count)) return 0.0;
        while (bestBpm < 70.0) bestBpm *= 2.0;
        while (bestBpm > 180.0) bestBpm /= 2.0;
        // Loops are usually whole numbers of beats: snap when close.
        const double rounded = std::round(bestBpm);
        return std::abs(rounded - bestBpm) < 0.6 ? rounded : bestBpm;
    }

    double tempoFromFileName(const juce::String& fileName)
    {
        const auto s = fileName.toLowerCase();
        int idx = s.indexOf("bpm");
        while (idx >= 0)
        {
            // number right before "bpm" (allowing "92bpm", "92 bpm", "92_bpm", "92-bpm")
            int e = idx - 1;
            while (e >= 0 && (s[e] == ' ' || s[e] == '_' || s[e] == '-')) --e;
            int b = e;
            while (b >= 0 && (juce::CharacterFunctions::isDigit(s[b]) || s[b] == '.')) --b;
            if (e > b)
            {
                const double v = s.substring(b + 1, e + 1).getDoubleValue();
                if (v >= 40.0 && v <= 300.0) return v;
            }
            // number right after "bpm" ("bpm92", "bpm_140")
            int a = idx + 3;
            while (a < s.length() && (s[a] == ' ' || s[a] == '_' || s[a] == '-')) ++a;
            int z = a;
            while (z < s.length() && (juce::CharacterFunctions::isDigit(s[z]) || s[z] == '.')) ++z;
            if (z > a)
            {
                const double v = s.substring(a, z).getDoubleValue();
                if (v >= 40.0 && v <= 300.0) return v;
            }
            idx = s.indexOf(idx + 3, "bpm");
        }
        return 0.0;
    }
}
}
