#pragma once

// Mix Assistant, part 2: turns what was heard into a mix plan - buses, effect
// returns, levels, pans, sends, "make room" EQ and Nova plugin chains.
// Pure data; MixApplier carries it out on the session.

#include "MixAnalysis.h"

namespace NovaStudio::MixAI
{
    struct Setting { juce::String name; juce::String value; };       // value as the plugin shows it ("100%", "1/4", "On")
    struct PluginStep
    {
        juce::String plugin; std::vector<Setting> settings; juce::String note; juce::MemoryBlock state;
        bool analyseMix = false;   // Nova AI Master: render the finished mix, analyse it, load with the plan
    };
    struct Send { juce::String toBus; float levelDb = -100.f; };
    struct EqDip { float freq = 2800.f, gainDb = -2.f, q = 0.9f; };

    struct PlannedBus
    {
        juce::String name;
        juce::Colour colour;
        Family family = Family::Other;
        bool isReturn = false;                 // effect return (fed by sends) instead of a group bus
        std::vector<int> members;              // track indices routed into it
        float volumeDb = 0.f;
        std::vector<PluginStep> chain;
        std::vector<Send> sends;
        std::vector<EqDip> eq;
        juce::String why;
    };

    struct TrackMove
    {
        int track = -1;
        Role role = Role::Other;
        bool change = true;                    // false = leave this track alone
        float volumeDb = 0.f, pan = 0.f;
        float clipGainDb = 0.f;                // raise a quiet recording before the fader (gain staging)
        juce::String toBus;                    // empty = Main Out
        std::vector<PluginStep> chain;
        std::vector<Send> sends;
        std::vector<EqDip> eq;
    };

    struct Options
    {
        bool route = true, levels = true, chains = true, makeRoom = true, tune = false, masterPolish = true;
        int intensity = 1;                     // 0 Light, 1 Balanced, 2 Full mix
        juce::String style = "Nova Balance";   // a Nova Target name
        juce::File reference;                  // optional reference song (overrides style for the mix bus)
    };

    struct Plan
    {
        std::vector<PlannedBus> buses;
        std::vector<TrackMove> tracks;
        std::vector<PluginStep> master;
        bool builtInLimiterOff = false;        // Nova AI Master does the limiting (the built-in one would squash + clip after it)
        juce::StringArray notes;               // anything the user should know (bus limit, missing roles…)
        int busesUsed = 0, busesAvailable = 0;
        int changeCount() const;
    };

    juce::StringArray styleNames();
    /// roles: the analysis' roles, possibly corrected by the user (same order as analysis.tracks)
    Plan makePlan (const SessionAnalysis& a, const std::vector<Role>& roles, const Session& s, const Options& o);
}
