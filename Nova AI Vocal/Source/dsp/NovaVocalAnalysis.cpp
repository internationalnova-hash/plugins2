#include "NovaVocalAnalysis.h"

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    // Neutral reference balance for a clean, well-recorded vocal, in the
    // same units as NovaVocalProfile (band PSD relative to 1-2 kHz).
    // Listen's corrective EQ moves the voice part of the way toward this;
    // the vibe's own flavour is then added by the macro knobs.
    constexpr float kRefMud      =  5.f;
    constexpr float kRefBox      =  2.f;
    constexpr float kRefPresence = -4.f;
    constexpr float kRefAir      = -20.f;

    constexpr float kTargetRmsDb = -20.f;

    float percentile (std::vector<float> v, float p)
    {
        if (v.empty()) return 0.f;
        const size_t idx = (size_t) juce::jlimit (0.0, (double) v.size() - 1.0, std::round (p * (double) (v.size() - 1)));
        std::nth_element (v.begin(), v.begin() + (long) idx, v.end());
        return v[idx];
    }

    float db10 (double power) { return (float) (10.0 * std::log10 (juce::jmax (power, 1.0e-20))); }
}

NovaVocalProfile analyzeNovaVocal (const float* const* channels, int numChannels, int numSamples, double sampleRate)
{
    NovaVocalProfile p;
    if (numChannels <= 0 || numSamples <= 0 || sampleRate <= 0) return p;

    constexpr int order = 11, N = 1 << order, hop = N / 2;
    if (numSamples < N) return p;

    // Mono sum
    std::vector<float> mono ((size_t) numSamples, 0.f);
    float peak = 0.f;
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
        {
            mono[(size_t) i] += channels[ch][i] / (float) numChannels;
            peak = juce::jmax (peak, std::abs (channels[ch][i]));
        }
    p.peakDb = 20.f * std::log10 (juce::jmax (peak, 1.0e-6f));

    const int numFrames = 1 + (numSamples - N) / hop;

    // Pass 1: frame loudness, to find where the singing is
    std::vector<float> frameDb ((size_t) numFrames);
    float maxFrameDb = -200.f;
    for (int f = 0; f < numFrames; ++f)
    {
        double sum = 0;
        const float* x = mono.data() + (size_t) f * hop;
        for (int i = 0; i < N; ++i) sum += (double) x[i] * x[i];
        frameDb[(size_t) f] = db10 (sum / N);
        maxFrameDb = juce::jmax (maxFrameDb, frameDb[(size_t) f]);
    }
    const float activeThresh = juce::jmax (maxFrameDb - 35.f, -60.f);

    // Pass 2: spectrum of active frames
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> window ((size_t) N, juce::dsp::WindowingFunction<float>::hann, false);
    std::vector<float> buf ((size_t) N * 2);
    std::vector<double> psd ((size_t) N / 2 + 1, 0.0);

    const double binHz = sampleRate / N;
    auto bin = [&] (double hz) { return juce::jlimit (1, N / 2, (int) std::round (hz / binHz)); };
    const int sibLo = bin (5000.0), sibHi = bin (10000.0), fullLo = bin (100.0), fullHi = bin (16000.0);

    std::vector<float> activeDb, sibRatios;
    double activePower = 0;

    for (int f = 0; f < numFrames; ++f)
    {
        if (frameDb[(size_t) f] < activeThresh) continue;

        std::fill (buf.begin(), buf.end(), 0.f);
        std::copy_n (mono.data() + (size_t) f * hop, N, buf.data());
        window.multiplyWithWindowingTable (buf.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);

        double sib = 0, full = 0;
        for (int k = 0; k <= N / 2; ++k)
        {
            const double pw = (double) buf[(size_t) k] * buf[(size_t) k];
            psd[(size_t) k] += pw;
            if (k >= sibLo && k <= sibHi)  sib += pw;
            if (k >= fullLo && k <= fullHi) full += pw;
        }

        sibRatios.push_back (db10 (sib) - db10 (full));
        activeDb.push_back (frameDb[(size_t) f]);
        activePower += std::pow (10.0, frameDb[(size_t) f] / 10.0);
    }

    if (activeDb.empty()) return p;

    p.activeSeconds   = (float) ((double) activeDb.size() * hop / sampleRate);
    p.activeRmsDb     = db10 (activePower / (double) activeDb.size());
    p.dynamicSpreadDb = percentile (activeDb, 0.95f) - percentile (activeDb, 0.10f);
    p.sibilanceDb     = percentile (sibRatios, 0.90f);

    auto band = [&] (double lo, double hi)
    {
        const int a = bin (lo), b = juce::jmax (bin (lo), bin (juce::jmin (hi, sampleRate * 0.49)));
        double s = 0;
        for (int k = a; k <= b; ++k) s += psd[(size_t) k];
        return db10 (s / (b - a + 1));
    };

    const float ref = band (1000.0, 2000.0);
    p.rumbleDb   = band (20.0, 80.0) - ref;
    p.lowDb      = band (100.0, 200.0) - ref;
    p.mudDb      = band (200.0, 450.0) - ref;
    p.boxDb      = band (500.0, 900.0) - ref;
    p.presenceDb = band (2000.0, 5000.0) - ref;
    p.sibBandDb  = band (5000.0, 9000.0) - ref;
    p.airDb      = band (10000.0, 16000.0) - ref;

    const auto key = estimateNovaKey (mono.data(), numSamples, sampleRate);
    p.keyValid = key.valid;
    p.keyPc    = key.keyPc;
    p.keyMinor = key.minor;

    p.valid = p.activeSeconds >= 1.5f;
    return p;
}

juce::String novaKeyName (int keyPc, bool minor)
{
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return juce::String (names[(keyPc % 12 + 12) % 12]) + (minor ? " minor" : " major");
}

NovaVocalMacros novaVibeDefaultMacros (int vibeIndex)
{
    NovaVocalMacros m;
    m.v = getNovaVibe (vibeIndex).macros;
    return m;
}

NovaVocalDecision decideNovaVocalMix (const NovaVocalProfile& p, int vibeIndex)
{
    NovaVocalDecision d;
    d.macros = novaVibeDefaultMacros (vibeIndex);

    if (! p.valid)
    {
        d.summary = "Not enough singing captured - try a longer take.";
        return d;
    }

    juce::StringArray notes;
    auto& m = d.macros.v;

    // Gain staging
    d.plan.active = true;
    d.plan.inputTrimDb = juce::jlimit (-18.f, 18.f, kTargetRmsDb - p.activeRmsDb);
    if (std::abs (d.plan.inputTrimDb) >= 3.f)
        notes.add (juce::String (d.plan.inputTrimDb > 0 ? "Quiet take" : "Hot take")
                   + " (" + juce::String (d.plan.inputTrimDb > 0 ? "+" : "") + juce::String (d.plan.inputTrimDb, 1) + " dB)");

    // Corrective EQ toward the neutral reference (partial moves, cut-biased)
    d.plan.mudDb      = juce::jlimit (-6.f, 2.f, (kRefMud      - p.mudDb)      * 0.6f);
    d.plan.boxDb      = juce::jlimit (-5.f, 2.f, (kRefBox      - p.boxDb)      * 0.5f);
    d.plan.presenceDb = juce::jlimit (-3.f, 4.f, (kRefPresence - p.presenceDb) * 0.5f);
    d.plan.airDb      = juce::jlimit (-2.f, 5.f, (kRefAir      - p.airDb)      * 0.35f);

    if (d.plan.mudDb <= -1.5f)      notes.add ("Muddy low-mids");
    if (d.plan.boxDb <= -1.5f)      notes.add ("Boxy");
    if (d.plan.presenceDb >= 1.5f)  notes.add ("Needs presence");
    if (d.plan.presenceDb <= -1.5f) notes.add ("Harsh upper-mids");
    if (d.plan.airDb >= 2.f)        notes.add ("Dark top end");

    // Knob nudges from what the voice actually does
    const float rumbleNudge = juce::jlimit (0.f, 0.2f, (p.rumbleDb + 25.f) * 0.01f);
    m[kClean] += rumbleNudge;
    if (rumbleNudge > 0.08f) notes.add ("Low rumble");

    const float sibNudge = juce::jlimit (-0.2f, 0.35f, (p.sibilanceDb + 12.f) * 0.04f);
    m[kDeEss] += sibNudge;
    if (sibNudge > 0.1f) notes.add ("Sharp S's");

    const float dynNudge = juce::jlimit (-0.15f, 0.2f, (p.dynamicSpreadDb - 18.f) * 0.015f);
    m[kPunch] += dynNudge;
    if (dynNudge > 0.08f)  notes.add ("Very dynamic");
    if (dynNudge < -0.08f) notes.add ("Already even");

    for (auto& v : m) v = juce::jlimit (0.f, 1.f, v);

    if (p.keyValid) notes.insert (0, "Key: " + novaKeyName (p.keyPc, p.keyMinor));
    d.summary = notes.isEmpty() ? juce::String ("Clean, balanced take") : notes.joinIntoString (" | ");
    return d;
}

// ── Persistence ──────────────────────────────────────────────────────────

std::unique_ptr<juce::XmlElement> NovaVocalProfile::toXml() const
{
    auto x = std::make_unique<juce::XmlElement> ("VocalProfile");
    x->setAttribute ("valid", valid);
    x->setAttribute ("activeSeconds", activeSeconds);
    x->setAttribute ("activeRmsDb", activeRmsDb);
    x->setAttribute ("peakDb", peakDb);
    x->setAttribute ("dynamicSpreadDb", dynamicSpreadDb);
    x->setAttribute ("sibilanceDb", sibilanceDb);
    x->setAttribute ("rumbleDb", rumbleDb);
    x->setAttribute ("lowDb", lowDb);
    x->setAttribute ("mudDb", mudDb);
    x->setAttribute ("boxDb", boxDb);
    x->setAttribute ("presenceDb", presenceDb);
    x->setAttribute ("sibBandDb", sibBandDb);
    x->setAttribute ("airDb", airDb);
    x->setAttribute ("keyValid", keyValid);
    x->setAttribute ("keyPc", keyPc);
    x->setAttribute ("keyMinor", keyMinor);
    return x;
}

NovaVocalProfile NovaVocalProfile::fromXml (const juce::XmlElement& x)
{
    NovaVocalProfile p;
    auto f = [&x] (const char* name, float def) { return (float) x.getDoubleAttribute (name, def); };
    p.valid           = x.getBoolAttribute ("valid", false);
    p.activeSeconds   = f ("activeSeconds", p.activeSeconds);
    p.activeRmsDb     = f ("activeRmsDb", p.activeRmsDb);
    p.peakDb          = f ("peakDb", p.peakDb);
    p.dynamicSpreadDb = f ("dynamicSpreadDb", p.dynamicSpreadDb);
    p.sibilanceDb     = f ("sibilanceDb", p.sibilanceDb);
    p.rumbleDb        = f ("rumbleDb", p.rumbleDb);
    p.lowDb           = f ("lowDb", p.lowDb);
    p.mudDb           = f ("mudDb", p.mudDb);
    p.boxDb           = f ("boxDb", p.boxDb);
    p.presenceDb      = f ("presenceDb", p.presenceDb);
    p.sibBandDb       = f ("sibBandDb", p.sibBandDb);
    p.airDb           = f ("airDb", p.airDb);
    p.keyValid        = x.getBoolAttribute ("keyValid", false);
    p.keyPc           = x.getIntAttribute ("keyPc", 0);
    p.keyMinor        = x.getBoolAttribute ("keyMinor", false);
    return p;
}

std::unique_ptr<juce::XmlElement> novaVocalPlanToXml (const NovaVocalPlan& plan)
{
    auto x = std::make_unique<juce::XmlElement> ("VocalPlan");
    x->setAttribute ("active", plan.active);
    x->setAttribute ("inputTrimDb", plan.inputTrimDb);
    x->setAttribute ("mudDb", plan.mudDb);
    x->setAttribute ("boxDb", plan.boxDb);
    x->setAttribute ("presenceDb", plan.presenceDb);
    x->setAttribute ("airDb", plan.airDb);
    return x;
}

NovaVocalPlan novaVocalPlanFromXml (const juce::XmlElement& x)
{
    NovaVocalPlan plan;
    plan.active      = x.getBoolAttribute ("active", false);
    plan.inputTrimDb = (float) x.getDoubleAttribute ("inputTrimDb", 0.0);
    plan.mudDb       = (float) x.getDoubleAttribute ("mudDb", 0.0);
    plan.boxDb       = (float) x.getDoubleAttribute ("boxDb", 0.0);
    plan.presenceDb  = (float) x.getDoubleAttribute ("presenceDb", 0.0);
    plan.airDb       = (float) x.getDoubleAttribute ("airDb", 0.0);
    return plan;
}
