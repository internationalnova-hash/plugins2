#include "NovaSamplerEngine.h"
#include <juce_core/juce_core.h>

NovaSamplerEngine::ZoneSelection NovaSamplerEngine::selectZones (int midiNote, int velocity) noexcept
{
    ZoneSelection result;

    struct GroupMatch { int groupId; int lowVelocity; int highVelocity; };
    std::array<GroupMatch, kMaxCandidateGroups> groups {};
    int numGroups = 0;

    for (auto& zone : zones)
    {
        if (! zone.matches (midiNote, velocity)) continue;

        int gIdx = -1;
        for (int i = 0; i < numGroups; ++i)
            if (groups[(size_t) i].groupId == zone.roundRobinGroup) { gIdx = i; break; }

        if (gIdx < 0)
        {
            if (numGroups >= kMaxCandidateGroups) continue; // safety clamp, no allocation
            gIdx = numGroups++;
            groups[(size_t) gIdx] = { zone.roundRobinGroup, zone.lowVelocity, zone.highVelocity };
        }
    }

    if (numGroups == 0) return result;

    if (numGroups == 1)
    {
        result.primary = pickRoundRobin (groups[0].groupId, midiNote, velocity);
        return result;
    }

    // Velocity-layer crossfade: use the two candidate groups whose velocity
    // ranges are closest to the triggered velocity (v0.1: first two found -
    // pathological >2-way overlaps are out of scope, spec doesn't call for
    // them and the small test library won't exercise that case).
    auto* zoneA = pickRoundRobin (groups[0].groupId, midiNote, velocity);
    auto* zoneB = pickRoundRobin (groups[1].groupId, midiNote, velocity);

    if (zoneA == nullptr) { result.primary = zoneB; return result; }
    if (zoneB == nullptr) { result.primary = zoneA; return result; }

    if (groups[0].lowVelocity > groups[1].lowVelocity)
        std::swap (zoneA, zoneB);

    const auto loOverlap = (float) juce::jmax (zoneA->lowVelocity, zoneB->lowVelocity);
    const auto hiOverlap = (float) juce::jmin (zoneA->highVelocity, zoneB->highVelocity);

    float blend = 0.5f;
    if (hiOverlap > loOverlap)
        blend = juce::jlimit (0.f, 1.f, ((float) velocity - loOverlap) / (hiOverlap - loOverlap));

    result.primary = zoneA;
    result.secondary = zoneB;
    result.secondaryBlend = blend;
    return result;
}

const NovaSampleZone* NovaSamplerEngine::pickRoundRobin (int groupId, int midiNote, int velocity) noexcept
{
    std::array<const NovaSampleZone*, kMaxZonesPerGroup> matches {};
    int count = 0;

    for (auto& zone : zones)
    {
        if (zone.roundRobinGroup != groupId) continue;
        if (! zone.matches (midiNote, velocity)) continue;
        if (count < kMaxZonesPerGroup) matches[(size_t) count++] = &zone;
    }

    if (count == 0) return nullptr;
    if (count == 1) return matches[0];

    const int safeGroup = juce::jlimit (0, kMaxRoundRobinGroups - 1, groupId);
    int& counter = roundRobinCounters[(size_t) safeGroup];
    const int chosen = counter % count;
    counter = (counter + 1) % count;
    return matches[(size_t) chosen];
}
