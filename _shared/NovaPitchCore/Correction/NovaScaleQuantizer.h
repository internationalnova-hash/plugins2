#pragma once

#include "../NovaPitchTypes.h"
#include <juce_core/juce_core.h>

// Stage 2 of the pitch-correction chain: snaps a detected pitch toward the
// nearest note in a user-selected key/scale.
//
// nearestScaleMidi() is the pure "which scale note is nearest" mapping -
// independently implemented from the same logic Nova Pitch's original
// quantizeToScale() used (round to nearest enabled scale degree, folding
// to the closest octave), kept as a standalone static function specifically
// so it can be validated in isolation against the retired implementation's
// known outputs.
//
// process() layers a smoothed correction on top of that pure mapping:
//   - Amount blends the corrected target between raw pitch (0) and fully
//     quantized (1).
//   - RetuneSpeed (0=fast/hard-tune, 1=slow/natural - same sense as the
//     original "tolerance" parameter) sets a real one-pole TIME-domain
//     glide toward that blended target, matching how "Retune Speed" reads
//     on the new UI (a speed, not a spatial blend width).
//   - VibratoPreserve/VibratoRateHz split the raw input into a slow
//     "center pitch" (which is what actually gets quantized/corrected) and
//     a fast oscillating component around vibratoRateHz, then add a
//     VibratoPreserve-scaled fraction of that oscillation back on top of
//     the corrected center - so natural vibrato survives correction
//     instead of being flattened, without synthesizing any artificial LFO.
//
// Real-time-safe: no allocation anywhere, all state is plain members.
namespace nova::pitch
{
    class NovaScaleQuantizer
    {
    public:
        NovaScaleQuantizer() = default;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        void setKeyAndScale (int keyPitchClass, Scale scale) noexcept;
        void setAmount (float amount0to1) noexcept;
        void setRetuneSpeed (float speed0to1) noexcept; // 0=fast/hard, 1=slow/natural
        void setVibratoPreserve (float amount0to1) noexcept;
        void setVibratoRateHz (float hz) noexcept;

        float process (float inputHz) noexcept;
        float getTargetHz() const noexcept { return lastTargetHz; }

        // Pure mapping, independent of all smoothing state - exact
        // equivalent of the retired quantizeToScale(): nearest enabled
        // scale degree to hz, folded to the closest octave. midi, not hz,
        // so callers needing hz can convert via midiToHzF().
        static int nearestScaleMidi (int keyPitchClass, Scale scale, float hz) noexcept;

    private:
        double dt = 1.0 / 44100.0;

        int keyPitchClass = 0;
        Scale scale = Scale::Chromatic;
        float amount = 1.0f;
        float retuneSpeedTau = 0.05f; // seconds, derived from setRetuneSpeed
        float vibratoPreserve = 0.0f;
        float vibratoRateAlpha = 0.0f; // one-pole coeff derived from vibrato rate Hz

        bool  primed = false;
        // double, not float: these are one-pole accumulators that can sit
        // holding a near-constant ~59-magnitude MIDI value for many
        // seconds at slow RetuneSpeed settings, where the per-sample
        // increment shrinks toward the target - in float, that increment
        // underflows below the representable step at that magnitude
        // before fully converging, permanently freezing several cents
        // short of the true target (measured, not theoretical - this
        // exact failure mode is why NovaMicroShiftLine's own position
        // tracking uses double for the same reason).
        double slowCenterMidi = 0.0;     // one-pole-tracked "center" pitch, for vibrato isolation
        double smoothedTargetMidi = 0.0; // time-glided corrected output

        // Peak-held (fast-attack/slow-decay) version of the deviation used
        // to decide slowCenterMidi's own tracking speed - see process()
        // for why a purely instantaneous deviation self-undermines partway
        // through a genuine note change.
        double peakDeviationHeld = 0.0;

        // The scale degree currently committed to - switching away from it
        // requires clearing a hysteresis margin (see process()), not just
        // crossing the naive nearest-degree midpoint. Without this, a
        // singer's pitch sitting anywhere near a boundary between two
        // scale degrees (which happens routinely DURING a note-to-note
        // transition, not just from noise) makes the quantized target
        // flip back and forth every hop; at a fast RetuneSpeed that
        // flicker reaches the output almost immediately and is audible as
        // a wobble/warble, worst exactly during vocal movement - a
        // settled sustained note sits solidly in the middle of its own
        // degree's territory, far from any boundary, which is why this
        // doesn't show up there.
        bool  quantizedPrimed = false;
        int   committedScaleMidi = 0;

        float lastTargetHz = 0.0f;
    };
}
