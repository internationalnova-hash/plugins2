#pragma once

#include "../NovaTapeTypes.h"
#include <juce_core/juce_core.h>
#include <cmath>

// Tape-specific nonlinearity - independently designed from first
// principles, NOT a port or derivative of Nova Heat's saturation curve.
// Nova Heat is a static per-sample waveshaper (drive a fixed curve,
// blend driven/dry via jmap) with no memory and no frequency-dependent
// behavior. Real magnetic tape saturation is fundamentally different: the
// tape's magnetization LAGS the input (hysteresis - finite domain
// response time), which is what gives tape its characteristic frequency-
// dependent, asymmetric, even-harmonic-rich "give" under drive, as
// opposed to a symmetric clipper's purely odd-harmonic, memoryless
// response.
//
// This class models that lag directly: a one-pole "magnetization memory"
// state chases the input, and the nonlinear curve is applied to a BLEND
// of the instantaneous input and that lagging memory (not the input
// alone). Fast-changing (high-frequency) content can't fully "catch up"
// the memory term, which naturally reduces effective saturation at high
// frequencies relative to sustained/low content - a structural mechanism,
// not a filtered-EQ approximation of one. The curve itself is an
// asymmetric rational soft-knee (x / (1 + k|x|), offset-shifted by the
// memory term) rather than a symmetric tanh/arctan - asymmetry is what
// produces tape's characteristic even-harmonic content (a real, well-
// documented tape signature, absent from symmetric clippers).
namespace nova::tape
{
    class NovaTapeSaturator
    {
    public:
        NovaTapeSaturator() = default;

        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate;
            reset();
        }

        void reset() noexcept
        {
            memoryL = memoryR = 0.0f;
            dcBlockStateInL = dcBlockStateOutL = 0.0f;
            dcBlockStateInR = dcBlockStateOutR = 0.0f;
        }

        void setProfile (const TapeTypeProfile& profile) noexcept
        {
            // Hysteresis "domain lag" time constant -> one-pole coefficient.
            const float tauSeconds = juce::jmax (0.05f, profile.hysteresisTimeMs) * 0.001f;
            memoryAlpha = 1.0f - std::exp (-1.0f / (tauSeconds * (float) sr));
            driveAmount = profile.driveAmount;
            biasAsymmetry = profile.biasAsymmetry;
            compressionAmount = profile.compressionAmount;
        }

        // biasKnob: -1..1 from the user's BIAS control, additively shifts
        // the profile's own baked-in asymmetry - musical range, not a
        // literal circuit-bias simulation.
        void setUserBias (float biasKnobMinus1to1) noexcept
        {
            userBias = juce::jlimit (-1.0f, 1.0f, biasKnobMinus1to1);
        }

        float processSampleL (float input) noexcept { return processChannel (input, memoryL); }
        float processSampleR (float input) noexcept { return processChannel (input, memoryR); }

        // DC blocking is a real, separate need here (not cosmetic): an
        // asymmetric transfer curve driven by real program material
        // produces a genuine DC-ish offset that must be removed before
        // the signal reaches the head-bump filter downstream, or the
        // bump's own resonance would ring on that offset.
        float dcBlockL (float x) noexcept { return dcBlock (x, dcBlockStateInL, dcBlockStateOutL); }
        float dcBlockR (float x) noexcept { return dcBlock (x, dcBlockStateInR, dcBlockStateOutR); }

    private:
        float processChannel (float input, float& memory) noexcept
        {
            memory += memoryAlpha * (input - memory);

            const float effectiveBias = juce::jlimit (0.0f, 1.0f, biasAsymmetry + 0.35f * userBias);
            const float drive = input * driveAmount + memory * effectiveBias * driveAmount * 0.6f;

            float shaped = shapeCurve (drive);

            // Extra level-dependent gain reduction (soft, program-dependent
            // "tape compression" under heavy drive) - blended in only via
            // compressionAmount, so Studio (low compressionAmount) stays
            // close to a pure saturation curve while Hot leans into it.
            if (compressionAmount > 0.0f)
            {
                const float envelope = std::abs (memory) + std::abs (input) * 0.5f;
                const float gr = 1.0f / (1.0f + compressionAmount * envelope * envelope);
                shaped = shaped * (1.0f - compressionAmount) + shaped * gr * compressionAmount;
            }

            return shaped;
        }

        static float shapeCurve (float x) noexcept
        {
            // Asymmetric rational soft-knee: x/(1+k|x|), with an offset
            // shift so positive and negative excursions see a different
            // effective knee - deliberately NOT tanh/arctan (Nova Heat's
            // family of curves), and deliberately offset-asymmetric rather
            // than a textbook symmetric soft-clipper.
            constexpr float k = 1.6f;
            const float shifted = x + 0.12f * x * x; // even-order pre-shift, itself a source of even harmonics
            return shifted / (1.0f + k * std::abs (shifted));
        }

        static float dcBlock (float x, float& stateIn, float& stateOut) noexcept
        {
            constexpr float r = 0.995f;
            const float y = x - stateIn + r * stateOut;
            stateIn = x;
            stateOut = y;
            return y;
        }

        double sr = 44100.0;
        float memoryAlpha = 0.1f;
        float driveAmount = 0.7f;
        float biasAsymmetry = 0.2f;
        float userBias = 0.0f;
        float compressionAmount = 0.0f;

        float memoryL = 0.0f, memoryR = 0.0f;
        float dcBlockStateInL = 0.0f, dcBlockStateOutL = 0.0f;
        float dcBlockStateInR = 0.0f, dcBlockStateOutR = 0.0f;
    };
}
