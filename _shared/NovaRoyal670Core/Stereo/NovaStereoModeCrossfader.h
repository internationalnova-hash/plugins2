#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace nova::royal670
{

// Click-free Stereo <-> M/S mode switching. Rather than a continuously-
// blended encode basis (which has no clean physical meaning at
// intermediate values - see the plan), a mode change starts a short
// (~50ms) transition: the engine runs BOTH the old-mode and new-mode
// processing chains in parallel only for that window, and this class
// tracks the crossfade progress and hands back the standard L/R<->M/S
// encode/decode math. Outside a transition only the active chain needs
// to run at all - this class's job is purely bookkeeping + math, not
// owning any of the actual (heavy, stateful) DSP chains themselves; the
// engine owns those and asks this class what to do with them.
class NovaStereoModeCrossfader
{
public:
    enum class Mode { Stereo, MidSide };

    NovaStereoModeCrossfader() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // Called once per block from the engine. No-op if unchanged; starts a
    // new transition if the mode actually changed.
    void setMode (Mode newMode) noexcept;

    // Advances the transition by one sample; call once per sample from
    // the engine's per-sample loop. Returns the current crossfade blend:
    // 0.0 = fully previousMode, 1.0 = fully activeMode. Always 1.0 when
    // not transitioning.
    float advanceSample() noexcept;

    bool isTransitioning() const noexcept { return transitioning; }
    Mode getActiveMode() const noexcept { return activeMode; }
    Mode getPreviousMode() const noexcept { return previousMode; }

    static void encode (float l, float r, Mode mode, float& chA, float& chB) noexcept
    {
        if (mode == Mode::MidSide)
        {
            chA = 0.5f * (l + r); // mid
            chB = 0.5f * (l - r); // side
        }
        else
        {
            chA = l;
            chB = r;
        }
    }

    static void decode (float chA, float chB, Mode mode, float& l, float& r) noexcept
    {
        if (mode == Mode::MidSide)
        {
            l = chA + chB; // mid + side
            r = chA - chB; // mid - side
        }
        else
        {
            l = chA;
            r = chB;
        }
    }

private:
    static constexpr float kTransitionSeconds = 0.05f;

    double sampleRate = 44100.0;
    Mode activeMode = Mode::Stereo;
    Mode previousMode = Mode::Stereo;
    bool transitioning = false;
    float transitionPos = 1.0f; // 0..1 progress through the transition
    float transitionStep = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaStereoModeCrossfader)
};

} // namespace nova::royal670
