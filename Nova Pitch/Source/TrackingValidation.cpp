// Tracking mode checks.
//  1. Normal mode is bit-identical to a golden render made before Tracking mode existed
//     (NOVA_PITCH_GOLDEN=<file>: records it if missing, compares if present).
//  2. Tracking mode: how late the output is, pitch still corrected, clean output.

#include "PluginProcessor.h"
#include "ParameterIDs.hpp"
#include "NovaPitchTunedEngine.h"
#include <juce_core/juce_core.h>
#include <cstdlib>
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
    constexpr int kBlock = 256;

    std::vector<float> makeVoice (float midi, int n, double startSec = 0.0)
    {
        std::vector<float> x ((size_t) n, 0.f);
        double ph = 0.0;
        for (int i = (int) (startSec * kSr); i < n; ++i)
        {
            const double t = i / kSr;
            const double f = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0) * (1.0 + 0.004 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t));
            ph += 2.0 * juce::MathConstants<double>::pi * f / kSr;
            double s = 0.0;
            for (int k = 1; k <= 12; ++k)
            {
                const double hz = f * k;
                s += (std::exp (-std::pow ((hz - 700.0) / 500.0, 2.0)) + 0.15) / k * std::sin (ph * k);
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

    std::vector<float> run (NovaPitchAudioProcessor& p, const std::vector<float>& in)
    {
        std::vector<float> out (in.size());
        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        for (size_t s = 0; s < in.size(); s += kBlock)
        {
            const int n = (int) juce::jmin ((size_t) kBlock, in.size() - s);
            buf.setSize (2, n, false, false, true);
            for (int i = 0; i < n; ++i) { buf.setSample (0, i, in[s + (size_t) i]); buf.setSample (1, i, in[s + (size_t) i]); }
            p.processBlock (buf, midi);
            for (int i = 0; i < n; ++i) out[s + (size_t) i] = buf.getSample (0, i);
        }
        return out;
    }

    float spectralMidi (const std::vector<float>& x, int from, int to, float guess)
    {
        const int len = to - from;
        double bestP = -1.0; float best = 0.f;
        for (int c = -100; c <= 100; ++c)
        {
            const float m = guess + c * 0.01f;
            const double w = 2.0 * juce::MathConstants<double>::pi * 440.0 * std::pow (2.0, (m - 69.0) / 12.0) / kSr;
            double re = 0, im = 0;
            for (int i = 0; i < len; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (len - 1));
                re += x[(size_t) (from + i)] * win * std::cos (w * i);
                im += x[(size_t) (from + i)] * win * std::sin (w * i);
            }
            if (re * re + im * im > bestP) { bestP = re * re + im * im; best = m; }
        }
        return best;
    }

    int firstAbove (const std::vector<float>& x, float thr)
    {
        for (size_t i = 0; i < x.size(); ++i) if (std::abs (x[i]) > thr) return (int) i;
        return -1;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const int n = (int) (3.0 * kSr);

    // Golden: the normal-mode output before Tracking mode existed
    std::cout << "\n[Tracking] normal mode unchanged\n";
    {
        const auto voice = makeVoice (51.7f, n);
        std::vector<float> all;
        for (int pass = 0; pass < 3; ++pass)
        {
            NovaPitchAudioProcessor p;
            setReal (p, ParameterIDs::scale, 1.f);
            setReal (p, ParameterIDs::tolerance, pass == 1 ? 40.f : 0.f);
            if (pass == 2) setReal (p, ParameterIDs::harmOn, 1.f);   // harmony path too
            p.prepareToPlay (kSr, kBlock);
            const auto out = run (p, voice);
            all.insert (all.end(), out.begin(), out.end());
        }
        const char* path = std::getenv ("NOVA_PITCH_GOLDEN");
        if (path != nullptr)
        {
            juce::File f (path);
            if (! f.existsAsFile())
            {
                f.replaceWithData (all.data(), all.size() * sizeof (float));
                std::cout << "  INFO  recorded golden (" << all.size() << " samples)\n";
            }
            else
            {
                juce::MemoryBlock mb;
                f.loadFileAsData (mb);
                const bool same = mb.getSize() == all.size() * sizeof (float)
                               && std::memcmp (mb.getData(), all.data(), mb.getSize()) == 0;
                check (same, "normal mode output is bit-identical to before Tracking mode");
            }
        }
        if (std::getenv ("NOVA_PITCH_GOLDEN_ONLY") != nullptr) return failures;
    }

    std::cout << "\n[Tracking] Tracking mode\n";
    {
        // A note that starts at 0.5 s: when does it come out?
        const auto voice = makeVoice (57.6f, n, 0.5);   // A3, 40 cents sharp
        auto latencyOf = [&] (bool tracking, float& correctedMidi, std::vector<float>& outKeep)
        {
            NovaPitchAudioProcessor p;
            setReal (p, ParameterIDs::scale, 1.f);          // C major: A is in key
            setReal (p, ParameterIDs::tolerance, 0.f);
            setReal (p, ParameterIDs::tracking, tracking ? 1.f : 0.f);
            p.prepareToPlay (kSr, kBlock);
            const auto out = run (p, voice);
            outKeep = out;
            correctedMidi = spectralMidi (out, (int) (1.5 * kSr), n, 57.f);
            const int inAt = firstAbove (voice, 0.02f), outAt = firstAbove (out, 0.02f);
            std::cout << "  INFO  " << (tracking ? "tracking" : "normal  ") << ": output " << juce::String ((outAt - inAt) / kSr * 1000.0, 1)
                      << " ms late, reported latency " << juce::String (p.getLatencySamples() / kSr * 1000.0, 1) << " ms\n";
            return (outAt - inAt) / kSr * 1000.0;
        };
        float mNormal = 0, mTrack = 0;
        std::vector<float> oN, oT;
        const double latN = latencyOf (false, mNormal, oN);
        const double latT = latencyOf (true, mTrack, oT);
        check (latN > 15.0, "normal mode keeps its full-accuracy delay");
        check (latT < 9.0, "tracking mode output is under 9 ms late (" + juce::String (latT, 1) + " ms)");
        check (std::abs (mTrack - 57.f) < 0.05f, "tracking mode still corrects to A3 (" + juce::String (mTrack, 2) + ")");
        check (std::abs (mNormal - 57.f) < 0.05f, "normal mode corrects to A3 (" + juce::String (mNormal, 2) + ")");

        // Clean: no clicks (no sample-to-sample jump far beyond the signal's own)
        float maxStepIn = 0.f, maxStepOut = 0.f;
        for (int i = (int) (0.6 * kSr); i < n; ++i)
        {
            maxStepIn = juce::jmax (maxStepIn, std::abs (voice[(size_t) i] - voice[(size_t) i - 1]));
            maxStepOut = juce::jmax (maxStepOut, std::abs (oT[(size_t) i] - oT[(size_t) i - 1]));
        }
        check (maxStepOut < maxStepIn * 2.5f, "tracking output has no clicks (" + juce::String (maxStepOut / juce::jmax (1e-6f, maxStepIn), 2) + "x the input's steepest step)");

        // A low voice (E2) too
        const auto low = makeVoice (40.3f, n, 0.5);
        NovaPitchAudioProcessor p;
        setReal (p, ParameterIDs::scale, 1.f);
        setReal (p, ParameterIDs::tolerance, 0.f);
        setReal (p, ParameterIDs::tracking, 1.f);
        p.prepareToPlay (kSr, kBlock);
        const auto out = run (p, low);
        const float m = spectralMidi (out, (int) (1.5 * kSr), n, 40.f);
        const double lat = (firstAbove (out, 0.02f) - firstAbove (low, 0.02f)) / kSr * 1000.0;
        check (std::abs (m - 40.f) < 0.05f, "low voice E2 corrected in tracking mode (" + juce::String (m, 2) + ")");
        check (lat < 14.0, "low voice under 14 ms late (" + juce::String (lat, 1) + " ms)");
        bool finite = true;
        for (auto v : out) if (! std::isfinite (v)) finite = false;
        check (finite, "all samples finite");

        // Switching modes on the fly is safe (the processor reconfigures on its timer)
        NovaPitchAudioProcessor q;
        q.prepareToPlay (kSr, kBlock);
        setReal (q, ParameterIDs::tracking, 1.f);
        const int before = q.getLatencySamples();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        check (q.getLatencySamples() < before, "latency reported to the host drops when Tracking is switched on ("
               + juce::String (before) + " -> " + juce::String (q.getLatencySamples()) + " samples)");
    }

    std::cout << "\n" << (failures == 0 ? "ALL PASS" : juce::String (failures) + " FAILED") << "\n";
    return failures == 0 ? 0 : 1;
}
