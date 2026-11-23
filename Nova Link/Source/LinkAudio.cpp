#include "LinkAudio.h"
#include <opus.h>
#include <cstring>

namespace NovaLink
{
    void writeHeader(uint8_t* d, const PacketHeader& h)
    {
        for (int i = 0; i < 4; ++i) d[i] = (uint8_t) (h.seq >> (8 * i));
        for (int i = 0; i < 8; ++i) d[4 + i] = (uint8_t) ((uint64_t) h.stamp >> (8 * i));
        d[12] = h.flags;
    }

    PacketHeader readHeader(const uint8_t* s)
    {
        PacketHeader h;
        for (int i = 0; i < 4; ++i) h.seq |= (uint32_t) s[i] << (8 * i);
        uint64_t st = 0;
        for (int i = 0; i < 8; ++i) st |= (uint64_t) s[4 + i] << (8 * i);
        h.stamp = (int64_t) st;
        h.flags = s[12];
        return h;
    }

    //==========================================================================
    // Encoder
    //==========================================================================
    StreamEncoder::StreamEncoder(int bitrate)
    {
        int err = 0;
        enc = opus_encoder_create(kNetRate, 2, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &err);
        if (enc != nullptr)
        {
            opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate));
            opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(8));
            opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));
            opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
        }
        for (auto& v : in48) v.assign(16384, 0.0f);
        for (auto& v : scratchIn) v.assign(16384, 0.0f);
    }

    StreamEncoder::~StreamEncoder() { if (enc) opus_encoder_destroy(enc); }
    void StreamEncoder::setBitrate(int bps) { if (enc) opus_encoder_ctl(enc, OPUS_SET_BITRATE(bps)); }

    void StreamEncoder::push(const float* const* input, int numChannels, int numSamples, double sampleRate, int64_t stamp48, uint8_t flags)
    {
        if (numSamples <= 0 || numChannels <= 0) return;
        numSamples = juce::jmin(numSamples, 8192);

        // Resample to 48 kHz on the audio thread (preallocated, allocation-free).
        const double ratio = sampleRate / kNetRate;   // input samples per output sample
        if (std::abs(sampleRate - lastRate) > 0.5) { for (auto& i : interp) i.reset(); lastRate = sampleRate; }
        int produced = 0;
        if (std::abs(ratio - 1.0) < 1.0e-9)
        {
            for (int ch = 0; ch < 2; ++ch)
                std::memcpy(in48[(size_t) ch].data(), input[juce::jmin(ch, numChannels - 1)], sizeof(float) * (size_t) numSamples);
            produced = numSamples;
        }
        else
        {
            // Fractional accumulator keeps output count in step with input over time.
            outAcc += numSamples / ratio;
            const int outN = juce::jlimit(0, 16000, (int) std::floor(outAcc));
            outAcc -= outN;
            for (int ch = 0; ch < 2; ++ch)
                interp[ch].process(ratio, input[juce::jmin(ch, numChannels - 1)], in48[(size_t) ch].data(), outN, numSamples, 0);
            produced = outN;
        }

        // Stamp mark for this push
        {
            int s1, n1, s2, n2;
            markFifo.prepareToWrite(1, s1, n1, s2, n2);
            if (n1 + n2 > 0)
            {
                marks[(size_t) (n1 > 0 ? s1 : s2)] = { writtenTotal, stamp48, flags, sampleRate };
                markFifo.finishedWrite(1);
            }
        }
        int s1, n1, s2, n2;
        fifo.prepareToWrite(produced, s1, n1, s2, n2);
        auto put = [&] (int start, int count, int offset)
        {
            for (int i = 0; i < count; ++i)
            {
                fifoData[(size_t) (start + i) * 2]     = in48[0][(size_t) (offset + i)];
                fifoData[(size_t) (start + i) * 2 + 1] = in48[1][(size_t) (offset + i)];
            }
        };
        put(s1, n1, 0);
        put(s2, n2, n1);
        fifo.finishedWrite(n1 + n2);
        writtenTotal += n1 + n2;
    }

    int StreamEncoder::pull(uint8_t* packet, int capacity)
    {
        if (enc == nullptr || fifo.getNumReady() < kFrame || capacity < (int) kHeaderSize + 64) return 0;

        // Latest stamp mark at or before this frame's first sample.
        while (markFifo.getNumReady() > 0)
        {
            int s1, n1, s2, n2;
            markFifo.prepareToRead(1, s1, n1, s2, n2);
            const auto& m = marks[(size_t) (n1 > 0 ? s1 : s2)];
            if (m.fifoPos > readTotal && haveMark) break;   // belongs to a later frame
            current = m;
            haveMark = true;
            markFifo.finishedRead(1);
        }

        float pcm[kFrame * 2];
        int s1, n1, s2, n2;
        fifo.prepareToRead(kFrame, s1, n1, s2, n2);
        std::memcpy(pcm, &fifoData[(size_t) s1 * 2], sizeof(float) * (size_t) n1 * 2);
        if (n2 > 0) std::memcpy(pcm + n1 * 2, &fifoData[(size_t) s2 * 2], sizeof(float) * (size_t) n2 * 2);
        fifo.finishedRead(n1 + n2);

        PacketHeader h;
        h.seq = seq++;
        h.flags = current.flags;
        // stamp advances with audio while the host plays; when stopped it stays put
        h.stamp = (current.flags & 1) ? current.stamp48 + (readTotal - current.fifoPos) : current.stamp48;
        readTotal += kFrame;

        const int bytes = opus_encode_float(enc, pcm, kFrame, packet + kHeaderSize, capacity - (int) kHeaderSize);
        if (bytes < 0) return 0;
        writeHeader(packet, h);
        return (int) kHeaderSize + bytes;
    }

    //==========================================================================
    // Decoder
    //==========================================================================
    StreamDecoder::StreamDecoder()
    {
        int err = 0;
        dec = opus_decoder_create(kNetRate, 2, &err);
        for (auto& v : res48) v.assign(32768, 0.0f);
    }

    StreamDecoder::~StreamDecoder() { if (dec) opus_decoder_destroy(dec); }

    void StreamDecoder::reset()
    {
        playing = false;
        frameReadPos = kFrame;
        carry = 0; carryStamp = -1;
        for (auto& i : interp) i.reset();
    }

    void StreamDecoder::insert(const uint8_t* packet, size_t size)
    {
        if (size <= kHeaderSize || size > kMaxPacket) return;
        const auto h = readHeader(packet);
        auto& slot = slots[h.seq % slots.size()];
        slot.seqTag.store(0xffffffffu, std::memory_order_release);   // invalidate while writing
        slot.h = h;
        slot.size = (int) (size - kHeaderSize);
        std::memcpy(slot.data.data(), packet + kHeaderSize, (size_t) slot.size);
        slot.seqTag.store(h.seq, std::memory_order_release);

        if (!any.load() || (int32_t) (h.seq - newestSeq.load()) > 0) newestSeq.store(h.seq);
        any.store(true);

        // Jitter: deviation of arrival spacing from the 10 ms frame time.
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (lastArrivalT > 0.0)
        {
            const double d = std::abs((now - lastArrivalT) - 10.0);
            jitterAcc = jitterAcc * 0.95 + d * 0.05;
            jitterMs.store((float) jitterAcc);
        }
        lastArrivalT = now;
        lastArrivalMs.store(juce::Time::getMillisecondCounter());
    }

    bool StreamDecoder::decodeNextFrame()
    {
        auto& slot = slots[nextSeq % slots.size()];
        int n = 0;
        if (slot.seqTag.load(std::memory_order_acquire) == nextSeq)
        {
            n = opus_decode_float(dec, slot.data.data(), slot.size, frame48.data(), kFrame, 0);
            frameStamp = slot.h.stamp;
            frameFlags = slot.h.flags;
            ++totalRecent;
        }
        else
        {
            // Lost or late: conceal (Opus fills in plausible audio).
            n = opus_decode_float(dec, nullptr, 0, frame48.data(), kFrame, 0);
            if (frameStamp >= 0 && (frameFlags & 1)) frameStamp += kFrame;
            ++lostRecent; ++totalRecent;
        }
        if (totalRecent >= 200)
        {
            lossPct.store(100.0f * (float) lostRecent / (float) totalRecent);
            lostRecent = totalRecent = 0;
        }
        ++nextSeq;
        frameReadPos = 0;
        return n > 0;
    }

    int64_t StreamDecoder::render(float* const* out, int numChannels, int numSamples, double sampleRate, uint8_t* flagsOut)
    {
        for (int ch = 0; ch < numChannels; ++ch) juce::FloatVectorOperations::clear(out[ch], numSamples);
        if (dec == nullptr || !any.load()) return -1;

        const uint32_t newest = newestSeq.load();
        // Adaptive buffer target: ~2x jitter, between 30 and 200 ms.
        targetFrames = juce::jlimit(3, 20, (int) std::ceil(jitterMs.load() * 2.0f / 10.0f) + 2);

        if (!playing)
        {
            // Wait until enough frames have arrived, then start that far behind the newest.
            static_assert(sizeof(uint32_t) == 4);
            if (!isReceiving()) return -1;
            nextSeq = newest - (uint32_t) targetFrames;
            playing = true;
            frameReadPos = kFrame;
            carry = 0; carryStamp = -1;
            for (auto& i : interp) i.reset();
        }
        else
        {
            // Drift / burst correction: too far behind -> skip ahead; too close -> hold back.
            const int ahead = (int32_t) (newest - nextSeq);
            if (ahead > targetFrames + 12) { nextSeq = newest - (uint32_t) targetFrames; frameReadPos = kFrame; }
            if (!isReceiving()) { playing = false; return -1; }
        }

        const double ratio = (double) kNetRate / sampleRate;   // 48k samples per output sample
        const int need48 = (int) std::ceil(numSamples * ratio) + 4;
        // Fill the 48 kHz staging area (after any samples carried over from last block).
        int have = carry;
        int64_t firstStamp = carry > 0 ? carryStamp : -1;
        while (have < need48 && have + kFrame <= (int) res48[0].size())
        {
            if (frameReadPos >= kFrame) decodeNextFrame();
            const int take = juce::jmin(kFrame - frameReadPos, need48 - have);
            if (firstStamp < 0 && frameStamp >= 0)
                firstStamp = frameStamp + ((frameFlags & 1) ? frameReadPos : 0) - ((frameFlags & 1) ? have : 0);
            for (int i = 0; i < take; ++i)
            {
                res48[0][(size_t) (have + i)] = frame48[(size_t) (frameReadPos + i) * 2];
                res48[1][(size_t) (have + i)] = frame48[(size_t) (frameReadPos + i) * 2 + 1];
            }
            have += take;
            frameReadPos += take;
        }
        // Resample 48 kHz -> host rate; keep what wasn't consumed for next time.
        int used = numSamples;
        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
            used = interp[ch].process(ratio, res48[(size_t) ch].data(), out[ch], numSamples, have, 0);
        for (int ch = 2; ch < numChannels; ++ch) juce::FloatVectorOperations::copy(out[ch], out[1], numSamples);
        used = juce::jlimit(0, have, used);
        carry = have - used;
        for (int ch = 0; ch < 2; ++ch)
            std::memmove(res48[(size_t) ch].data(), res48[(size_t) ch].data() + used, sizeof(float) * (size_t) carry);
        carryStamp = firstStamp >= 0 ? firstStamp + ((frameFlags & 1) ? used : 0) : -1;

        bufferMs.store((float) ((int32_t) (newest - nextSeq)) * 10.0f);
        if (flagsOut) *flagsOut = frameFlags;
        return firstStamp;
    }
}
