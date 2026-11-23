#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

struct OpusEncoder;
struct OpusDecoder;

namespace NovaLink
{
    // Live audio between the studios: 48 kHz stereo Opus in 10 ms frames.
    // Every frame carries the host-timeline position (in 48 kHz samples) of its first sample,
    // so both studios always know which moment of the host's song a piece of audio belongs to.
    constexpr int kNetRate = 48000;
    constexpr int kFrame = 480;           // 10 ms
    constexpr int kMaxPacket = 1500;

    struct PacketHeader
    {
        uint32_t seq = 0;
        int64_t stamp = 0;                // host timeline, 48 kHz samples
        uint8_t flags = 0;                // bit0 playing, bit1 recording, bit2 talkback
    };
    constexpr size_t kHeaderSize = 4 + 8 + 1;

    void writeHeader(uint8_t* dst, const PacketHeader& h);
    PacketHeader readHeader(const uint8_t* src);

    // Encoder side: audio thread pushes plugin-rate audio + timeline stamp; a worker thread
    // pulls 10 ms network frames out (resampled to 48 kHz) and encodes them.
    class StreamEncoder
    {
    public:
        StreamEncoder(int bitrate);
        ~StreamEncoder();
        void setBitrate(int bps);

        // Audio thread. `stamp48` = host timeline (48 kHz samples) of input[0].
        void push(const float* const* input, int numChannels, int numSamples, double sampleRate, int64_t stamp48, uint8_t flags);

        // Worker thread: produces at most one packet (header + Opus). Returns size, 0 if none ready.
        int pull(uint8_t* packet, int capacity);

    private:
        OpusEncoder* enc = nullptr;
        // plugin-rate staging FIFO (interleaved stereo) + per-push stamps
        juce::AbstractFifo fifo { 48000 * 2 };
        std::vector<float> fifoData = std::vector<float>((size_t) 48000 * 2 * 2);
        struct StampMark { int64_t fifoPos; int64_t stamp48; uint8_t flags; double rate; };
        juce::AbstractFifo markFifo { 1024 };
        std::array<StampMark, 1024> marks {};
        int64_t writtenTotal = 0, readTotal = 0;   // samples through the fifo
        // worker-side resampling state
        juce::LagrangeInterpolator interp[2];
        std::vector<float> in48[2];
        StampMark current {};
        bool haveMark = false;
        uint32_t seq = 0;
        double lastRate = 0.0;
        double outAcc = 0.0;
        std::vector<float> scratchIn[2];
    };

    // Decoder side: network thread inserts packets; audio thread renders plugin-rate audio
    // with a jitter buffer (adaptive delay), packet-loss concealment and drift correction.
    class StreamDecoder
    {
    public:
        StreamDecoder();
        ~StreamDecoder();

        void insert(const uint8_t* packet, size_t size);              // network thread
        // Audio thread: renders numSamples at sampleRate. Returns the stamp (48 kHz host
        // timeline) of the first rendered sample, or -1 if nothing is playing yet.
        int64_t render(float* const* out, int numChannels, int numSamples, double sampleRate, uint8_t* flagsOut = nullptr);
        void reset();

        float getJitterMs() const noexcept { return jitterMs.load(); }
        float getLossPercent() const noexcept { return lossPct.load(); }
        float getBufferMs() const noexcept { return bufferMs.load(); }
        bool isReceiving() const noexcept { return juce::Time::getMillisecondCounter() - lastArrivalMs.load() < 500; }

    private:
        bool decodeNextFrame();   // into frame48

        struct Slot { std::atomic<uint32_t> seqTag { 0xffffffffu }; int size = 0; PacketHeader h; std::array<uint8_t, kMaxPacket> data {}; };
        std::array<Slot, 256> slots;
        std::atomic<uint32_t> newestSeq { 0 }, firstSeq { 0 };
        std::atomic<bool> any { false };
        OpusDecoder* dec = nullptr;
        bool playing = false;
        uint32_t nextSeq = 0;
        std::array<float, kFrame * 2> frame48 {};   // interleaved
        int frameReadPos = kFrame;
        int64_t frameStamp = -1;
        uint8_t frameFlags = 0;
        int targetFrames = 4;
        juce::LagrangeInterpolator interp[2];
        std::vector<float> res48[2];
        int carry = 0;
        int64_t carryStamp = -1;
        // stats
        std::atomic<juce::uint32> lastArrivalMs { 0 };
        std::atomic<float> jitterMs { 0.0f }, lossPct { 0.0f }, bufferMs { 0.0f };
        double lastArrivalT = 0.0, jitterAcc = 0.0;
        int lostRecent = 0, totalRecent = 0;
    };
}
