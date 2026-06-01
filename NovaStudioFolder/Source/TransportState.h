#pragma once

#include <JuceHeader.h>

namespace NovaStudio
{
    class TransportState : public juce::ChangeBroadcaster
    {
    public:
        TransportState();
        ~TransportState() override;

        enum class PlaybackState
        {
            Stopped,
            Playing,
            Recording
        };

        void setSampleRate(double rate) noexcept;
        double getSampleRate() const noexcept;

        void setTempo(int bpm) noexcept;
        int getTempo() const noexcept;

        void setLoopRange(int64_t startSample, int64_t endSample) noexcept;
        void clearLoopRange() noexcept;
        bool hasLoopRange() const noexcept;

        void setRecordArmed(bool armed) noexcept;
        bool isRecordArmed() const noexcept;

        void setInputMonitoring(bool enabled) noexcept;
        bool isInputMonitoring() const noexcept;

        void play() noexcept;
        void stop() noexcept;
        void toggleRecord() noexcept;
        void setLooping(bool enabled) noexcept;
        bool isLooping() const noexcept;
        bool isPlaying() const noexcept;

        void setPositionSamples(int64_t samplePos, bool notify = true) noexcept;
        int64_t getPositionSamples() const noexcept;

        void startRecording() noexcept;
        void stopRecording() noexcept;
        bool isRecording() const noexcept;

        int64_t getLoopStartSample() const noexcept;
        int64_t getLoopEndSample() const noexcept;

        juce::String getTimecodeString(int64_t samplePosition) const;
        juce::String getBarBeatString(int64_t samplePosition) const;

    private:
        void updatePositionFromClock() const noexcept;
        int64_t getPlaybackPositionInternal() const noexcept;

        std::atomic<double> sampleRate { 44100.0 };
        std::atomic<int> tempoBpm { 120 };
        std::atomic<bool> loopEnabled { false };
        std::atomic<bool> recordArmed { false };
        std::atomic<bool> inputMonitor { false };
        std::atomic<bool> playing { false };
        std::atomic<bool> recording { false };

        mutable std::atomic<int64_t> currentSamplePosition { 0 };
        mutable std::atomic<int64_t> lastClockSamplePosition { 0 };
        mutable std::atomic<double> playStartTimeMs { 0.0 };

        std::atomic<int64_t> loopStartSample { 0 };
        std::atomic<int64_t> loopEndSample { 0 };
    };
}
