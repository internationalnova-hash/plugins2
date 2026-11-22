#pragma once

#include "NovaBassSampleZone.h"
#include <array>
#include <vector>

namespace nova::bass
{

// Zone lookup / round-robin / velocity-crossfade decision engine, ported
// unchanged in shape from Nova Keys' NovaSamplerEngine (that pool-with-
// note-stealing pattern is confirmed correct for polyphonic sample
// playback - only the interpolation quality downstream in
// NovaBassSamplePlayback changes for Nova Bass). Owns the zone list; does
// NOT render audio itself.
//
// Threading contract: addZone()/clearZones() are message-thread-only setup
// calls (preset load). selectZones() is realtime-safe (no allocation, no
// locks) and is the only method meant to run on the audio thread.
class NovaBassSamplerEngine
{
public:
    static constexpr int kMaxRoundRobinGroups = 64;
    static constexpr int kMaxCandidateGroups = 4;   // distinct groups matching one note+velocity (velocity-layer crossfade fan-out)
    static constexpr int kMaxZonesPerGroup = 8;      // round-robin siblings within one group

    struct ZoneSelection
    {
        const NovaBassSampleZone* primary = nullptr;
        const NovaBassSampleZone* secondary = nullptr;
        float secondaryBlend = 0.f; // 0 = only primary, 1 = only secondary
    };

    void prepare (double outputSampleRate) noexcept { sampleRate = outputSampleRate; }

    void clearZones() { zones.clear(); resetRoundRobinState(); }
    void addZone (NovaBassSampleZone zone) { zones.push_back (std::move (zone)); }
    void resetRoundRobinState() noexcept { roundRobinCounters.fill (0); }

    int getNumZones() const noexcept { return (int) zones.size(); }

    // Realtime-safe: no allocation, fixed-size local scratch only.
    ZoneSelection selectZones (int midiNote, int velocity) noexcept;

private:
    const NovaBassSampleZone* pickRoundRobin (int groupId, int midiNote, int velocity) noexcept;

    std::vector<NovaBassSampleZone> zones;
    std::array<int, kMaxRoundRobinGroups> roundRobinCounters {};
    double sampleRate = 44100.0;
};

} // namespace nova::bass
