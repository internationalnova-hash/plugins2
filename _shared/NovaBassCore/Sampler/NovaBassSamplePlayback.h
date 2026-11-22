#pragma once

#include "NovaBassSamplerEngine.h"

namespace nova::bass
{

// Renders one triggered zone-selection: pitch-shifts via Catmull-Rom
// (4-point cubic) interpolated resampling from each zone's root note,
// handles forward/ping-pong looping, and crossfades two layers when a
// ZoneSelection carries a secondary zone (velocity-layer boundary).
//
// Deliberately upgraded from Nova Keys' linear (2-point) interpolation:
// research confirmed Keys' NovaSamplePlayback uses linear resampling,
// which is a real fidelity risk here specifically because Nova Bass's
// sample layers are sub-bass content - pitch-shifting a low-frequency
// sample pushes energy into a range where linear interpolation's imaging
// artifacts become audible as flutter/graininess (the same failure mode
// that made NovaMicroShiftLine.h in NovaDoublerCore move to Catmull-Rom).
// The interpolation formula below is ported verbatim from that class.
//
// Pure rendering state - no allocation after construction, safe to call
// renderAdding() from the audio thread every block.
class NovaBassSamplePlayback
{
public:
    void start (const NovaBassSamplerEngine::ZoneSelection& selection, int midiNote, double outputSampleRate, float extraCents = 0.f) noexcept;
    void reset() noexcept { active = false; primary = {}; secondary = {}; }

    bool isActive() const noexcept { return active; }

    // Adds (not overwrites) rendered audio into outL/outR so multiple
    // voices can sum directly. Returns true while still producing audio;
    // false once every layer has finished (non-looping sample exhausted).
    bool renderAdding (float* outL, float* outR, int numSamples) noexcept;

private:
    struct Layer
    {
        const NovaBassSampleZone* zone = nullptr;
        double position = 0.0;
        double increment = 1.0;
        int direction = 1; // for ping-pong looping
        bool finished = true;
    };

    static void startLayer (Layer& layer, const NovaBassSampleZone* zone, int midiNote, double outputSampleRate, float extraCents) noexcept;
    static float readCatmullRom (const juce::AudioBuffer<float>& buf, int channel, double pos) noexcept;
    static bool advance (Layer& layer) noexcept;

    Layer primary, secondary;
    float secondaryBlend = 0.f;
    bool active = false;
};

} // namespace nova::bass
