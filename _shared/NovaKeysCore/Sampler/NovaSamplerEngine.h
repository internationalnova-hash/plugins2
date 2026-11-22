#pragma once

#include "NovaSampleZone.h"
#include <array>
#include <vector>

// Zone lookup / round-robin / velocity-crossfade decision engine (spec
// sections 2-4). Owns the zone list; does NOT render audio itself (see
// NovaSamplePlayback for that) - this class only answers "which zone(s)
// should this note-on use".
//
// Threading contract: addZone()/clearZones() are message-thread-only setup
// calls, expected to run BEFORE playback begins for a given zone set (preset
// load). selectZones() is realtime-safe (no allocation, no locks) and is the
// only method meant to be called from the audio thread once zones are
// loaded - it only reads the zone vector and mutates a fixed-size counter
// array, never resizes anything.
class NovaSamplerEngine
{
public:
    static constexpr int kMaxRoundRobinGroups = 64;
    static constexpr int kMaxCandidateGroups = 4;   // distinct groups matching one note+velocity (velocity-layer crossfade fan-out)
    static constexpr int kMaxZonesPerGroup = 8;      // round-robin siblings within one group

    struct ZoneSelection
    {
        const NovaSampleZone* primary = nullptr;
        const NovaSampleZone* secondary = nullptr;
        float secondaryBlend = 0.f; // 0 = only primary, 1 = only secondary
    };

    void prepare (double outputSampleRate) noexcept { sampleRate = outputSampleRate; }

    void clearZones() { zones.clear(); resetRoundRobinState(); }
    void addZone (NovaSampleZone zone) { zones.push_back (std::move (zone)); }
    void resetRoundRobinState() noexcept { roundRobinCounters.fill (0); }

    int getNumZones() const noexcept { return (int) zones.size(); }

    // Realtime-safe: no allocation, fixed-size local scratch only.
    ZoneSelection selectZones (int midiNote, int velocity) noexcept;

private:
    const NovaSampleZone* pickRoundRobin (int groupId, int midiNote, int velocity) noexcept;

    std::vector<NovaSampleZone> zones;
    std::array<int, kMaxRoundRobinGroups> roundRobinCounters {};
    double sampleRate = 44100.0;
};
