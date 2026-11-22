#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace nova::ride
{
    struct RideBreakpoint
    {
        double timeSeconds = 0.0;
        float correctionDb = 0.0f;
    };

    // Captures Nova Ride's generated gain trajectory while WRITE is engaged,
    // for export as an honest, tool-agnostic breakpoint file (plain
    // time/dB CSV) - not a claim of writing DAW automation lanes, which
    // hosts don't generically expose to a VST3/AU plugin. Reusable as-is
    // for a future Nova Studio integration that writes ride data directly
    // into native track/clip automation.
    //
    // Fixed-capacity, pre-reserved in prepare() (the only allocation) -
    // pushIfEngaged() is audio-thread-safe (classic single-producer/
    // single-consumer: monotonically-increasing atomic write index, no
    // locks, no reallocation).
    class NovaRideAutomationBuffer
    {
    public:
        NovaRideAutomationBuffer() = default;

        void prepare (double sampleRate, double maxDurationMinutes = 30.0);
        void reset() noexcept;

        // Audio thread. Call once per block. Throttled internally (~50ms or
        // >0.1dB change) so it doesn't grow unboundedly fast. Resets/starts
        // a fresh capture on the off->on transition of writeEnabled.
        void pushIfEngaged (bool writeEnabled, double timeSeconds, float correctionDb) noexcept;

        // Message thread only.
        int getNumPoints() const noexcept { return writeIndex.load(); }
        RideBreakpoint getPoint (int index) const noexcept;
        bool exportToCsv (const juce::File& destination) const;

    private:
        std::vector<RideBreakpoint> points;
        std::atomic<int> writeIndex { 0 };
        int capacity = 0;

        bool wasEngaged = false;
        double lastPushedTimeSeconds = -1.0;
        float lastPushedCorrectionDb = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRideAutomationBuffer)
    };
}
