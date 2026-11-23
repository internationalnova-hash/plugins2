#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "Session.h"
#include "ClipStreamer.h"

namespace NovaStudio
{
    // Read-only copy of everything the audio thread needs to play the arrangement.
    // Built on the message thread from the Session whenever it changes, then published with
    // an atomic pointer swap. The audio thread never touches the Session itself, so editing
    // while playing can't race with playback.
    struct PlayClip
    {
        std::shared_ptr<ClipStreamer> src;
        int64_t start = 0;        // timeline (session-rate samples)
        int64_t length = 0;
        int64_t srcStart = 0;     // position in the file at clip start, in session-rate samples
        float   gain = 1.0f;
        int64_t fadeIn = 0, fadeOut = 0;
        int64_t fadeOffset = 0;   // this segment's offset into the original clip (for fades)
        int64_t fullLength = 0;   // the original clip's length
    };

    struct PlayNote
    {
        int64_t on = 0, off = 0;  // timeline samples
        int pitch = 60;
        juce::uint8 velocity = 100;
    };

    struct TrackPlayData
    {
        std::vector<PlayClip> clips;   // sorted by start
        std::vector<PlayNote> notes;   // sorted by on
        std::vector<AutomationLane> lanes;
        int automationMode = 0;        // 0 Read, 1 Touch, 2 Latch, 3 Write, 4 Off
    };

    struct PlaySnapshot
    {
        std::vector<TrackPlayData> tracks;
        double sampleRate = 44100.0;
        double baseBpm = 120.0;
        std::vector<std::pair<double, double>> tempoMap;   // (seconds, bpm)
        int timeSigNum = 4, timeSigDen = 4;
        uint64_t hash = 0;

        double bpmAt(double seconds) const noexcept
        {
            double bpm = baseBpm;
            for (auto& p : tempoMap) { if (p.first > seconds) break; bpm = p.second; }
            return bpm;
        }

        // Beats here are metronome beats (quarter notes scaled by the time-signature denominator).
        double secondsToBeats(double seconds) const noexcept
        {
            double beats = 0.0, t = 0.0, bpm = baseBpm;
            for (auto& p : tempoMap)
            {
                if (p.first >= seconds) break;
                if (p.first > t) { beats += (p.first - t) * bpm / 60.0; t = p.first; }
                bpm = p.second;
            }
            return (beats + (seconds - t) * bpm / 60.0) * (timeSigDen / 4.0);
        }

        double beatsToSeconds(double beats) const noexcept
        {
            beats /= (timeSigDen / 4.0);
            double b = 0.0, t = 0.0, bpm = baseBpm;
            for (auto& p : tempoMap)
            {
                if (p.first <= t) { bpm = p.second; continue; }
                const double seg = (p.first - t) * bpm / 60.0;
                if (b + seg >= beats) break;
                b += seg; t = p.first; bpm = p.second;
            }
            return t + (beats - b) * 60.0 / bpm;
        }
    };
}
