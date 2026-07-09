#pragma once

#include <JuceHeader.h>
#include "Session.h"
#include "TransportState.h"

namespace NovaStudio
{
    class TimelineModel
    {
    public:
        TimelineModel(const Session& session, const TransportState& transportState);

        void setZoomFactor(double zoom) noexcept;
        double getZoomFactor() const noexcept;

        void  setViewStartSamples(int64_t s) noexcept { viewStartSamples = juce::jmax<int64_t>(0, s); }
        int64_t getViewStartSamples() const noexcept  { return viewStartSamples; }
        void scrollByPixels(double deltaPixels) noexcept;  // positive = scroll right

        double getPixelsPerBeat() const noexcept;
        double getPixelsPerSecond() const noexcept;

        int64_t secondsToSamples(double seconds) const noexcept;
        double samplesToSeconds(int64_t samples) const noexcept;
        int getBeatsPerBar() const noexcept;

        double getVisibleRangeSeconds(int width) const noexcept;
        double getSamplePositionForX(int x) const noexcept;
        double getXForSamplePosition(int64_t samplePosition, int width) const noexcept;

        juce::String getTimecodeString(int64_t samplePosition) const;
        juce::String getBarBeatString(int64_t samplePosition) const;

        juce::Array<int64_t> getMarkerPositions() const;
        int64_t getLoopStartSample() const noexcept;
        int64_t getLoopEndSample() const noexcept;
        bool isLooping() const noexcept;

        double getSampleRate() const noexcept;
        int getTempo() const noexcept;

    private:
        const Session& session;
        const TransportState& transportState;
        double zoomFactor = 1.0;
        int64_t viewStartSamples = 0;
    };
}
