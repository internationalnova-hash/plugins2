#include "AlignEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace NovaAlign
{
    static bool loadAudioFileToBuffer (const juce::File& file, juce::AudioBuffer<float>& buffer, double& sampleRate)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr)
            return false;

        sampleRate = reader->sampleRate;
        const int numChannels = static_cast<int> (reader->numChannels);
        const int64_t numSamples = reader->lengthInSamples;

        if (numSamples <= 0)
            return false;

        buffer.setSize (numChannels, static_cast<int> (numSamples));
        buffer.clear();
        reader->read (&buffer, 0, static_cast<int> (numSamples), 0, true, true);
        return true;
    }

    static juce::AudioBuffer<float> createMonoMix (const juce::AudioBuffer<float>& source)
    {
        const int numSamples = source.getNumSamples();
        const int numChannels = source.getNumChannels();
        juce::AudioBuffer<float> mono (1, numSamples);
        mono.clear();

        for (int channel = 0; channel < numChannels; ++channel)
            mono.addFrom (0, 0, source, channel, 0, numSamples, 1.0f / static_cast<float> (numChannels));

        return mono;
    }

    static juce::AudioBuffer<float> createEnvelope (const juce::AudioBuffer<float>& source,
                                                    int frameSize = 512,
                                                    int hopSize = 256)
    {
        const int numSamples = source.getNumSamples();
        const int numFrames = juce::jmax (1, 1 + ((numSamples - frameSize) / hopSize));
        juce::AudioBuffer<float> envelope (1, numFrames);
        envelope.clear();

        for (int frame = 0; frame < numFrames; ++frame)
        {
            const int start = frame * hopSize;
            const int length = juce::jmin (frameSize, numSamples - start);
            float energy = 0.0f;

            for (int channel = 0; channel < source.getNumChannels(); ++channel)
            {
                const float* data = source.getReadPointer (channel);
                for (int i = 0; i < length; ++i)
                    energy += data[start + i] * data[start + i];
            }

            envelope.setSample (0, frame, std::sqrt (energy / static_cast<float> (length + 1)));
        }

        return envelope;
    }

    static juce::Array<AlignEngine::AnchorPoint> detectAnchorPoints (const juce::AudioBuffer<float>& envelope,
                                                                      float sensitivity)
    {
        juce::Array<AlignEngine::AnchorPoint> anchors;
        const int numFrames = envelope.getNumSamples();
        const float threshold = juce::jmax (0.001f, sensitivity * 0.08f);

        for (int i = 2; i < numFrames - 2; ++i)
        {
            const float current = envelope.getSample (0, i);
            if (current < threshold)
                continue;

            const float previous = envelope.getSample (0, i - 1);
            const float next = envelope.getSample (0, i + 1);
            if (current >= previous && current >= next)
            {
                AlignEngine::AnchorPoint anchor;
                anchor.samplePosition = static_cast<int64_t> (i);
                anchor.strength = current;
                anchors.add (anchor);
            }
        }

        if (anchors.size() > 24)
        {
            anchors.removeRange (24, anchors.size() - 24);
        }

        return anchors;
    }

    static int64_t crossCorrelateRange (const float* guideData,
                                        const float* targetData,
                                        int64_t length,
                                        int64_t maxShift)
    {
        int64_t bestLag = 0;
        double bestScore = -1.0;

        const int64_t step = juce::jmax<int64_t> (1, maxShift / 16);

        for (int64_t lag = -maxShift; lag <= maxShift; lag += step)
        {
            int64_t guideStart = lag < 0 ? -lag : 0;
            int64_t targetStart = lag > 0 ? lag : 0;
            const int64_t overlap = length - juce::jmax (guideStart, targetStart);
            if (overlap < 32)
                continue;

            double sumGT = 0.0;
            double energyG = 0.0;
            double energyT = 0.0;

            for (int64_t i = 0; i < overlap; ++i)
            {
                const double g = guideData[guideStart + i];
                const double t = targetData[targetStart + i];
                sumGT += g * t;
                energyG += g * g;
                energyT += t * t;
            }

            const double denom = std::sqrt (energyG * energyT) + 1e-12;
            const double score = sumGT / denom;
            if (score > bestScore)
            {
                bestScore = score;
                bestLag = lag;
            }
        }

        return bestLag;
    }

    static juce::Array<AlignEngine::WarpPoint> estimateWarpPoints (const juce::AudioBuffer<float>& guideEnvelope,
                                                                    const juce::AudioBuffer<float>& targetEnvelope,
                                                                    const juce::AudioBuffer<float>& monoGuide,
                                                                    const juce::AudioBuffer<float>& monoTarget,
                                                                    const AlignEngine::AlignSettings& settings,
                                                                    int64_t sampleRate)
    {
        const auto guideAnchors = detectAnchorPoints (guideEnvelope, settings.phraseSensitivity);
        const auto targetAnchors = detectAnchorPoints (targetEnvelope, settings.phraseSensitivity);
        juce::Array<AlignEngine::WarpPoint> warpPoints;

        if (guideAnchors.isEmpty() || targetAnchors.isEmpty())
        {
            AlignEngine::WarpPoint fallback;
            fallback.samplePosition = 0;
            fallback.shiftSamples = 0;
            warpPoints.add (fallback);
            return warpPoints;
        }

        const int anchorCount = juce::jmin (guideAnchors.size(), targetAnchors.size());
        const int64_t maxShiftSamples = static_cast<int64_t> ((sampleRate * settings.maxShiftMs) / 1000.0);
        const int64_t windowHalf = static_cast<int64_t> (juce::jmax<double> (256.0, sampleRate * 0.12));

        for (int i = 0; i < anchorCount; ++i)
        {
            const int64_t guidePos = guideAnchors[i].samplePosition * 512;
            const int64_t targetPos = targetAnchors[i].samplePosition * 512;
            const int64_t guideStart = juce::jmax<int64_t> (0, guidePos - windowHalf);
            const int64_t targetStart = juce::jmax<int64_t> (0, targetPos - windowHalf);
            const int64_t guideLen = juce::jmin<int64_t> (windowHalf * 2, monoGuide.getNumSamples() - guideStart);
            const int64_t targetLen = juce::jmin<int64_t> (windowHalf * 2, monoTarget.getNumSamples() - targetStart);
            const int64_t effectiveLen = juce::jmin (guideLen, targetLen);

            if (effectiveLen < 64)
                continue;

            const float* guideData = monoGuide.getReadPointer (0) + guideStart;
            const float* targetData = monoTarget.getReadPointer (0) + targetStart;
            const int64_t step = juce::jmax<int64_t> (1, maxShiftSamples / 32);

            int64_t bestLag = 0;
            double bestScore = -1.0;
            for (int64_t lag = -maxShiftSamples; lag <= maxShiftSamples; lag += step)
            {
                const int64_t guideOffset = lag < 0 ? -lag : 0;
                const int64_t targetOffset = lag > 0 ? lag : 0;
                const int64_t overlap = effectiveLen - juce::jmax (guideOffset, targetOffset);
                if (overlap < 32)
                    continue;

                double sumGT = 0.0;
                double energyG = 0.0;
                double energyT = 0.0;
                for (int64_t j = 0; j < overlap; ++j)
                {
                    const double g = guideData[guideOffset + j];
                    const double t = targetData[targetOffset + j];
                    sumGT += g * t;
                    energyG += g * g;
                    energyT += t * t;
                }

                const double denom = std::sqrt (energyG * energyT) + 1e-12;
                const double score = sumGT / denom;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestLag = lag;
                }
            }

            const int64_t rawShift = (guidePos - targetPos) + bestLag;
            const double weightedShift = rawShift * (0.4 + settings.consonantPriority * 0.6);
            const int64_t boundedShift = static_cast<int64_t> (juce::jlimit<double> (-maxShiftSamples, maxShiftSamples, weightedShift));

            AlignEngine::WarpPoint warp;
            warp.samplePosition = targetPos;
            warp.shiftSamples = boundedShift;
            warpPoints.add (warp);
        }

        if (! warpPoints.isEmpty() && warpPoints.size() > 1)
        {
            AlignEngine::WarpPoint first;
            first.samplePosition = 0;
            first.shiftSamples = warpPoints.getFirst().shiftSamples;
            AlignEngine::WarpPoint last;
            last.samplePosition = monoTarget.getNumSamples() - 1;
            last.shiftSamples = warpPoints.getLast().shiftSamples;
            warpPoints.insert (0, first);
            warpPoints.add (last);
        }

        return warpPoints;
    }

    static int64_t interpolateWarpShift (int64_t samplePosition,
                                        const juce::Array<AlignEngine::WarpPoint>& warpPoints)
    {
        if (warpPoints.isEmpty())
            return 0;

        if (samplePosition <= warpPoints.getFirst().samplePosition)
            return warpPoints.getFirst().shiftSamples;

        if (samplePosition >= warpPoints.getLast().samplePosition)
            return warpPoints.getLast().shiftSamples;

        for (int i = 1; i < warpPoints.size(); ++i)
        {
            const auto& previous = warpPoints.getReference (i - 1);
            const auto& current = warpPoints.getReference (i);
            if (samplePosition <= current.samplePosition)
            {
                const double ratio = static_cast<double> (samplePosition - previous.samplePosition) / static_cast<double> (current.samplePosition - previous.samplePosition);
                return static_cast<int64_t> (std::round (previous.shiftSamples + ratio * (current.shiftSamples - previous.shiftSamples)));
            }
        }

        return warpPoints.getLast().shiftSamples;
    }

    static juce::AudioBuffer<float> computeWarpedBuffer (const juce::AudioBuffer<float>& input,
                                                         const juce::Array<AlignEngine::WarpPoint>& warpPoints)
    {
        const int numChannels = input.getNumChannels();
        const int numSamples = input.getNumSamples();
        juce::AudioBuffer<float> output;
        output.setSize (numChannels, numSamples);
        output.clear();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* source = input.getReadPointer (channel);
            auto* dest = output.getWritePointer (channel);

            for (int64_t sample = 0; sample < numSamples; ++sample)
            {
                int64_t shift = interpolateWarpShift (sample, warpPoints);
                const double sourceIndex = sample - static_cast<double> (shift);
                const int64_t sourceFloor = static_cast<int64_t> (std::floor (sourceIndex));
                const double frac = sourceIndex - static_cast<double> (sourceFloor);

                if (sourceFloor >= 0 && sourceFloor + 1 < numSamples)
                {
                    const float a = source[sourceFloor];
                    const float b = source[sourceFloor + 1];
                    dest[sample] = static_cast<float> (a + (b - a) * frac);
                }
                else if (sourceFloor >= 0 && sourceFloor < numSamples)
                {
                    dest[sample] = source[sourceFloor];
                }
            }
        }

        return output;
    }

    //==========================================================================
    // Alignment core (dynamic time warping)
    //
    // 1. Features every ~5.8 ms: log energy in low/mid/high bands + onset strength,
    //    normalised per file so a quieter double still matches its lead.
    // 2. Dynamic time warping inside a +/- max-shift band finds, for every moment of the
    //    target, the matching moment of the guide.
    // 3. The resulting shift curve is smoothed (Naturalness), scaled (Amount), pulled toward
    //    the overall offset (Tightness), and its changes are pushed into quiet gaps with a
    //    slope limit, so the voice isn't audibly sped up or slowed down mid-word.
    //==========================================================================
    namespace dtw
    {
        constexpr int kHop = 256;

        static std::vector<std::array<float, 4>> features(const float* x, int n, double sr)
        {
            // Band split with one-pole filters: low < ~300 Hz, high > ~3 kHz, mid = rest.
            const float aLow  = (float) std::exp(-2.0 * juce::MathConstants<double>::pi * 300.0 / sr);
            const float aHigh = (float) std::exp(-2.0 * juce::MathConstants<double>::pi * 3000.0 / sr);
            float lp = 0.0f, lp2 = 0.0f;
            const int frames = n / kHop;
            std::vector<std::array<float, 4>> f((size_t) juce::jmax(0, frames));
            std::array<double, 3> acc {};
            float prevTotal = 0.0f;
            for (int fr = 0; fr < frames; ++fr)
            {
                acc = {};
                for (int i = fr * kHop; i < (fr + 1) * kHop; ++i)
                {
                    lp  = aLow * lp + (1.0f - aLow) * x[i];
                    lp2 = aHigh * lp2 + (1.0f - aHigh) * x[i];
                    const float low = lp, high = x[i] - lp2, mid = x[i] - low - high;
                    acc[0] += low * low; acc[1] += mid * mid; acc[2] += high * high;
                }
                auto& v = f[(size_t) fr];
                float total = 0.0f;
                for (int b = 0; b < 3; ++b)
                {
                    v[(size_t) b] = (float) std::log10(acc[(size_t) b] / kHop + 1.0e-9);
                    total += (float) acc[(size_t) b];
                }
                const float lt = (float) std::log10(total / kHop + 1.0e-9);
                v[3] = juce::jmax(0.0f, lt - prevTotal) * 2.0f;   // onset strength
                prevTotal = lt;
            }
            // Normalise each feature (z-score) so level differences don't matter.
            for (int b = 0; b < 4; ++b)
            {
                double m = 0, sq = 0;
                for (auto& v : f) { m += v[(size_t) b]; sq += (double) v[(size_t) b] * v[(size_t) b]; }
                m /= juce::jmax((size_t) 1, f.size());
                const double sd = std::sqrt(juce::jmax(1.0e-9, sq / juce::jmax((size_t) 1, f.size()) - m * m));
                for (auto& v : f) v[(size_t) b] = (float) ((v[(size_t) b] - m) / sd);
            }
            return f;
        }

        static std::vector<float> energy(const float* x, int n)
        {
            std::vector<float> e((size_t) (n / kHop));
            for (size_t fr = 0; fr < e.size(); ++fr)
            {
                double a = 0;
                for (int i = (int) fr * kHop; i < ((int) fr + 1) * kHop; ++i) a += x[i] * x[i];
                e[fr] = (float) std::sqrt(a / kHop);
            }
            return e;
        }

        // Returns, per target frame, the shift in frames (guideFrame - targetFrame).
        static std::vector<float> warpPath(const std::vector<std::array<float, 4>>& g,
                                           const std::vector<std::array<float, 4>>& t, int band)
        {
            const int G = (int) g.size(), T = (int) t.size();
            std::vector<float> shift((size_t) T, 0.0f);
            if (G < 4 || T < 4) return shift;
            band = juce::jmax(4, band);
            const int W = 2 * band + 1;
            const float inf = 1.0e30f;
            // Cost and back-pointers in a band around the diagonal: j (target) vs i (guide) = j + d
            std::vector<float> D((size_t) T * W, inf);
            std::vector<unsigned char> back((size_t) T * W, 0);
            auto dist = [&] (int i, int j)
            {
                float d = 0;
                for (int b = 0; b < 4; ++b) { const float e = g[(size_t) i][(size_t) b] - t[(size_t) j][(size_t) b]; d += e * e; }
                return std::sqrt(d);
            };
            auto at = [&] (int j, int k) -> float& { return D[(size_t) j * W + (size_t) k]; };
            for (int j = 0; j < T; ++j)
                for (int k = 0; k < W; ++k)
                {
                    const int i = j + k - band;
                    if (i < 0 || i >= G) continue;
                    const float c = dist(i, j);
                    if (j == 0) { at(j, k) = c + 0.01f * std::abs(k - band); continue; }
                    // predecessors: (i-1, j-1) diag, (i, j-1) -> k+1, (i-1, j) -> k-1
                    float best = at(j - 1, k); unsigned char how = 0;                      // diagonal
                    if (k + 1 < W && at(j - 1, k + 1) + c * 0.5f < best) { best = at(j - 1, k + 1) + c * 0.5f; how = 1; }
                    if (k - 1 >= 0 && at(j, k - 1) + c * 0.5f < best) { best = at(j, k - 1) + c * 0.5f; how = 2; }
                    at(j, k) = best + c;
                    back[(size_t) j * W + (size_t) k] = how;
                }
            // end: best cell in the last target frame
            int k = band; float best = inf;
            for (int kk = 0; kk < W; ++kk) if (at(T - 1, kk) < best) { best = at(T - 1, kk); k = kk; }
            std::vector<double> sum((size_t) T, 0.0); std::vector<int> cnt((size_t) T, 0);
            for (int j = T - 1; j >= 0;)
            {
                sum[(size_t) j] += (double) (k - band); cnt[(size_t) j]++;
                const auto how = back[(size_t) j * W + (size_t) k];
                if (j == 0) break;
                if (how == 0) { --j; }
                else if (how == 1) { --j; ++k; }
                else { --k; if (k < 0) { k = 0; --j; } }
            }
            for (int j = 0; j < T; ++j) shift[(size_t) j] = cnt[(size_t) j] ? (float) (sum[(size_t) j] / cnt[(size_t) j]) : 0.0f;
            return shift;
        }
    }

    // Shift in samples for every target sample (output[n] = target[n - shift[n]]).
    static std::vector<float> computeShiftCurve(const juce::AudioBuffer<float>& monoGuide, const juce::AudioBuffer<float>& monoTarget,
                                                const AlignEngine::AlignSettings& settings, double sampleRate, float& averageShift)
    {
        const int n = monoTarget.getNumSamples();
        const auto fg = dtw::features(monoGuide.getReadPointer(0), monoGuide.getNumSamples(), sampleRate);
        const auto ft = dtw::features(monoTarget.getReadPointer(0), n, sampleRate);
        const auto et = dtw::energy(monoTarget.getReadPointer(0), n);
        const int band = (int) (sampleRate * settings.maxShiftMs / 1000.0 / dtw::kHop);
        auto path = dtw::warpPath(fg, ft, band);   // guide - target, in frames
        const int T = (int) path.size();
        std::vector<float> s((size_t) T);
        for (int j = 0; j < T; ++j) s[(size_t) j] = path[(size_t) j] * dtw::kHop;   // late target -> negative -> read ahead

        // Only trust the path where the target is actually singing; hold it through silence.
        float peak = 0; for (auto e : et) peak = juce::jmax(peak, e);
        const float gate = peak * 0.05f;
        float last = 0; bool have = false;
        for (int j = 0; j < T; ++j)
        {
            if (j < (int) et.size() && et[(size_t) j] > gate) { last = s[(size_t) j]; have = true; }
            else if (have) s[(size_t) j] = last;
        }
        for (int j = T - 1; j >= 0 && !(j < (int) et.size() && et[(size_t) j] > gate); --j) s[(size_t) j] = last;
        if (!have) std::fill(s.begin(), s.end(), 0.0f);

        // Median (removes path jitter), then the overall offset for Tightness.
        auto median = [] (const std::vector<float>& v, int r)
        {
            std::vector<float> out(v.size());
            for (int i = 0; i < (int) v.size(); ++i)
            {
                std::vector<float> w;
                for (int k = juce::jmax(0, i - r); k <= juce::jmin((int) v.size() - 1, i + r); ++k) w.push_back(v[(size_t) k]);
                std::nth_element(w.begin(), w.begin() + (long) w.size() / 2, w.end());
                out[(size_t) i] = w[w.size() / 2];
            }
            return out;
        };
        s = median(s, 4);
        std::vector<float> sorted = s;
        std::nth_element(sorted.begin(), sorted.begin() + (long) sorted.size() / 2, sorted.end());
        const float global = sorted.empty() ? 0.0f : sorted[sorted.size() / 2];
        for (auto& v : s) v = settings.amount * (global + settings.tightness * (v - global));

        // Sung segments (target loud), with short dips (< 25 ms) bridged.
        std::vector<bool> loud((size_t) T, false);
        for (int j = 0; j < T; ++j) loud[(size_t) j] = j < (int) et.size() && et[(size_t) j] > gate;
        const int bridge = (int) (0.025 * sampleRate / dtw::kHop);
        for (int j = 0; j < T;)
        {
            if (loud[(size_t) j]) { ++j; continue; }
            int k = j; while (k < T && !loud[(size_t) k]) ++k;
            if (j > 0 && k < T && k - j <= bridge) for (int q = j; q < k; ++q) loud[(size_t) q] = true;
            j = k;
        }

        // Within each segment: the segment's own median offset (a pure move, no pitch change),
        // plus slow drift (Naturalness smoothing, max 3% speed change) for long phrases.
        std::vector<float> outS(s.size(), 0.0f);
        const double tau = (0.04 + 0.25 * settings.naturalness) * sampleRate / dtw::kHop;
        const float alpha = (float) std::exp(-1.0 / juce::jmax(1.0, tau));
        const float loudLimit = 0.03f * dtw::kHop;
        std::vector<std::pair<int, int>> segs;
        for (int j = 0; j < T;)
        {
            if (!loud[(size_t) j]) { ++j; continue; }
            int k = j; while (k < T && loud[(size_t) k]) ++k;
            segs.push_back({ j, k });
            std::vector<float> seg(s.begin() + j, s.begin() + k);
            std::vector<float> srt = seg;
            std::nth_element(srt.begin(), srt.begin() + (long) srt.size() / 2, srt.end());
            const float base = srt[srt.size() / 2];
            std::vector<float> drift(seg.size());
            for (size_t q = 0; q < seg.size(); ++q) drift[q] = seg[q] - base;
            for (size_t q = 1; q < drift.size(); ++q) drift[q] = alpha * drift[q - 1] + (1.0f - alpha) * drift[q];
            for (size_t q = drift.size() - 1; q-- > 0;) drift[q] = alpha * drift[q + 1] + (1.0f - alpha) * drift[q];
            for (size_t q = 0; q < seg.size(); ++q) outS[(size_t) j + q] = base + drift[q];
            for (int pass = 0; pass < 2; ++pass)   // slope limit inside the segment
                for (int qq = 1; qq < k - j; ++qq)
                {
                    const int q = pass == 0 ? j + qq : k - 1 - qq;
                    const int pr = pass == 0 ? q - 1 : q + 1;
                    outS[(size_t) q] = juce::jlimit(outS[(size_t) pr] - loudLimit, outS[(size_t) pr] + loudLimit, outS[(size_t) q]);
                }
            j = k;
        }
        // Gaps: move from one segment's offset to the next while it's silent.
        if (segs.empty()) std::fill(outS.begin(), outS.end(), 0.0f);
        else
        {
            for (int j = 0; j < segs.front().first; ++j) outS[(size_t) j] = outS[(size_t) segs.front().first];
            for (int j = segs.back().second; j < T; ++j) outS[(size_t) j] = outS[(size_t) segs.back().second - 1];
            for (size_t g = 0; g + 1 < segs.size(); ++g)
            {
                const int a0 = segs[g].second, a1 = segs[g + 1].first;   // gap [a0, a1)
                const float v0 = outS[(size_t) a0 - 1], v1 = outS[(size_t) a1];
                for (int j = a0; j < a1; ++j)
                    outS[(size_t) j] = v0 + (v1 - v0) * (float) (j - a0 + 1) / (float) (a1 - a0 + 1);
            }
        }
        s.swap(outS);

        double avg = 0; for (auto v : s) avg += v;
        averageShift = T > 0 ? (float) (avg / T) : 0.0f;

        // Per-sample curve (linear between frame centres)
        std::vector<float> curve((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const double fpos = (double) i / dtw::kHop - 0.5;
            const int j0 = juce::jlimit(0, juce::jmax(0, T - 1), (int) std::floor(fpos));
            const int j1 = juce::jmin(juce::jmax(0, T - 1), j0 + 1);
            const float fr = (float) juce::jlimit(0.0, 1.0, fpos - j0);
            curve[(size_t) i] = T > 0 ? s[(size_t) j0] + fr * (s[(size_t) j1] - s[(size_t) j0]) : 0.0f;
        }
        return curve;
    }

    // output[n] = input[n - curve[n]] (cubic interpolation)
    static juce::AudioBuffer<float> renderWithShiftCurve (const juce::AudioBuffer<float>& input, const std::vector<float>& curve)
    {
        const int numChannels = input.getNumChannels(), n = input.getNumSamples();
        juce::AudioBuffer<float> out (numChannels, n);
        out.clear();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* x = input.getReadPointer (ch);
            float* y = out.getWritePointer (ch);
            for (int k = 0; k < n; ++k)
            {
                const double src = k - (double) curve[(size_t) k];
                const int i = (int) std::floor (src);
                const float t = (float) (src - i);
                if (i < 1 || i + 2 >= n) continue;
                const float xm1 = x[i - 1], x0 = x[i], x1 = x[i + 1], x2 = x[i + 2];
                const float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f, b = w + a;
                y[k] = (((a * t) - b) * t + c) * t + x0;
            }
        }
        return out;
    }

    static bool writeBufferToWavFile (const juce::AudioBuffer<float>& buffer,
                                      const juce::File& destinationFile,
                                      double sampleRate)
    {
        std::unique_ptr<juce::FileOutputStream> fileStream (destinationFile.createOutputStream());
        if (fileStream == nullptr || ! fileStream->openedOk())
            return false;

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (fileStream.release(), sampleRate, static_cast<unsigned int> (buffer.getNumChannels()), 32, {}, 0));
        if (writer == nullptr)
            return false;

        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        return true;
    }

    // Shared core: loads both files, computes the warp curve, and renders the
    // warped Target into outBuffer/outSampleRate. Both public entry points
    // delegate here so file-writing and in-memory use share one code path.
    static bool alignTargetClipCore (const juce::File& guideFile,
                                     const juce::File& targetFile,
                                     const AlignEngine::AlignSettings& settings,
                                     juce::AudioBuffer<float>& outBuffer,
                                     double& outSampleRate,
                                     int64_t& outShiftSamples)
    {
        juce::AudioBuffer<float> guideBuffer;
        juce::AudioBuffer<float> targetBuffer;
        double guideSampleRate = 0.0;
        double targetSampleRate = 0.0;

        if (! loadAudioFileToBuffer (guideFile, guideBuffer, guideSampleRate))
            return false;
        if (! loadAudioFileToBuffer (targetFile, targetBuffer, targetSampleRate))
            return false;

        // Different sample rates: convert the target to the guide's rate.
        if (std::abs (guideSampleRate - targetSampleRate) > 1e-3)
        {
            const double ratio = targetSampleRate / guideSampleRate;
            const int newLen = (int) std::floor (targetBuffer.getNumSamples() / ratio);
            juce::AudioBuffer<float> conv (targetBuffer.getNumChannels(), juce::jmax (1, newLen));
            for (int ch = 0; ch < targetBuffer.getNumChannels(); ++ch)
            {
                juce::LagrangeInterpolator interp;
                interp.process (ratio, targetBuffer.getReadPointer (ch), conv.getWritePointer (ch), newLen);
            }
            targetBuffer = std::move (conv);
            targetSampleRate = guideSampleRate;
        }

        const double sampleRate = targetSampleRate;
        const auto monoGuide = createMonoMix (guideBuffer);
        const auto monoTarget = createMonoMix (targetBuffer);
        float averageShift = 0.0f;
        const auto curve = computeShiftCurve (monoGuide, monoTarget, settings, sampleRate, averageShift);
        outSampleRate = sampleRate;
        outShiftSamples = (int64_t) std::llround (averageShift);
        outBuffer = renderWithShiftCurve (targetBuffer, curve);
        return true;
    }

    bool AlignEngine::alignTargetClip (const juce::File& guideFile,
                                       const juce::File& targetFile,
                                       const juce::File& destinationFile,
                                       const AlignSettings& settings,
                                       int64_t& outShiftSamples)
    {
        juce::AudioBuffer<float> renderedBuffer;
        double sampleRate = 0.0;

        if (! alignTargetClipCore (guideFile, targetFile, settings, renderedBuffer, sampleRate, outShiftSamples))
            return false;

        return writeBufferToWavFile (renderedBuffer, destinationFile, sampleRate);
    }

    bool AlignEngine::alignTargetClipToBuffer (const juce::File& guideFile,
                                               const juce::File& targetFile,
                                               const AlignSettings& settings,
                                               juce::AudioBuffer<float>& outBuffer,
                                               double& outSampleRate,
                                               int64_t& outShiftSamples)
    {
        return alignTargetClipCore (guideFile, targetFile, settings, outBuffer, outSampleRate, outShiftSamples);
    }
}
