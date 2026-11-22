#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "NovaRideConstants.h"
#include "Detection/NovaRideDetector.h"
#include "Detection/NovaRideSidechainAnalyzer.h"
#include "Engine/NovaRideGainEngine.h"
#include "RideHistory/NovaRideAutomationBuffer.h"

namespace nova::ride
{
    // Top-level DSP orchestrator: owns one Detector (vocal), one
    // SidechainAnalyzer (music), one GainEngine, an AutomationBuffer, plus
    // the equal-power bypass crossfade and output trim. This is exactly
    // what PluginProcessor::processBlock calls, and what
    // NovaRideEngineValidation links/tests headlessly with no
    // PluginProcessor/host involved.
    class NovaRideEngine
    {
    public:
        NovaRideEngine() = default;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        struct Params
        {
            float targetDb = 0.0f;
            float rangeDb = 6.0f;
            float speedNorm = 0.5f;
            float sensitivityNorm = 0.5f;
            float outputDb = 0.0f;
            int   modeIndex = 0; // 0 = Natural, 1 = Tight, 2 = Live
            bool  musicEnabled = false;
            bool  writeEnabled = false;
            bool  bypassed = false;
        };

        void setParams (const Params& newParams) noexcept;

        struct Telemetry
        {
            float inputLevelDb = kMinDb;
            float outputLevelDb = kMinDb;
            float rideCorrectionDb = 0.0f;
            bool  gateOpen = false;
            bool  sidechainConnected = false; // host has the sidechain bus wired/enabled
            float sidechainPresence = 0.0f;   // 0..1, whether it's actually carrying real signal right now
        };

        // mainBuffer: read/write in place, the vocal signal.
        // sidechainBuffer: read-only music bus (may legitimately have 0
        // channels if the host hasn't connected/enabled it).
        // sidechainConnected: whether the host has the sidechain bus
        // enabled at all (independent of whether it's currently carrying
        // audible signal - that's sidechainAnalyzer's job).
        void processBlock (juce::AudioBuffer<float>& mainBuffer,
                           const juce::AudioBuffer<float>& sidechainBuffer,
                           bool sidechainConnected) noexcept;

        const Telemetry& getTelemetry() const noexcept { return telemetry; }
        double getElapsedSeconds() const noexcept { return elapsedSeconds; }

        NovaRideAutomationBuffer& getAutomationBuffer() noexcept { return automationBuffer; }
        const NovaRideAutomationBuffer& getAutomationBuffer() const noexcept { return automationBuffer; }

    private:
        NovaRideDetector detector;
        NovaRideSidechainAnalyzer sidechainAnalyzer;
        NovaRideGainEngine gainEngine;
        NovaRideAutomationBuffer automationBuffer;

        juce::SmoothedValue<float> outputTrimLinear;
        juce::SmoothedValue<float> bypassAmount; // 0 = fully processed, 1 = fully bypassed (dry)

        double sampleRate = 44100.0;
        double elapsedSeconds = 0.0;

        float inputMeterDb = kMinDb;
        float outputMeterDb = kMinDb;

        Params currentParams;
        Telemetry telemetry;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaRideEngine)
    };
}
