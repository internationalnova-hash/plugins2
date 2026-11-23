// Nova Link tests: two studios connected in one process.
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "LinkTransport.h"
#include "LinkAudio.h"
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
    int64_t hostPos = 0;
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
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::cout << "Nova Link tests\n";
    testTransport();
    testAudioPath();
    std::cout << "\n" << passes << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
