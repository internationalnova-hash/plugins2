#pragma once

// Mix Assistant, part 1: listens to every track and works out what it is.
// Reads each track's clips straight from disk (what was recorded, not what
// the plugins currently do to it), measures it, and decides a role with a
// plain-words reason and a confidence. Vocals are then sorted into lead,
// doubles, harmonies and ad-libs by comparing their pitch with the lead.

#include <JuceHeader.h>
#include "../Session.h"
#include <functional>
#include <vector>

namespace NovaStudio::MixAI
{
    enum class Role
    {
        Lead, Double, Harmony, Adlib,           // vocals
        Kick, Snare, Clap, HiHat, Perc,         // drums
        Bass,                                   // 808 / bass
        Keys, Guitar, Pad, Beat,                // music (Beat = a 2-track instrumental)
        Fx, Other, Skip
    };
    enum class Family { Vocals, Drums, Bass, Music, Fx, Other };

    const char* roleName (Role r) noexcept;
    Family familyOf (Role r) noexcept;
    juce::Colour roleColour (Role r) noexcept;
    juce::StringArray allRoleNames();
    Role roleFromName (const juce::String& name) noexcept;

    struct Features
    {
        float seconds = 0, activeFrac = 0, loudDb = -120, peakDb = -120;
        float band[6] {};            // energy share: <60, 60-250, 250-1k, 1k-4k, 4k-8k, >8k Hz
        float centroidHz = 0, onsetRate = 0, decayMs = 0, sustain = 0;
        float voicedFrac = 0, pitchMedian = 0, pitchWobble = 0;
        float onBeat = 0, backbeat = 0, width = 0;
        int   midiNotes = 0;
        float midiAvgPitch = 0, midiAvgBeats = 0;
    };

    struct TrackAnalysis
    {
        int trackIndex = -1;
        juce::String name;
        bool midi = false, stereo = false;
        Features f;
        Role role = Role::Other;
        float confidence = 0;
        juce::String heard;              // short reason, e.g. "sung - 60% of the song"
        juce::String clipHint;           // first clip's file name (a naming hint when the track is "Audio 1")
        std::vector<float> env;          // frame level, dB (46 ms frames)
        std::vector<float> pitch;        // frame pitch, MIDI (0 = unvoiced)
    };

    struct SessionAnalysis
    {
        double seconds = 0, bpm = 120;
        int keyTonic = 0;
        bool keyMinor = false, keyKnown = false;
        std::vector<TrackAnalysis> tracks;
    };

    constexpr double kRate = 22050.0;    // analysis sample rate
    constexpr int kHop = 1024;           // ~46 ms frames

    /// Listens to every audio/MIDI track (aux and master tracks are skipped).
    SessionAnalysis analyse (const Session& s, std::function<bool (float)> progress = {});

    /// Pieces, exposed for tests
    Features measure (const std::vector<float>& L, const std::vector<float>& R, double bpm,
                      std::vector<float>& envOut, std::vector<float>& pitchOut);
    void classify (TrackAnalysis& t);
    void sortVocals (SessionAnalysis& a);
    void detectKey (SessionAnalysis& a);
}
