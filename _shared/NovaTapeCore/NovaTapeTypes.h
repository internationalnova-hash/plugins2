#pragma once

// Shared enums and per-personality/per-speed tuning tables for Nova Tape.
// Deliberately independent of Nova Heat's CharacterMode/ModeProfile design
// (see NovaTapeSaturator.h for why) - these are tape-transport concepts
// (speed, bias, head response), not a generic saturation "character".
namespace nova::tape
{
    enum class TapeType { Studio = 0, Vintage = 1, Hot = 2 };
    enum class IpsSpeed { Speed7_5 = 0, Speed15 = 1, Speed30 = 2 };

    inline float ipsToNumeric (IpsSpeed s) noexcept
    {
        switch (s)
        {
            case IpsSpeed::Speed7_5: return 7.5f;
            case IpsSpeed::Speed15:  return 15.0f;
            case IpsSpeed::Speed30:  return 30.0f;
        }
        return 15.0f;
    }

    // Per-TapeType tuning - each field independently authored to give the
    // three personalities a genuinely different character, not just a
    // gain-scaled copy of one shared curve.
    struct TapeTypeProfile
    {
        float driveAmount;         // overall gain into the saturator's nonlinearity
        float hysteresisTimeMs;    // "domain lag" time constant - larger = thicker/slower-reacting saturation
        float biasAsymmetry;       // 0-1, how asymmetric (even-harmonic-rich) the curve is
        float preEmphasisDb;       // HF boost before the nonlinearity/head filter
        float deEmphasisDb;        // HF cut after the head filter (independently tunable, not a perfect cancel)
        float headBumpGainDb;      // additional head-bump gain on top of the speed-driven amount
        float hfRolloffTiltDb;     // additional HF rolloff on top of the speed-driven amount
        float noiseCharacterGain;  // relative noise-floor loudness at a given NOISE knob setting
        float compressionAmount;   // 0-1, extra level-dependent gain reduction layered on the driven signal
    };

    inline TapeTypeProfile getTapeTypeProfile (TapeType type) noexcept
    {
        switch (type)
        {
            // STUDIO - clean, wide, polished 2-track/mastering character:
            // light drive, fast/tight hysteresis (least "smeared"), mostly
            // symmetric (little even-harmonic coloration), minimal EQ
            // shaping, lowest noise.
            case TapeType::Studio:
                return { 0.55f, 2.0f, 0.18f, 1.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.10f };

            // VINTAGE - warmer/thicker, softer top end, stronger head bump:
            // more drive, slower/thicker hysteresis memory, more asymmetric
            // (more even-harmonic rich), stronger pre/de-emphasis shaping,
            // extra head-bump and HF rolloff on top of the speed defaults.
            case TapeType::Vintage:
                return { 0.85f, 6.5f, 0.42f, 2.5f, 3.2f, 1.6f, 2.2f, 1.3f, 0.28f };

            // HOT - driven tape: highest drive, fastest-reacting but most
            // compressed (transient rounding via compressionAmount), still
            // musical rather than fuzz - moderate hysteresis (enough to
            // round transients without smearing everything), strong but
            // not extreme asymmetry.
            case TapeType::Hot:
                return { 1.35f, 3.2f, 0.30f, 1.6f, 1.4f, 0.9f, 0.6f, 0.9f, 0.55f };
        }
        return getTapeTypeProfile (TapeType::Studio);
    }

    // Per-speed physical-response tuning - head bump frequency/gain and HF
    // rolloff frequency, independent of TapeType (TapeTypeProfile above
    // adds ON TOP of these as a further per-personality shaping).
    struct IpsProfile
    {
        float headBumpFreqHz;
        float headBumpQ;
        float headBumpGainDb;
        float hfRolloffFreqHz;   // -3dB-ish shelf corner
        float noiseFloorGain;    // relative to a 15 IPS baseline of 1.0
    };

    inline IpsProfile getIpsProfile (IpsSpeed speed) noexcept
    {
        switch (speed)
        {
            // 7.5 IPS - warmest/darkest: largest, lowest head bump, earliest rolloff, noisiest.
            case IpsSpeed::Speed7_5:
                return { 55.0f, 0.65f, 3.4f, 9500.0f, 1.6f };
            // 15 IPS - balanced classic tape character (default).
            case IpsSpeed::Speed15:
                return { 68.0f, 0.85f, 2.0f, 14500.0f, 1.0f };
            // 30 IPS - cleanest/tightest: smallest/tightest bump, most extended top end, quietest.
            case IpsSpeed::Speed30:
                return { 78.0f, 1.15f, 0.7f, 18500.0f, 0.6f };
        }
        return getIpsProfile (IpsSpeed::Speed15);
    }
}
