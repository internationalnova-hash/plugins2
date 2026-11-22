// Headless validation for Nova AI Vocal's DSP.
//
//   NovaAIVocalValidation                    -> synthetic checks only
//   NovaAIVocalValidation <in.wav> <outDir>  -> checks, then analyze the file
//                                               like Listen and render every vibe

#include <cstdlib>
#include "dsp/NovaFactoryPresets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>

#include "dsp/NovaVocalAnalysis.h"

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }

    float rmsDb (const juce::AudioBuffer<float>& b, int start = 0)
    {
        double s = 0; long n = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = start; i < b.getNumSamples(); ++i) { s += (double) b.getSample (ch, i) * b.getSample (ch, i); ++n; }
        return (float) (10.0 * std::log10 (juce::jmax (s / juce::jmax (1L, n), 1e-20)));
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i))) return false;
        return true;
    }

    // Runs the chain over a buffer in host-sized blocks.
    void run (NovaVocalChain& chain, juce::AudioBuffer<float>& b, int block = 512)
    {
        for (int s = 0; s < b.getNumSamples(); s += block)
        {
            const int n = juce::jmin (block, b.getNumSamples() - s);
            chain.process (b.getWritePointer (0) + s, b.getWritePointer (1) + s, n);
        }
    }

    void syntheticChecks()
    {
        constexpr double sr = 48000.0;
        const int len = (int) sr * 3;

        std::printf ("Synthetic checks\n");

        // 1. All macros at zero, no plan, mix 100 %: should be ~transparent at 1 kHz.
        {
            juce::AudioBuffer<float> b (2, len);
            for (int i = 0; i < len; ++i)
                b.setSample (0, i, 0.1f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / sr)),
                b.setSample (1, i, b.getSample (0, i));
            const float inDb = rmsDb (b, len / 2);

            NovaVocalChain chain;
            chain.setMacros ({});
            chain.prepare (sr, 512);
            run (chain, b);
            check (std::abs (rmsDb (b, len / 2) - inDb) < 0.5f,
                   "Zero settings are transparent at 1 kHz (" + juce::String (rmsDb (b, len / 2) - inDb, 2) + " dB)");
        }

        // 2. Mix at 0 % passes the dry signal exactly, even with everything on.
        {
            juce::Random rng (1);
            juce::AudioBuffer<float> b (2, len), ref;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < len; ++i) b.setSample (ch, i, (rng.nextFloat() * 2.f - 1.f) * 0.2f);
            ref.makeCopyOf (b);

            NovaVocalChain chain;
            NovaVocalMacros all; all.v.fill (1.f);
            chain.setMacros (all);
            chain.setMix (0.f);
            chain.prepare (sr, 512);
            run (chain, b);
            float maxDiff = 0;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < len; ++i) maxDiff = juce::jmax (maxDiff, std::abs (b.getSample (ch, i) - ref.getSample (ch, i)));
            check (maxDiff < 1e-5f, "Mix 0 % returns the dry signal");
        }

        // 3. Every vibe, every macro at 100 %, extreme plan, loud noise then
        //    silence: output stays finite and the echo/reverb tails die away
        //    (no runaway feedback).
        for (int v = 0; v < kNumVibes; ++v)
        {
            const int total = (int) sr * 8, on = (int) sr * 3;
            juce::Random rng (v + 7);
            juce::AudioBuffer<float> b (2, total);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < total; ++i)
                    b.setSample (ch, i, i < on ? (rng.nextFloat() * 2.f - 1.f) * 0.9f : 0.f);

            NovaVocalChain chain;
            NovaVocalMacros all; all.v.fill (1.f);
            chain.setVibe (v);
            chain.setMacros (all);
            NovaVocalPlan plan; plan.active = true; plan.inputTrimDb = 18.f; plan.airDb = 6.f; plan.mudDb = -6.f;
            chain.setPlan (plan);
            chain.prepare (sr, 512);
            run (chain, b, 333); // odd block size on purpose

            juce::AudioBuffer<float> active (b.getArrayOfWritePointers(), 2, 0, on);
            juce::AudioBuffer<float> tail (b.getArrayOfWritePointers(), 2, total - (int) (sr * 0.25), (int) (sr * 0.25));
            const float drop = rmsDb (active) - rmsDb (tail);
            check (allFinite (b) && drop > 45.f,
                   juce::String (getNovaVibe (v).name) + " at 100 %: finite, tails decay " + juce::String (drop, 1) + " dB");
        }

        // 4. De-esser reduces a sibilant burst more than a vowel.
        {
            NovaVocalChain chain;
            NovaVocalMacros m; m.v[kDeEss] = 1.f;
            chain.setMacros (m);
            chain.prepare (sr, 512);
            juce::AudioBuffer<float> b (2, len);
            for (int i = 0; i < len; ++i)
            {
                const double t = i / sr;
                const float s = (float) (0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 7500.0 * t));
                b.setSample (0, i, s); b.setSample (1, i, s);
            }
            const float inDb = rmsDb (b, len / 2);
            run (chain, b);
            const float red = inDb - rmsDb (b, len / 2);
            check (red > 3.f, "De-ess at 100 % turns down a 7.5 kHz 'S' (" + juce::String (red, 1) + " dB)");
        }
    }

    // "G major" / "A minor" -> pitch class + minor flag; -1 if not parsed
    int parseKey (const juce::String& name, bool& minor)
    {
        static const juce::StringArray sharps { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const juce::StringArray flats  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        const auto root = name.upToFirstOccurrenceOf (" ", false, false).trim();
        minor = name.containsIgnoreCase ("min");
        int pc = sharps.indexOf (root, true);
        if (pc < 0) pc = flats.indexOf (root, true);
        return pc;
    }

    // What the tracker hears: time per note name and the singer's average
    // offset from equal temperament.
    void printPitchDiagnostics (const juce::AudioBuffer<float>& src, double sr)
    {
        std::vector<float> mono ((size_t) src.getNumSamples());
        for (int i = 0; i < src.getNumSamples(); ++i) mono[(size_t) i] = 0.5f * (src.getSample (0, i) + src.getSample (1, i));
        NovaPitchTracker t; t.prepare (sr);
        double hist[12] = {}; double sx = 0, sy = 0; int n = 0;
        for (auto v : mono)
            if (t.push (v) && t.getHz() > 0.f)
            {
                const float m = 69.f + 12.f * std::log2 (t.getHz() / 440.f);
                const int r = (int) std::round (m);
                hist[(r % 12 + 12) % 12] += 1;
                const double a = 2.0 * juce::MathConstants<double>::pi * (m - r);
                sx += std::cos (a); sy += std::sin (a); ++n;
            }
        static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        std::printf ("Notes heard (%% of sung time):");
        for (int i = 0; i < 12; ++i) if (n > 0 && hist[i] / n >= 0.02) std::printf (" %s %.0f%%", names[i], 100.0 * hist[i] / n);
        std::printf ("\nSinger's average offset from in-tune: %+.0f cents\n", n > 0 ? std::atan2 (sy, sx) / (2.0 * juce::MathConstants<double>::pi) * 100.0 : 0.0);
    }

    void renderFile (const juce::File& in, const juce::File& outDir, const juce::String& keyOverride = {})
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (in));
        if (reader == nullptr) { std::printf ("Could not read %s\n", in.getFullPathName().toRawUTF8()); ++failures; return; }

        const double sr = reader->sampleRate;
        const int len = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> src (2, len);
        reader->read (&src, 0, len, 0, true, true);
        if (reader->numChannels == 1) src.copyFrom (1, 0, src, 0, 0, len);

        std::printf ("\nFile: %s (%.1f s, %.0f Hz, %u ch)\n", in.getFileName().toRawUTF8(), len / sr, sr, reader->numChannels);

        auto p = analyzeNovaVocal (src.getArrayOfReadPointers(), 2, len, sr);
        printPitchDiagnostics (src, sr);
        std::printf ("Listen's key guess: %s\n", p.keyValid ? novaKeyName (p.keyPc, p.keyMinor).toRawUTF8() : "unsure (chromatic)");
        if (keyOverride.isNotEmpty())
        {
            bool minor = false;
            const int pc = parseKey (keyOverride, minor);
            if (pc >= 0) { p.keyValid = true; p.keyPc = pc; p.keyMinor = minor; }
            std::printf ("Key forced to: %s\n", novaKeyName (p.keyPc, p.keyMinor).toRawUTF8());
        }
        std::printf ("Profile: valid=%d singing=%.1fs rms=%.1f peak=%.1f spread=%.1f sib=%.1f\n",
                     (int) p.valid, p.activeSeconds, p.activeRmsDb, p.peakDb, p.dynamicSpreadDb, p.sibilanceDb);
        std::printf ("Bands vs 1-2k: rumble=%.1f low=%.1f mud=%.1f box=%.1f pres=%.1f sib=%.1f air=%.1f\n",
                     p.rumbleDb, p.lowDb, p.mudDb, p.boxDb, p.presenceDb, p.sibBandDb, p.airDb);
        check (p.valid, "Listen hears enough singing in the file");

        outDir.createDirectory();
        juce::WavAudioFormat wav;

        // One render: tuner (recipe's tuning style) -> chain (vibe or full preset recipe)
        auto renderOne = [&] (const juce::String& label, int v, const NovaVibe* voicing,
                              const NovaVocalMacros& macros, const NovaVocalPlan& plan)
        {
            NovaVocalChain chain;
            chain.setVibe (v);
            chain.setVoicing (voicing);
            chain.setMacros (macros);
            chain.setPlan (plan);
            chain.setTempo (120.0);
            chain.prepare (sr, 512);

            const NovaVibe& V = voicing != nullptr ? *voicing : getNovaVibe (v);
            NovaVocalTuner tuner;
            tuner.prepare (sr, 512);
            auto tp = NovaVocalTuner::fromTuneKnob (macros.v[kTune], p.keyValid ? p.keyPc : 0,
                                                    p.keyValid ? NovaVocalTuner::scaleMaskFor (p.keyMinor) : 0xFFF);
            tp.retuneMs *= V.retuneScale; tp.humanize = V.tuneHumanize; tp.vibratoPreserve = V.tuneVibrato;
            tuner.setParams (tp);
            const int lat = tuner.getLatencySamples();

            juce::AudioBuffer<float> b (2, len + lat);
            b.clear();
            for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, src, ch, 0, len);
            for (int s = 0; s < len + lat; s += 512)
            {
                const int n = juce::jmin (512, len + lat - s);
                tuner.process (b.getWritePointer (0) + s, b.getWritePointer (1) + s, n);
            }
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::copy (b.getWritePointer (ch), b.getReadPointer (ch) + lat, len);
            b.setSize (2, len, true);
            double grSum = 0, dsSum = 0; int blocks = 0; float grMax = 0;
            for (int s = 0; s < len; s += 512)
            {
                const int n = juce::jmin (512, len - s);
                chain.process (b.getWritePointer (0) + s, b.getWritePointer (1) + s, n);
                grSum += chain.getCompGainReductionDb(); dsSum += chain.getDeEssGainReductionDb(); ++blocks;
                grMax = juce::jmax (grMax, chain.getCompGainReductionDb());
            }

            const float peakDb = juce::Decibels::gainToDecibels (b.getMagnitude (0, len));
            std::printf ("  out: rms %.1f dB  peak %.1f dBFS  comp GR avg %.1f / max %.1f dB  de-ess avg %.1f dB\n",
                         rmsDb (b), peakDb, grSum / blocks, grMax, dsSum / blocks);
            check (allFinite (b), label + " render is finite");
            check (peakDb < 6.f, label + " stays under +6 dBFS before safety trim");

            if (peakDb > -1.f) b.applyGain (juce::Decibels::decibelsToGain (-1.f - peakDb));

            auto outFile = outDir.getChildFile (in.getFileNameWithoutExtension() + " - " + label
                                                + (keyOverride.isNotEmpty() ? " (" + novaKeyName (p.keyPc, p.keyMinor) + ")" : juce::String()) + ".wav");
            outFile.deleteFile();
            std::unique_ptr<juce::OutputStream> os (outFile.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
            if (w != nullptr)
            {
                os.release();
                w->writeFromAudioSampleBuffer (b, 0, len);
            }
        };

        // NOVA_PRESETS="Puerto Rico,Toronto 6AM" (or "all") renders factory
        // presets instead of the six vibes.
        const juce::String presetList (std::getenv ("NOVA_PRESETS") != nullptr ? std::getenv ("NOVA_PRESETS") : "");
        if (presetList.isNotEmpty())
        {
            juce::StringArray want; want.addTokens (presetList, ",", "\"");
            want.trim(); want.removeEmptyStrings();
            for (const auto& fp : getNovaFactoryPresets())
            {
                if (! presetList.equalsIgnoreCase ("all") && ! want.contains (fp.name)) continue;
                const auto d = decideNovaVocalMix (p, fp.vibe);
                NovaVocalMacros m;
                for (int i = 0; i < kNumMacros; ++i) m.v[(size_t) i] = fp.voicing.macros[(size_t) i];
                std::printf ("\nPreset: %s (%s, base %s)\n", fp.name, fp.group, getNovaVibe (fp.vibe).name);
                renderOne (juce::String ("Preset - ") + fp.name, fp.vibe, &fp.voicing, m, d.plan);
            }
            return;
        }

        for (int v = 0; v < kNumVibes; ++v)
        {
            const auto d = decideNovaVocalMix (p, v);
            std::printf ("\n%s\n  plan: trim %+.1f  mud %+.1f  box %+.1f  pres %+.1f  air %+.1f\n"
                         "  knobs: clean %.2f  de-ess %.2f  punch %.2f  air %.2f  warmth %.2f  space %.2f  tune %.2f\n  notes: %s\n",
                         getNovaVibe (v).name, d.plan.inputTrimDb, d.plan.mudDb, d.plan.boxDb, d.plan.presenceDb, d.plan.airDb,
                         d.macros.v[0], d.macros.v[1], d.macros.v[2], d.macros.v[3], d.macros.v[4], d.macros.v[5], d.macros.v[6],
                         d.summary.toRawUTF8());
            renderOne (getNovaVibe (v).name, v, nullptr, d.macros, d.plan);
        }
    }
}

int main (int argc, char* argv[])
{
    syntheticChecks();

    if (argc >= 3)
        renderFile (juce::File (juce::String::fromUTF8 (argv[1])), juce::File (juce::String::fromUTF8 (argv[2])),
                    argc >= 4 ? juce::String::fromUTF8 (argv[3]) : juce::String());

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
