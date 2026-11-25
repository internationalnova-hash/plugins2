// Drives the real NovaNotesProcessor like a DAW would: capture -> analysis ->
// AI Assist -> timeline-locked playback -> compare -> session save/reload.
//   NovaNotesProcessorTests <vocal.wav>

#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

static int fails = 0, passes = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    (ok ? passes : fails)++;
}

struct FakeHost : juce::AudioPlayHead
{
    juce::int64 pos = 0;
    bool playing = false;
    double sr = 48000.0, bpm = 96.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setTimeInSamples (pos);
        p.setBpm (bpm);
        p.setPpqPosition ((double) pos / sr * bpm / 60.0);
        return p;
    }
};

static void pump (NovaNotesProcessor& p, int maxMs, std::function<bool()> done)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) maxMs;
    while (juce::Time::getMillisecondCounter() < end)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        if (done()) { juce::MessageManager::getInstance()->runDispatchLoopUntil (120); return; }
    }
    juce::ignoreUnused (p);
}

// Plays `in` through the processor with the transport rolling from hostStart; returns the output (mono L).
static std::vector<float> play (NovaNotesProcessor& p, FakeHost& host, const std::vector<float>& in, juce::int64 hostStart, int pre)
{
    const int block = 512, total = (int) in.size() + pre * 2;
    std::vector<float> out ((size_t) total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    host.playing = true;
    for (int i = 0; i < total; i += block)
    {
        const int n = std::min (block, total - i);
        buf.setSize (2, n, false, false, true);
        for (int k = 0; k < n; ++k)
        {
            const int src = i + k - pre;
            const float v = (src >= 0 && src < (int) in.size()) ? in[(size_t) src] : 0.f;
            buf.setSample (0, k, v); buf.setSample (1, k, v);
        }
        host.pos = hostStart - pre + i;
        p.processBlock (buf, midi);
        for (int k = 0; k < n; ++k) out[(size_t) (i + k)] = buf.getSample (0, k);
    }
    host.playing = false;
    buf.setSize (2, block, false, false, true);
    buf.clear();
    p.processBlock (buf, midi);   // transport stopped
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: NovaNotesProcessorTests vocal.wav\n"); return 2; }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::String (argv[1]))));
    if (r == nullptr) { std::printf ("can't read %s\n", argv[1]); return 2; }
    const double sr = r->sampleRate;
    const int len = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (sr * 30));
    juce::AudioBuffer<float> file ((int) r->numChannels, len);
    r->read (&file, 0, len, 0, true, true);
    std::vector<float> vocal ((size_t) len);
    for (int i = 0; i < len; ++i) vocal[(size_t) i] = file.getSample (0, i);

    FakeHost host;
    host.sr = sr;
    const juce::int64 hostStart = (juce::int64) (sr * 2.0);

    auto p = std::make_unique<NovaNotesProcessor>();
    p->setPlayHead (&host);
    p->prepareToPlay (sr, 512);

    // 1. capture
    p->armCapture();
    check (p->getCaptureState() == NovaNotesProcessor::capArmed, "capture arms");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);   // timer pre-allocates
    {
        const int block = 512;
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        host.playing = true;
        for (int i = 0; i < len; i += block)
        {
            const int n = std::min (block, len - i);
            buf.setSize (2, n, false, false, true);
            for (int k = 0; k < n; ++k) { buf.setSample (0, k, vocal[(size_t) (i + k)]); buf.setSample (1, k, vocal[(size_t) (i + k)]); }
            host.pos = hostStart + i;
            p->processBlock (buf, midi);
            if ((i / block) % 64 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);   // timer keeps allocating
        }
        host.playing = false;
        buf.setSize (2, block, false, false, true);
        p->processBlock (buf, midi);
    }
    check (std::abs (p->getCapturedSeconds() - len / sr) < 0.02, "captured " + juce::String (p->getCapturedSeconds(), 2) + " s");

    pump (*p, 20000, [&] { return p->hasTake() && ! p->isBusy(); });
    const auto notes0 = NovaNotes::notesFromJson (p->getNotesJson());
    check (notes0.size() > 5, "analysis found " + juce::String ((int) notes0.size()) + " notes");
    std::printf ("info: %s\n", p->getInfoJson().toRawUTF8());

    // 2. untouched take plays back identical to the input
    auto out = play (*p, host, vocal, hostStart, 4096);
    float d0 = 0.f;
    for (int i = 0; i < len; ++i) d0 = std::max (d0, std::abs (out[(size_t) i + 4096] - vocal[(size_t) i]));
    check (d0 < 1.0e-6f, "unedited playback matches the input (max diff " + juce::String (d0, 7) + ")");

    // 3. AI Assist, then playback carries the tuned notes in sync
    const auto msg = p->runAssist (1, {});
    std::printf ("assist: %s\n", msg.toRawUTF8());
    pump (*p, 20000, [&] { return ! p->isBusy(); });
    const auto tuned = NovaNotes::notesFromJson (p->getNotesJson());
    out = play (*p, host, vocal, hostStart, 4096);
    std::vector<float> edited (out.begin() + 4096, out.begin() + 4096 + len);
    const auto tr = NovaNotes::trackPitch (edited.data(), len, sr);
    float sum = 0.f; int c = 0;
    for (const auto& n : tuned)
    {
        if (n.end - n.start < 30) continue;
        sum += std::abs (NovaNotes::measureCenter (tr, n.start, n.end) - n.pitch) * 100.f;
        ++c;
    }
    check (c > 0 && sum / c < 15.f, "played-back notes hit their targets (mean " + juce::String (c ? sum / c : 0.f, 1) + " cents)");
    float pre = 0.f;
    for (int i = 0; i < 4000; ++i) pre = std::max (pre, std::abs (out[(size_t) i]));
    check (pre == 0.f, "audio before the take passes through untouched");

    // 4. compare: Original plays the input
    p->setShowOriginal (true);
    auto orig = play (*p, host, vocal, hostStart, 0);
    float d1 = 0.f;
    for (int i = 0; i < len; ++i) d1 = std::max (d1, std::abs (orig[(size_t) i] - vocal[(size_t) i]));
    check (d1 < 1.0e-6f, "Original button plays what was sung");
    p->setShowOriginal (false);

    // 5. undo / redo
    p->undo();
    pump (*p, 5000, [&] { return ! p->isBusy(); });
    check (! NovaNotes::anyEdits (NovaNotes::notesFromJson (p->getNotesJson())), "undo removes the AI edits");
    p->redo();
    pump (*p, 5000, [&] { return ! p->isBusy(); });

    // 6. session save -> new instance -> same notes, same sound
    juce::MemoryBlock state;
    p->getStateInformation (state);
    std::printf ("session state: %.1f KB for %.1f s of audio\n", state.getSize() / 1024.0, len / sr);
    auto q = std::make_unique<NovaNotesProcessor>();
    q->setPlayHead (&host);
    q->prepareToPlay (sr, 512);
    q->setStateInformation (state.getData(), (int) state.getSize());
    pump (*q, 20000, [&] { return q->hasTake() && ! q->isBusy(); });
    check (q->getNotesJson() == p->getNotesJson(), "reloaded session has identical notes");
    auto out2 = play (*q, host, vocal, hostStart, 4096);
    float d2 = 0.f;
    for (size_t i = 0; i < out.size(); ++i) d2 = std::max (d2, std::abs (out2[i] - out[i]));
    check (d2 < 3.0e-3f, "reloaded session sounds the same (24-bit storage) (max diff " + juce::String (d2, 6) + ")");

    // 6b. a key the user picked sticks through reload
    p->setKey (7, false);
    juce::MemoryBlock st2;
    p->getStateInformation (st2);
    auto k2 = std::make_unique<NovaNotesProcessor>();
    k2->setStateInformation (st2.getData(), (int) st2.getSize());
    pump (*k2, 20000, [&] { return k2->hasTake() && ! k2->isBusy(); });
    check (k2->getInfoJson().contains ("\"tonic\":7,\"minor\":false") && k2->getInfoJson().contains ("\"userKey\":true"), "your key (G major) is kept in the session");
    k2.reset();

    // 7. a new capture replaces the take
    p->clearTake();
    pump (*p, 1000, [&] { return ! p->hasTake(); });
    auto passthru = play (*p, host, vocal, hostStart, 0);
    float d3 = 0.f;
    for (int i = 0; i < len; ++i) d3 = std::max (d3, std::abs (passthru[(size_t) i] - vocal[(size_t) i]));
    check (d3 == 0.f, "after clearing, audio passes straight through");

    p.reset(); q.reset();
    std::printf ("\n%d passed, %d failed\n", passes, fails);
    return fails == 0 ? 0 : 1;
}
