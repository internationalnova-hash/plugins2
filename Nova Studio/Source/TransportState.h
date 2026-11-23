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

        void setTempo(double bpm) noexcept;
        double getTempo() const noexcept;
        void setTimeSignature(int numerator, int denominator) noexcept;
        int getTimeSigNumerator() const noexcept { return timeSigNum.load(); }
        int getTimeSigDenominator() const noexcept { return timeSigDen.load(); }

        // Audio-clock transport: the audio thread advances the position by exactly the
        // samples it renders, so consecutive blocks are always contiguous.
        void advancePosition(int64_t numSamples) noexcept { currentSamplePosition.fetch_add(numSamples); }
        void setPositionFromAudioThread(int64_t samplePos) noexcept { currentSamplePosition.store(samplePos); }

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

        // ── Punch in/out ──────────────────────────────────────────────────────
        void setPunchRange(int64_t inSample, int64_t outSample) noexcept;
        void setPunchEnabled(bool on) noexcept;
        bool isPunchEnabled() const noexcept;
        int64_t getPunchInSample() const noexcept;
        int64_t getPunchOutSample() const noexcept;

        juce::String getTimecodeString(int64_t samplePosition) const;
        juce::String getBarBeatString(int64_t samplePosition) const;

    private:

        std::atomic<double> sampleRate { 44100.0 };
        std::atomic<double> tempoBpm { 120.0 };
        std::atomic<int> timeSigNum { 4 }, timeSigDen { 4 };
        std::atomic<bool> loopEnabled { false };
        std::atomic<bool> recordArmed { false };
        std::atomic<bool> inputMonitor { false };
        std::atomic<bool> playing { false };
        std::atomic<bool> recording { false };

        std::atomic<int64_t> currentSamplePosition { 0 };

        std::atomic<int64_t> loopStartSample { 0 };
        std::atomic<int64_t> loopEndSample { 0 };

        std::atomic<bool>    punchEnabled   { false };
        std::atomic<int64_t> punchInSample  { 0 };
        std::atomic<int64_t> punchOutSample { 0 };
    };
}
