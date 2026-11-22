// Headless validation of NovaSamplerEngine (zone lookup / velocity crossfade
// / round-robin) and NovaSamplePlayback (pitch-shift / looping), driven
// against synthetic sine-tone zones - no real sample library needed. Proves
// the multisample MECHANISM works before any real content sourcing (spec
// section 35: "the objective is ENGINE VALIDATION").

#include <juce_audio_basics/juce_audio_basics.h>
#include "Sampler/NovaSamplerEngine.h"
#include "Sampler/NovaSamplePlayback.h"
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

    std::shared_ptr<juce::AudioBuffer<float>> makeSineBuffer (double freqHz, double sampleRate, int numSamples, int numChannels = 1)
    {
        auto buf = std::make_shared<juce::AudioBuffer<float>> (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* d = buf->getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * (double) i / sampleRate);
        }
        return buf;
    }

    NovaSampleZone makeZone (const juce::String& name, int rootNote, int lowKey, int highKey,
                              int lowVel, int highVel, int rrGroup,
                              std::shared_ptr<const juce::AudioBuffer<float>> buffer, double sr)
    {
        NovaSampleZone z;
        z.name = name;
        z.rootNote = rootNote;
        z.lowKey = lowKey; z.highKey = highKey;
        z.lowVelocity = lowVel; z.highVelocity = highVel;
        z.roundRobinGroup = rrGroup;
        z.buffer = std::move (buffer);
        z.sourceSampleRate = sr;
        return z;
    }

    // Counts zero-crossings in a mono signal - a cheap, robust-enough proxy
    // for "did this play back at roughly the expected pitch".
    int countZeroCrossings (const std::vector<float>& samples)
    {
        int crossings = 0;
        for (size_t i = 1; i < samples.size(); ++i)
            if ((samples[i - 1] < 0.f) != (samples[i] < 0.f)) ++crossings;
        return crossings;
    }
}

int main()
{
    constexpr double sr = 44100.0;

    std::cout << "\n[Sampler] Basic zone lookup: in-range and out-of-range\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (sr);
        auto buf = makeSineBuffer (440.0, sr, 4096);
        engine.addZone (makeZone ("Piano C4", 60, 48, 72, 1, 127, 0, buf, sr));

        auto sel = engine.selectZones (60, 100);
        check (sel.primary != nullptr && sel.primary->name == "Piano C4", "note 60 vel 100 matches the single zone");

        auto outOfRange = engine.selectZones (90, 100);
        check (outOfRange.primary == nullptr, "note 90 (outside key range) matches nothing");
    }

    std::cout << "\n[Sampler] Round robin rotates through all siblings, no immediate repeat\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (sr);
        auto b1 = makeSineBuffer (200.0, sr, 2048);
        auto b2 = makeSineBuffer (200.0, sr, 2048);
        auto b3 = makeSineBuffer (200.0, sr, 2048);
        engine.addZone (makeZone ("Snare RR1", 38, 38, 38, 1, 127, 5, b1, sr));
        engine.addZone (makeZone ("Snare RR2", 38, 38, 38, 1, 127, 5, b2, sr));
        engine.addZone (makeZone ("Snare RR3", 38, 38, 38, 1, 127, 5, b3, sr));

        juce::StringArray order;
        for (int i = 0; i < 9; ++i)
        {
            auto sel = engine.selectZones (38, 100);
            order.add (sel.primary != nullptr ? sel.primary->name : "null");
        }
        check (order.size() == 9 && ! order.contains ("null"), "9 triggers all resolved to a zone");

        bool sawAllThree = order.contains ("Snare RR1") && order.contains ("Snare RR2") && order.contains ("Snare RR3");
        check (sawAllThree, "round robin visited all 3 siblings over 9 triggers");

        bool noImmediateRepeat = true;
        for (int i = 1; i < order.size(); ++i)
            if (order[i] == order[i - 1]) { noImmediateRepeat = false; break; }
        check (noImmediateRepeat, "round robin never repeats the same sample twice in a row (avoids machine-gun effect, spec section 4)");

        check (order[0] == "Snare RR1" && order[3] == "Snare RR1" && order[6] == "Snare RR1", "rotation is a clean cycle of period 3");
    }

    std::cout << "\n[Sampler] Velocity-layer crossfade blends smoothly across the overlap\n";
    {
        NovaSamplerEngine engine;
        engine.prepare (sr);
        auto soft = makeSineBuffer (300.0, sr, 2048);
        auto hard = makeSineBuffer (300.0, sr, 2048);
        engine.addZone (makeZone ("Soft", 60, 60, 60, 1, 70, 0, soft, sr));
        engine.addZone (makeZone ("Hard", 60, 60, 60, 40, 127, 1, hard, sr));
        // overlap region is velocity 40-70

        auto atLowEdge  = engine.selectZones (60, 40);
        auto atMid      = engine.selectZones (60, 55);
        auto atHighEdge = engine.selectZones (60, 70);
        auto belowOverlap = engine.selectZones (60, 20);
        auto aboveOverlap = engine.selectZones (60, 120);

        check (atLowEdge.secondary != nullptr, "velocity 40 (overlap start) selects both layers");
        check (atLowEdge.secondaryBlend < 0.1f, "velocity 40 blend is near 0 (mostly Soft)");
        check (atHighEdge.secondaryBlend > 0.9f, "velocity 70 blend is near 1 (mostly Hard)");
        check (std::abs (atMid.secondaryBlend - 0.5f) < 0.15f, "velocity 55 (overlap midpoint) blends roughly 50/50");
        check (belowOverlap.secondary == nullptr && belowOverlap.primary->name == "Soft", "velocity 20 (below overlap) is Soft only, no crossfade");
        check (aboveOverlap.secondary == nullptr && aboveOverlap.primary->name == "Hard", "velocity 120 (above overlap) is Hard only, no crossfade");
    }

    std::cout << "\n[Playback] Pitch-shifting: +12 semitones roughly doubles zero-crossing rate\n";
    {
        auto buf = makeSineBuffer (220.0, sr, (int) sr); // 1 second @ 220Hz, plenty of headroom
        NovaSampleZone zone = makeZone ("Test", 60, 0, 127, 1, 127, 0, buf, sr);
        NovaSamplerEngine::ZoneSelection sel;
        sel.primary = &zone;

        NovaSamplePlayback atRoot;
        atRoot.start (sel, 60, sr); // no pitch shift
        std::vector<float> outL (8192, 0.f), outR (8192, 0.f);
        atRoot.renderAdding (outL.data(), outR.data(), (int) outL.size());
        const int crossingsAtRoot = countZeroCrossings (outL);

        NovaSamplePlayback upOctave;
        upOctave.start (sel, 72, sr); // +12 semitones
        std::vector<float> outL2 (8192, 0.f), outR2 (8192, 0.f);
        upOctave.renderAdding (outL2.data(), outR2.data(), (int) outL2.size());
        const int crossingsUpOctave = countZeroCrossings (outL2);

        std::cout << "    crossings @root=" << crossingsAtRoot << " @+12st=" << crossingsUpOctave << "\n";
        const double ratio = (double) crossingsUpOctave / (double) juce::jmax (1, crossingsAtRoot);
        check (ratio > 1.8 && ratio < 2.2, "playing +12 semitones above root roughly doubles the frequency");
    }

    std::cout << "\n[Playback] Non-looping sample finishes; looping sample keeps producing audio past the source length\n";
    {
        auto buf = makeSineBuffer (300.0, sr, 1000); // short buffer, no loop
        NovaSampleZone zone = makeZone ("Short", 60, 0, 127, 1, 127, 0, buf, sr);
        NovaSamplerEngine::ZoneSelection sel; sel.primary = &zone;

        NovaSamplePlayback pb;
        pb.start (sel, 60, sr);
        std::vector<float> outL (4000, 0.f), outR (4000, 0.f); // render well past the 1000-sample buffer
        pb.renderAdding (outL.data(), outR.data(), (int) outL.size());
        check (! pb.isActive(), "non-looping sample finishes once it runs past the source buffer");

        NovaSampleZone loopedZone = makeZone ("Loop", 60, 0, 127, 1, 127, 0, buf, sr);
        loopedZone.loopEnabled = true;
        loopedZone.loopMode = NovaLoopMode::Forward;
        loopedZone.loopStart = 100;
        loopedZone.loopEnd = 900;
        NovaSamplerEngine::ZoneSelection loopedSel; loopedSel.primary = &loopedZone;

        NovaSamplePlayback loopedPb;
        loopedPb.start (loopedSel, 60, sr);
        std::vector<float> loutL (4000, 0.f), loutR (4000, 0.f);
        loopedPb.renderAdding (loutL.data(), loutR.data(), (int) loutL.size());
        check (loopedPb.isActive(), "looping sample is still active well past the source buffer length");

        float peak = 0.f;
        for (float s : loutL) peak = juce::jmax (peak, std::abs (s));
        check (peak > 0.1f, "looping sample keeps producing real (non-silent) audio");
    }

    std::cout << "\n[Playback] Ping-pong loop stays bounded within the loop region\n";
    {
        auto buf = makeSineBuffer (250.0, sr, 1000);
        NovaSampleZone zone = makeZone ("PingPong", 60, 0, 127, 1, 127, 0, buf, sr);
        zone.loopEnabled = true;
        zone.loopMode = NovaLoopMode::PingPong;
        zone.loopStart = 200;
        zone.loopEnd = 800;
        NovaSamplerEngine::ZoneSelection sel; sel.primary = &zone;

        NovaSamplePlayback pb;
        pb.start (sel, 60, sr);
        std::vector<float> outL (20000, 0.f), outR (20000, 0.f); // many loop cycles
        pb.renderAdding (outL.data(), outR.data(), (int) outL.size());
        check (pb.isActive(), "ping-pong loop is still active after many cycles");

        float peak = 0.f;
        for (float s : outL) peak = juce::jmax (peak, std::abs (s));
        check (peak > 0.1f && peak < 1.0f, "ping-pong loop output stays in a sane, non-blown-up amplitude range");
    }

    std::cout << "\n==============================\n";
    std::cout << (failures == 0 ? "ALL SAMPLER VALIDATION TESTS PASSED\n"
                                : juce::String (failures) + " TEST(S) FAILED\n");
    std::cout << "==============================\n";

    return failures == 0 ? 0 : 1;
}
