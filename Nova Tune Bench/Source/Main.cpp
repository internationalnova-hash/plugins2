// Nova Tune Bench - offline scoring of pitch-correction plugins.
//
//   NovaTuneBench list  <plugin.vst3|.component> [--sub <name>]
//   NovaTuneBench synth <outDir>
//   NovaTuneBench run   --plugin <path|none> [--sub <name>] [--label <name>]
//                       [--set "Param Name=value"]... [--key C] [--scale chromatic|major|minor]
//                       [--measured-latency] [--shift N | --flatten-midi M] --out <dir> <input.wav | synth>...
//   NovaTuneBench score --in <dry.wav> --out <tuned.wav> [--key C] [--scale ...]
//
// "--plugin none" passes audio straight through: a sanity check for the
// scorer itself (dry off-key input should score badly on accuracy and
// perfectly on clicks/dropouts/latency).

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include <memory>

#include "Analysis.h"
#include "Synth.h"

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlock = 512;

    struct Args
    {
        juce::StringArray positional;
        juce::StringPairArray opts;
        juce::StringArray sets;
        bool measuredLatency = false;
    };

    Args parse (int argc, char* argv[])
    {
        Args a;
        for (int i = 2; i < argc; ++i)
        {
            const juce::String s = juce::String::fromUTF8 (argv[i]);
            if (s == "--measured-latency") { a.measuredLatency = true; continue; }
            if (s.startsWith ("--") && i + 1 < argc)
            {
                const juce::String v = juce::String::fromUTF8 (argv[++i]);
                if (s == "--set") a.sets.add (v);
                else a.opts.set (s.substring (2), v);
                continue;
            }
            a.positional.add (s);
        }
        return a;
    }

    int keyFromName (const juce::String& k)
    {
        static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const juce::StringArray flats { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        const int i = names.indexOf (k.trim(), true);
        return i >= 0 ? i : juce::jmax (0, flats.indexOf (k.trim(), true));
    }

    bool readWav (const juce::File& f, juce::AudioBuffer<float>& out, double& sr)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) return false;
        sr = r->sampleRate;
        out.setSize (2, (int) r->lengthInSamples);
        r->read (&out, 0, (int) r->lengthInSamples, 0, true, true);
        if (r->numChannels == 1) out.copyFrom (1, 0, out, 0, 0, out.getNumSamples());
        return true;
    }

    void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
    {
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        if (os == nullptr) return;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
        if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
    }

    // Some tuners only behave when the host says it is playing.
    struct BenchPlayHead : juce::AudioPlayHead
    {
        juce::int64 samplePos = 0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setIsPlaying (true);
            p.setBpm (120.0);
            p.setTimeInSamples (samplePos);
            p.setTimeInSeconds ((double) samplePos / kSampleRate);
            p.setTimeSignature (TimeSignature { 4, 4 });
            p.setPpqPosition ((double) samplePos / kSampleRate * 2.0);
            return p;
        }
    };

    std::unique_ptr<juce::AudioPluginInstance> loadPlugin (const juce::String& path, const juce::String& sub, double sr)
    {
        juce::AudioPluginFormatManager fm;
        fm.addDefaultFormats();
        juce::OwnedArray<juce::PluginDescription> types;
        for (auto* fmt : fm.getFormats())
            if (fmt->fileMightContainThisPluginType (path))
                fmt->findAllTypesForFile (types, path);

        if (types.isEmpty()) { std::printf ("No plugin found in %s\n", path.toRawUTF8()); return {}; }

        juce::PluginDescription* chosen = types[0];
        if (sub.isNotEmpty())
            for (auto* t : types)
                if (t->name.containsIgnoreCase (sub)) { chosen = t; break; }

        juce::String err;
        auto inst = fm.createPluginInstance (*chosen, sr, kBlock, err);
        if (inst == nullptr) std::printf ("Could not load %s: %s\n", chosen->name.toRawUTF8(), err.toRawUTF8());
        return inst;
    }

    bool applySets (juce::AudioPluginInstance& p, const juce::StringArray& sets)
    {
        bool ok = true;
        for (const auto& s : sets)
        {
            const auto name = s.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = s.fromFirstOccurrenceOf ("=", false, false).trim();
            juce::AudioProcessorParameter* found = nullptr;
            for (auto* prm : p.getParameters())
                if (prm->getName (200).equalsIgnoreCase (name)) { found = prm; break; }
            if (found == nullptr)
                for (auto* prm : p.getParameters())
                    if (prm->getName (200).containsIgnoreCase (name)) { found = prm; break; }
            if (found == nullptr) { std::printf ("  ! no parameter matching '%s'\n", name.toRawUTF8()); ok = false; continue; }

            const float norm = value.endsWithChar ('n') ? value.dropLastCharacters (1).getFloatValue()
                                                        : found->getValueForText (value);
            found->setValue (juce::jlimit (0.f, 1.f, norm));
            std::printf ("  set %-28s -> %s\n", found->getName (200).toRawUTF8(), found->getCurrentValueAsText().toRawUTF8());
        }
        return ok;
    }

    // Renders input through the plugin; output is the same length plus tail.
    juce::AudioBuffer<float> process (juce::AudioPluginInstance* p, const juce::AudioBuffer<float>& in, double sr, int& latency)
    {
        const int tail = (int) (1.0 * sr);
        juce::AudioBuffer<float> out (2, in.getNumSamples() + tail);
        out.clear();
        latency = 0;

        if (p == nullptr) { for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, 0, in, ch, 0, in.getNumSamples()); return out; }

        BenchPlayHead ph;
        p->setPlayHead (&ph);
        p->prepareToPlay (sr, kBlock);
        latency = p->getLatencySamples();

        const int nCh = juce::jmax (2, p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
        juce::AudioBuffer<float> buf (nCh, kBlock);
        juce::MidiBuffer midi;

        // Warm-up with silence so detectors/delay lines start clean
        for (int s = 0; s < (int) (0.5 * sr); s += kBlock)
        {
            buf.clear();
            p->processBlock (buf, midi);
            ph.samplePos += kBlock;
        }

        const int total = out.getNumSamples();
        for (int s = 0; s < total; s += kBlock)
        {
            const int n = juce::jmin (kBlock, total - s);
            buf.setSize (nCh, n, false, false, true);
            buf.clear();
            for (int ch = 0; ch < juce::jmin (2, p->getTotalNumInputChannels()); ++ch)
                if (s < in.getNumSamples())
                    buf.copyFrom (ch, 0, in, ch, s, juce::jmin (n, in.getNumSamples() - s));
            if (p->getTotalNumInputChannels() == 1 && s < in.getNumSamples())
            {
                buf.addFrom (0, 0, in, 1, s, juce::jmin (n, in.getNumSamples() - s));
                buf.applyGain (0, 0, n, 0.5f);
            }
            p->processBlock (buf, midi);
            ph.samplePos += n;
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, s, buf, juce::jmin (ch, p->getTotalNumOutputChannels() - 1), 0, n);
        }

        p->releaseResources();
        p->setPlayHead (nullptr);
        return out;
    }

    // Precise latency: an exactly in-tune harmonic note (A3) after silence.
    // A tuner should leave it almost untouched, so the waveform itself
    // cross-correlates cleanly. Returns -1 if no clear match.
    int probeLatency (juce::AudioPluginInstance* p, double sr, float& corrOut)
    {
        const int pre = (int) (0.5 * sr), len = (int) (1.0 * sr);
        juce::AudioBuffer<float> probe (2, pre + len + (int) (0.5 * sr));
        probe.clear();
        for (int i = 0; i < len; ++i)
        {
            const double t = i / sr;
            double s = 0;
            for (int k = 1; k * 220.0 < sr * 0.45; ++k)
                s += std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * k * t) / k * (k < 6 ? 1.0 : 0.4);
            const float env = (float) juce::jmin (1.0, i / (0.005 * sr));
            probe.setSample (0, pre + i, 0.25f * env * (float) s);
            probe.setSample (1, pre + i, 0.25f * env * (float) s);
        }

        int dummy = 0;
        const auto out = process (p, probe, sr, dummy);

        const int win = (int) (0.2 * sr), maxLag = (int) (0.4 * sr);
        // Window spans the note onset: a steady periodic tone alone would
        // correlate equally well at every pitch period.
        const int start = pre - (int) (0.02 * sr);
        const float* x = probe.getReadPointer (0) + start;
        const float* y = out.getReadPointer (0);
        double ex = 0; for (int i = 0; i < win; ++i) ex += (double) x[i] * x[i];
        int best = -1; double bestC = 0;
        for (int lag = 0; lag <= maxLag; ++lag)
        {
            const float* yy = y + start + lag;
            double c = 0, ey = 0;
            for (int i = 0; i < win; ++i) { c += (double) x[i] * yy[i]; ey += (double) yy[i] * yy[i]; }
            const double nc = c / std::sqrt (ex * ey + 1e-20);
            if (nc > bestC) { bestC = nc; best = lag; }
        }
        corrOut = (float) bestC;
        return bestC > 0.6 ? best : -1;
    }

    void printHeader()
    {
        std::printf ("\n%-26s %7s %6s %6s %6s %5s %6s %6s %6s %6s %6s %13s\n",
                     "input", "inTune%", "err c", "warbl", "wrong%", "oct%", "snapMs", "lost%", "rough", "clk/m", "lvl dB", "latency r/m");
        std::printf ("%s\n", juce::String::repeatedString ("-", 118).toRawUTF8());
    }

    void printRow (const juce::String& name, const TuneScore& s)
    {
        std::printf ("%-26s %7.1f %6.1f %6.1f %6.1f %5.1f %6.0f %6.1f %6.3f %6.1f %6.1f %6.1f/%-6.1f\n",
                     name.substring (0, 26).toRawUTF8(), s.inTunePct, s.medianErrCents, s.warbleCents, s.wrongNotePct,
                     s.octaveErrPct, s.snapMs, s.voicedLossPct, s.roughness, s.clicksPerMin, s.levelDb,
                     s.reportedLatencyMs, s.measuredLatencyMs);
    }

    juce::String csvRow (const juce::String& label, const juce::String& input, const TuneScore& s)
    {
        return label + "," + input + "," + juce::String (s.inTunePct, 2) + "," + juce::String (s.medianErrCents, 2) + ","
             + juce::String (s.warbleCents, 2) + "," + juce::String (s.wrongNotePct, 2) + "," + juce::String (s.octaveErrPct, 2) + ","
             + juce::String (s.snapMs, 1) + "," + juce::String (s.voicedLossPct, 2) + "," + juce::String (s.roughness, 4) + ","
             + juce::String (s.clicksPerMin, 2) + "," + juce::String (s.levelDb, 2) + ","
             + juce::String (s.reportedLatencyMs, 2) + "," + juce::String (s.measuredLatencyMs, 2) + "\n";
    }

    int cmdList (const Args& a)
    {
        if (a.positional.isEmpty()) return 1;
        auto p = loadPlugin (a.positional[0], a.opts["sub"], kSampleRate);
        if (p == nullptr) return 1;
        std::printf ("%s  (latency %d samples)\n", p->getName().toRawUTF8(), p->getLatencySamples());
        int i = 0;
        for (auto* prm : p->getParameters())
            std::printf ("  %3d  %-34s = %-16s steps=%d\n", i++, prm->getName (200).toRawUTF8(),
                         prm->getCurrentValueAsText().toRawUTF8(), prm->getNumSteps());
        return 0;
    }

    int cmdSynth (const Args& a)
    {
        if (a.positional.isEmpty()) return 1;
        const juce::File dir (a.positional[0]);
        for (auto& v : makeSynthVocals (kSampleRate))
        {
            writeWav (dir.getChildFile (v.name + ".wav"), v.audio, kSampleRate);
            std::printf ("%-20s %s\n", v.name.toRawUTF8(), v.description.toRawUTF8());
        }
        return 0;
    }

    int cmdRun (const Args& a)
    {
        const juce::String pluginPath = a.opts["plugin"];
        const juce::String label = a.opts.containsKey ("label") ? a.opts["label"]
                                 : (pluginPath == "none" ? juce::String ("dry") : juce::File (pluginPath).getFileNameWithoutExtension());
        const juce::File outDir = juce::File (a.opts["out"]).getChildFile (label);
        const int key = keyFromName (a.opts.containsKey ("key") ? a.opts["key"] : "C");
        const int mask = scaleMaskFromName (a.opts.containsKey ("scale") ? a.opts["scale"] : "chromatic");

        std::printf ("\n=== %s ===\n", label.toRawUTF8());

        std::unique_ptr<juce::AudioPluginInstance> plugin;
        if (pluginPath != "none")
        {
            plugin = loadPlugin (pluginPath, a.opts["sub"], kSampleRate);
            if (plugin == nullptr) return 1;
            plugin->enableAllBuses();
            juce::AudioProcessor::BusesLayout stereo;
            stereo.inputBuses.add (juce::AudioChannelSet::stereo());
            stereo.outputBuses.add (juce::AudioChannelSet::stereo());
            if (! plugin->setBusesLayout (stereo))
            {
                juce::AudioProcessor::BusesLayout mono;
                mono.inputBuses.add (juce::AudioChannelSet::mono());
                mono.outputBuses.add (juce::AudioChannelSet::mono());
                plugin->setBusesLayout (mono);
            }
            plugin->prepareToPlay (kSampleRate, kBlock); // some plugins only expose params after prepare
            applySets (*plugin, a.sets);
        }

        int probe = -1;
        if (plugin != nullptr)
        {
            // VST3 delivers parameter changes with the next audio block - push
            // one through so settings like Low Latency are live before probing.
            {
                juce::AudioBuffer<float> flush (2, (int) (0.1 * kSampleRate));
                flush.clear();
                int unused = 0;
                process (plugin.get(), flush, kSampleRate, unused);
            }

            float corr = 0;
            probe = probeLatency (plugin.get(), kSampleRate, corr);
            std::printf ("  latency: reports %.1f ms, probe measures %s (match %.2f)\n",
                         1000.0 * plugin->getLatencySamples() / kSampleRate,
                         probe >= 0 ? (juce::String (1000.0 * probe / kSampleRate, 1) + " ms").toRawUTF8() : "unclear", corr);
        }

        // Gather inputs
        struct Input { juce::String name; juce::AudioBuffer<float> audio; };
        std::vector<Input> inputs;
        for (const auto& pth : a.positional)
        {
            if (pth == "synth")
            {
                for (auto& v : makeSynthVocals (kSampleRate)) inputs.push_back ({ v.name, std::move (v.audio) });
                continue;
            }
            juce::AudioBuffer<float> b; double sr = 0;
            if (! readWav (juce::File (pth), b, sr)) { std::printf ("Can't read %s\n", pth.toRawUTF8()); continue; }
            if (std::abs (sr - kSampleRate) > 1.0)
            {
                // Simple resample to the bench rate
                juce::AudioBuffer<float> r (2, (int) std::ceil (b.getNumSamples() * kSampleRate / sr));
                for (int ch = 0; ch < 2; ++ch)
                {
                    juce::LagrangeInterpolator li;
                    li.process (sr / kSampleRate, b.getReadPointer (ch), r.getWritePointer (ch), r.getNumSamples());
                }
                b = std::move (r);
            }
            inputs.push_back ({ juce::File (pth).getFileNameWithoutExtension(), std::move (b) });
        }

        juce::File csv = juce::File (a.opts["out"]).getChildFile ("results.csv");
        if (! csv.existsAsFile())
            csv.replaceWithText ("label,input,inTunePct,medianErrCents,warbleCents,wrongNotePct,octaveErrPct,snapMs,voicedLossPct,roughness,clicksPerMin,levelDb,reportedLatencyMs,measuredLatencyMs\n");

        printHeader();
        for (auto& in : inputs)
        {
            int latency = 0;
            auto out = process (plugin.get(), in.audio, kSampleRate, latency);
            writeWav (outDir.getChildFile (in.name + ".wav"), out, kSampleRate);
            const auto score = scoreTuning (in.audio, out, kSampleRate, key, mask, latency, a.measuredLatency || probe >= 0,
                                           // probe can't read a shifted note - fall back to what the plugin reports
                                           probe >= 0 ? probe : (plugin == nullptr ? 0 : latency),
                                           a.opts["shift"].getFloatValue(),
                                           a.opts.containsKey ("flatten-midi") ? a.opts["flatten-midi"].getIntValue() : -1);
            printRow (in.name, score);
            csv.appendText (csvRow (label, in.name, score));
        }
        return 0;
    }

    // Side-by-side pitch trace of two files every 50 ms (output shifted by --lagms).
    int cmdTrack (const Args& a)
    {
        juce::AudioBuffer<float> in, out; double sr1 = 0, sr2 = 0;
        if (! readWav (juce::File (a.opts["in"]), in, sr1) || ! readWav (juce::File (a.opts["out"]), out, sr2)) return 1;
        const int lag = (int) (a.opts["lagms"].getFloatValue() * 0.001 * sr2);
        auto mono = [] (const juce::AudioBuffer<float>& b, int off)
        {
            std::vector<float> m ((size_t) juce::jmax (0, b.getNumSamples() - off));
            for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (b.getSample (0, (int) i + off) + b.getSample (1, (int) i + off));
            return m;
        };
        const auto mi = mono (in, 0), mo = mono (out, lag);
        const auto ti = trackPitch (mi.data(), (int) mi.size(), sr1);
        const auto to = trackPitch (mo.data(), (int) mo.size(), sr2);
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        auto fmt = [] (const PitchFrame& f) -> juce::String
        {
            if (! f.voiced) return "   -      ";
            const int n = (int) std::round (f.midi);
            const int c = (int) std::round ((f.midi - n) * 100);
            return juce::String (names[(n % 12 + 12) % 12]) + juce::String (n / 12 - 1) + (c >= 0 ? " +" : " ") + juce::String (c) + "c";
        };
        std::printf ("  time    input        output\n");
        for (size_t i = 0; i < std::min (ti.frames.size(), to.frames.size()); i += 10)
            if (ti.frames[i].voiced || to.frames[i].voiced)
                std::printf ("%6.2fs  %-12s %-12s\n", i * ti.hopSeconds, fmt (ti.frames[i]).toRawUTF8(), fmt (to.frames[i]).toRawUTF8());
        return 0;
    }

    int cmdScore (const Args& a)
    {
        juce::AudioBuffer<float> in, out; double sr1 = 0, sr2 = 0;
        if (! readWav (juce::File (a.opts["in"]), in, sr1) || ! readWav (juce::File (a.opts["out"]), out, sr2) || std::abs (sr1 - sr2) > 1)
        {
            std::printf ("Need two readable WAVs at the same sample rate\n");
            return 1;
        }
        const int key = keyFromName (a.opts.containsKey ("key") ? a.opts["key"] : "C");
        const int mask = scaleMaskFromName (a.opts.containsKey ("scale") ? a.opts["scale"] : "chromatic");
        printHeader();
        const int known = a.opts.containsKey ("lagms") ? (int) (a.opts["lagms"].getFloatValue() * 0.001 * sr1) : -1;
        printRow (juce::File (a.opts["out"]).getFileNameWithoutExtension(), scoreTuning (in, out, sr1, key, mask, 0, true, known));
        return 0;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 2)
    {
        std::printf ("usage: NovaTuneBench list|synth|run|score ... (see Source/Main.cpp header)\n");
        return 1;
    }

    const juce::String cmd = juce::String::fromUTF8 (argv[1]);
    const auto args = parse (argc, argv);

    if (cmd == "list")  return cmdList (args);
    if (cmd == "synth") return cmdSynth (args);
    if (cmd == "run")   return cmdRun (args);
    if (cmd == "score") return cmdScore (args);
    if (cmd == "track") return cmdTrack (args);

    std::printf ("unknown command %s\n", cmd.toRawUTF8());
    return 1;
}
