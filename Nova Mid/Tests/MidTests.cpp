// Nova Mid DSP tests: transparency at zero, midrange-focused push, a hard ceiling,
// NaN safety and exact latency reporting.
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "MidEngine.h"

static int failures = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    (ok ? passes : failures)++;
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << "\n";
}

static constexpr double SR = 48000.0;
static constexpr int BLOCK = 512;

struct Run
{
    std::vector<float> l, r;
};

static Run render (nm::MidEngine& e, const std::function<float (int)>& gen, int total)
{
    Run out { std::vector<float> ((size_t) total), std::vector<float> ((size_t) total) };
    for (int i = 0; i < total; ++i) out.l[(size_t) i] = out.r[(size_t) i] = gen (i);
    for (int pos = 0; pos < total; pos += BLOCK)
    {
        const int n = std::min (BLOCK, total - pos);
        float* ch[] { out.l.data() + pos, out.r.data() + pos };
        e.process (ch, 2, n);
    }
    return out;
}

static nm::MidEngine makeEngine (float push, float ceiling, float out = 0.0f)
{
    nm::MidEngine e;
    e.setPushDb (push);
    e.setCeilingDb (ceiling);
    e.setOutputDb (out);
    e.prepare (SR, BLOCK, 2);
    return e;
}

static float rmsDb (const std::vector<float>& v, int from)
{
    double s = 0;
    for (size_t i = (size_t) from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return (float) (10.0 * std::log10 (s / (double) (v.size() - (size_t) from) + 1e-30));
}

static float sineGainDb (float push, double freq, float levelDb = -36.0f)
{
    auto e = makeEngine (push, 0.0f);
    const float a = juce::Decibels::decibelsToGain (levelDb);
    const int total = (int) SR;
    auto out = render (e, [&] (int i) { return a * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / SR); }, total);
    return rmsDb (out.l, total / 2) - (levelDb - 3.0103f);
}

static void testTransparent()
{
    std::cout << "Transparent at zero\n";
    for (double f : { 100.0, 1000.0, 8000.0 })
        check (std::abs (sineGainDb (0.0f, f)) < 0.05f, "push 0 leaves a " + juce::String (f, 0) + " Hz sine unchanged");
}

static void testPushShape()
{
    std::cout << "Push is a wide midrange bell\n";
    const float mid = sineGainDb (12.0f, nm::kBellFreqHz);
    check (std::abs (mid - 12.0f) < 0.3f, "push 12 dB adds 12 dB at the centre (" + juce::String (mid, 2) + ")");
    const float lo = sineGainDb (12.0f, 60.0), hi = sineGainDb (12.0f, 12000.0);
    check (lo < 3.0f, "sub stays out of it: 60 Hz +" + juce::String (lo, 2) + " dB");
    check (hi < 3.0f, "air stays out of it: 12 kHz +" + juce::String (hi, 2) + " dB");
    const float wide = sineGainDb (12.0f, 400.0);
    check (wide > 6.0f, "wide enough to reach the low mids: 400 Hz +" + juce::String (wide, 2) + " dB");
    check (std::abs (sineGainDb (20.0f, nm::kBellFreqHz) - 20.0f) < 0.3f, "full push is 20 dB");
}

static void testCeiling()
{
    std::cout << "Ceiling\n";
    for (float ceil : { 0.0f, -6.0f, -24.0f })
    {
        auto e = makeEngine (20.0f, ceil);
        juce::Random rng (7);
        auto out = render (e, [&] (int i) { return (float) (1.5 * std::sin (juce::MathConstants<double>::twoPi * 850.0 * i / SR)) + rng.nextFloat() * 0.5f - 0.25f; }, (int) SR);
        float pk = 0.0f;
        for (size_t i = 0; i < out.l.size(); ++i) pk = std::max ({ pk, std::abs (out.l[i]), std::abs (out.r[i]) });
        check (juce::Decibels::gainToDecibels (pk) <= ceil + 0.001f,
               "hot signal never passes a " + juce::String (ceil, 0) + " dBFS ceiling (peak " + juce::String (juce::Decibels::gainToDecibels (pk), 2) + ")");
        check (e.getLastOverDb() > 6.0f, "soft-clip meter reports the overdrive (" + juce::String (e.getLastOverDb(), 1) + " dB)");
    }

    auto e = makeEngine (0.0f, -6.0f);
    render (e, [] (int i) { return 0.1f * (float) std::sin (0.05 * i); }, 4096);
    check (e.getLastOverDb() == 0.0f, "quiet signal under the ceiling reads 0.0 over");

    // Louder without peaking higher: with push, RMS rises while peak is held.
    auto dry = makeEngine (0.0f, -3.0f), wet = makeEngine (10.0f, -3.0f);
    auto gen = [] (int i) { return 0.6f * (float) std::sin (juce::MathConstants<double>::twoPi * 700.0 * i / SR); };
    const auto a = render (dry, gen, (int) SR), b = render (wet, gen, (int) SR);
    check (rmsDb (b.l, (int) SR / 2) > rmsDb (a.l, (int) SR / 2) + 1.5f, "push makes it denser at the same ceiling");
}

static void testOutputTrim()
{
    std::cout << "Output trim\n";
    auto e = makeEngine (0.0f, 0.0f, -6.0f);
    const int total = (int) SR;
    auto out = render (e, [] (int i) { return 0.05f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / SR); }, total);
    const float got = rmsDb (out.l, total / 2) - (juce::Decibels::gainToDecibels (0.05f) - 3.0103f);
    check (std::abs (got + 6.0f) < 0.1f, "output -6 dB lowers the level by 6 dB");
}

static void testNaN()
{
    std::cout << "Safety\n";
    auto e = makeEngine (12.0f, -3.0f);
    auto out = render (e, [] (int i) { return i == 100 ? std::numeric_limits<float>::quiet_NaN() : (i == 200 ? std::numeric_limits<float>::infinity() : 0.3f); }, 8192);
    bool finite = true;
    for (auto v : out.l) finite = finite && std::isfinite (v);
    check (finite, "NaN/inf input never reaches the output");
    float tail = 0.0f;
    for (size_t i = 6000; i < out.l.size(); ++i) tail = std::max (tail, std::abs (out.l[i]));
    check (tail > 0.1f, "audio recovers after a bad sample");
}

static void testLatency()
{
    std::cout << "Latency\n";
    auto e = makeEngine (0.0f, 0.0f);
    const int lat = e.getLatencySamples();
    auto out = render (e, [] (int i) { return i == 1000 ? 0.25f : 0.0f; }, 4096);
    int peakAt = 0;
    for (int i = 0; i < 4096; ++i) if (std::abs (out.l[(size_t) i]) > std::abs (out.l[(size_t) peakAt])) peakAt = i;
    check (lat > 0 && std::abs ((peakAt - 1000) - lat) <= 1,
           "reported latency " + juce::String (lat) + " matches the measured delay " + juce::String (peakAt - 1000));
}

int main()
{
    testTransparent();
    testPushShape();
    testCeiling();
    testOutputTrim();
    testNaN();
    testLatency();
    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
