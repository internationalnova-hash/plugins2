// Headless checks for the Harmony feature, driving the real processor.
//  1. Harmony off: output is bit-identical to the tuning engine alone.
//  2. Harmony switched on then off: once faded, bit-identical again.
//  3. Scale mode: each part lands on the right in-key note (measured pitch).
//  4. MIDI mode: a part plays the held note.
//  5. 32 voices, 4 parts: finite, sane level, CPU time.

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include "NovaPitchTunedEngine.h"
#include "NovaPitchHarmony.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <cmath>

namespace
{
    int failures = 0;
    void check (bool cond, const juce::String& what)
    {
        std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! cond) ++failures;
    }

    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    // Voice-like test tone: harmonics with a vowel-ish envelope, slightly flat, light vibrato
    std::vector<float> makeVoice (float midi, int numSamples)
    {
        std::vector<float> x ((size_t) numSamples);
        double ph = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = i / kSr;
            const double f = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0) * (1.0 + 0.004 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t));
            ph += 2.0 * juce::MathConstants<double>::pi * f / kSr;
            double s = 0.0;
            for (int k = 1; k <= 12; ++k)
            {
                const double hz = f * k;
                const double env = std::exp (-std::pow ((hz - 700.0) / 500.0, 2.0)) + 0.6 * std::exp (-std::pow ((hz - 1200.0) / 400.0, 2.0)) + 0.15;
                s += env / k * std::sin (ph * k);
            }
            x[(size_t) i] = (float) (0.25 * s);
        }
        return x;
    }

    void setReal (NovaPitchAudioProcessor& p, const char* id, float real)
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
        param->setValueNotifyingHost (param->convertTo0to1 (real));
    }

    // Run the processor over a mono signal (fed to both channels); returns L and R
    void runProcessor (NovaPitchAudioProcessor& p, const std::vector<float>& in,
                       std::vector<float>& outL, std::vector<float>& outR,
                       const std::function<void (int)>& perBlock = {},
                       juce::MidiBuffer* midiAtStart = nullptr)
    {
        const int n = (int) in.size();
        outL.assign ((size_t) n, 0.f);
        outR.assign ((size_t) n, 0.f);
        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        for (int start = 0, b = 0; start < n; start += kBlock, ++b)
        {
            const int len = juce::jmin (kBlock, n - start);
            buf.setSize (2, len, false, false, true);
            for (int i = 0; i < len; ++i) { buf.setSample (0, i, in[(size_t) (start + i)]); buf.setSample (1, i, in[(size_t) (start + i)]); }
            if (perBlock) perBlock (b);
            midi.clear();
            if (b == 0 && midiAtStart != nullptr) midi = *midiAtStart;
            p.processBlock (buf, midi);
            for (int i = 0; i < len; ++i) { outL[(size_t) (start + i)] = buf.getSample (0, i); outR[(size_t) (start + i)] = buf.getSample (1, i); }
        }
    }

    // Engine alone with the processor's default params (the reference)
    void runEngineOnly (const std::vector<float>& in, std::vector<float>& outL, std::vector<float>& outR, NovaPitchAudioProcessor& defaultsFrom)
    {
        NovaPitchTunedEngine e;
        e.prepare (kSr, kBlock, false);
        NovaPitchTunedEngine::Params p;
        auto v = [&] (const char* id) { return defaultsFrom.apvts.getRawParameterValue (id)->load(); };
        p.keyPitchClass = (int) v (ParameterIDs::key);
        p.scale = (nova::pitch::Scale) (int) v (ParameterIDs::scale);
        p.retuneSpeedPercent = v (ParameterIDs::tolerance);
        p.amountPercent = v (ParameterIDs::amount);
        p.confidenceThresholdPercent = v (ParameterIDs::confidenceThreshold);
        p.vibratoPreservePercent = v (ParameterIDs::vibrato);
        p.vibratoRateHz = v (ParameterIDs::vibratoRate);
        p.formantAmountPercent = v (ParameterIDs::formant);
        p.formantWarpPercent = v (ParameterIDs::formantWarp);
        p.rangePercent = v (ParameterIDs::range);
        p.mixPercent = v (ParameterIDs::mix);
        p.outputGainDb = v (ParameterIDs::outputGain);
        p.transientsPercent = v (ParameterIDs::transients);
        p.bypassed = false;
        const int n = (int) in.size();
        outL.assign ((size_t) n, 0.f);
        outR.assign ((size_t) n, 0.f);
        for (int start = 0; start < n; start += kBlock)
        {
            const int len = juce::jmin (kBlock, n - start);
            e.setParams (p);
            e.process (in.data() + start, in.data() + start, outL.data() + start, outR.data() + start, len);
        }
    }

    // Median pitch (MIDI) of a signal over [from, to)
    float medianMidi (const std::vector<float>& x, int from, int to)
    {
        NovaPitchTracker t;
        t.prepare (kSr);
        std::vector<float> est;
        for (int i = from; i < to; ++i)
            if (t.push (x[(size_t) i]) && t.getHz() > 0.f)
                est.push_back (69.f + 12.f * std::log2 (t.getHz() / 440.f));
        if (est.empty()) return 0.f;
        std::sort (est.begin(), est.end());
        return est[est.size() / 2];
    }

    // Fundamental by a fine frequency scan (1-cent steps, Hann window) around a guess
    float spectralMidi (const std::vector<float>& x, int from, int to, float guessMidi)
    {
        const int len = to - from;
        double bestP = -1.0; float best = 0.f;
        for (int c = -150; c <= 150; ++c)
        {
            const float m = guessMidi + c * 0.01f;
            const double w = 2.0 * juce::MathConstants<double>::pi * 440.0 * std::pow (2.0, (m - 69.0) / 12.0) / kSr;
            double re = 0, im = 0;
            for (int i = 0; i < len; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (len - 1));
                const double v = x[(size_t) (from + i)] * win;
                re += v * std::cos (w * i); im += v * std::sin (w * i);
            }
            const double pw = re * re + im * im;
            if (pw > bestP) { bestP = pw; best = m; }
        }
        return best;
    }

    float rms (const std::vector<float>& x, int from, int to)
    {
        double s = 0.0;
        for (int i = from; i < to; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
        return (float) std::sqrt (s / juce::jmax (1, to - from));
    }

    void prepareDefaults (NovaPitchAudioProcessor& p)
    {
        setReal (p, ParameterIDs::key, 0.f);     // C
        setReal (p, ParameterIDs::scale, 1.f);   // Major
        setReal (p, ParameterIDs::tolerance, 0.f);
        p.prepareToPlay (kSr, kBlock);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;
    const int n = (int) (3.0 * kSr);
    const auto voice = makeVoice (51.7f, n); // E3, a bit flat

    std::cout << "\n[Harmony] interval steps\n";
    check (NovaPitchHarmony::intervalShift (52, 2, 0, 0xAB5) == 3,  "C major, E: 3rd above = G (+3)");
    check (NovaPitchHarmony::intervalShift (52, 4, 0, 0xAB5) == 7,  "C major, E: 5th above = B (+7)");
    check (NovaPitchHarmony::intervalShift (60, 2, 0, 0xAB5) == 4,  "C major, C: 3rd above = E (+4)");
    check (NovaPitchHarmony::intervalShift (60, 7, 0, 0xAB5) == -3, "C major, C: 3rd below = A (-3)");
    check (NovaPitchHarmony::intervalShift (57, 2, 9, 0x5AD) == 3,  "A minor, A: 3rd above = C (+3)");
    check (NovaPitchHarmony::intervalShift (52, 11, 0, 0xAB5) == -12, "Octave below = -12");
    check (NovaPitchHarmony::intervalShift (52, 0, 0, 0xAB5) == 0,  "Unison = 0");

    std::cout << "\n[Diag] raw tuner fixed shift on a steady tone (no correction)\n";
    for (float semis : { 3.f, 7.f, -12.f })
    {
        const auto tone = makeVoice (52.f, n);
        NovaVocalTuner t; t.prepare (kSr, kBlock, false);
        NovaVocalTuner::Params tp; tp.enabled = false; tp.fixedShiftSemis = semis; t.setParams (tp);
        std::vector<float> a (tone), b (tone);
        for (int s0 = 0; s0 < n; s0 += kBlock) t.process (a.data() + s0, b.data() + s0, juce::jmin (kBlock, n - s0));
        std::cout << "  INFO  shift " << semis << " -> " << spectralMidi (a, n - (int) kSr, n, 52.f + semis) - 52.f << "\n";
    }

    std::cout << "\n[Harmony] off = identical to the tuning engine\n";
    {
        NovaPitchAudioProcessor p;
        prepareDefaults (p);
        std::vector<float> pl, pr, el, er;
        runProcessor (p, voice, pl, pr);
        runEngineOnly (voice, el, er, p);
        bool same = true;
        for (int i = 0; i < n; ++i) if (pl[(size_t) i] != el[(size_t) i] || pr[(size_t) i] != er[(size_t) i]) { same = false; break; }
        check (same, "bit-identical output with Harmony off");
    }

    std::cout << "\n[Harmony] on then off = identical again once faded\n";
    {
        NovaPitchAudioProcessor p;
        prepareDefaults (p);
        std::vector<float> pl, pr, el, er;
        const int offBlock = (int) (1.0 * kSr) / kBlock;
        runProcessor (p, voice, pl, pr, [&] (int b)
        {
            if (b == 0)        setReal (p, ParameterIDs::harmOn, 1.f);
            if (b == offBlock) setReal (p, ParameterIDs::harmOn, 0.f);
        });
        runEngineOnly (voice, el, er, p);
        const int from = (int) (1.5 * kSr);
        bool same = true;
        for (int i = from; i < n; ++i) if (pl[(size_t) i] != el[(size_t) i] || pr[(size_t) i] != er[(size_t) i]) { same = false; break; }
        check (same, "bit-identical after Harmony is switched off");
        check (rms (pl, (int) (0.4 * kSr), offBlock * kBlock) > rms (el, (int) (0.4 * kSr), offBlock * kBlock) * 1.02f,
               "harmony was audible while on");
    }

    std::cout << "\n[Harmony] Scale mode notes (C major, singing E3)\n";
    {
        const struct { int interval; float expect; const char* name; } cases[] = {
            { 2, 55.f, "3rd above -> G3" }, { 4, 59.f, "5th above -> B3" },
            { 11, 40.f, "octave below -> E2" }, { 7, 48.f, "3rd below -> C3" } };
        for (const auto& c : cases)
        {
            NovaPitchAudioProcessor p;
            prepareDefaults (p);
            setReal (p, ParameterIDs::harmOn, 1.f);
            setReal (p, ParameterIDs::harmVoices, 1.f);
            setReal (p, ParameterIDs::harmMix, 100.f);
            setReal (p, ParameterIDs::partOn[0], 1.f);
            setReal (p, ParameterIDs::partOn[1], 0.f);
            setReal (p, ParameterIDs::partInterval[0], (float) c.interval);
            setReal (p, ParameterIDs::partLevel[0], 0.f);
            std::vector<float> pl, pr, el, er;
            runProcessor (p, voice, pl, pr);
            runEngineOnly (voice, el, er, p);
            std::vector<float> harm ((size_t) n);
            for (int i = 0; i < n; ++i) harm[(size_t) i] = pl[(size_t) i] - el[(size_t) i];
            const float m = spectralMidi (harm, n - (int) kSr, n, c.expect);
            const float ms = spectralMidi (harm, n - (int) kSr, n, c.expect);
            std::cout << "  INFO  lead " << spectralMidi (el, n - (int) kSr, n, 52.f) << "  part (spectral) " << ms << "\n";
            check (std::abs (m - c.expect) < 0.06f, juce::String (c.name) + " (measured " + juce::String (m, 2) + ")");
            check (std::abs (p.getHarmony().getPartMidi (0) - c.expect) < 0.01f, juce::String ("  keyboard shows ") + c.name);
        }
    }

    std::cout << "\n[Harmony] phrases with gaps and a note change (3rd above, C major)\n";
    {
        const int seg = (int) (0.8 * kSr), gap = (int) (0.4 * kSr);
        auto a = makeVoice (52.f, seg), b = makeVoice (57.f, seg);   // E3, then A3
        std::vector<float> sig;
        sig.insert (sig.end(), a.begin(), a.end());
        sig.insert (sig.end(), (size_t) gap, 0.f);
        sig.insert (sig.end(), b.begin(), b.end());
        sig.insert (sig.end(), (size_t) gap, 0.f);
        NovaPitchAudioProcessor p;
        prepareDefaults (p);
        setReal (p, ParameterIDs::harmOn, 1.f);
        setReal (p, ParameterIDs::harmVoices, 8.f);
        setReal (p, ParameterIDs::partOn[1], 0.f);
        setReal (p, ParameterIDs::partInterval[0], 2.f);
        std::vector<float> pl, pr, el, er;
        runProcessor (p, sig, pl, pr);
        runEngineOnly (sig, el, er, p);
        const int sN = (int) sig.size();
        std::vector<float> h ((size_t) sN);
        for (int i = 0; i < sN; ++i) h[(size_t) i] = pl[(size_t) i] - el[(size_t) i];
        const int lat = p.getLatencySamples();
        // Isolate part 1's core voice: rerun with 1 voice to measure the pitch of each phrase
        NovaPitchAudioProcessor p1;
        prepareDefaults (p1);
        setReal (p1, ParameterIDs::harmOn, 1.f); setReal (p1, ParameterIDs::harmVoices, 1.f);
        setReal (p1, ParameterIDs::partOn[1], 0.f); setReal (p1, ParameterIDs::partInterval[0], 2.f);
        setReal (p1, ParameterIDs::partLevel[0], 0.f); setReal (p1, ParameterIDs::harmMix, 100.f);
        std::vector<float> ql, qr; runProcessor (p1, sig, ql, qr);
        std::vector<float> h1 ((size_t) sN);
        for (int i = 0; i < sN; ++i) h1[(size_t) i] = ql[(size_t) i] - el[(size_t) i];
        const int s2 = seg + gap;
        const float m1 = spectralMidi (h1, lat + (int) (0.3 * kSr), lat + seg - 2400, 55.f);
        const float m2 = spectralMidi (h1, s2 + lat + (int) (0.3 * kSr), s2 + lat + seg - 2400, 60.f);
        check (std::abs (m1 - 55.f) < 0.06f, "phrase 1: E3 -> harmony G3 (" + juce::String (m1, 2) + ")");
        check (std::abs (m2 - 60.f) < 0.06f, "phrase 2: A3 -> harmony C4 (" + juce::String (m2, 2) + ")");
        // No burst at phrase starts: the first 150 ms of each phrase stays near the steady level
        auto peakIn = [&] (int a0, int a1) { float m = 0.f; for (int i = a0; i < a1; ++i) m = juce::jmax (m, std::abs (h[(size_t) i])); return m; };
        const float steady = peakIn (lat + (int) (0.3 * kSr), lat + (int) (0.7 * kSr));
        const float on2 = peakIn (s2 + lat - 2400, s2 + lat + 7200);
        const float tail = peakIn (seg + lat + 4800, s2 + lat - 4800);
        std::cout << "  INFO  around phrase 2 (10 ms):";
        for (int w0 = s2 + lat - 4800; w0 < s2 + lat + 9600; w0 += 480) std::cout << " " << juce::String (peakIn (w0, w0 + 480), 2);
        std::cout << "\n";
        std::cout << "  INFO  steady " << steady << ", phrase-2 onset " << on2 << ", in the gap " << tail << "\n";
        check (on2 < steady * 1.6f, "no burst at the second phrase start");
        check (tail < steady * 0.1f, "harmony is quiet in the gap");
    }

    std::cout << "\n[Harmony] MIDI mode\n";
    {
        NovaPitchAudioProcessor p;
        prepareDefaults (p);
        setReal (p, ParameterIDs::harmOn, 1.f);
        setReal (p, ParameterIDs::harmMode, 1.f);
        setReal (p, ParameterIDs::harmVoices, 1.f);
        setReal (p, ParameterIDs::harmMix, 100.f);
        setReal (p, ParameterIDs::partOn[1], 0.f);
        setReal (p, ParameterIDs::partLevel[0], 0.f);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0); // A3
        std::vector<float> pl, pr, el, er;
        runProcessor (p, voice, pl, pr, {}, &m);
        runEngineOnly (voice, el, er, p);
        std::vector<float> harm ((size_t) n);
        for (int i = 0; i < n; ++i) harm[(size_t) i] = pl[(size_t) i] - el[(size_t) i];
        const float mm = spectralMidi (harm, n - (int) kSr, n, 57.f);
        check (std::abs (mm - 57.f) < 0.06f, "held A3 -> part plays A3 (measured " + juce::String (mm, 2) + ")");
    }

    std::cout << "\n[Harmony] 32 voices, 4 parts\n";
    {
        NovaPitchAudioProcessor p;
        prepareDefaults (p);
        setReal (p, ParameterIDs::harmOn, 1.f);
        setReal (p, ParameterIDs::harmVoices, 32.f);
        setReal (p, ParameterIDs::harmWidth, 200.f);
        setReal (p, ParameterIDs::harmHumanize, 100.f);
        for (int i = 0; i < 4; ++i) setReal (p, ParameterIDs::partOn[i], 1.f);
        std::vector<float> pl, pr;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        runProcessor (p, voice, pl, pr);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        bool finite = true; float peak = 0.f;
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (pl[(size_t) i]) || ! std::isfinite (pr[(size_t) i])) finite = false;
            peak = juce::jmax (peak, std::abs (pl[(size_t) i]), std::abs (pr[(size_t) i]));
        }
        check (finite, "all samples finite");
        float inPeak = 0.f; for (auto x : voice) inPeak = juce::jmax (inPeak, std::abs (x));
        std::cout << "  INFO  input peak " << inPeak << "\n";
        // Loudness, not peak: a perfectly steady synthetic tone lines the voices
        // up far more than a real singer does, so peaks overstate it.
        const float rOut = rms (pl, (int) (0.5 * kSr), n), rIn = rms (voice, (int) (0.5 * kSr), n);
        std::cout << "  INFO  peak " << peak << ", loudness vs lead alone: +" << juce::String (20.f * std::log10 (rOut / rIn), 1) << " dB\n";
        check (rOut < 2.0f * rIn, "full 32-voice stack adds under 6 dB of loudness");
        std::vector<float> el, er; runEngineOnly (voice, el, er, p);
        std::vector<float> h ((size_t) n); int peakAt = 0;
        for (int i = 0; i < n; ++i) { h[(size_t) i] = pl[(size_t) i] - el[(size_t) i]; if (std::abs (pl[(size_t) i]) > std::abs (pl[(size_t) peakAt])) peakAt = i; }
        float steadyPeak = 0.f; for (int i = (int) (0.5 * kSr); i < n; ++i) steadyPeak = juce::jmax (steadyPeak, std::abs (pl[(size_t) i]), std::abs (el[(size_t) i]));
        float leadPeak = 0.f; for (int i = (int) (0.5 * kSr); i < n; ++i) leadPeak = juce::jmax (leadPeak, std::abs (el[(size_t) i]));
        { float hp = 0.f; int hi = 0; for (int i = 0; i < (int) (0.5 * kSr); ++i) if (std::abs (h[(size_t) i]) > hp) { hp = std::abs (h[(size_t) i]); hi = i; }
          float lp = 0.f; for (int i = 0; i < (int) (0.5 * kSr); ++i) lp = juce::jmax (lp, std::abs (el[(size_t) i]));
          std::cout << "  INFO  harmony peak per 10 ms:";
          for (int w0 = 0; w0 < (int) (0.3 * kSr); w0 += 480) { float m = 0.f; for (int i = w0; i < w0 + 480; ++i) m = juce::jmax (m, std::abs (h[(size_t) i])); std::cout << " " << juce::String (m, 2); }
          std::cout << "\n";
          std::cout << "  INFO  first 0.5 s: lead peak " << lp << ", harmony peak " << hp << " at " << juce::String (hi / kSr, 4) << " s\n"; }
        std::cout << "  INFO  steady peak " << steadyPeak << " (lead alone " << leadPeak << ")\n";
        std::cout << "  INFO  harmony alone vs lead: " << juce::String (20.f * std::log10 (rms (h, (int) (0.5 * kSr), n) / rms (el, (int) (0.5 * kSr), n)), 1)
                  << " dB, peak at " << juce::String (peakAt / kSr, 3) << " s\n";
        check (p.getHarmony().getActiveVoiceCount() == 32, "32 voices active");
        const double pct = ms / (n / kSr * 1000.0) * 100.0;
        std::cout << "  INFO  CPU: " << juce::String (pct, 1) << "% of one core for 3 s of audio (whole plugin)\n";
        check (pct < 25.0, "CPU under 25% of one core");
    }

    std::cout << "\n" << (failures == 0 ? "ALL PASS" : juce::String (failures) + " FAILED") << "\n";
    return failures == 0 ? 0 : 1;
}
