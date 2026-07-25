#pragma once

#include <string>
#include <array>
#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// One named listening position within an environment (e.g. "Driver" for a
// car, "Sweet Spot" for a studio). Each environment owns its own list -
// there is no shared/global position vocabulary - so adding a new
// environment with its own position names never requires touching the UI
// or the engine's position-handling code, only this file.
struct PositionDefinition
{
    std::string label;
    float lateralBias;   // added to stereoOffset; positive boosts the left channel (near-side proximity)
    float diffuseAmount; // 0..1, extra "less focused" treatment for off-axis/rear spots
};

// Max positions any single environment can offer. This is a real ceiling,
// not a style choice: the real-time `position` AudioParameterChoice (see
// ParameterIDs::numPositions) has a fixed slot count for its whole
// lifetime - VST3/AU hosts cache a parameter's choice-list shape once, so
// it can't grow/shrink per environment. 4 covers every environment planned
// so far (cars, studios, clubs, home theater all describe <=4 positions);
// environments needing fewer just leave the extra slots unused
// (numPositions < kMaxPositions).
inline constexpr int kMaxPositions = 4;

// One entry per listening environment. Numbers are acoustic-modeling
// estimates, not measurements of real rooms/devices - tuned by ear/reason
// for a clearly distinct, musically useful translation check per
// environment, not scientific room-accuracy.
struct EnvironmentProfile
{
    std::string name;
    std::string category;
    std::string descriptor;

    float lowCutHz;
    float highCutHz;
    float lowShelfGain;
    float lowShelfFreq;
    float midPeakGain;
    float midPeakFreq;
    float midPeakQ;
    float highShelfGain;
    float highShelfFreq;

    float stereoWidth;       // 0 = mono, 1 = unchanged
    float stereoOffset;      // -1..1, listening-position bias
    float compressionAmount; // 0..1
    float saturationAmount;  // 0..1
    float roomAmount;        // 0..1, overall room/reflection contribution
    float earlyReflectionAmount; // 0..1, shapes the synthetic IR's early taps
    float noiseAmount;       // 0..1, ambience bed level (pre noise_amount param scaling)

    bool supportsPosition;   // false = Position control is disabled entirely (e.g. Smartphone)
    int numPositions;        // 0..kMaxPositions, how many of `positions` are valid
    int defaultPositionIndex; // explicit per-environment default - never assumed to be 0; also
                               // the fallback used if a stored/automated index is out of range
                               // for this environment (see EnvironmentChain::process)
    std::array<PositionDefinition, kMaxPositions> positions; // this environment's own position map

    int impulseResponseId;   // stable slot id; real licensed IRs can replace the synthetic ones later
};

namespace EnvironmentLibrary
{
    // Indices match ParameterIDs::environmentNames. Only the first 4 have
    // real profiles in this phase; index 4..11 fall back to a neutral
    // passthrough profile (getProfile() below), matching the plan.
    enum Id
    {
        NovaMainRoom = 0,
        Sedan = 4,
        Smartphone = 6,
        MonoCube = 11
    };

    inline const EnvironmentProfile& neutralProfile()
    {
        static const EnvironmentProfile neutral {
            "Unbuilt", "—", "Not yet available in this build.",
            20.0f, 20000.0f,
            0.0f, 100.0f,
            0.0f, 1000.0f, 0.7f,
            0.0f, 8000.0f,
            1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            false, 0, 0, {},
            -1
        };
        return neutral;
    }

    inline const EnvironmentProfile& novaMainRoom()
    {
        static const EnvironmentProfile p {
            "Nova Main Room", "Studios", "Large professional control room \xE2\x80\xA2 Full-range \xE2\x80\xA2 Controlled reflections",
            20.0f, 20000.0f,
            0.0f, 90.0f,
            0.0f, 1000.0f, 0.7f,
            0.0f, 9000.0f,
            1.0f, 0.0f,
            0.05f, 0.02f, 0.15f, 0.15f, 0.0f,
            true, 4, 0, {{
                { "Sweet Spot",     0.0f,  0.0f },
                { "Engineer Left", -0.2f,  0.0f },
                { "Engineer Right", 0.2f,  0.0f },
                { "Client Sofa",    0.0f,  0.5f }
            }},
            0
        };
        return p;
    }

    inline const EnvironmentProfile& sedan()
    {
        static const EnvironmentProfile p {
            "Sedan", "Cars", "Mid-size car \xE2\x80\xA2 Door speakers \xE2\x80\xA2 Cabin reflections",
            90.0f, 12000.0f,
            3.0f, 150.0f,
            2.5f, 2500.0f, 1.2f,
            -4.0f, 6000.0f,
            0.6f, 0.15f,
            0.3f, 0.25f, 0.5f, 0.6f, 0.15f,
            true, 4, 0, {{
                { "Driver",          -0.15f, 0.0f },
                { "Front Passenger",  0.15f, 0.0f },
                { "Rear Left",       -0.12f, 0.4f },
                { "Rear Right",       0.12f, 0.4f }
            }},
            1
        };
        return p;
    }

    inline const EnvironmentProfile& smartphone()
    {
        static const EnvironmentProfile p {
            "Smartphone", "Consumer", "Mono \xE2\x80\xA2 Strong low cutoff \xE2\x80\xA2 Upper-mid emphasis",
            400.0f, 9000.0f,
            -6.0f, 300.0f,
            5.0f, 3000.0f, 1.5f,
            -3.0f, 7000.0f,
            0.05f, 0.0f,
            0.5f, 0.4f, 0.05f, 0.05f, 0.0f,
            false, 0, 0, {},
            2
        };
        return p;
    }

    inline const EnvironmentProfile& monoCube()
    {
        static const EnvironmentProfile p {
            "Mono Cube", "Utility", "Mono \xE2\x80\xA2 Band-limited \xE2\x80\xA2 Midrange-focused balance check",
            150.0f, 10000.0f,
            -3.0f, 200.0f,
            1.5f, 1500.0f, 0.9f,
            -2.0f, 8000.0f,
            0.0f, 0.0f,
            0.15f, 0.1f, 0.1f, 0.1f, 0.0f,
            false, 0, 0, {},
            3
        };
        return p;
    }

    // The 4 environments this phase actually implements, keyed by their
    // stable ParameterIDs::environmentNames index.
    inline const EnvironmentProfile& getProfile (int environmentIndex)
    {
        switch (environmentIndex)
        {
            case NovaMainRoom: return novaMainRoom();
            case Sedan:        return sedan();
            case Smartphone:   return smartphone();
            case MonoCube:     return monoCube();
            default:           return neutralProfile();
        }
    }

    inline bool isImplemented (int environmentIndex)
    {
        return environmentIndex == NovaMainRoom || environmentIndex == Sedan
            || environmentIndex == Smartphone || environmentIndex == MonoCube;
    }

    //==========================================================================
    // Procedural synthetic IR generator. No licensed/real-world impulse
    // responses exist for this phase, so each environment's room/reflection
    // character is synthesized: a handful of sparse early-reflection taps
    // (spacing/count scaled by earlyReflectionAmount) followed by a
    // progressively low-pass-filtered exponential noise tail (density/length
    // scaled by roomAmount). Deterministic (fixed seed) so the same
    // environment always sounds the same. Must only ever be called off the
    // audio thread (construction time / environment-change time).
    inline juce::AudioBuffer<float> generateSyntheticIR (const EnvironmentProfile& profile, double sampleRate)
    {
        const float decaySeconds = juce::jlimit (0.03f, 0.45f,
            0.04f + profile.roomAmount * 0.35f + profile.earlyReflectionAmount * 0.08f);
        const int numSamples = juce::jmax (16, static_cast<int> (decaySeconds * sampleRate));

        juce::AudioBuffer<float> ir (2, numSamples);
        ir.clear();

        juce::Random random (static_cast<juce::int64> (profile.impulseResponseId) * 7919 + 17);

        // Sparse early-reflection taps.
        const int numTaps = juce::jmax (2, static_cast<int> (profile.earlyReflectionAmount * 14.0f));
        for (int t = 0; t < numTaps; ++t)
        {
            const float tapTimeFrac = (t + 1.0f) / (numTaps + 1.0f);
            const int sampleIndex = juce::jlimit (0, numSamples - 1,
                static_cast<int> (tapTimeFrac * decaySeconds * sampleRate * 0.55f));
            const float amp = std::pow (1.0f - tapTimeFrac, 1.6f) * (0.35f + 0.25f * random.nextFloat());
            for (int ch = 0; ch < 2; ++ch)
            {
                const float sign = random.nextFloat() > 0.5f ? 1.0f : -1.0f;
                ir.addSample (ch, sampleIndex, sign * amp);
            }
        }

        // Diffuse decaying tail, one-pole-lowpassed progressively as it decays
        // (crude high-frequency air/material absorption).
        float lpStateL = 0.0f, lpStateR = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float envelope = std::exp (-3.5f * (static_cast<float> (i) / static_cast<float> (numSamples)));
            const float lpCoeff = juce::jlimit (0.05f, 0.85f, 0.85f - 0.7f * (static_cast<float> (i) / numSamples));

            const float noiseL = random.nextFloat() * 2.0f - 1.0f;
            const float noiseR = random.nextFloat() * 2.0f - 1.0f;
            lpStateL = lpCoeff * lpStateL + (1.0f - lpCoeff) * noiseL;
            lpStateR = lpCoeff * lpStateR + (1.0f - lpCoeff) * noiseR;

            const float tailGain = envelope * profile.roomAmount * 0.6f;
            ir.addSample (0, i, lpStateL * tailGain);
            ir.addSample (1, i, lpStateR * tailGain);
        }

        // Direct sound / unity tap at sample 0 so the convolution doesn't
        // itself add latency or remove the dry-ish character of the wet path.
        ir.addSample (0, 0, 1.0f);
        ir.addSample (1, 0, 1.0f);

        // Normalise peak to a safe level so convolution can't blow up gain.
        const float peak = juce::jmax (ir.getMagnitude (0, 0, numSamples), ir.getMagnitude (1, 0, numSamples));
        if (peak > 1.0e-6f)
            ir.applyGain (1.0f / peak);

        return ir;
    }
}
