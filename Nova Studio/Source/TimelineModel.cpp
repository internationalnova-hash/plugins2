#include "TimelineModel.h"

namespace NovaStudio
{
    TimelineModel::TimelineModel(const Session& sessionRef, const TransportState& transportRef)
        : session(sessionRef), transportState(transportRef)
    {
    }

    void TimelineModel::setZoomFactor(double zoom) noexcept
    {
        zoomFactor = juce::jmax(0.2, juce::jmin(zoom, 6.0));
    }

    double TimelineModel::getZoomFactor() const noexcept
    {
        return zoomFactor;
    }

    double TimelineModel::getPixelsPerBeat() const noexcept
    {
        return 80.0 * zoomFactor;
    }

    double TimelineModel::getPixelsPerSecond() const noexcept
    {
        const double beatsPerSecond = transportState.getTempo() / 60.0;
        return getPixelsPerBeat() * beatsPerSecond;
    }

    int64_t TimelineModel::secondsToSamples(double seconds) const noexcept
    {
        return static_cast<int64_t>(seconds * transportState.getSampleRate());
    }

    double TimelineModel::samplesToSeconds(int64_t samples) const noexcept
    {
        return static_cast<double>(samples) / transportState.getSampleRate();
    }

    int TimelineModel::getBeatsPerBar() const noexcept
    {
        return 4;
    }

    double TimelineModel::getVisibleRangeSeconds(int width) const noexcept
    {
        return width / getPixelsPerSecond();
    }

    double TimelineModel::getSamplePositionForX(int x) const noexcept
    {
        const double seconds = x / getPixelsPerSecond();
        return viewStartSamples + secondsToSamples(seconds);
    }

    double TimelineModel::getXForSamplePosition(int64_t samplePosition, int width) const noexcept
    {
        const double seconds = samplesToSeconds(samplePosition - viewStartSamples);
        return seconds * getPixelsPerSecond();
    }

    void TimelineModel::scrollByPixels(double deltaPixels) noexcept
    {
        const double deltaSamples = deltaPixels / getPixelsPerSecond() * transportState.getSampleRate();
        viewStartSamples = juce::jmax<int64_t>(0, viewStartSamples + static_cast<int64_t>(deltaSamples));
    }

    juce::String TimelineModel::getTimecodeString(int64_t samplePosition) const
    {
        return transportState.getTimecodeString(samplePosition);
    }

    juce::String TimelineModel::getBarBeatString(int64_t samplePosition) const
    {
        return transportState.getBarBeatString(samplePosition);
    }

    double TimelineModel::getSampleRate() const noexcept
    {
        return transportState.getSampleRate();
    }

    int TimelineModel::getTempo() const noexcept
    {
        return transportState.getTempo();
    }

    juce::Array<int64_t> TimelineModel::getMarkerPositions() const
    {
        juce::Array<int64_t> positions;
        for (int i = 0; i < session.getNumMarkers(); ++i)
            positions.add(secondsToSamples(session.getMarker(i).timeSeconds));
        return positions;
    }

    int64_t TimelineModel::getLoopStartSample() const noexcept
    {
        return transportState.getLoopStartSample();
    }

    int64_t TimelineModel::getLoopEndSample() const noexcept
    {
        return transportState.getLoopEndSample();
    }

    bool TimelineModel::isLooping() const noexcept
    {
        return transportState.isLooping();
    }
}
