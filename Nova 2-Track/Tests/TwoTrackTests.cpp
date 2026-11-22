// Nova 2-Track DSP tests: each control does what its name says, and nothing else.
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "TwoTrackEngine.h"

static int failures = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    (ok ? passes : failures)++;
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << "\n";
}

static constexpr double SR = 48000.0;
static constexpr int BLOCK = 512;
static const double TWO_PI = juce::MathConstants<double>::twoPi;

struct Stereo { std::vector<float> l, r; };

static Stereo run (const tt::Settings& s, const std::function<std::pair<float, float> (int)>& gen, int total, int* latencyOut = nullptr)
{
    tt::TwoTrackEngine e;
    e.set (s);
    e.prepare (SR, BLOCK);
    if (latencyOut) *latencyOut = e.getLatencySamples();
    Stereo o { std::vector<float> ((size_t) total), std::vector<float> ((size_t) total) };
    for (int i = 0; i < total; ++i) { auto [a, b] = gen (i); o.l[(size_t) i] = a; o.r[(size_t) i] = b; }
    for (int pos = 0; pos < total; pos += BLOCK)
    {
        const int n = std::min (BLOCK, total - pos);
        e.process (o.l.data() + pos, o.r.data() + pos, n);
    }
    return o;
}

static double rms (const std::vector<float>& v, int from)
{
    double s = 0; for (size_t i = (size_t) from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (v.size() - (size_t) from)) + 1e-12;
}
static double db (double g) { return 20.0 * std::log10 (g); }
static Stereo midSide (const Stereo& x)
{
    Stereo ms { x.l, x.r };
    for (size_t i = 0; i < x.l.size(); ++i) { ms.l[i] = 0.5f * (x.l[i] + x.r[i]); ms.r[i] = 0.5f * (x.l[i] - x.r[i]); }
    return ms;
}

static double toneGain (const tt::Settings& s, double f, bool side);

static void testNeutral()
{
    std::cout << "Neutral at defaults\n";
    tt::Settings s;
    bool flat = true; double worst = 0;
    for (double f : { 40.0, 100.0, 300.0, 1000.0, 3000.0, 8000.0, 15000.0 })
        for (bool side : { false, true })
        {
            const double g = toneGain (s, f, side);
            worst = std::max (worst, std::abs (g));
            flat = flat && std::abs (g) < 0.1;
        }
    check (flat, "default settings are flat from 40 Hz to 15 kHz, centre and sides (worst " + juce::String (worst, 3) + " dB)");

    // The stereo image is untouched: a hard-left signal stays hard left.
    auto o = run (s, [] (int i) { return std::pair { 0.2f * (float) std::sin (TWO_PI * 500 * i / SR), 0.0f }; }, 24000);
    check (rms (o.r, 12000) < 1.0e-3 * rms (o.l, 12000), "a hard-left signal stays hard left");
}

static void testWidth()
{
    std::cout << "Width / Low End\n";
    juce::Random rng (2);
    std::vector<float> nl (48000), nr (48000);
    for (size_t i = 0; i < nl.size(); ++i) { nl[i] = (rng.nextFloat() - 0.5f) * 0.3f; nr[i] = (rng.nextFloat() - 0.5f) * 0.3f; }
    auto gen = [&] (int i) { return std::pair { nl[(size_t) i], nr[(size_t) i] }; };

    tt::Settings base, wide, mono;
    wide.width = 200; mono.width = 0; mono.lowEnd = 100;
    auto ratio = [&] (const tt::Settings& s) { auto ms = midSide (run (s, gen, 48000)); return db (rms (ms.r, 4000) / rms (ms.l, 4000)); };
    const double r0 = ratio (base), r2 = ratio (wide);
    check (r2 > r0 + 4.0, "width 200% widens the image (side/mid " + juce::String (r0, 1) + " -> " + juce::String (r2, 1) + " dB)");
    auto m = run (mono, gen, 48000);
    double diff = 0; for (size_t i = 4000; i < m.l.size(); ++i) diff = std::max (diff, (double) std::abs (m.l[i] - m.r[i]));
    check (diff < 1.0e-4, "width 0% with Low End 100% is fully mono");

    tt::Settings low; low.lowEnd = 100;
    auto sideAt = [&] (const tt::Settings& s, double f)
    {
        auto o = run (s, [f] (int i) { const float v = 0.25f * (float) std::sin (TWO_PI * f * i / SR); return std::pair { v, -v }; }, 48000);
        return db (rms (midSide (o).r, 24000) / (0.25 / std::sqrt (2.0)));
    };
    check (sideAt (low, 50.0) < -12.0, "Low End folds 50 Hz side content to the centre (" + juce::String (sideAt (low, 50.0), 1) + " dB)");
    check (std::abs (sideAt (low, 2000.0)) < 0.5, "Low End leaves 2 kHz side content alone (" + juce::String (sideAt (low, 2000.0), 2) + " dB)");
}

static double toneGain (const tt::Settings& s, double f, bool side)
{
    auto o = run (s, [f, side] (int i) { const float v = 0.1f * (float) std::sin (TWO_PI * f * i / SR); return std::pair { v, side ? -v : v }; }, 48000);
    auto ms = midSide (o);
    return db (rms (side ? ms.r : ms.l, 24000) / (0.1 / std::sqrt (2.0)));
}

static void testEq()
{
    std::cout << "Vox Carve / Instruments\n";
    tt::Settings carve; carve.voxCarve = 100;
    check (std::abs (toneGain (carve, 2000, false) + 6.0) < 0.5, "Vox Carve 100% cuts 6 dB at 2 kHz in the centre (" + juce::String (toneGain (carve, 2000, false), 2) + ")");
    check (std::abs (toneGain (carve, 2000, true)) < 0.3, "Vox Carve leaves the sides alone (" + juce::String (toneGain (carve, 2000, true), 2) + ")");
    check (std::abs (toneGain (carve, 100, false)) < 0.5, "Vox Carve leaves the low end alone");

    tt::Settings inst; inst.instruments = 100;
    check (std::abs (toneGain (inst, 900, true) - 6.0) < 0.5, "Instruments 100% adds 6 dB of side body at 900 Hz (" + juce::String (toneGain (inst, 900, true), 2) + ")");
    check (std::abs (toneGain (inst, 900, false)) < 0.3, "Instruments leaves the centre alone");

    tt::Settings low; low.lowEnd = 100;
    check (toneGain (low, 50, false) > 2.0, "Low End adds centre weight at 50 Hz (" + juce::String (toneGain (low, 50, false), 2) + " dB)");
}

static void testTransients()
{
    std::cout << "Punch / Energy\n";
    auto burst = [] (double f) {
        return [f] (int i) {
            const int k = i % 12000; // a hit every 250 ms
            const float env = (float) std::exp (-k / (0.04 * SR));
            const float v = 0.25f * env * (float) std::sin (TWO_PI * f * k / SR);
            return std::pair { v, v };
        };
    };
    auto peak = [] (const Stereo& o) { float p = 0; for (size_t i = 12000; i < o.l.size(); ++i) p = std::max (p, std::abs (o.l[i])); return (double) p; };
    tt::Settings off, punch, energy; punch.punch = 100; energy.energy = 100;
    const double p0 = peak (run (off, burst (60), 60000)), p1 = peak (run (punch, burst (60), 60000));
    check (db (p1 / p0) > 2.0, "Punch lifts kick attacks (+" + juce::String (db (p1 / p0), 1) + " dB)");
    const double e0 = peak (run (off, burst (3000), 60000)), e1 = peak (run (energy, burst (3000), 60000));
    check (db (e1 / e0) > 2.0, "Energy lifts mid-range attacks (+" + juce::String (db (e1 / e0), 1) + " dB)");

    auto steady = [] (int i) { const float v = 0.2f * (float) std::sin (TWO_PI * 60 * i / SR); return std::pair { v, v }; };
    const double s0 = rms (run (off, steady, 48000).l, 24000), s1 = rms (run (punch, steady, 48000).l, 24000);
    check (std::abs (db (s1 / s0)) < 0.5, "Punch leaves sustained bass alone (" + juce::String (db (s1 / s0), 2) + " dB)");
}

static void testClipper()
{
    std::cout << "Soft clipper / safety\n";
    auto loud = [] (int i) { const float v = 0.9f * (float) std::sin (TWO_PI * 200 * i / SR); return std::pair { v, -0.7f * v }; };
    tt::Settings s; s.clipOn = true; s.ceilingDb = -6; s.inputDb = 12;
    auto o = run (s, loud, 48000);
    float pk = 0; for (size_t i = 0; i < o.l.size(); ++i) pk = std::max ({ pk, std::abs (o.l[i]), std::abs (o.r[i]) });
    check (db (pk) <= -6.0 + 1e-3, "clipper holds a -6 dBFS ceiling with +12 dB input (peak " + juce::String (db (pk), 2) + ")");

    tt::Settings noClip; noClip.inputDb = 12;
    auto o2 = run (noClip, loud, 48000);
    pk = 0; for (auto v : o2.l) pk = std::max (pk, std::abs (v));
    check (pk <= 1.0f, "with the clipper off the output still never passes 0 dBFS");

    tt::Settings any; any.punch = 80; any.energy = 80; any.width = 150;
    auto o3 = run (any, [] (int i) { return std::pair { i == 50 ? std::numeric_limits<float>::quiet_NaN() : 0.2f * (float) std::sin (0.05 * i),
                                                        i == 70 ? std::numeric_limits<float>::infinity() : 0.2f * (float) std::cos (0.05 * i) }; }, 16000);
    bool finite = true; for (size_t i = 0; i < o3.l.size(); ++i) finite = finite && std::isfinite (o3.l[i]) && std::isfinite (o3.r[i]);
    float tail = 0; for (size_t i = 12000; i < o3.l.size(); ++i) tail = std::max (tail, std::abs (o3.l[i]));
    check (finite && tail > 0.1f, "NaN/inf input never reaches the output and audio recovers");
}

static void testLatency()
{
    std::cout << "Latency\n";
    int lat = 0;
    auto o = run (tt::Settings {}, [] (int i) { const float v = i == 1000 ? 0.25f : 0.0f; return std::pair { v, v }; }, 4096, &lat);
    int at = 0; for (int i = 0; i < 4096; ++i) if (std::abs (o.l[(size_t) i]) > std::abs (o.l[(size_t) at])) at = i;
    check (lat > 0 && std::abs ((at - 1000) - lat) <= 1, "reported latency " + juce::String (lat) + " matches measured " + juce::String (at - 1000));
}

int main()
{
    testNeutral();
    testWidth();
    testEq();
    testTransients();
    testClipper();
    testLatency();
    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
