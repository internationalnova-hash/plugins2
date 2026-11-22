#include "NovaRideEngine.h"

namespace nova::ride
{
    namespace
    {
        constexpr float kBypassRampSeconds = 0.02f; // 20ms equal-power crossfade
        constexpr float kOutputTrimRampSeconds = 0.02f;
        constexpr float kHalfPi = juce::MathConstants<float>::halfPi;

        float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
    }

    void NovaRideEngine::prepare (double newSampleRate, int maxBlockSize)
    {
        juce::ignoreUnused (maxBlockSize);
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

        detector.prepare (sampleRate);
        sidechainAnalyzer.prepare (sampleRate);
        gainEngine.prepare (sampleRate);
        automationBuffer.prepare (sampleRate);

        outputTrimLinear.reset (sampleRate, kOutputTrimRampSeconds);
        outputTrimLinear.setCurrentAndTargetValue (1.0f);
        bypassAmount.reset (sampleRate, kBypassRampSeconds);
        bypassAmount.setCurrentAndTargetValue (0.0f);

        reset();
        setParams (currentParams);
    }

    void NovaRideEngine::reset() noexcept
    {
        detector.reset();
        sidechainAnalyzer.reset();
        gainEngine.reset();
        automationBuffer.reset();
        elapsedSeconds = 0.0;
        inputMeterDb = kMinDb;
        outputMeterDb = kMinDb;
        telemetry = Telemetry {};
    }

    void NovaRideEngine::setParams (const Params& newParams) noexcept
    {
        currentParams = newParams;

        const auto& mode = modeTimingsFor (currentParams.modeIndex);
        detector.setParams (mode, currentParams.speedNorm, currentParams.sensitivityNorm);
        gainEngine.setParams (mode, currentParams.speedNorm, currentParams.sensitivityNorm, currentParams.rangeDb);

        outputTrimLinear.setTargetValue (juce::Decibels::decibelsToGain (currentParams.outputDb, -100.0f));
        bypassAmount.setTargetValue (currentParams.bypassed ? 1.0f : 0.0f);
    }

    void NovaRideEngine::processBlock (juce::AudioBuffer<float>& mainBuffer,
                                       const juce::AudioBuffer<float>& sidechainBuffer,
                                       bool sidechainConnected) noexcept
    {
        const int numSamples = mainBuffer.getNumSamples();
        const int numMainChannels = mainBuffer.getNumChannels();
        const int numSidechainChannels = sidechainBuffer.getNumChannels();

        float blockInPeakLin = 0.0f;
        float blockOutPeakLin = 0.0f;
        bool lastGateOpen = telemetry.gateOpen;
        float lastCorrectionDb = gainEngine.getCorrectionDb();
        float lastSidechainPresence = telemetry.sidechainPresence;

        for (int i = 0; i < numSamples; ++i)
        {
            // Mono-sum the main bus for detection purposes (the correction
            // itself is applied identically to every channel below).
            float monoSample = 0.0f;
            for (int ch = 0; ch < numMainChannels; ++ch)
                monoSample += mainBuffer.getReadPointer (ch)[i];
            if (numMainChannels > 0)
                monoSample /= static_cast<float> (numMainChannels);

            blockInPeakLin = juce::jmax (blockInPeakLin, std::abs (monoSample));

            float scMonoSample = 0.0f;
            if (sidechainConnected && numSidechainChannels > 0)
            {
                for (int ch = 0; ch < numSidechainChannels; ++ch)
                    scMonoSample += sidechainBuffer.getReadPointer (ch)[i];
                scMonoSample /= static_cast<float> (numSidechainChannels);
            }

            const auto detectorResult = detector.processSample (monoSample);
            const auto sidechainResult = sidechainAnalyzer.processSample (scMonoSample, sidechainConnected);

            float effectiveTargetDb = currentParams.targetDb;
            if (currentParams.musicEnabled)
            {
                const float blend = sidechainResult.sidechainPresence;
                effectiveTargetDb = lerp (currentParams.targetDb, sidechainResult.musicLevelDb + currentParams.targetDb, blend);
            }

            const float rideLinearGain = gainEngine.process (detectorResult.levelDb, detectorResult.gateOpen,
                                                              detectorResult.fastEnvRising,
                                                              detectorResult.breathLikelihood, effectiveTargetDb);
            const float trimLinear = outputTrimLinear.getNextValue();

            const float bypassAmt = bypassAmount.getNextValue();
            const float wetMix = std::cos (bypassAmt * kHalfPi);
            const float dryMix = std::sin (bypassAmt * kHalfPi);

            float outSampleForMeter = 0.0f;
            for (int ch = 0; ch < numMainChannels; ++ch)
            {
                auto* data = mainBuffer.getWritePointer (ch);
                // Sanitize immediately: dry is reused below in the bypass
                // blend, and NaN * 0.0 is still NaN in IEEE754, so a raw NaN
                // input sample would survive even a fully-wet (dryMix==0)
                // blend if left unsanitized here.
                float dry = data[i];
                if (! std::isfinite (dry))
                    dry = 0.0f;

                float wet = dry * rideLinearGain * trimLinear;
                if (! std::isfinite (wet))
                    wet = 0.0f;
                const float out = wet * wetMix + dry * dryMix;
                data[i] = out;
                outSampleForMeter += out;
            }
            if (numMainChannels > 0)
                outSampleForMeter /= static_cast<float> (numMainChannels);
            blockOutPeakLin = juce::jmax (blockOutPeakLin, std::abs (outSampleForMeter));

            automationBuffer.pushIfEngaged (currentParams.writeEnabled, elapsedSeconds, gainEngine.getCorrectionDb());

            lastGateOpen = detectorResult.gateOpen;
            lastCorrectionDb = gainEngine.getCorrectionDb();
            lastSidechainPresence = sidechainResult.sidechainPresence;
            elapsedSeconds += 1.0 / sampleRate;
        }

        const float inPeakDb = juce::Decibels::gainToDecibels (blockInPeakLin, kMinDb);
        const float outPeakDb = juce::Decibels::gainToDecibels (blockOutPeakLin, kMinDb);
        inputMeterDb = kMeterHoldCoeff * inputMeterDb + kMeterRiseCoeff * inPeakDb;
        outputMeterDb = kMeterHoldCoeff * outputMeterDb + kMeterRiseCoeff * outPeakDb;

        telemetry.inputLevelDb = inputMeterDb;
        telemetry.outputLevelDb = outputMeterDb;
        telemetry.rideCorrectionDb = lastCorrectionDb;
        telemetry.gateOpen = lastGateOpen;
        telemetry.sidechainConnected = sidechainConnected;
        telemetry.sidechainPresence = lastSidechainPresence;
    }
}
