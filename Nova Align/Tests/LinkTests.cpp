// Nova Align linked-instance tests: role defaults, guide recording only when a
// target is armed, timeline-aligned copy, group isolation, pass-through safety.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "LinkHub.h"
#include "AlignEngine.h"
#include "CaptureEngine.h"

using NovaAlign::LinkEndpoint;
using NovaAlign::CaptureEngine;

static int failures = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    (ok ? passes : failures)++;
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << "\n";
}

static constexpr double SR = 48000.0;
static constexpr int BLOCK = 480;

static float guideSignal (juce::int64 t)  { return 0.5f * std::sin ((float) t * 0.013f); }
static float targetSignal (juce::int64 t) { return 0.25f * std::sin ((float) t * 0.031f); }

// Plays the timeline from startT for numBlocks, feeding the guide endpoint and
// the target's capture engine the way processBlock does.
static void play (LinkEndpoint* guide, CaptureEngine* capture, juce::int64 startT, int numBlocks,
                  juce::int64 targetJoinT = -1)
{
    juce::AudioBuffer<float> g (2, BLOCK), t (2, BLOCK), empty;
    for (int b = 0; b < numBlocks; ++b)
    {
        const juce::int64 T = startT + (juce::int64) b * BLOCK;
        for (int i = 0; i < BLOCK; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                g.setSample (ch, i, guideSignal (T + i));
                t.setSample (ch, i, targetSignal (T + i));
            }
        if (guide) guide->processGuideBlock (g, true, T, SR);
        if (capture && (targetJoinT < 0 || T >= targetJoinT)) capture->processBlock (empty, t, true, SR, T);
    }
    juce::AudioBuffer<float> silent (2, BLOCK);
    silent.clear();
    if (guide) guide->processGuideBlock (silent, false, startT + (juce::int64) numBlocks * BLOCK, SR);
    if (capture) capture->processBlock (empty, silent, false, SR, startT + (juce::int64) numBlocks * BLOCK);
}

int main()
{
    std::cout << "Nova Align link tests\n";

    {
        LinkEndpoint first;
        first.chooseDefaultRole();
        LinkEndpoint second;
        second.chooseDefaultRole();
        LinkEndpoint third;
        third.chooseDefaultRole();
        check (first.getRole() == LinkEndpoint::Role::target, "first instance defaults to TARGET");
        check (second.getRole() == LinkEndpoint::Role::guide, "second instance defaults to GUIDE");
        check (third.getRole() == LinkEndpoint::Role::target, "third instance defaults to TARGET (guide exists)");
        const auto info = first.getGroupInfo();
        check (info.guides == 1 && info.targets == 2, "group info counts 1 guide, 2 targets");
    }

    {
        LinkEndpoint target, guide;
        guide.setRole (LinkEndpoint::Role::guide);
        guide.prepare (SR);
        target.prepare (SR);

        // Not armed: guide must not record.
        play (&guide, nullptr, 96000, 50);
        juce::AudioBuffer<float> out;
        double rate = 0;
        check (! target.copyGuideTake (-1, 0, out, rate), "guide records nothing when no target is armed");

        // Armed: one play pass fills both.
        CaptureEngine capture;
        capture.prepare (SR);
        capture.arm (CaptureEngine::Slot::target);
        target.setArmed (true);
        play (&guide, &capture, 96000, 200);

        const auto start = capture.getStartTimelineSample();
        const int written = capture.getWrittenSamples();
        check (start == 96000 && written == 200 * BLOCK, "target capture knows its timeline start and length");

        const bool ok = target.copyGuideTake (start, written, out, rate);
        target.setArmed (false);
        check (ok && out.getNumSamples() == written && rate == SR, "target pulls the linked guide take");

        float maxErr = 0;
        for (int i = 0; i < written; ++i)
            maxErr = std::max (maxErr, std::abs (out.getSample (0, i) - guideSignal (start + i)));
        check (maxErr < 1.0e-6f, "guide take is sample-accurate on the timeline");

        const auto file = capture.finishAndWriteToFile();
        check (file.existsAsFile(), "target take written to disk");
        file.deleteFile();
    }

    {
        // Target capture starts later than the guide (e.g. host started the
        // target's processing a few blocks late): copy must line up by timeline.
        LinkEndpoint target, guide;
        guide.setRole (LinkEndpoint::Role::guide);
        guide.prepare (SR);
        CaptureEngine capture;
        capture.prepare (SR);
        capture.arm (CaptureEngine::Slot::target);
        target.setArmed (true);

        juce::AudioBuffer<float> g (2, BLOCK), t (2, BLOCK), empty;
        const juce::int64 t0 = 10000;
        for (int b = 0; b < 100; ++b)
        {
            const juce::int64 T = t0 + (juce::int64) b * BLOCK;
            for (int i = 0; i < BLOCK; ++i)
                for (int ch = 0; ch < 2; ++ch) { g.setSample (ch, i, guideSignal (T + i)); t.setSample (ch, i, targetSignal (T + i)); }
            guide.processGuideBlock (g, true, T, SR);
            // Target sees the transport as stopped for its first 7 blocks.
            capture.processBlock (empty, t, b >= 7, SR, T);
        }
        const auto start = capture.getStartTimelineSample();
        juce::AudioBuffer<float> out;
        double rate = 0;
        const bool ok = target.copyGuideTake (start, capture.getWrittenSamples(), out, rate);
        float maxErr = 0;
        for (int i = 0; i < out.getNumSamples(); ++i)
            maxErr = std::max (maxErr, std::abs (out.getSample (1, i) - guideSignal (start + i)));
        check (ok && start == t0 + 7 * BLOCK && maxErr < 1.0e-6f, "late-starting target still lines up with the guide");
        target.setArmed (false);
        capture.cancel();
    }

    {
        // Groups are isolated.
        LinkEndpoint targetA, guideB;
        targetA.setGroup (0);
        guideB.setGroup (1);
        guideB.setRole (LinkEndpoint::Role::guide);
        guideB.prepare (SR);
        targetA.setArmed (true);
        play (&guideB, nullptr, 0, 50);
        juce::AudioBuffer<float> out;
        double rate = 0;
        check (! targetA.copyGuideTake (-1, 0, out, rate), "target in link A ignores guide in link B");
        check (guideB.getGroupInfo().guides == 1 && targetA.getGroupInfo().guides == 0, "group info is per link");
        targetA.setArmed (false);
    }

    {
        // Guide destroyed mid-session: copy fails cleanly instead of crashing.
        LinkEndpoint target;
        {
            LinkEndpoint guide;
            guide.setRole (LinkEndpoint::Role::guide);
            guide.prepare (SR);
            target.setArmed (true);
            play (&guide, nullptr, 0, 50);
        }
        juce::AudioBuffer<float> out;
        double rate = 0;
        check (! target.copyGuideTake (-1, 0, out, rate), "removing the guide instance is safe");
        target.setArmed (false);
    }

    {
        // Guide-only pass (Guide REC with no sidechain): numSamples 0 takes everything.
        LinkEndpoint target, guide;
        guide.setRole (LinkEndpoint::Role::guide);
        guide.prepare (SR);
        target.setArmed (true);
        play (&guide, nullptr, 5000, 120);
        juce::AudioBuffer<float> out;
        double rate = 0;
        const bool ok = target.copyGuideTake (-1, 0, out, rate);
        check (ok && out.getNumSamples() == 120 * BLOCK && std::abs (out.getSample (0, 100) - guideSignal (5100)) < 1.0e-6f,
               "guide-only capture takes the whole guide pass");
        target.setArmed (false);
    }

    {
        // Alignment accuracy: a double where every syllable is off by a different amount.
        const double sr = 44100.0;
        static const double on[]  = { 0.20, 0.55, 0.80, 1.30, 1.55, 2.10, 2.45, 2.90 };
        static const double dur[] = { 0.25, 0.15, 0.35, 0.18, 0.40, 0.22, 0.30, 0.25 };
        static const double off[] = { 0.035, -0.020, 0.045, 0.010, -0.040, 0.025, 0.060, -0.015 };
        auto make = [&] (const juce::String& name, bool loose)
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("nova_align_" + name + ".wav");
            f.deleteFile();
            juce::AudioBuffer<float> b (1, (int) (sr * 3.5));
            b.clear();
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / sr;
                for (int k = 0; k < 8; ++k)
                {
                    const double s0 = on[k] + (loose ? off[k] : 0.0);
                    if (t >= s0 && t < s0 + dur[k])
                        b.setSample (0, i, (float) (std::sin (juce::MathConstants<double>::pi * (t - s0) / dur[k])
                                                    * std::sin (2.0 * juce::MathConstants<double>::pi * (180.0 + 40 * k) * (t - s0)) * (loose ? 0.35 : 0.5)));
                }
            }
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 1, 32, {}, 0));
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            return f;
        };
        const auto gf = make ("guide", false), tf = make ("target", true);
        NovaAlign::AlignEngine::AlignSettings st;
        st.amount = 1.0f; st.tightness = 1.0f; st.naturalness = 0.2f;
        juce::AudioBuffer<float> out; double rate = 0; int64_t shift = 0;
        const bool ok = NovaAlign::AlignEngine::alignTargetClipToBuffer (gf, tf, st, out, rate, shift);
        auto onset = [&] (const juce::AudioBuffer<float>& b, double expect)
        {
            const int from = (int) ((expect - 0.09) * sr), to = (int) ((expect + 0.09) * sr);
            float pk = 0; for (int i = from; i < to; i += 44) pk = juce::jmax (pk, b.getRMSLevel (0, i, 88));
            for (int i = from; i < to; i += 22) if (b.getRMSLevel (0, i, 88) > pk * 0.2f) return i / sr;
            return expect;
        };
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> gr (fm.createReaderFor (gf));
        juce::AudioBuffer<float> gb (1, (int) gr->lengthInSamples);
        gr->read (&gb, 0, gb.getNumSamples(), 0, true, false);
        double err = 0;
        for (int k = 0; k < 8 && ok; ++k) err += std::abs (onset (out, on[k]) - onset (gb, on[k])) * 1000.0;
        std::cout << "  avg syllable error after align: " << err / 8 << " ms (was ~32 ms)\n";
        check (ok && err / 8 < 10.0, "loose double: every syllable pulled onto the guide");
    }

    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
