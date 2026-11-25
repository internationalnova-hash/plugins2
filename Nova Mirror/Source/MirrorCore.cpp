#include "MirrorCore.h"

#include <algorithm>
#include <cmath>

namespace NovaMirror
{
namespace
{
    constexpr float kTwoPi = 6.28318530718f;
    inline float toDb (float p) noexcept { return 10.f * std::log10 (p + 1.0e-14f); }

    struct ModeTuning { float speedMul, maxDb, release; int freqSmooth; };
    ModeTuning tuning (int mode) noexcept
    {
        switch (mode)
        {
            case 0:  return { 1.0f, 9.f, 0.16f, 3 };   // vocal: smooth across frequency
            case 1:  return { 0.7f, 10.f, 0.12f, 2 };  // beat: quicker
            case 3:  return { 1.8f, 4.f, 0.30f, 3 };   // master: gentle and slow
            default: return { 1.0f, 8.f, 0.15f, 2 };   // mix bus
        }
    }

    void smoothAcross (BandArray& v, int passes)
    {
        for (int p = 0; p < passes; ++p)
        {
            BandArray o = v;
            for (int b = 0; b < kBands; ++b)
            {
                const float l = v[(size_t) std::max (0, b - 1)], r = v[(size_t) std::min (kBands - 1, b + 1)];
                o[(size_t) b] = 0.25f * l + 0.5f * v[(size_t) b] + 0.25f * r;
            }
            v = o;
        }
    }

    float rangeWeight (float f, float from, float to) noexcept
    {
        if (f < from) return std::max (0.f, 1.f - std::log2 (from / f) / 0.33f);
        if (f > to)   return std::max (0.f, 1.f - std::log2 (f / to) / 0.33f);
        return 1.f;
    }
}

float bandCenter (int b) noexcept
{
    return kFMin * std::pow (kFMax / kFMin, (float) b / (float) (kBands - 1));
}

int bandIndexFor (float hz) noexcept
{
    const float x = std::log (hz / kFMin) / std::log (kFMax / kFMin) * (float) (kBands - 1);
    return juce::jlimit (0, kBands - 1, (int) std::lround (x));
}

const char* sectionName (int cls) noexcept
{
    switch (cls)
    {
        case secBreak: return "Break";
        case secVerse: return "Verse";
        case secHook:  return "Hook";
        default:       return "Whole song";
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Band map
// ════════════════════════════════════════════════════════════════════════════
void BandMap::build (double sampleRate, int fftSize)
{
    numBins = fftSize / 2 + 1;
    const float binHz = (float) sampleRate / (float) fftSize;
    const float step = std::pow (kFMax / kFMin, 1.f / (float) (kBands - 1));
    for (int b = 0; b < kBands; ++b)
    {
        const float c = bandCenter (b), lo = c / step, hi = c * step;
        auto& band = bands[(size_t) b];
        band.w.clear();
        int first = std::max (1, (int) std::floor (lo / binHz));
        int last  = std::min (numBins - 1, (int) std::ceil (hi / binHz));
        if (last < first) last = first;
        band.first = first;
        float sum = 0.f;
        for (int k = first; k <= last; ++k)
        {
            const float f = (float) k * binHz;
            float w = 0.f;
            if (f >= lo && f <= c) w = (f - lo) / (c - lo);
            else if (f > c && f <= hi) w = (hi - f) / (hi - c);
            band.w.push_back (w);
            sum += w;
        }
        if (sum < 1.0e-6f)   // band narrower than a bin: use the nearest bin
        {
            band.first = juce::jlimit (1, numBins - 1, (int) std::lround (c / binHz));
            band.w.assign (1, 1.f);
            sum = 1.f;
        }
        band.norm = 1.f / sum;
    }

    binLo.assign ((size_t) numBins, 0);
    binFrac.assign ((size_t) numBins, 0.f);
    for (int k = 0; k < numBins; ++k)
    {
        const float f = std::max (kFMin, (float) k * binHz);
        float x = std::log (f / kFMin) / std::log (kFMax / kFMin) * (float) (kBands - 1);
        x = juce::jlimit (0.f, (float) (kBands - 1), x);
        const int lo = std::min (kBands - 2, (int) std::floor (x));
        binLo[(size_t) k] = lo;
        binFrac[(size_t) k] = x - (float) lo;
    }
}

void BandMap::bandPower (const float* binPower, BandArray& out) const
{
    for (int b = 0; b < kBands; ++b)
    {
        const auto& band = bands[(size_t) b];
        float s = 0.f;
        for (size_t i = 0; i < band.w.size(); ++i) s += band.w[i] * binPower[band.first + (int) i];
        out[(size_t) b] = s * band.norm;
    }
}

void BandMap::binGains (const BandArray& gainDb, float* out) const
{
    for (int k = 0; k < numBins; ++k)
    {
        const int lo = binLo[(size_t) k];
        const float u = binFrac[(size_t) k];
        const float db = gainDb[(size_t) lo] + (gainDb[(size_t) lo + 1] - gainDb[(size_t) lo]) * u;
        out[k] = std::pow (10.f, db / 20.f);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Profiles
// ════════════════════════════════════════════════════════════════════════════
juce::var Profile::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name);
    o->setProperty ("kind", kind);
    o->setProperty ("seconds", seconds);
    auto arr = [] (const BandArray& a)
    {
        juce::Array<juce::var> v;
        for (float x : a) v.add (std::round (x * 100.f) / 100.f);
        return juce::var (v);
    };
    juce::Array<juce::var> m, s, h;
    for (int c = 0; c < kSectionClasses; ++c) { m.add (arr (mono[(size_t) c])); s.add (arr (side[(size_t) c])); h.add (has[(size_t) c]); }
    o->setProperty ("mono", m);
    o->setProperty ("side", s);
    o->setProperty ("has", h);
    juce::Array<juce::var> secs;
    for (auto& sec : sections)
    {
        auto* so = new juce::DynamicObject();
        so->setProperty ("s", sec.start); so->setProperty ("e", sec.end); so->setProperty ("c", sec.cls);
        secs.add (juce::var (so));
    }
    o->setProperty ("sections", secs);
    juce::Array<juce::var> pk;
    for (float p : peaks) pk.add (std::round (p * 1000.f) / 1000.f);
    o->setProperty ("peaks", pk);
    return juce::var (o);
}

Profile Profile::fromVar (const juce::var& v)
{
    Profile p;
    p.name = v["name"].toString();
    p.kind = v["kind"].toString().isNotEmpty() ? v["kind"].toString() : juce::String ("reference");
    p.seconds = (double) v["seconds"];
    auto load = [] (const juce::var& a, BandArray& out)
    {
        if (auto* arr = a.getArray())
            for (int b = 0; b < kBands && b < arr->size(); ++b) out[(size_t) b] = (float) (double) (*arr)[b];
    };
    for (int c = 0; c < kSectionClasses; ++c)
    {
        if (auto* m = v["mono"].getArray()) if (c < m->size()) load ((*m)[c], p.mono[(size_t) c]);
        if (auto* s = v["side"].getArray()) if (c < s->size()) load ((*s)[c], p.side[(size_t) c]);
        if (auto* h = v["has"].getArray()) if (c < h->size()) p.has[(size_t) c] = (bool) (*h)[c];
    }
    p.has[0] = true;
    if (auto* secs = v["sections"].getArray())
        for (auto& s : *secs) p.sections.push_back ({ (float) (double) s["s"], (float) (double) s["e"], (int) s["c"] });
    if (auto* pk = v["peaks"].getArray())
        for (auto& x : *pk) p.peaks.push_back ((float) (double) x);
    return p;
}

Profile analyseReference (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name)
{
    Profile prof;
    prof.name = name;
    prof.kind = "reference";
    const int n = audio.getNumSamples(), nch = audio.getNumChannels();
    prof.seconds = n / sampleRate;
    if (n <= 0 || nch <= 0) return prof;

    const int order = sampleRate > 64000.0 ? 13 : 12, N = 1 << order, hop = N / 2;
    juce::dsp::FFT fft (order);
    BandMap map;
    map.build (sampleRate, N);
    std::vector<float> win ((size_t) N), a ((size_t) N * 2), b ((size_t) N * 2), pm ((size_t) N / 2 + 1), ps ((size_t) N / 2 + 1);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) N);

    const float* L = audio.getReadPointer (0);
    const float* R = nch > 1 ? audio.getReadPointer (1) : L;

    struct FrameInfo { BandArray m, s; float total; };
    std::vector<FrameInfo> frames;
    for (int start = 0; start + N <= std::max (n, N); start += hop)
    {
        std::fill (a.begin(), a.end(), 0.f);
        std::fill (b.begin(), b.end(), 0.f);
        for (int i = 0; i < N; ++i)
        {
            const int k = start + i;
            const float l = k < n ? L[k] : 0.f, r = k < n ? R[k] : 0.f;
            a[(size_t) i] = 0.5f * (l + r) * win[(size_t) i];
            b[(size_t) i] = 0.5f * (l - r) * win[(size_t) i];
        }
        fft.performRealOnlyForwardTransform (a.data(), true);
        fft.performRealOnlyForwardTransform (b.data(), true);
        float total = 0.f;
        for (int k = 0; k <= N / 2; ++k)
        {
            pm[(size_t) k] = a[(size_t) 2 * k] * a[(size_t) 2 * k] + a[(size_t) 2 * k + 1] * a[(size_t) 2 * k + 1];
            ps[(size_t) k] = b[(size_t) 2 * k] * b[(size_t) 2 * k] + b[(size_t) 2 * k + 1] * b[(size_t) 2 * k + 1];
            total += pm[(size_t) k];
        }
        FrameInfo fi;
        map.bandPower (pm.data(), fi.m);
        map.bandPower (ps.data(), fi.s);
        fi.total = total;
        frames.push_back (fi);
        if (start + N >= n) break;
    }

    // 3 s windows, 1.5 s apart, classed by loudness relative to the loudest
    const double frameSec = hop / sampleRate;
    const int wFrames = std::max (1, (int) std::lround (3.0 / frameSec)), wHop = std::max (1, wFrames / 2);
    struct Win { int a, b; float level; int cls; };
    std::vector<Win> wins;
    for (int a0 = 0; a0 < (int) frames.size(); a0 += wHop)
    {
        const int b0 = std::min ((int) frames.size(), a0 + wFrames);
        double s = 0;
        for (int f = a0; f < b0; ++f) s += frames[(size_t) f].total;
        wins.push_back ({ a0, b0, (float) (s / std::max (1, b0 - a0)), secAll });
        if (b0 == (int) frames.size()) break;
    }
    float maxLevel = 0.f;
    for (auto& w : wins) maxLevel = std::max (maxLevel, w.level);

    std::array<BandArray, kSectionClasses> sumM {}, sumS {};
    std::array<int, kSectionClasses> count {};
    const bool longEnough = prof.seconds >= 8.0 && wins.size() >= 3;
    for (auto& w : wins)
    {
        const float rel = toDb (w.level) - toDb (maxLevel);
        if (rel < -30.f) { w.cls = -1; continue; }   // silence
        w.cls = ! longEnough ? secAll : rel > -2.5f ? secHook : rel > -7.f ? secVerse : secBreak;
    }
    for (const auto& fr : frames)   // whole-song average over non-silent frames
    {
        if (toDb (fr.total) < toDb (maxLevel) - 30.f) continue;
        for (int b = 0; b < kBands; ++b) { sumM[0][(size_t) b] += fr.m[(size_t) b]; sumS[0][(size_t) b] += fr.s[(size_t) b]; }
        ++count[0];
    }
    if (longEnough)
        for (const auto& w : wins)
        {
            if (w.cls <= 0) continue;
            for (int f = w.a; f < w.b; ++f)
                for (int b = 0; b < kBands; ++b)
                {
                    sumM[(size_t) w.cls][(size_t) b] += frames[(size_t) f].m[(size_t) b];
                    sumS[(size_t) w.cls][(size_t) b] += frames[(size_t) f].s[(size_t) b];
                }
            count[(size_t) w.cls] += w.b - w.a;
        }
    for (int c = 0; c < kSectionClasses; ++c)
    {
        prof.has[(size_t) c] = count[(size_t) c] > 0;
        const int cc = std::max (1, count[(size_t) c]);
        for (int b = 0; b < kBands; ++b)
        {
            prof.mono[(size_t) c][(size_t) b] = toDb (sumM[(size_t) c][(size_t) b] / (float) cc);
            prof.side[(size_t) c][(size_t) b] = toDb (sumS[(size_t) c][(size_t) b] / (float) cc);
        }
    }
    prof.has[0] = true;

    // Display sections: merge neighbouring windows of the same class
    for (const auto& w : wins)
    {
        const float s = (float) (w.a * frameSec), e = (float) (std::min ((int) frames.size(), w.a + wHop) * frameSec);
        const int cls = w.cls < 0 ? secBreak : w.cls;
        if (! prof.sections.empty() && prof.sections.back().cls == cls) prof.sections.back().end = e;
        else prof.sections.push_back ({ s, e, cls });
    }
    if (! prof.sections.empty()) prof.sections.back().end = (float) prof.seconds;

    // Waveform overview
    const int P = 600;
    prof.peaks.assign (P, 0.f);
    float peakAll = 1.0e-6f;
    for (int p = 0; p < P; ++p)
    {
        const int a0 = (int) ((int64_t) n * p / P), b0 = (int) ((int64_t) n * (p + 1) / P);
        float m = 0.f;
        for (int i = a0; i < b0; ++i) m = std::max ({ m, std::abs (L[i]), std::abs (R[i]) });
        prof.peaks[(size_t) p] = m;
        peakAll = std::max (peakAll, m);
    }
    for (auto& p : prof.peaks) p /= peakAll;
    return prof;
}

// ════════════════════════════════════════════════════════════════════════════
// Nova Targets: designed tonal-balance curves (constant-Q band levels, dB)
// ════════════════════════════════════════════════════════════════════════════
namespace
{
    struct Bump { float hz, db, octaves; };
    struct TargetDef { const char* name; float tilt; std::vector<Bump> bumps; float hiRoll; };

    const std::vector<TargetDef>& defs()
    {
        static const std::vector<TargetDef> d {
            { "Nova Balance", -4.0f, { { 55, 3.0f, 1.2f }, { 300, -1.5f, 1.2f }, { 3000, 1.0f, 1.5f }, { 12000, 1.0f, 1.2f } }, 16000 },
            { "Trap",         -4.2f, { { 48, 7.0f, 1.1f }, { 350, -2.0f, 1.3f }, { 3000, 1.0f, 1.4f }, { 11000, 0.5f, 1.2f } }, 16000 },
            { "Drill",        -4.0f, { { 45, 8.0f, 1.0f }, { 300, -2.5f, 1.2f }, { 4000, 1.5f, 1.4f } }, 15000 },
            { "R&B",          -4.5f, { { 60, 4.0f, 1.2f }, { 200, 1.5f, 1.0f }, { 2500, -0.5f, 1.2f }, { 12000, 1.5f, 1.4f } }, 17000 },
            { "Reggaeton",    -4.0f, { { 60, 5.0f, 1.1f }, { 1500, 1.0f, 1.5f }, { 5000, 2.0f, 1.3f } }, 16000 },
            { "Afrobeats",    -4.0f, { { 70, 4.0f, 1.2f }, { 400, -1.0f, 1.2f }, { 6000, 2.0f, 1.4f } }, 16000 },
            { "Pop",          -3.8f, { { 60, 3.0f, 1.2f }, { 3000, 1.5f, 1.4f }, { 12000, 2.0f, 1.3f } }, 18000 },
            { "House",        -3.5f, { { 55, 6.0f, 1.0f }, { 400, -1.5f, 1.2f }, { 10000, 2.0f, 1.4f } }, 18000 },
        };
        return d;
    }
}

juce::StringArray novaTargetNames()
{
    juce::StringArray s;
    for (auto& d : defs()) s.add (d.name);
    return s;
}

Profile novaTarget (const juce::String& name)
{
    Profile p;
    p.name = name;
    p.kind = "nova";
    const TargetDef* def = &defs().front();
    for (auto& d : defs()) if (name == d.name) def = &d;
    for (int b = 0; b < kBands; ++b)
    {
        const float f = bandCenter (b);
        float db = def->tilt * std::log2 (f / 1000.f);
        if (f < 35.f) db -= 12.f * std::log2 (35.f / f);          // sub-sonic roll-off
        for (const auto& bump : def->bumps)
        {
            const float x = std::log2 (f / bump.hz) / bump.octaves;
            db += bump.db * std::exp (-x * x * 2.f);
        }
        if (f > def->hiRoll) db -= 18.f * std::log2 (f / def->hiRoll);
        p.mono[0][(size_t) b] = db;
        // sides: quieter, little low end
        p.side[0][(size_t) b] = db - 12.f - (f < 150.f ? 9.f * std::log2 (150.f / f) : 0.f);
    }
    p.has = { true, false, false, false };
    return p;
}

// ════════════════════════════════════════════════════════════════════════════
// Real-time engine: STFT (sqrt-Hann, 75% overlap), per-band dynamic gains
// ════════════════════════════════════════════════════════════════════════════
void MatchEngine::prepare (double sampleRate, int, int numChannels)
{
    sr = sampleRate;
    nCh = juce::jlimit (1, 2, numChannels);
    fftOrder = sampleRate > 64000.0 ? 13 : 12;
    fftSize = 1 << fftOrder;
    hop = fftSize / 4;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = std::sqrt (0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) fftSize));
    map.build (sampleRate, fftSize);
    inFifo.assign (2, std::vector<float> ((size_t) fftSize, 0.f));
    outAcc.assign (2, std::vector<float> ((size_t) fftSize, 0.f));
    frame.assign (2, std::vector<float> ((size_t) fftSize * 2, 0.f));
    power.assign ((size_t) fftSize / 2 + 1, 0.f);
    power2.assign ((size_t) fftSize / 2 + 1, 0.f);
    gains.assign ((size_t) fftSize / 2 + 1, 1.f);
    reset();
}

void MatchEngine::reset()
{
    for (auto& v : inFifo) std::fill (v.begin(), v.end(), 0.f);
    for (auto& v : outAcc) std::fill (v.begin(), v.end(), 0.f);
    fifoPos = 0;
    for (auto& l : lanes) l = Lane {};
    slowMs = runMax = 0.f;
    heardSec = 0.0;
    curSection = secAll;
    frozenCaptured = false;
}

void MatchEngine::process (float* const* ch, int numChannels, int numSamples)
{
    const bool ms = params.stereo != 0 && numChannels > 1;
    for (int i = 0; i < numSamples; ++i)
    {
        float a = ch[0][i], b = numChannels > 1 ? ch[1][i] : 0.f;
        if (ms) { const float m = 0.5f * (a + b), s = 0.5f * (a - b); a = m; b = s; }
        const int w = fftSize - hop + fifoPos;
        inFifo[0][(size_t) w] = a;
        inFifo[1][(size_t) w] = b;
        float oa = outAcc[0][(size_t) fifoPos], ob = outAcc[1][(size_t) fifoPos];
        if (ms) { const float l = oa + ob, r = oa - ob; oa = l; ob = r; }
        ch[0][i] = oa;
        if (numChannels > 1) ch[1][i] = ob;
        if (++fifoPos == hop)
        {
            processFrame();
            fifoPos = 0;
        }
    }
}

int MatchEngine::liveSection()
{
    if (! params.sections || profile == nullptr) return secAll;
    if (heardSec < 4.0 || runMax <= 0.f) return secAll;
    const float rel = toDb (slowMs) - toDb (runMax);
    const int cls = rel > -2.5f ? secHook : rel > -7.f ? secVerse : secBreak;
    return profile->has[(size_t) cls] ? cls : secAll;
}

void MatchEngine::analyseLane (Lane& lane, const BandArray& p, int side, bool transient)
{
    const auto tune = tuning (params.mode);
    const float dt = (float) hop / (float) sr;
    const float a = std::exp (-dt / std::max (0.01f, params.speedMs * tune.speedMul / 1000.f));

    if (! lane.primed) { lane.smoothP = p; lane.primed = true; }
    else for (int b = 0; b < kBands; ++b) lane.smoothP[(size_t) b] = a * lane.smoothP[(size_t) b] + (1.f - a) * p[(size_t) b];

    float totalSmooth = 0.f;
    for (float v : lane.smoothP) totalSmooth += v;

    BandArray desired {};
    BandArray inDb {};
    for (int b = 0; b < kBands; ++b) inDb[(size_t) b] = toDb (lane.smoothP[(size_t) b]);

    if (profile != nullptr && totalSmooth > 1.0e-9f)
    {
        // target shape: the live section's curve, eased in over ~1.5 s
        const auto& src = (side ? profile->side : profile->mono)[(size_t) curSection];
        const float ta = std::exp (-dt / 1.5f);
        if (! lane.targetPrimed) { lane.target = src; lane.targetPrimed = true; }
        else for (int b = 0; b < kBands; ++b) lane.target[(size_t) b] = ta * lane.target[(size_t) b] + (1.f - ta) * src[(size_t) b];

        BandArray w {};
        float sw = 0.f, mi = 0.f, mt = 0.f;
        for (int b = 0; b < kBands; ++b)
        {
            w[(size_t) b] = rangeWeight (bandCenter (b), params.fromHz, params.toHz);
            sw += w[(size_t) b];
            mi += w[(size_t) b] * inDb[(size_t) b];
            mt += w[(size_t) b] * lane.target[(size_t) b];
        }
        if (sw > 0.f)
        {
            mi /= sw; mt /= sw;
            for (int b = 0; b < kBands; ++b)
            {
                const float diff = (lane.target[(size_t) b] - mt) - (inDb[(size_t) b] - mi);
                float g = diff < 0.f ? diff * params.cut : diff * params.boost;
                g = juce::jlimit (-tune.maxDb, tune.maxDb, g) * w[(size_t) b];
                desired[(size_t) b] = g;
            }
            smoothAcross (desired, tune.freqSmooth);
        }
    }

    // Punch Guard: let hits through untouched
    if (transient && params.punchGuard)
        for (auto& g : desired) g *= 0.25f;

    const float att = std::exp (-dt / 0.025f), rel = std::exp (-dt / tune.release);
    for (int b = 0; b < kBands; ++b)
    {
        float& g = lane.gainDb[(size_t) b];
        const float d = desired[(size_t) b];
        const float c = std::abs (d) > std::abs (g) ? att : rel;
        g = c * g + (1.f - c) * d;
    }
    const float la = std::exp (-dt / 4.f);
    for (int b = 0; b < kBands; ++b)
        lane.longGainDb[(size_t) b] = la * lane.longGainDb[(size_t) b] + (1.f - la) * lane.gainDb[(size_t) b];
}

void MatchEngine::processFrame()
{
    const int bins = fftSize / 2 + 1;
    const bool stereoIn = nCh > 1;
    const int mode = stereoIn ? params.stereo : 0;

    for (int c = 0; c < 2; ++c)
    {
        auto& f = frame[(size_t) c];
        std::fill (f.begin(), f.end(), 0.f);
        if (c == 1 && ! stereoIn) continue;
        for (int i = 0; i < fftSize; ++i) f[(size_t) i] = inFifo[(size_t) c][(size_t) i] * window[(size_t) i];
        fft->performRealOnlyForwardTransform (f.data(), true);
    }
    auto binPow = [&] (int c, std::vector<float>& out)
    {
        const auto& f = frame[(size_t) c];
        for (int k = 0; k < bins; ++k) out[(size_t) k] = f[(size_t) 2 * k] * f[(size_t) 2 * k] + f[(size_t) 2 * k + 1] * f[(size_t) 2 * k + 1];
    };

    // section tracking on the overall level
    const float dt = (float) hop / (float) sr;
    {
        binPow (0, power);
        float tot = 0.f;
        for (int k = 0; k < bins; ++k) tot += power[(size_t) k];
        if (stereoIn && mode == 0) { binPow (1, power2); for (int k = 0; k < bins; ++k) tot += power2[(size_t) k]; tot *= 0.5f; }
        const float sa = std::exp (-dt / 3.f);
        slowMs = sa * slowMs + (1.f - sa) * tot;
        runMax = std::max (runMax * std::exp (-dt / 60.f), slowMs);
        if (tot > 1.0e-8f) heardSec += dt;
        curSection = liveSection();
    }

    // which lanes listen to what
    struct Job { int lane; int srcA; int srcB; bool side; };   // srcB -1 = none
    Job jobs[2]; int nJobs = 0;
    if (! stereoIn)          jobs[nJobs++] = { 0, 0, -1, false };
    else if (mode == 0)      jobs[nJobs++] = { 0, 0, 1, false };
    else if (mode == 1)      jobs[nJobs++] = { 0, 0, -1, false };
    else if (mode == 2)      jobs[nJobs++] = { 1, 1, -1, true };
    else                   { jobs[nJobs++] = { 0, 0, -1, false }; jobs[nJobs++] = { 1, 1, -1, true }; }

    std::array<bool, 2> touched { false, false };
    for (int j = 0; j < nJobs; ++j)
    {
        auto& job = jobs[j];
        binPow (job.srcA, power);
        if (job.srcB >= 0) { binPow (job.srcB, power2); for (int k = 0; k < bins; ++k) power[(size_t) k] = 0.5f * (power[(size_t) k] + power2[(size_t) k]); }
        BandArray bp {};
        map.bandPower (power.data(), bp);
        float inst = 0.f;
        for (float v : bp) inst += v;
        auto& lane = lanes[job.lane];
        const bool transient = lane.shortTotal > 1.0e-10f && inst > lane.shortTotal * 4.f;
        const float sa = std::exp (-dt / 0.08f);
        lane.shortTotal = sa * lane.shortTotal + (1.f - sa) * inst;
        analyseLane (lane, bp, job.side ? 1 : 0, transient);

        // Freeze to EQ: hold the average of what it has been doing
        BandArray applied = lane.gainDb;
        if (params.freeze)
        {
            if (! frozenCaptured) { lanes[0].frozenDb = lanes[0].longGainDb; lanes[1].frozenDb = lanes[1].longGainDb; frozenCaptured = true; }
            applied = lane.frozenDb;
        }
        map.binGains (applied, gains.data());

        const int targets[2] = { job.srcA, job.srcB };
        for (int t : targets)
        {
            if (t < 0) continue;
            auto& f = frame[(size_t) t];
            for (int k = 0; k < bins; ++k) { f[(size_t) 2 * k] *= gains[(size_t) k]; f[(size_t) 2 * k + 1] *= gains[(size_t) k]; }
            touched[(size_t) t] = true;
        }

        if (job.lane == jobs[0].lane)
        {
            // meters (first lane)
            const juce::SpinLock::ScopedLockType sl (meterLock);
            float mi = 0.f, mt = 0.f, sw = 0.f, before = 0.f, after = 0.f;
            BandArray w {};
            for (int b = 0; b < kBands; ++b)
            {
                w[(size_t) b] = rangeWeight (bandCenter (b), params.fromHz, params.toHz);
                meters.inDb[(size_t) b] = toDb (lane.smoothP[(size_t) b]);
                sw += w[(size_t) b]; mi += w[(size_t) b] * meters.inDb[(size_t) b]; mt += w[(size_t) b] * lane.target[(size_t) b];
            }
            if (sw > 0.f) { mi /= sw; mt /= sw; }
            for (int b = 0; b < kBands; ++b)
            {
                meters.targetDb[(size_t) b] = profile != nullptr ? lane.target[(size_t) b] - mt + mi : -200.f;
                meters.gainDb[(size_t) b] = applied[(size_t) b];
                const float d = (lane.target[(size_t) b] - mt) - (meters.inDb[(size_t) b] - mi);
                before += w[(size_t) b] * d * d;
                after += w[(size_t) b] * (d - applied[(size_t) b]) * (d - applied[(size_t) b]);
            }
            // 100% = output tone on the target; each dB of remaining difference costs 8%
            meters.match = (profile != nullptr && sw > 0.f) ? juce::jlimit (0.f, 100.f, 100.f - 8.f * std::sqrt (after / sw)) : 0.f;
            meters.section = curSection;
            meters.active = profile != nullptr;
        }
    }
    if (! params.freeze) frozenCaptured = false;

    // back to time domain, overlap-add
    for (int c = 0; c < 2; ++c)
    {
        auto& acc = outAcc[(size_t) c];
        std::copy (acc.begin() + hop, acc.end(), acc.begin());
        std::fill (acc.end() - hop, acc.end(), 0.f);
        if (c == 1 && ! stereoIn) continue;
        auto& f = frame[(size_t) c];
        fft->performRealOnlyInverseTransform (f.data());
        // sqrt-Hann in and out at 75% overlap sums to 2: scale by 0.5
        for (int i = 0; i < fftSize; ++i) acc[(size_t) i] += f[(size_t) i] * window[(size_t) i] * 0.5f;
        juce::ignoreUnused (touched);
    }
    for (int c = 0; c < 2; ++c)
        std::copy (inFifo[(size_t) c].begin() + hop, inFifo[(size_t) c].end(), inFifo[(size_t) c].begin());
}

void MatchEngine::getMeters (EngineMeters& out) const
{
    const juce::SpinLock::ScopedLockType sl (meterLock);
    out = meters;
}
}
