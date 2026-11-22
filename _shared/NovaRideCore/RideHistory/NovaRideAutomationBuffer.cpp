#include "NovaRideAutomationBuffer.h"

namespace nova::ride
{
    namespace
    {
        constexpr double kPushIntervalSeconds = 0.05;
        constexpr float kPushLevelThresholdDb = 0.1f;
    }

    void NovaRideAutomationBuffer::prepare (double sampleRate, double maxDurationMinutes)
    {
        juce::ignoreUnused (sampleRate);
        // Worst case ~1 point per kPushIntervalSeconds, plus headroom.
        const double pointsPerSecond = 1.0 / kPushIntervalSeconds;
        capacity = juce::jmax (1024, static_cast<int> (maxDurationMinutes * 60.0 * pointsPerSecond));
        points.assign (static_cast<size_t> (capacity), RideBreakpoint {});
        reset();
    }

    void NovaRideAutomationBuffer::reset() noexcept
    {
        writeIndex.store (0);
        wasEngaged = false;
        lastPushedTimeSeconds = -1.0;
        lastPushedCorrectionDb = 0.0f;
    }

    void NovaRideAutomationBuffer::pushIfEngaged (bool writeEnabled, double timeSeconds, float correctionDb) noexcept
    {
        if (! writeEnabled)
        {
            wasEngaged = false;
            return;
        }

        if (! wasEngaged)
        {
            writeIndex.store (0);
            lastPushedTimeSeconds = -1.0;
            wasEngaged = true;
        }

        const bool timeThresholdMet = (lastPushedTimeSeconds < 0.0) || ((timeSeconds - lastPushedTimeSeconds) >= kPushIntervalSeconds);
        const bool levelThresholdMet = std::abs (correctionDb - lastPushedCorrectionDb) >= kPushLevelThresholdDb;
        if (! timeThresholdMet && ! levelThresholdMet)
            return;

        const int idx = writeIndex.load();
        if (idx >= capacity)
            return; // fixed capacity reached - stop appending rather than reallocate on the audio thread

        points[static_cast<size_t> (idx)] = { timeSeconds, correctionDb };
        writeIndex.store (idx + 1);

        lastPushedTimeSeconds = timeSeconds;
        lastPushedCorrectionDb = correctionDb;
    }

    RideBreakpoint NovaRideAutomationBuffer::getPoint (int index) const noexcept
    {
        if (index < 0 || index >= writeIndex.load() || index >= capacity)
            return {};
        return points[static_cast<size_t> (index)];
    }

    bool NovaRideAutomationBuffer::exportToCsv (const juce::File& destination) const
    {
        juce::String csv;
        csv << "time_seconds,gain_db\n";
        const int n = writeIndex.load();
        for (int i = 0; i < n; ++i)
        {
            const auto& p = points[static_cast<size_t> (i)];
            csv << juce::String (p.timeSeconds, 4) << "," << juce::String (p.correctionDb, 3) << "\n";
        }
        return destination.replaceWithText (csv);
    }
}
