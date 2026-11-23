#include "ClipStreamer.h"

namespace NovaStudio
{
    ClipStreamer::ClipStreamer(juce::AudioFormatManager& f, const juce::File& fileToStream, int64_t startSessionSamples, double sessionRate)
        : formats(f), file(fileToStream)
    {
        std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));
        if (r == nullptr)
            return;

        fileRate    = r->sampleRate > 0 ? r->sampleRate : 44100.0;
        numChannels = juce::jlimit(1, 2, (int) r->numChannels);
        fileLength  = r->lengthInSamples;
        valid = true;
        const int64_t fileStartSample = (int64_t) std::llround((double) startSessionSamples * fileRate / juce::jmax(1.0, sessionRate));

        headStart = juce::jlimit((int64_t) 0, fileLength, fileStartSample);
        headLen   = juce::jmin((int64_t) (fileRate * kHeadSeconds), fileLength - headStart);
        if (headLen > 0)
        {
            head.setSize(numChannels, (int) headLen);
            head.clear();
            r->read(&head, 0, (int) headLen, headStart, true, numChannels > 1);
        }

        ringSize = (int) (fileRate * kRingSeconds);
        ring.setSize(numChannels, ringSize);
        ring.clear();
        fillScratch.setSize(numChannels, 8192);
        validStart = validEnd = headStart + headLen;   // stream continues right after the head
    }

    ClipStreamer::~ClipStreamer() = default;

    void ClipStreamer::loadRegion(int64_t fileStart, int64_t num, juce::AudioBuffer<float>& dest)
    {
        dest.setSize(numChannels, (int) juce::jmax((int64_t) 1, num));
        dest.clear();
        if (num <= 0) return;
        std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));
        if (r != nullptr)
            r->read(&dest, 0, (int) num, fileStart, true, numChannels > 1);
    }

    void ClipStreamer::setPinnedRegion(int64_t fileStart, int64_t num)
    {
        if (! valid) return;
        fileStart = juce::jlimit((int64_t) 0, fileLength, fileStart);
        num = juce::jmin(num, fileLength - fileStart);
        if (fileStart == pinStart && num == pinLen) return;
        pinStart = fileStart;
        pinLen = num;
        loadRegion(pinStart, pinLen, pinned);
    }

    bool ClipStreamer::readFromRegion(const juce::AudioBuffer<float>& region, int64_t regionStart, int64_t regionLen,
                                      float* const* dest, int destChannels, int64_t fileStart, int num, int destOffset) const noexcept
    {
        if (regionLen <= 0 || fileStart < regionStart || fileStart + num > regionStart + regionLen)
            return false;
        const int off = (int) (fileStart - regionStart);
        for (int ch = 0; ch < destChannels; ++ch)
            juce::FloatVectorOperations::copy(dest[ch] + destOffset,
                                              region.getReadPointer(juce::jmin(ch, numChannels - 1), off), num);
        return true;
    }

    bool ClipStreamer::read(float* const* dest, int destChannels, int64_t fileStart, int num) noexcept
    {
        if (num <= 0) return true;
        for (int ch = 0; ch < destChannels; ++ch)
            juce::FloatVectorOperations::clear(dest[ch], num);
        if (! valid) return false;

        // Outside the file: silence (that's correct, not a dropout).
        int destOffset = 0;
        if (fileStart < 0)
        {
            const int skip = (int) juce::jmin((int64_t) num, -fileStart);
            destOffset += skip; fileStart += skip; num -= skip;
        }
        if (num <= 0 || fileStart >= fileLength) return true;
        num = (int) juce::jmin((int64_t) num, fileLength - fileStart);

        wantedPos.store(fileStart + num, std::memory_order_relaxed);
        lastWantTimeMs.store(juce::Time::getMillisecondCounter(), std::memory_order_relaxed);

        if (readFromRegion(head, headStart, headLen, dest, destChannels, fileStart, num, destOffset))
            return true;
        if (readFromRegion(pinned, pinStart, pinLen, dest, destChannels, fileStart, num, destOffset))
            return true;

        // Ring buffer. Check the valid window before and after copying; if the background
        // thread moved the window meanwhile, the copy may be torn -> treat as a miss.
        const uint32_t gen = generation.load(std::memory_order_acquire);
        const int64_t vs = validStart.load(std::memory_order_acquire);
        const int64_t ve = validEnd.load(std::memory_order_acquire);
        if (fileStart < vs || fileStart + num > ve)
            return false;

        for (int ch = 0; ch < destChannels; ++ch)
        {
            const float* src = ring.getReadPointer(juce::jmin(ch, numChannels - 1));
            int done = 0;
            while (done < num)
            {
                const int slot = (int) ((fileStart + done) % ringSize);
                const int chunk = juce::jmin(num - done, ringSize - slot);
                juce::FloatVectorOperations::copy(dest[ch] + destOffset + done, src + slot, chunk);
                done += chunk;
            }
        }

        if (generation.load(std::memory_order_acquire) != gen || validStart.load(std::memory_order_acquire) > fileStart)
        {
            for (int ch = 0; ch < destChannels; ++ch)
                juce::FloatVectorOperations::clear(dest[ch] + destOffset, num);
            return false;
        }
        return true;
    }

    int ClipStreamer::useTimeSlice()
    {
        if (! valid) return 500;
        const juce::ScopedLock sl(fillLock);

        const int64_t wantRaw = wantedPos.load(std::memory_order_relaxed);
        const uint32_t sinceWant = juce::Time::getMillisecondCounter() - lastWantTimeMs.load(std::memory_order_relaxed);

        // Idle for a while: close the file so big sessions don't exhaust file handles.
        if (wantRaw < 0 || sinceWant > 5000)
        {
            reader.reset();
            return 200;
        }

        if (reader == nullptr)
        {
            reader.reset(formats.createReaderFor(file));
            if (reader == nullptr) return 500;
        }

        int64_t vs = validStart.load(), ve = validEnd.load();

        // While playing from a RAM-cached region, stream what comes right after it.
        int64_t want = wantRaw;
        if (headLen > 0 && want >= headStart && want <= headStart + headLen) want = headStart + headLen;
        else if (pinLen > 0 && want >= pinStart && want <= pinStart + pinLen) want = pinStart + pinLen;
        if (want >= fileLength && ve >= fileLength) return 50;

        // The reader jumped outside the window (seek / loop / other region): restart there.
        if (want < vs || want > ve + ringSize / 4)
        {
            generation.fetch_add(1, std::memory_order_acq_rel);
            vs = ve = juce::jmax((int64_t) 0, want);
            validStart.store(vs, std::memory_order_release);
            validEnd.store(ve, std::memory_order_release);
        }

        const int64_t target = juce::jmin(fileLength, want + (int64_t) (ringSize * 3 / 4));
        if (ve >= target)
            return 20;

        const int toRead = (int) juce::jmin((int64_t) fillScratch.getNumSamples(), target - ve);
        // Free room first: drop samples that the new chunk will overwrite.
        const int64_t newEnd = ve + toRead;
        if (newEnd - vs > ringSize)
        {
            vs = newEnd - ringSize;
            validStart.store(vs, std::memory_order_release);
        }

        fillScratch.clear();
        reader->read(&fillScratch, 0, toRead, ve, true, numChannels > 1);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* dst = ring.getWritePointer(ch);
            const float* src = fillScratch.getReadPointer(ch);
            int done = 0;
            while (done < toRead)
            {
                const int slot = (int) ((ve + done) % ringSize);
                const int chunk = juce::jmin(toRead - done, ringSize - slot);
                juce::FloatVectorOperations::copy(dst + slot, src + done, chunk);
                done += chunk;
            }
        }
        validEnd.store(newEnd, std::memory_order_release);
        return 0;   // more to do: come back straight away
    }

    void ClipStreamer::prefetchBlocking(int64_t fileStart, int timeoutMs)
    {
        if (! valid) return;
        if (fileStart >= headStart && fileStart < headStart + headLen) return;
        if (pinLen > 0 && fileStart >= pinStart && fileStart < pinStart + pinLen) return;

        wantedPos.store(fileStart);
        lastWantTimeMs.store(juce::Time::getMillisecondCounter());
        const auto deadline = juce::Time::getMillisecondCounter() + (uint32_t) timeoutMs;
        const int64_t need = juce::jmin(fileLength, fileStart + (int64_t) (fileRate * 0.5));
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            if (validStart.load() <= fileStart && validEnd.load() >= need) return;
            useTimeSlice();
        }
    }
}
