#pragma once

#include "NovaSamplerEngine.h"

// Renders one triggered zone-selection (spec sections 1-4): pitch-shifts via
// linear-interpolated resampling from each zone's root note, handles
// forward/ping-pong looping, and crossfades two layers when a ZoneSelection
// carries a secondary zone (velocity-layer boundary). Pure rendering state -
// no allocation after construction, safe to call renderAdding() from the
// audio thread every block.
class NovaSamplePlayback
{
public:
    // `extraCents` lets a caller (e.g. a layer applying its own tune/octave
    // on top of the zone's own root-note/tuning) add further pitch offset
    // without needing to fake a fractional MIDI note number.
    void start (const NovaSamplerEngine::ZoneSelection& selection, int midiNote, double outputSampleRate, float extraCents = 0.f) noexcept;
    void reset() noexcept { active = false; primary = {}; secondary = {}; }

    bool isActive() const noexcept { return active; }

    // Adds (not overwrites) rendered audio into outL/outR so multiple voices
    // can sum directly. Returns true while still producing audio; false once
    // every layer has finished (non-looping sample exhausted), at which
    // point the caller should retire the voice.
    bool renderAdding (float* outL, float* outR, int numSamples) noexcept;

private:
    struct Layer
    {
        const NovaSampleZone* zone = nullptr;
        double position = 0.0;
        double increment = 1.0;
        int direction = 1; // for ping-pong looping
        bool finished = true;
    };

    static void startLayer (Layer& layer, const NovaSampleZone* zone, int midiNote, double outputSampleRate, float extraCents) noexcept;
    static float readInterpolated (const juce::AudioBuffer<float>& buf, int channel, double pos) noexcept;
    // Advances position by `increment`, handling loop wrap/reflection. Returns
    // false if the layer has run past its sample with no loop (finished).
    static bool advance (Layer& layer) noexcept;

    Layer primary, secondary;
    float secondaryBlend = 0.f;
    bool active = false;
};
