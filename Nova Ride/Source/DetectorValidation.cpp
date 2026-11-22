// Headless validation for NovaRideDetector + NovaRideSidechainAnalyzer.
// No PluginProcessor, no host - synthetic signals only. See CMakeLists.txt's
// NovaRideDetectorValidation target (links only these two .cpp files).
#include <cmath>
#include <cstdio>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Detection/NovaRideDetector.h"
#include "Detection/NovaRideSidechainAnalyzer.h"
#include "NovaRideConstants.h"

using namespace nova::ride;

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& label)
    {
        if (condition)
            std::printf ("PASS: %s\n", label.toRawUTF8());
        else
        {
            std::printf ("FAIL: %s\n", label.toRawUTF8());
            ++failures;
        }
    }

    void section (const char* title)
    {
        std::printf ("\n=== %s ===\n", title);
    }

    // Generates numSamples of a sine wave at the given peak dBFS.
    std::vector<float> makeSine (double sampleRate, double freqHz, float dBFS, int numSamples)
    {
        std::vector<float> out (static_cast<size_t> (numSamples));
        const float amp = juce::Decibels::decibelsToGain (dBFS, -100.0f);
        for (int i = 0; i < numSamples; ++i)
            out[static_cast<size_t> (i)] = amp * std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * i / sampleRate);
        return out;
    }

    std::vector<float> makeSilence (int numSamples)
    {
        return std::vector<float> (static_cast<size_t> (numSamples), 0.0f);
    }

    bool allFinite (const std::vector<NovaRideDetector::Result>& results)
    {
        for (const auto& r : results)
            if (! std::isfinite (r.levelDb) || ! std::isfinite (r.activityConfidence) || ! std::isfinite (r.breathLikelihood))
                return false;
        return true;
    }

    std::vector<NovaRideDetector::Result> runDetector (NovaRideDetector& d, const std::vector<float>& samples)
    {
        std::vector<NovaRideDetector::Result> results;
        results.reserve (samples.size());
        for (float s : samples)
            results.push_back (d.processSample (s));
        return results;
    }

    void testSilenceProducesNoActivity()
    {
        section ("Silence produces no activity");
        const double sr = 48000.0;
        NovaRideDetector detector;
        detector.prepare (sr);
        detector.setParams (kNaturalMode, 0.5f, 0.5f);

        auto results = runDetector (detector, makeSilence (static_cast<int> (sr))); // 1s
        bool anyGateOpen = false;
        for (const auto& r : results)
            if (r.gateOpen) anyGateOpen = true;

        check (! anyGateOpen, "gate never opens on pure silence");
        check (allFinite (results), "all results finite during silence");
        check (results.back().levelDb < -40.0f, "settled level reads very low, not chasing upward");
    }

    void testSteadySineOpensGateAndTracksLevel()
    {
        section ("Steady sine opens gate and tracks level");
        const double sr = 48000.0;
        NovaRideDetector detector;
        detector.prepare (sr);
        detector.setParams (kNaturalMode, 0.5f, 0.5f);

        auto samples = makeSine (sr, 220.0, -12.0f, static_cast<int> (sr * 2)); // 2s @ -12dBFS
        auto results = runDetector (detector, samples);

        check (results.back().gateOpen, "gate opens and stays open on a steady tone");
        // RMS of a sine is ~-3.01dB below its peak dBFS; our detector tracks
        // rectified-average envelope of |sample|, whose relationship to peak
        // dB is a further ~-3.9dB (mean(|sin|) = 2/pi) - so expect roughly
        // -12 - 3.9 =~ -15.9dB, allow a generous window either side.
        check (results.back().levelDb > -22.0f && results.back().levelDb < -10.0f,
               "tracked level is in a sane range relative to a -12dBFS tone (got "
                   + juce::String (results.back().levelDb, 2) + " dB)");
        check (allFinite (results), "all results finite on steady sine");
    }

    void testQuietToLoudPhraseTransition()
    {
        section ("Quiet-to-loud phrase transition");
        const double sr = 48000.0;
        NovaRideDetector detector;
        detector.prepare (sr);
        detector.setParams (kNaturalMode, 0.5f, 0.5f);

        auto quiet = makeSine (sr, 220.0, -30.0f, static_cast<int> (sr * 1));
        auto loud = makeSine (sr, 220.0, -6.0f, static_cast<int> (sr * 1));

        auto quietResults = runDetector (detector, quiet);
        auto loudResults = runDetector (detector, loud);

        check (loudResults.back().levelDb > quietResults.back().levelDb + 10.0f,
               "level clearly rises after the loud transition");
        check (loudResults.back().gateOpen, "gate is open once the phrase is clearly audible");
        check (allFinite (loudResults), "finite through the transition");
    }

    void testLoudToQuietPhraseTransition()
    {
        section ("Loud-to-quiet phrase transition");
        const double sr = 48000.0;
        NovaRideDetector detector;
        detector.prepare (sr);
        detector.setParams (kNaturalMode, 0.5f, 0.5f);

        auto loud = makeSine (sr, 220.0, -6.0f, static_cast<int> (sr * 1));
        auto quiet = makeSine (sr, 220.0, -30.0f, static_cast<int> (sr * 1));

        auto loudResults = runDetector (detector, loud);
        auto quietResults = runDetector (detector, quiet);

        check (quietResults.back().levelDb < loudResults.back().levelDb - 10.0f,
               "level clearly falls after the quiet transition");
        check (allFinite (quietResults), "finite through the transition");
    }

    void testSilenceBetweenPhrasesClosesGateWithHysteresis()
    {
        section ("Silence between phrases closes the gate (with hold/hysteresis)");
        const double sr = 48000.0;
        NovaRideDetector detector;
        detector.prepare (sr);
        detector.setParams (kNaturalMode, 0.5f, 0.5f);

        auto phrase1 = makeSine (sr, 220.0, -10.0f, static_cast<int> (sr * 0.5));
        auto gap = makeSilence (static_cast<int> (sr * 1.5)); // well past hold time
        auto phrase2 = makeSine (sr, 220.0, -10.0f, static_cast<int> (sr * 0.5));

        runDetector (detector, phrase1);
        auto gapResults = runDetector (detector, gap);
        auto phrase2Results = runDetector (detector, phrase2);

        check (! gapResults.back().gateOpen, "gate closes during a real silence gap");
        check (phrase2Results.back().gateOpen, "gate reopens for the next phrase");
    }

    void testBreathLikeLowLevelEventSuppressed()
    {
        section ("Breath-like short low-level event is flagged, sustained quiet phrase is not (by the end)");
        const double sr = 48000.0;

        auto peakBreathLikelihood = [] (const std::vector<NovaRideDetector::Result>& results)
        {
            float peak = 0.0f;
            for (const auto& r : results) peak = juce::jmax (peak, r.breathLikelihood);
            return peak;
        };

        NovaRideDetector detector;
        detector.prepare (sr);
        detector.setParams (kNaturalMode, 0.5f, 0.5f);

        // Short burst just above the gate-open threshold, ~180ms - under
        // kBreathConfirmMs (200ms), long enough for the ~300ms medium
        // envelope to clearly cross the gate-open threshold.
        auto breath = makeSine (sr, 220.0, -32.0f, static_cast<int> (sr * 0.18));
        auto breathResults = runDetector (detector, breath);
        const float breathPeak = peakBreathLikelihood (breathResults);

        detector.reset();
        // Sustained quiet phrase at the same level for 1.5s - long past the
        // breath-confirm window, so breathLikelihood should have peaked
        // early and decayed back down well before the end.
        auto sustained = makeSine (sr, 220.0, -32.0f, static_cast<int> (sr * 1.5));
        auto sustainedResults = runDetector (detector, sustained);

        check (breathPeak > 0.3f, "a short low-level burst reaches a real peak breath-likelihood (got "
                                       + juce::String (breathPeak, 3) + ")");
        check (sustainedResults.back().breathLikelihood < breathPeak,
               "a sustained quiet phrase's breath-likelihood has decayed back down by the end, well below the short burst's peak ("
                   + juce::String (sustainedResults.back().breathLikelihood, 3) + " vs " + juce::String (breathPeak, 3) + ")");
    }

    void testSensitivityChangesResponsiveness()
    {
        section ("Sensitivity changes gate responsiveness");
        const double sr = 48000.0;
        auto testSignal = makeSine (sr, 220.0, -24.0f, static_cast<int> (sr * 0.3));

        NovaRideDetector lowSens;
        lowSens.prepare (sr);
        lowSens.setParams (kNaturalMode, 0.5f, 0.0f);
        auto lowResults = runDetector (lowSens, testSignal);

        NovaRideDetector highSens;
        highSens.prepare (sr);
        highSens.setParams (kNaturalMode, 0.5f, 1.0f);
        auto highResults = runDetector (highSens, testSignal);

        auto firstOpenIndex = [] (const std::vector<NovaRideDetector::Result>& r) -> int
        {
            for (size_t i = 0; i < r.size(); ++i)
                if (r[i].gateOpen) return static_cast<int> (i);
            return -1;
        };

        const int lowOpenAt = firstOpenIndex (lowResults);
        const int highOpenAt = firstOpenIndex (highResults);

        check (highOpenAt >= 0, "high sensitivity opens the gate for a moderate-level signal");
        check (lowOpenAt < 0 || highOpenAt <= lowOpenAt,
               "higher sensitivity opens at least as quickly as lower sensitivity");
    }

    void testNanInfInputResilience()
    {
        section ("NaN/Inf input resilience");
        const double sr = 48000.0;
        NovaRideDetector detector;
        detector.prepare (sr);
        detector.setParams (kNaturalMode, 0.5f, 0.5f);

        std::vector<float> nasty (2000, 0.0f);
        for (size_t i = 0; i < nasty.size(); i += 3) nasty[i] = std::numeric_limits<float>::quiet_NaN();
        for (size_t i = 1; i < nasty.size(); i += 3) nasty[i] = std::numeric_limits<float>::infinity();
        for (size_t i = 2; i < nasty.size(); i += 3) nasty[i] = -std::numeric_limits<float>::infinity();

        auto results = runDetector (detector, nasty);
        check (allFinite (results), "detector output stays finite when fed NaN/Inf input");

        // Should recover cleanly on real signal afterward.
        auto recovery = runDetector (detector, makeSine (sr, 220.0, -10.0f, static_cast<int> (sr * 0.5)));
        check (allFinite (recovery), "detector recovers to finite output on real signal after NaN/Inf");
        check (recovery.back().gateOpen, "detector resumes normal gating after recovering from NaN/Inf");
    }

    void testSampleRateIndependence()
    {
        section ("Sample-rate independence");
        const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
        for (double sr : rates)
        {
            NovaRideDetector detector;
            detector.prepare (sr);
            detector.setParams (kNaturalMode, 0.5f, 0.5f);

            auto samples = makeSine (sr, 220.0, -10.0f, static_cast<int> (sr * 1.0)); // 1 second, real time
            auto results = runDetector (detector, samples);

            check (results.back().gateOpen, "gate opens within 1s of a clear tone @ " + juce::String (sr, 0) + "Hz");
            check (allFinite (results), "finite output @ " + juce::String (sr, 0) + "Hz");
        }
    }

    void testResetClearsState()
    {
        section ("reset() clears state");
        const double sr = 48000.0;
        NovaRideDetector detector;
        detector.prepare (sr);
        detector.setParams (kNaturalMode, 0.5f, 0.5f);

        runDetector (detector, makeSine (sr, 220.0, -6.0f, static_cast<int> (sr * 1)));
        detector.reset();

        auto result = detector.processSample (0.0f);
        check (! result.gateOpen, "gate reads closed immediately after reset");
        check (result.levelDb < -40.0f, "level reads very low immediately after reset");
    }

    // ---- Sidechain analyzer ----

    void testSidechainConnectedVsDisconnected()
    {
        section ("Sidechain analyzer: connected vs disconnected");
        const double sr = 48000.0;
        auto music = makeSine (sr, 440.0, -12.0f, static_cast<int> (sr * 1));

        NovaRideSidechainAnalyzer connected;
        connected.prepare (sr);
        NovaRideSidechainAnalyzer::Result lastConnected;
        for (float s : music) lastConnected = connected.processSample (s, true);

        NovaRideSidechainAnalyzer disconnected;
        disconnected.prepare (sr);
        NovaRideSidechainAnalyzer::Result lastDisconnected;
        for (float s : music) lastDisconnected = disconnected.processSample (s, false);

        check (lastConnected.sidechainPresence > 0.5f, "presence ramps up when connected and carrying real signal");
        check (lastDisconnected.sidechainPresence < 0.1f, "presence stays near zero when the bus isn't connected, even with the same signal");
        check (std::isfinite (lastConnected.musicLevelDb) && std::isfinite (lastDisconnected.musicLevelDb), "levels stay finite either way");
    }
}

int main()
{
    testSilenceProducesNoActivity();
    testSteadySineOpensGateAndTracksLevel();
    testQuietToLoudPhraseTransition();
    testLoudToQuietPhraseTransition();
    testSilenceBetweenPhrasesClosesGateWithHysteresis();
    testBreathLikeLowLevelEventSuppressed();
    testSensitivityChangesResponsiveness();
    testNanInfInputResilience();
    testSampleRateIndependence();
    testResetClearsState();
    testSidechainConnectedVsDisconnected();

    std::printf ("\n  NovaRideDetectorValidation: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
