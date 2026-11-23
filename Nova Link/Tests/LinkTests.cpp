// Nova Link tests: two studios connected in one process.
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "LinkTransport.h"
#include "LinkAudio.h"
#include "LinkSession.h"
#include <thread>
#include <iostream>

using namespace NovaLink;

static int passes = 0, failures = 0;
static void check(bool ok, const char* what)
{
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << "\n";
    (ok ? passes : failures)++;
}

template <typename Pred> static bool waitFor(Pred p, int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end) { if (p()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    return p();
}

static void testTransport()
{
    std::cout << "\n== Transport (loopback matchmaking) ==\n";
    auto [sigHost, sigArtist] = LoopbackSignaling::makePair();
    auto host = std::make_shared<LinkTransport>();
    auto artist = std::make_shared<LinkTransport>();

    std::atomic<int> ctlAtArtist { 0 }, ctlAtHost { 0 }, audioAtHost { 0 };
    std::atomic<size_t> fileBytes { 0 };
    std::string lastCtl;
    std::mutex m;
    artist->onControl = [&] (const std::string& s) { std::lock_guard<std::mutex> g(m); lastCtl = s; ++ctlAtArtist; };
    host->onControl = [&] (const std::string&) { ++ctlAtHost; };
    host->onAudio = [&] (const uint8_t*, size_t) { ++audioAtHost; };
    host->onFile = [&] (const uint8_t*, size_t n) { fileBytes += n; };

    const auto t0 = juce::Time::getMillisecondCounter();
    host->start(LinkTransport::Role::Host, sigHost);
    artist->start(LinkTransport::Role::Artist, sigArtist);
    const bool connected = waitFor([&] { return host->getState() == LinkTransport::State::Connected
                                                && artist->getState() == LinkTransport::State::Connected; }, 15000);
    std::cout << "    connected in " << (juce::Time::getMillisecondCounter() - t0) << " ms\n";
    check(connected, "two studios connect peer-to-peer");
    if (!connected) return;

    check(host->sendControl("{\"t\":\"chat\",\"text\":\"Ready?\"}"), "host sends chat");
    check(waitFor([&] { return ctlAtArtist.load() == 1; }, 3000), "artist receives chat");
    { std::lock_guard<std::mutex> g(m); check(lastCtl.find("Ready?") != std::string::npos, "chat text intact"); }
    artist->sendControl("{\"t\":\"chat\",\"text\":\"Yes\"}");
    check(waitFor([&] { return ctlAtHost.load() == 1; }, 3000), "host receives reply");

    std::vector<uint8_t> pkt(300, 7);
    for (int i = 0; i < 200; ++i) artist->sendAudio(pkt.data(), pkt.size());
    waitFor([&] { return audioAtHost.load() >= 200; }, 3000);
    std::cout << "    audio packets: " << audioAtHost.load() << "/200\n";
    check(audioAtHost.load() >= 190, "live audio packets arrive (lossy channel, local)");

    // 3 MB "take" in 64 KB chunks with back-pressure
    std::vector<uint8_t> chunk(64 * 1024, 3);
    size_t sent = 0;
    const size_t total = 3 * 1024 * 1024;
    while (sent < total)
    {
        if (artist->sendFile(chunk.data(), chunk.size())) sent += chunk.size();
        else std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    check(waitFor([&] { return fileBytes.load() == total; }, 20000), "3 MB take transferred reliably");

    artist->stop();
    check(waitFor([&] { return host->getState() == LinkTransport::State::Failed || host->getState() == LinkTransport::State::Closed; }, 10000),
          "host notices the artist leaving");
    host->stop();
}

static void testAudioPath()
{
    std::cout << "\n== Live audio (Opus, jitter buffer, timeline stamps) ==\n";
    StreamEncoder enc(192000);
    StreamDecoder dec;
    const double srIn = 44100.0, srOut = 48000.0;   // studios at different sample rates
    const int block = 256;
    std::vector<float> L(block), R(block);
    const float* in[2] = { L.data(), R.data() };
    juce::Random rng(1);
    std::vector<std::vector<uint8_t>> inFlight;   // packets "on the network"
    std::vector<float> outL(512), outR(512);
    float* out[2] = { outL.data(), outR.data() };
    std::vector<float> received;
    int64_t firstStamp = -1, lastStamp = -1;
    bool stampsMonotonic = true;
    double phase = 0.0;
    int64_t hostPos = (int64_t) (10.0 * srIn);   // song already at 10 s
    int dropped = 0;

    for (int b = 0; b < 1200; ++b)   // ~7 s
    {
        for (int i = 0; i < block; ++i) { L[(size_t) i] = R[(size_t) i] = 0.5f * (float) std::sin(phase); phase += 2.0 * juce::MathConstants<double>::pi * 1000.0 / srIn; }
        enc.push(in, 2, block, srIn, (int64_t) std::llround(hostPos * kNetRate / srIn), 1);
        hostPos += block;
        uint8_t pkt[kMaxPacket];
        while (int n = enc.pull(pkt, sizeof(pkt)))
        {
            if (rng.nextFloat() < 0.02f) { ++dropped; continue; }   // 2% loss
            inFlight.emplace_back(pkt, pkt + n);
        }
        // network: deliver with jitter (sometimes hold packets back a few blocks)
        if (!inFlight.empty() && rng.nextFloat() > 0.3f)
        {
            for (auto& p : inFlight) dec.insert(p.data(), p.size());
            inFlight.clear();
        }
        // receiver renders at its own block size/rate
        const int outBlock = (int) std::llround(block * srOut / srIn);
        const auto st = dec.render(out, 2, outBlock, srOut);
        if (st >= 0)
        {
            if (lastStamp >= 0 && st < lastStamp) stampsMonotonic = false;
            if (firstStamp < 0) firstStamp = st;
            lastStamp = st;
            received.insert(received.end(), outL.begin(), outL.begin() + outBlock);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1500));
    }
    std::cout << "    received " << received.size() / srOut << " s, dropped " << dropped << " packets, jitter "
              << dec.getJitterMs() << " ms, buffer " << dec.getBufferMs() << " ms\n";
    check(received.size() > srOut * 4, "audio plays at the other end");
    // frequency check on the last 2 s
    int crossings = 0;
    const size_t from = received.size() - (size_t) (srOut * 2);
    for (size_t i = from + 1; i < received.size(); ++i) if ((received[i - 1] < 0) != (received[i] < 0)) ++crossings;
    const double f = crossings / 2.0 / 2.0;
    std::cout << "    measured " << f << " Hz (sent 1000 Hz at 44.1k, played at 48k)\n";
    check(std::abs(f - 1000.0) < 15.0, "pitch survives resampling + codec + loss");
    float rms = 0; for (size_t i = from; i < received.size(); ++i) rms += received[i] * received[i];
    rms = std::sqrt(rms / (float) (received.size() - from));
    check(std::abs(juce::Decibels::gainToDecibels(rms / (0.5f / std::sqrt(2.0f)))) < 2.0f, "level preserved (+/-2 dB)");
    check(stampsMonotonic && lastStamp > firstStamp, "host-timeline stamps advance in order");
    // The last rendered stamp should be close to what was sent (minus the buffer delay).
    const double sentSec = hostPos / srIn, gotSec = lastStamp / (double) kNetRate;
    std::cout << "    last stamp " << gotSec << " s vs sent up to " << sentSec << " s\n";
    check(sentSec - gotSec > 0.0 && sentSec - gotSec < 0.3, "stamps match the sender's timeline");
}

static void testFullSession()
{
    std::cout << "\n== Full session: host records a take from the artist ==\n";
    auto [sh, sa] = LoopbackSignaling::makePair();
    LinkSession host, artist;
    const double srH = 48000.0, srA = 44100.0;
    const int block = 256;
    host.prepare(srH, block);
    artist.prepare(srA, block);
    host.startWithSignaling(LinkSession::Role::Host, sh, "NOVA-TEST-TEST", "Studio A");
    artist.startWithSignaling(LinkSession::Role::Artist, sa, "NOVA-TEST-TEST", "Marcus");
    check(waitFor([&] { return host.getState() == LinkTransport::State::Connected && artist.getState() == LinkTransport::State::Connected; }, 15000),
          "session connects");
    check(waitFor([&] { return host.getPeerName() == "Marcus" && artist.getPeerName() == "Studio A"; }, 5000), "studios exchange names");

    // Real-time "audio threads" for both studios
    std::atomic<bool> run { true }, recording { false };
    std::atomic<double> hostSec { 10.0 };    // song position when recording starts: 10 s
    std::thread hostAudio([&]
    {
        juce::AudioBuffer<float> io(2, block);
        double ph = 0;
        while (run)
        {
            for (int i = 0; i < block; ++i) { const float v = 0.3f * (float) std::sin(ph); io.setSample(0, i, v); io.setSample(1, i, v); ph += 2 * juce::MathConstants<double>::pi * 220.0 / srH; }
            HostPosition p; p.seconds = hostSec.load(); p.bpm = 120; p.ppq = p.seconds * 2; p.playing = true; p.recording = recording.load();
            host.processBlock(io, nullptr, p, true);
            hostSec.store(hostSec.load() + block / srH);
            std::this_thread::sleep_for(std::chrono::microseconds((int) (block / srH * 1e6)));
        }
    });
    std::thread artistAudio([&]
    {
        juce::AudioBuffer<float> io(2, block);
        double ph = 0;
        while (run)
        {
            for (int i = 0; i < block; ++i) { const float v = 0.4f * (float) std::sin(ph); io.setSample(0, i, v); io.setSample(1, i, v); ph += 2 * juce::MathConstants<double>::pi * 660.0 / srA; }
            HostPosition none;
            artist.processBlock(io, nullptr, none, false);
            std::this_thread::sleep_for(std::chrono::microseconds((int) (block / srA * 1e6)));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(800));   // let audio settle
    const double recStart = hostSec.load();
    recording = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    recording = false;

    host.sendChat("Nice one, again from the hook");
    juce::Array<Section> secs; secs.add({ 0.0, "Intro" }); secs.add({ 16.0, "Hook" });
    host.setSections(secs);

    auto takeReady = [&]
    {
        const auto st = host.getStateJson();
        if (auto* t = st.getProperty("takes", {}).getArray())
            for (auto& tk : *t) if (tk.getProperty("state", "").toString() == "ready") return true;
        return false;
    };
    const bool ready = waitFor(takeReady, 20000);
    check(ready, "lossless take arrives at the host");

    const auto hs = host.getStateJson();
    const auto tk = (*hs.getProperty("takes", {}).getArray())[0];
    const double start = (double) tk.getProperty("start", 0.0);
    const double len = (double) tk.getProperty("length", 0.0);
    std::cout << "    take starts at " << start << " s (host hit record at " << recStart << " s), length " << len << " s\n";
    check(std::abs(start - recStart) < 0.25, "take placed at the host's song position (within the stream delay)");
    check(len > 1.5 && len < 2.6, "take length matches the recording");
    {
        auto f = juce::File(tk.getProperty("file", "").toString());
        std::unique_ptr<juce::AudioFormatReader> r(juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(f), true));
        check(r != nullptr && std::abs(r->sampleRate - srA) < 1, "take is the artist's own lossless recording (44.1 kHz)");
        if (r)
        {
            juce::AudioBuffer<float> b(1, (int) r->lengthInSamples);
            r->read(&b, 0, b.getNumSamples(), 0, true, false);
            int crossings = 0;
            for (int i = 1; i < b.getNumSamples(); ++i) if ((b.getSample(0, i - 1) < 0) != (b.getSample(0, i) < 0)) ++crossings;
            const double f0 = crossings / 2.0 / (b.getNumSamples() / r->sampleRate);
            std::cout << "    take tone " << f0 << " Hz (artist sang 660 Hz)\n";
            check(std::abs(f0 - 660.0) < 5.0, "take contains the artist's audio, uncompressed");
        }
    }
    check(waitFor([&] { return artist.getStateJson().getProperty("chat", {}).size() >= 1; }, 3000), "artist gets the chat message");
    check(waitFor([&] { return artist.getStateJson().getProperty("sections", {}).size() == 2; }, 3000), "artist sees the host's sections");
    const double artistSees = (double) artist.getStateJson().getProperty("pos", {}).getProperty("sec", 0.0);
    std::cout << "    artist sees host position " << artistSees << " s (host at " << hostSec.load() << " s)\n";
    check(std::abs(artistSees - hostSec.load()) < 0.6, "artist's session strip follows the host's timeline");
    const auto aex = host.exportTakeAligned((int) tk.getProperty("id", 0));
    check(aex.existsAsFile(), "take exports lined up from 0:00 for dragging into the DAW");

    run = false;
    hostAudio.join(); artistAudio.join();
    artist.leave(); host.leave();
    juce::File(tk.getProperty("file", "").toString()).getParentDirectory().deleteRecursively();
}

static void testHttpMatchmaking(const juce::String& server)
{
    std::cout << "\n== Matchmaking through the store (" << server << ") ==\n";
    const auto code = makeSessionCode();
    auto host = std::make_shared<LinkTransport>();
    auto artist = std::make_shared<LinkTransport>();
    const auto t0 = juce::Time::getMillisecondCounter();
    host->start(LinkTransport::Role::Host, std::make_shared<HttpSignaling>(server, code, "host"));
    artist->start(LinkTransport::Role::Artist, std::make_shared<HttpSignaling>(server, code, "artist"));
    const bool ok = waitFor([&] { return host->getState() == LinkTransport::State::Connected && artist->getState() == LinkTransport::State::Connected; }, 30000);
    std::cout << "    " << code << " connected in " << (juce::Time::getMillisecondCounter() - t0) << " ms\n";
    check(ok, "studios find each other by session code via the store");
    check(normaliseSessionCode(code.toLowerCase().replace("-", " ")) == code, "codes can be typed in any case/spacing");
    artist->stop(); host->stop();
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::cout << "Nova Link tests\n";
    testTransport();
    testAudioPath();
    testFullSession();
    if (auto srv = juce::SystemStats::getEnvironmentVariable("NOVA_LINK_TEST_SERVER", {}); srv.isNotEmpty())
        testHttpMatchmaking(srv);
    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
