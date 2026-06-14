#include "TransportState.h"

namespace NovaStudio
{
    TransportState::TransportState() = default;
    TransportState::~TransportState() = default;

    void TransportState::setSampleRate(double rate) noexcept
    {
        sampleRate.store(rate);
        sendChangeMessage();
    }

    double TransportState::getSampleRate() const noexcept
    {
        return sampleRate.load();
    }

    void TransportState::setTempo(int bpm) noexcept
    {
        tempoBpm.store(juce::jlimit(20, 300, bpm));
        sendChangeMessage();
    }

    int TransportState::getTempo() const noexcept
    {
        return tempoBpm.load();
    }

    void TransportState::setLoopRange(int64_t startSample, int64_t endSample) noexcept
    {
        if (startSample < 0)
            startSample = 0;

        if (endSample <= startSample)
            endSample = startSample;

        loopStartSample.store(startSample);
        loopEndSample.store(endSample);
        sendChangeMessage();
    }

    void TransportState::clearLoopRange() noexcept
    {
        loopEnabled.store(false);
        loopStartSample.store(0);
        loopEndSample.store(0);
        sendChangeMessage();
    }

    bool TransportState::hasLoopRange() const noexcept
    {
        return loopEndSample.load() > loopStartSample.load();
    }

    void TransportState::setRecordArmed(bool armed) noexcept
    {
        recordArmed.store(armed);
        sendChangeMessage();
    }

    bool TransportState::isRecordArmed() const noexcept
    {
        return recordArmed.load();
    }

    void TransportState::setInputMonitoring(bool enabled) noexcept
    {
        inputMonitor.store(enabled);
        sendChangeMessage();
    }

    bool TransportState::isInputMonitoring() const noexcept
    {
        return inputMonitor.load();
    }

    void TransportState::play() noexcept
    {
        if (playing.load())
            return;

        playStartTimeMs.store(juce::Time::getMillisecondCounterHiRes());
        lastClockSamplePosition.store(getPlaybackPositionInternal());
        playing.store(true);
        sendChangeMessage();
    }

    void TransportState::stop() noexcept
    {
        if (!playing.load())
            return;

        setPositionSamples(getPlaybackPositionInternal(), false);
        playing.store(false);
        sendChangeMessage();
    }

    void TransportState::toggleRecord() noexcept
    {
        if (!recordArmed.load())
        {
            setRecordArmed(true);
        }

        if (recording.load())
        {
            stopRecording();
        }
        else
        {
            startRecording();
        }
    }

    void TransportState::startRecording() noexcept
    {
        if (!recordArmed.load())
            return;

        if (!playing.load())
            play();

        recording.store(true);
        sendChangeMessage();
    }

    void TransportState::stopRecording() noexcept
    {
        if (!recording.load())
            return;

        recording.store(false);
        stop();
        sendChangeMessage();
    }

    bool TransportState::isRecording() const noexcept
    {
        return recording.load();
    }

    void TransportState::setLooping(bool enabled) noexcept
    {
        loopEnabled.store(enabled);
        sendChangeMessage();
    }

    bool TransportState::isLooping() const noexcept
    {
        return loopEnabled.load() && hasLoopRange();
    }

    void TransportState::setPositionSamples(int64_t samplePos, bool notify) noexcept
    {
        currentSamplePosition.store(samplePos);
        lastClockSamplePosition.store(samplePos);
        playStartTimeMs.store(juce::Time::getMillisecondCounterHiRes());
        if (notify)
            sendChangeMessage();
    }

    int64_t TransportState::getPositionSamples() const noexcept
    {
        return getPlaybackPositionInternal();
    }

    void TransportState::updatePositionFromClock() const noexcept
    {
        if (!playing.load())
            return;

        const double sampleRateValue = sampleRate.load();
        const double currentTimeMs = juce::Time::getMillisecondCounterHiRes();
        const double elapsedMs = currentTimeMs - playStartTimeMs.load();
        const double samplesElapsed = elapsedMs * (sampleRateValue / 1000.0);
        const int64_t position = lastClockSamplePosition.load() + static_cast<int64_t>(std::floor(samplesElapsed));
        currentSamplePosition.store(position);
    }

    int64_t TransportState::getPlaybackPositionInternal() const noexcept
    {
        if (!playing.load())
            return currentSamplePosition.load();

        const double sampleRateValue = sampleRate.load();
        const double currentTimeMs = juce::Time::getMillisecondCounterHiRes();
        const double elapsedMs = currentTimeMs - playStartTimeMs.load();
        const double samplesElapsed = elapsedMs * (sampleRateValue / 1000.0);
        int64_t position = lastClockSamplePosition.load() + static_cast<int64_t>(std::floor(samplesElapsed));

        if (isLooping())
        {
            const int64_t start = loopStartSample.load();
            const int64_t end = loopEndSample.load();
            const int64_t length = end - start;
            if (length > 0)
            {
                const int64_t offset = (position - start) % length;
                position = start + (offset < 0 ? offset + length : offset);
            }
        }

        return position;
    }

    int64_t TransportState::getLoopStartSample() const noexcept
    {
        return loopStartSample.load();
    }

    int64_t TransportState::getLoopEndSample() const noexcept
    {
        return loopEndSample.load();
    }

    juce::String TransportState::getTimecodeString(int64_t samplePosition) const
    {
        const double seconds = static_cast<double>(samplePosition) / sampleRate.load();
        const int hours = static_cast<int>(seconds / 3600.0);
        const int minutes = static_cast<int>(std::fmod(seconds / 60.0, 60.0));
        const int secs = static_cast<int>(std::fmod(seconds, 60.0));
        const int millis = static_cast<int>((seconds - std::floor(seconds)) * 1000.0);
        return juce::String::formatted("%02d:%02d:%02d.%03d", hours, minutes, secs, millis);
    }

    bool TransportState::isPlaying() const noexcept
    {
        return playing.load();
    }

    juce::String TransportState::getBarBeatString(int64_t samplePosition) const
    {
        const double seconds = static_cast<double>(samplePosition) / sampleRate.load();
        const double beatsPerSecond = tempoBpm.load() / 60.0;
        const double totalBeats = seconds * beatsPerSecond;
        const int bar = static_cast<int>(totalBeats / 4.0) + 1;
        const int beat = (static_cast<int>(std::floor(totalBeats)) % 4) + 1;
        const int subbeat = static_cast<int>((totalBeats - std::floor(totalBeats)) * 1000.0);
        return juce::String::formatted("%d.%d.%03d", bar, beat, subbeat);
    }
}
