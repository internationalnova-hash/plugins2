#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace NovaStudio
{
    // Streams one clip's audio for the real-time thread.
    //
    // - The first seconds after the clip's start point are cached in RAM when the streamer
    //   is created, so a clip always starts instantly (after a seek, a loop wrap or a split).
    // - An optional "pinned" region (the loop start, when it falls inside the clip) is cached
    //   the same way so loops wrap without a gap.
    // - Everything else is streamed into a ring buffer by a background TimeSliceThread, a few
    //   seconds ahead of where the audio thread last read.
    // The file is only kept open while the clip is near the playhead, so large sessions don't
    // run out of file handles.
    //
    // Threading: create/pin on the message thread before the streamer is published; after that
    // only the audio thread calls read() and only the background thread calls useTimeSlice().
    class ClipStreamer : public juce::TimeSliceClient
    {
    public:
        // startSessionSamples: where in the file the clip starts, in session-rate samples.
        ClipStreamer(juce::AudioFormatManager& formats, const juce::File& file, int64_t startSessionSamples, double sessionRate);
        ~ClipStreamer() override;

        bool isValid() const noexcept { return valid; }
        double getFileSampleRate() const noexcept { return fileRate; }
        int getNumChannels() const noexcept { return numChannels; }
        int64_t getFileLength() const noexcept { return fileLength; }
        const juce::File& getFile() const noexcept { return file; }
        int64_t getFileStartSample() const noexcept { return headStart; }

        // Message thread, before publishing: keep [start, start+len) of the file in RAM.
        void setPinnedRegion(int64_t fileStart, int64_t numFileSamples);
        int64_t getPinnedStart() const noexcept { return pinStart; }

        // Audio thread. Copies file samples [fileStart, fileStart+num) into dest (numChannels
        // channels, missing data -> zeros). Returns false if any part wasn't available yet.
        bool read(float* const* dest, int destChannels, int64_t fileStart, int num) noexcept;

        // Message/offline thread: block until [fileStart, fileStart+num) is buffered (or timeout).
        void prefetchBlocking(int64_t fileStart, int timeoutMs);

        int useTimeSlice() override;

        static constexpr double kHeadSeconds = 2.0;
        static constexpr double kRingSeconds = 6.0;

    private:
        void loadRegion(int64_t fileStart, int64_t num, juce::AudioBuffer<float>& dest);
        bool readFromRegion(const juce::AudioBuffer<float>& region, int64_t regionStart, int64_t regionLen,
                            float* const* dest, int destChannels, int64_t fileStart, int num, int destOffset) const noexcept;

        juce::AudioFormatManager& formats;
        juce::File file;
        bool valid = false;
        double fileRate = 44100.0;
        int numChannels = 2;
        int64_t fileLength = 0;

        juce::AudioBuffer<float> head;      // [headStart, headStart + headLen)
        int64_t headStart = 0, headLen = 0;
        juce::AudioBuffer<float> pinned;    // [pinStart, pinStart + pinLen)
        int64_t pinStart = -1, pinLen = 0;

        // Ring buffer: holds file samples [validStart, validEnd), slot = pos % ringSize.
        juce::AudioBuffer<float> ring;
        int ringSize = 0;
        std::atomic<int64_t> validStart { 0 }, validEnd { 0 };
        std::atomic<uint32_t> generation { 0 };      // bumped when the ring jumps
        std::atomic<int64_t> wantedPos { -1 };       // where the audio thread wants to read
        std::atomic<uint32_t> lastWantTimeMs { 0 };

        std::unique_ptr<juce::AudioFormatReader> reader;   // background thread only
        juce::AudioBuffer<float> fillScratch;              // background thread only
        juce::CriticalSection fillLock;                    // serialises useTimeSlice vs prefetchBlocking
    };
}
