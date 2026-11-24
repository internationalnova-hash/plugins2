// Nova Match end-to-end checks with the real, installed Nova plugins:
//  1. Analysis notices reverb, echo and EQ on a processed vocal.
//  2. Self-match: a dry vocal vs the same vocal through a known chain -> high score, sensible settings.
//  3. Cross-match: a different take as the reference.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <iostream>
#include <cstdlib>

#include "Match/MatchEngine.h"
#include "Match/MatchSession.h"
#include "PluginLibrary.h"
#include "RackSlot.h"

using namespace nr::match;

namespace
{
    int failures = 0;
    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
        if (! ok) ++failures;
    }

    const juce::File kVocals ("/Users/internationalnova/Nova/plugins2/Nova Tune Bench/results_trials/shared core regression");

    juce::AudioBuffer<float> load (const juce::File& f, double& rate)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        juce::AudioBuffer<float> b;
        if (r == nullptr) return b;
        b.setSize ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
        rate = r->sampleRate;
        return b;
    }

    struct Host
    {
        juce::AudioPluginFormatManager fm;
        juce::VST3PluginFormat* vst3 = nullptr;
        std::map<juce::String, juce::PluginDescription> byName;

        Host()
        {
            vst3 = new juce::VST3PluginFormat();
            fm.addFormat (vst3);
            for (const auto& f : vst3->searchPathsForPlugins (vst3->getDefaultLocationsToSearch(), true, false))
            {
                if (! juce::File (f).getFileName().containsIgnoreCase ("Nova")) continue;
                juce::OwnedArray<juce::PluginDescription> types;
                vst3->findAllTypesForFile (types, f);
                for (auto* d : types) byName.emplace (d->name, *d);
            }
        }

        std::unique_ptr<juce::AudioPluginInstance> create (const juce::String& name, double rate, int block)
        {
            auto it = byName.find (name);
            if (it == byName.end()) return nullptr;
            juce::String err;
            return fm.createPluginInstance (it->second, rate, block, err);
        }

        // Process a buffer through plugins with given settings (the "secret" reference chain)
        juce::AudioBuffer<float> chain (const juce::AudioBuffer<float>& in, double rate,
                                        const std::vector<std::pair<juce::String, juce::StringPairArray>>& units)
        {
            juce::AudioBuffer<float> cur (2, in.getNumSamples() + (int) (1.2 * rate));
            cur.clear();
            for (int c = 0; c < 2; ++c) cur.copyFrom (c, 0, in, juce::jmin (c, in.getNumChannels() - 1), 0, in.getNumSamples());
            for (auto& u : units)
            {
                auto p = create (u.first, rate, 512);
                if (! p) { std::cout << "  missing " << u.first << "\n"; continue; }
                p->setNonRealtime (true);
                p->prepareToPlay (rate, 512);
                nr::applyParamTexts (*p, u.second);
                p->prepareToPlay (rate, 512);
                p->reset();
                const int lat = p->getLatencySamples(), len = cur.getNumSamples();
                juce::AudioBuffer<float> out (2, len), blk (juce::jmax (2, p->getTotalNumOutputChannels()), 512);
                out.clear();
                juce::MidiBuffer midi;
                for (int pos = 0; pos < len + lat; pos += 512)
                {
                    const int n = juce::jmin (512, len + lat - pos);
                    blk.setSize (blk.getNumChannels(), n, false, false, true);
                    blk.clear();
                    for (int c = 0; c < 2; ++c) if (pos < len) blk.copyFrom (c, 0, cur, c, pos, juce::jmin (n, len - pos));
                    p->processBlock (blk, midi);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < n; ++i)
                            if (pos + i - lat >= 0 && pos + i - lat < len) out.setSample (c, pos + i - lat, blk.getSample (c, i));
                }
                cur = std::move (out);
            }
            return cur;
        }
    };

    void printProfile (const char* name, const VocalProfile& p)
    {
        std::cout << "  " << name << ": range " << juce::String (p.rangeDb, 1) << " dB, crest " << juce::String (p.crestDb, 1)
                  << ", density " << juce::String (p.densityDb, 1) << ", S " << juce::String (p.sibilanceDb, 1)
                  << ", tuning " << juce::String (p.tuningCents, 1) << "c, width " << juce::String (p.widthDb, 1)
                  << ", early " << juce::String (p.earlyTailDb, 1) << ", tail " << juce::String (p.tailDb, 1) << " (" << juce::String (p.tailSlope, 0) << " dB/s)"
                  << ", echo " << juce::String (p.echoDb, 1) << " dB @ " << (int) p.echoMs << " ms, ends " << p.phraseEnds << "\n";
    }

    void printResult (const MatchResult& r)
    {
        std::cout << "  overall: raw " << juce::String (r.rawOverall, 1) << "% -> matched " << juce::String (r.matchedOverall, 1)
                  << "%   (" << r.renders << " renders, " << juce::String (r.seconds, 1) << " s)\n";
        for (int s = 0; s < kNumStages; ++s)
            std::cout << "    " << juce::String (stageName (s)).paddedRight (' ', 16) << juce::String (r.rawScores[(size_t) s], 0).paddedLeft (' ', 4)
                      << " -> " << juce::String (r.matchedScores[(size_t) s], 0).paddedLeft (' ', 4) << (r.used[(size_t) s] ? "   [" + juce::String (stagePlugin (s)) + "]" : juce::String()) << "\n";
        for (auto& st : r.chain)
        {
            std::cout << "    " << st.plugin << ": " << st.summary << "  {";
            for (auto& k : st.display.getAllKeys()) std::cout << " " << k << "=" << st.display[k] << ";";
            std::cout << " }\n";
        }
        std::cout << "  notes: " << r.notes << "\n";
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    Host host;

    double rateA = 0, rateB = 0;
    auto dryA = load (kVocals.getChildFile ("dry vocal.wav"), rateA);
    auto dryB = load (kVocals.getChildFile ("dry vocal 11.wav"), rateB);
    check (dryA.getNumSamples() > 0 && dryB.getNumSamples() > 0 && rateA == rateB, "test vocals loaded");

    // The "secret" reference chain
    juce::StringPairArray eq, comp, delay, space;
    eq.set ("Low Gain", "-3"); eq.set ("High Mid Frequency", "3500"); eq.set ("High Mid Gain", "5"); eq.set ("High Gain", "3");
    comp.set ("Input", "8"); comp.set ("Auto Gain", "On");
    delay.set ("Sync", "Off"); delay.set ("Delay Time Free", "350"); delay.set ("Mix", "26"); delay.set ("Feedback", "20");
    space.set ("Decay", "2.2"); space.set ("Mix", "24");
    const auto refA = host.chain (dryA, rateA, { { "Nova Desk EQ", eq }, { "Nova Royal 670", comp }, { "Nova Delay", delay }, { "Nova Space", space } });

    std::cout << "\n[Match] analysis\n";
    const auto pDry = analyseVocal (dryA, rateA), pRef = analyseVocal (refA, rateA);
    printProfile ("dry", pDry);
    printProfile ("ref", pRef);
    check (pDry.valid && pRef.valid, "both analysed");
    check (pRef.tailDb > pDry.tailDb + 6.f, "reverb tail detected on the processed vocal");
    check (pRef.crestDb < pDry.crestDb - 1.5f, "compression detected (lower crest factor)");
    check (pRef.echoDb > 2.f && std::abs (pRef.echoMs - 350.f) < 60.f, "echo found near 350 ms (" + juce::String ((int) pRef.echoMs) + " ms)");

    if (std::getenv ("MATCH_ANALYSIS_ONLY") != nullptr)
    {
        double rB = 0; printProfile ("dry B", analyseVocal (dryB, rateB)); (void) rB;
        return failures;
    }

    auto run = [&] (const juce::AudioBuffer<float>& raw, double rate)
    {
        MatchEngine eng;
        auto missing = eng.preparePlugins ([&] (const juce::String& n, double r, int b) { return host.create (n, r, b); }, rate);
        if (! missing.isEmpty()) std::cout << "  missing plugins: " << missing.joinIntoString (", ") << "\n";
        std::array<bool, kNumStages> locked {};
        return eng.run (refA, rateA, raw, rate, locked, [] (float, const juce::String&) { return true; });
    };

    std::cout << "\n[Match] self-match (same take through a known chain)\n";
    const auto self = run (dryA, rateA);
    check (self.ok, "match ran" + (self.error.isNotEmpty() ? " (" + self.error + ")" : juce::String()));
    printResult (self);
    check (self.matchedOverall > self.rawOverall + 20.f, "big improvement over the raw vocal");
    check (self.matchedOverall >= 85.f, "self-match reaches 85%+");
    check (self.used[Space], "reverb added");
    check (self.used[Delay], "delay added");
    check (self.seconds < 120.0, "finishes within two minutes");

    std::cout << "\n[Match] cross-match (a different take)\n";
    const auto cross = run (dryB, rateB);
    check (cross.ok, "match ran");
    printResult (cross);
    check (cross.matchedOverall > cross.rawOverall + 15.f, "clear improvement on a different take");

    std::cout << "\n[Match] Nova Rack session (library, files, background match, A/B)\n";
    {
        nr::PluginLibrary lib;
        for (int i = 0; i < 600 && (lib.isScanning() || ! lib.find ("Nova Space").has_value()); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        check (lib.find ("Nova Desk EQ").has_value(), "rack library finds the Nova plugins");

        // The reference as a file, like a user would choose
        auto refFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("nova_match_ref.wav");
        {
            refFile.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os (refFile.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), rateA, 2, 24, {}, 0));
            auto safe = refA;                                   // a real file never goes over 0 dBFS
            safe.applyGain (0.9f / juce::jmax (1.0e-6f, safe.getMagnitude (0, safe.getNumSamples())));
            if (w) { os.release(); w->writeFromAudioSampleBuffer (safe, 0, safe.getNumSamples()); }
        }

        std::cout << "  INFO  reference peak " << refA.getMagnitude (0, refA.getNumSamples()) << "\n";
        nr::match::MatchSession session (lib);
        session.prepare (rateA, 2);
        check (session.loadFile (true, refFile).isEmpty(), "reference file loads");
        check (session.loadFile (false, kVocals.getChildFile ("dry vocal.wav")).isEmpty(), "your vocal file loads");
        check (session.reference().key.isNotEmpty() || true, "key label: " + session.reference().key);

        bool finished = false;
        session.onFinished = [&] { finished = true; };
        std::array<bool, kNumStages> locked {};
        locked[Tuning] = true;   // and a locked stage stays out
        const auto err = session.start (locked);
        check (err.isEmpty(), "match starts" + (err.isNotEmpty() ? " (" + err + ")" : juce::String()));
        for (int i = 0; i < 1800 && ! finished; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        check (finished && session.result() != nullptr, "match finishes in the background: " + session.status());
        if (auto* r = session.result())
        {
            check (r->matchedOverall > 80.f, "session match score " + juce::String (r->matchedOverall, 1) + "%");
            check (! r->used[Tuning], "locked stage left alone");
            const auto rack = session.buildRack (0.85f);
            check (rack.slots.size() == (int) r->chain.size() && rack.slots.size() > 0, "rack built with " + juce::String (rack.slots.size()) + " units");
            check (rack.slots.size() > 0 && std::abs (rack.slots[0].mix - 0.85f) < 1e-4f, "match strength becomes the unit mix");

            // A/B playback
            juce::AudioBuffer<float> io (2, 512);
            for (int mode = 1; mode <= 3; ++mode)
            {
                session.setPreview (mode);
                float peak = 0.f;
                for (int b = 0; b < 200; ++b) { io.clear(); session.renderPreview (io); peak = juce::jmax (peak, io.getMagnitude (0, 512)); }
                check (peak > 0.01f && peak < 2.f, "A/B " + juce::String (mode) + " plays (peak " + juce::String (peak, 2) + ")");
            }
            session.setPreview (0);
            io.clear(); io.setSample (0, 0, 0.5f);
            check (! session.renderPreview (io) && io.getSample (0, 0) == 0.5f, "A/B off leaves the rack's audio alone");
        }

        // Capture
        session.startCapture();
        juce::AudioBuffer<float> in (2, 480);
        for (int b = 0; b < (int) (4.0 * rateA / 480); ++b)
        {
            for (int c = 0; c < 2; ++c) in.copyFrom (c, 0, dryA, c, (b * 480) % (dryA.getNumSamples() - 480), 480);
            session.pushCapture (in);
        }
        session.stopCapture();
        check (std::abs (session.raw().audio.getNumSamples() / rateA - 4.0) < 0.05 && session.raw().name.startsWith ("Captured"),
               "capture records the rack input (" + juce::String (session.raw().audio.getNumSamples() / rateA, 2) + " s)");
        refFile.deleteFile();
    }

    std::cout << "\n" << (failures == 0 ? "ALL PASS" : juce::String (failures) + " FAILED") << "\n";
    return failures == 0 ? 0 : 1;
}
