#pragma once

#include <array>

// A "vibe" is a mix style, labelled by sound rather than by artist name.
// Each one sets how the seven macro knobs behave (frequencies, compressor
// speed, delay timing, reverb size) plus where those knobs start.
//
// The macro knobs scale toward these maxima: Air = 1.0 gives the full
// presenceMaxDb/airMaxDb boost, Space = 1.0 gives the full delay/reverb
// send, and so on. Listen then nudges the starting knob positions to suit
// the actual voice (see NovaVocalDecision).

enum NovaMacro { kClean = 0, kDeEss, kPunch, kAir, kWarmth, kSpace, kTune, kNumMacros };

struct NovaVibe
{
    const char* name;

    // Tone
    float hpfMaxHz;        // high-pass corner at Clean = 1
    float mudCutMaxDb;     // 300 Hz cut at Clean = 1
    float presenceHz;
    float presenceMaxDb;   // at Air = 1
    float airHz;           // high shelf corner
    float airMaxDb;        // at Air = 1
    float deEssHz;         // sibilance band starts here

    // Dynamics
    float compAttackMs;
    float compReleaseMs;
    float compRatioMax;    // ratio at Punch = 1

    // Space
    float delayBeats;      // 0.5 = 1/8, 0.75 = dotted 1/8, 1 = 1/4, 1.5 = dotted 1/4
    float delayFeedback;
    float delayLowpassHz;
    float delaySend;       // at Space = 1
    float reverbRoom;
    float reverbDamping;
    float reverbSend;      // at Space = 1

    std::array<float, kNumMacros> macros; // Clean, De-Ess, Punch, Air, Warmth, Space, Tune

    // Character beyond the knobs (artist presets set these; the six vibes
    // keep the defaults, so they sound exactly as before).
    float retuneScale  = 1.0f;  // x the Tune knob's retune time (<1 snappier, >1 more natural)
    float tuneHumanize = 0.0f;  // 0..1 let notes already near pitch through
    float tuneVibrato  = 0.0f;  // 0..1 keep the singer's own vibrato
    float satDriveMax  = 5.0f;  // Warmth = 1 drive (tanh, parallel)
    float satMixMax    = 0.75f; // Warmth = 1 parallel blend
};

inline constexpr int kNumVibes = 6;

inline const std::array<NovaVibe, kNumVibes>& getNovaVibes()
{
    static const std::array<NovaVibe, kNumVibes> vibes {{
        // name               hpf   mud  presHz presDb airHz  airDb  deEss  att  rel  ratio  dBeats fb    dLP    dSend room  damp  rSend   Clean De-Ess Punch Air  Warmth Space Tune
        { "Reggaeton Bright", 140.f, 5.f, 4000.f, 5.f, 12000.f, 8.f, 6500.f, 3.f,  60.f, 6.f,  0.75f, 0.32f, 6000.f, 0.35f, 0.62f, 0.45f, 0.30f, { 0.60f, 0.55f, 0.70f, 0.75f, 0.25f, 0.55f, 0.95f } },
        { "Melodic Trap",     150.f, 5.f, 3500.f, 4.f, 11000.f, 8.f, 6500.f, 2.f,  50.f, 8.f,  1.00f, 0.38f, 5000.f, 0.40f, 0.75f, 0.50f, 0.35f, { 0.65f, 0.50f, 0.75f, 0.70f, 0.30f, 0.60f, 1.00f } },
        { "R&B Smooth",       110.f, 4.f, 3000.f, 3.f, 10000.f, 7.f, 6000.f, 10.f, 120.f, 4.f, 1.50f, 0.30f, 4500.f, 0.30f, 0.85f, 0.60f, 0.40f, { 0.50f, 0.50f, 0.55f, 0.60f, 0.45f, 0.65f, 0.55f } },
        { "Pop Polished",     120.f, 4.f, 3500.f, 4.f, 12000.f, 7.f, 7000.f, 5.f,  80.f, 5.f,  0.50f, 0.25f, 6000.f, 0.25f, 0.60f, 0.50f, 0.30f, { 0.60f, 0.60f, 0.65f, 0.65f, 0.30f, 0.40f, 0.70f } },
        { "Drill Dark",       160.f, 6.f, 2500.f, 5.f,  9000.f, 3.f, 6000.f, 1.f,  40.f, 8.f,  0.50f, 0.20f, 3500.f, 0.30f, 0.45f, 0.70f, 0.20f, { 0.70f, 0.45f, 0.85f, 0.35f, 0.60f, 0.30f, 0.45f } },
        { "Raw Natural",       90.f, 3.f, 3000.f, 2.f, 12000.f, 4.f, 6500.f, 15.f, 150.f, 3.f, 1.00f, 0.20f, 5000.f, 0.20f, 0.50f, 0.50f, 0.25f, { 0.40f, 0.40f, 0.35f, 0.40f, 0.15f, 0.15f, 0.00f } },
    }};
    return vibes;
}

inline const NovaVibe& getNovaVibe (int index)
{
    const auto& v = getNovaVibes();
    return v[(size_t) (index < 0 ? 0 : (index >= kNumVibes ? kNumVibes - 1 : index))];
}
