#pragma once

#include <array>
#include <vector>

#include "NovaVibes.h"

// Factory presets: sounds inspired by artists, named by vibe/wordplay - the
// plugin never shows an artist's real name. The "inspired by" notes below are
// for us only.
//
// Each preset picks a base vibe (its EQ voicing, de-ess band, delay/reverb
// character - see NovaVibes.h) and sets the seven macros. Loading one keeps
// Listen's voice corrections (if the voice was analysed) and the Key setting,
// since the key belongs to the song, not the preset.
//
// Vibe index: 0 Reggaeton Bright, 1 Melodic Trap, 2 R&B Smooth,
//             3 Pop Polished, 4 Drill Dark, 5 Raw Natural
struct NovaFactoryPreset
{
    const char* name;
    const char* group;
    int vibe;          // base vibe: card shown, and Listen plans the voice's corrections with it
    NovaVibe voicing;  // the preset's full recipe (EQ, de-ess band, compressor, delay, reverb,
                       // tuning style, saturation) - macros inside are the knob positions
    float mix      = 1.f;
    float outputDb = 0.f;
};

// Preset that uses its base vibe's recipe with its own knob positions.
inline NovaFactoryPreset fp (const char* name, const char* group, int vibe, std::array<float, 7> macros)
{
    NovaVibe v = getNovaVibe (vibe);
    v.name = name;
    for (size_t i = 0; i < 7; ++i) v.macros[i] = macros[i];
    return { name, group, vibe, v };
}

// Preset with its own signature recipe.
//   tone:  hpfMaxHz, mudCutMaxDb, presenceHz, presenceMaxDb, airHz, airMaxDb, deEssHz
//   comp:  attackMs, releaseMs, ratioMax
//   space: delayBeats, delayFeedback, delayLowpassHz, delaySend, reverbRoom, reverbDamping, reverbSend
//   knobs: Clean, De-Ess, Punch, Air, Warmth, Space, Tune
//   tune:  retuneScale, humanize, vibrato     sat: driveMax, mixMax
struct NovaRecipe
{
    float hpf, mud, presHz, presDb, airHz, airDb, deEssHz;
    float att, rel, ratio;
    float dBeats, dFb, dLp, dSend, room, damp, rSend;
    std::array<float, 7> knobs;
    float retuneScale = 1.f, humanize = 0.f, vibrato = 0.f, satDrive = 5.f, satMix = 0.75f;
};

inline NovaFactoryPreset fpv (const char* name, const char* group, int vibe, const NovaRecipe& r)
{
    NovaVibe v { name, r.hpf, r.mud, r.presHz, r.presDb, r.airHz, r.airDb, r.deEssHz,
                 r.att, r.rel, r.ratio,
                 r.dBeats, r.dFb, r.dLp, r.dSend, r.room, r.damp, r.rSend,
                 { r.knobs[0], r.knobs[1], r.knobs[2], r.knobs[3], r.knobs[4], r.knobs[5], r.knobs[6] } };
    v.retuneScale = r.retuneScale; v.tuneHumanize = r.humanize; v.tuneVibrato = r.vibrato;
    v.satDriveMax = r.satDrive;    v.satMixMax    = r.satMix;
    return { name, group, vibe, v };
}

inline const std::vector<NovaFactoryPreset>& getNovaFactoryPresets()
{
    static const std::vector<NovaFactoryPreset> presets {
        // ── Latin ─────────────────────────────────────────────────────────
        fpv ("Puerto Rico", "LATIN", 0, { 110.00f, 4.00f, 2800.00f, 5.00f, 11000.00f, 6.00f, 6000.00f,
              4.00f, 70.00f, 6.00f,
              0.50f, 0.25f, 4500.00f, 0.30f, 0.60f, 0.55f, 0.25f,
              { 0.55f, 0.50f, 0.70f, 0.65f, 0.35f, 0.45f, 0.85f },
              0.60f, 0.10f, 0.00f, 5.00f, 0.75f }), // Bad Bunny: deep baritone kept full, mid-forward 2.8k, short 1/8 slap, medium plate, fast tune
        fpv ("Odisea Nights", "LATIN", 0, { 140.00f, 5.00f, 4500.00f, 5.00f, 12500.00f, 10.00f, 6500.00f,
              3.00f, 60.00f, 6.00f,
              0.75f, 0.35f, 6000.00f, 0.35f, 0.78f, 0.35f, 0.40f,
              { 0.60f, 0.60f, 0.70f, 0.85f, 0.20f, 0.60f, 1.00f },
              0.50f, 0.00f, 0.00f, 5.00f, 0.75f }), // Ozuna: bright high tenor, very airy, heavy tune, lush dotted-1/8 echoes and big bright verb
        fpv ("Medellin Neon", "LATIN", 0, { 140.00f, 5.00f, 3800.00f, 5.00f, 12000.00f, 8.00f, 6500.00f,
              3.00f, 50.00f, 6.00f,
              0.50f, 0.22f, 6000.00f, 0.25f, 0.55f, 0.45f, 0.22f,
              { 0.60f, 0.55f, 0.70f, 0.75f, 0.25f, 0.45f, 0.90f },
              0.70f, 0.00f, 0.00f, 5.00f, 0.75f }), // J Balvin: clean pop-reggaeton, crisp mids, tight comp, short 1/8 echo, modest room
        fpv ("Pink Paisa", "LATIN", 3, { 120.00f, 4.00f, 5000.00f, 4.00f, 13000.00f, 9.00f, 7800.00f,
              5.00f, 80.00f, 5.00f,
              0.75f, 0.30f, 6000.00f, 0.25f, 0.70f, 0.40f, 0.32f,
              { 0.60f, 0.60f, 0.70f, 0.80f, 0.25f, 0.50f, 0.85f },
              0.70f, 0.10f, 0.00f, 5.00f, 0.75f }), // Karol G: bright glossy female top, airy, smooth tune, dotted-1/8 echoes
        fpv ("Perreo Clasico", "LATIN", 0, { 140.00f, 5.00f, 2500.00f, 6.00f, 10000.00f, 4.00f, 6500.00f,
              2.00f, 40.00f, 8.00f,
              0.25f, 0.15f, 6000.00f, 0.25f, 0.45f, 0.60f, 0.15f,
              { 0.65f, 0.50f, 0.85f, 0.60f, 0.40f, 0.30f, 0.40f },
              1.20f, 0.30f, 0.00f, 7.00f, 0.80f }), // Daddy Yankee: 2000s reggaeton - aggressive mids, hard comp, 1/16 slap, dry, light tune
        fpv ("Afrodisiaco Silk", "LATIN", 2, { 110.00f, 4.00f, 3200.00f, 3.00f, 12000.00f, 9.00f, 6000.00f,
              8.00f, 110.00f, 4.50f,
              1.00f, 0.38f, 4500.00f, 0.35f, 0.85f, 0.40f, 0.45f,
              { 0.55f, 0.55f, 0.60f, 0.75f, 0.35f, 0.65f, 0.80f },
              0.60f, 0.15f, 0.30f, 5.00f, 0.75f }), // Rauw Alejandro: silky airy falsetto, smooth comp, lush 1/4 echoes and big verb
        fpv ("Hasta la Muerte", "LATIN", 1, { 150.00f, 5.00f, 2400.00f, 4.00f, 9500.00f, 4.00f, 5800.00f,
              3.00f, 60.00f, 8.00f,
              0.50f, 0.35f, 3200.00f, 0.35f, 0.70f, 0.65f, 0.30f,
              { 0.65f, 0.50f, 0.75f, 0.60f, 0.45f, 0.55f, 1.00f },
              0.35f, 0.00f, 0.00f, 8.00f, 0.90f }), // Anuel AA: raspy dark trap-latino, hard tune, saturated, dark echoes
        fpv ("Medellin Green", "LATIN", 0, { 140.00f, 5.00f, 3000.00f, 3.00f, 11500.00f, 8.00f, 6500.00f,
              8.00f, 120.00f, 5.00f,
              0.75f, 0.38f, 4500.00f, 0.38f, 0.82f, 0.45f, 0.40f,
              { 0.60f, 0.55f, 0.65f, 0.75f, 0.35f, 0.55f, 0.95f },
              0.60f, 0.15f, 0.20f, 5.00f, 0.75f }), // Feid: soft breathy laid-back vocal, smooth tune, dreamy wet echoes
        fpv ("San Juan King", "LATIN", 0, { 140.00f, 5.00f, 3000.00f, 5.00f, 10500.00f, 5.00f, 6500.00f,
              2.00f, 45.00f, 7.00f,
              0.50f, 0.20f, 6000.00f, 0.22f, 0.50f, 0.60f, 0.18f,
              { 0.65f, 0.50f, 0.80f, 0.65f, 0.40f, 0.40f, 0.90f },
              0.60f, 0.00f, 0.00f, 5.00f, 0.75f }), // Myke Towers: rap-reggaeton, punchy mids, tight comp, short echo, small room
        fpv ("Medellin Lover", "LATIN", 0, { 100.00f, 5.00f, 3500.00f, 4.00f, 12000.00f, 8.00f, 6500.00f,
              6.00f, 90.00f, 5.00f,
              0.75f, 0.30f, 6000.00f, 0.28f, 0.72f, 0.45f, 0.35f,
              { 0.60f, 0.60f, 0.65f, 0.75f, 0.30f, 0.50f, 0.85f },
              0.70f, 0.20f, 0.20f, 5.00f, 0.75f }), // Maluma: romantic pop-reggaeton, warm body kept, glossy top
        fpv ("Pepas Rave", "LATIN", 0, { 140.00f, 5.00f, 4000.00f, 5.00f, 12000.00f, 9.00f, 6500.00f,
              3.00f, 50.00f, 7.00f,
              0.75f, 0.45f, 6000.00f, 0.40f, 0.85f, 0.30f, 0.40f,
              { 0.65f, 0.55f, 0.75f, 0.75f, 0.35f, 0.60f, 0.95f },
              0.50f, 0.00f, 0.00f, 6.00f, 0.80f }), // Farruko: EDM-reggaeton, big bright and wide, long echoes
        fpv ("Corrido Tumbado", "LATIN", 5, { 120.00f, 3.00f, 1800.00f, 5.00f, 10000.00f, 3.00f, 6000.00f,
              5.00f, 80.00f, 5.00f,
              0.25f, 0.12f, 5000.00f, 0.20f, 0.45f, 0.60f, 0.18f,
              { 0.60f, 0.55f, 0.70f, 0.55f, 0.45f, 0.35f, 0.40f },
              1.50f, 0.50f, 0.60f, 6.00f, 0.80f }), // Peso Pluma: nasal, raw mid-forward, slapback, almost untuned

        // ── Hip-hop ───────────────────────────────────────────────────────
        fpv ("Compton Poet", "HIP-HOP", 5, { 100.00f, 3.00f, 3500.00f, 5.00f, 11000.00f, 6.00f, 6500.00f,
              4.00f, 60.00f, 6.00f,
              0.25f, 0.10f, 5000.00f, 0.12f, 0.40f, 0.60f, 0.12f,
              { 0.60f, 0.55f, 0.75f, 0.55f, 0.35f, 0.30f, 0.10f },
              1.50f, 0.50f, 0.50f, 5.00f, 0.75f }), // Kendrick Lamar: dry upfront crisp delivery, punchy, barely any space or tune
        fpv ("Ville Storyteller", "HIP-HOP", 5, { 90.00f, 3.00f, 2500.00f, 3.00f, 9500.00f, 3.00f, 6500.00f,
              10.00f, 120.00f, 4.00f,
              0.50f, 0.12f, 3500.00f, 0.12f, 0.50f, 0.70f, 0.15f,
              { 0.60f, 0.50f, 0.70f, 0.50f, 0.45f, 0.25f, 0.05f },
              1.50f, 0.60f, 0.60f, 4.00f, 0.60f }), // J. Cole: warm, dry, mid-heavy and soulful
        fpv ("Heartbreak 808", "HIP-HOP", 1, { 150.00f, 5.00f, 3000.00f, 3.00f, 10000.00f, 6.00f, 6500.00f,
              6.00f, 100.00f, 5.00f,
              1.00f, 0.45f, 3500.00f, 0.40f, 0.90f, 0.55f, 0.50f,
              { 0.60f, 0.55f, 0.70f, 0.70f, 0.40f, 0.65f, 1.00f },
              0.20f, 0.00f, 0.00f, 5.00f, 0.75f }), // Kanye (808s era): robotic autotune, cold, huge dark verb and long echoes
        fpv ("New Orleans Martian", "HIP-HOP", 1, { 150.00f, 5.00f, 3200.00f, 6.00f, 11000.00f, 6.00f, 6500.00f,
              3.00f, 50.00f, 8.00f,
              0.50f, 0.25f, 5000.00f, 0.25f, 0.55f, 0.55f, 0.20f,
              { 0.65f, 0.50f, 0.80f, 0.65f, 0.45f, 0.40f, 0.90f },
              0.30f, 0.00f, 0.00f, 8.00f, 0.90f }), // Lil Wayne: raspy crunchy hard tune, mid-forward
        fpv ("Pink Queen", "HIP-HOP", 3, { 120.00f, 4.00f, 4500.00f, 6.00f, 12500.00f, 9.00f, 8000.00f,
              1.50f, 40.00f, 8.00f,
              0.50f, 0.15f, 6000.00f, 0.15f, 0.45f, 0.50f, 0.15f,
              { 0.65f, 0.65f, 0.85f, 0.80f, 0.25f, 0.40f, 0.55f },
              0.80f, 0.20f, 0.00f, 5.00f, 0.75f }), // Nicki Minaj: very bright, aggressive, heavily compressed and upfront
        fpv ("Bronx Bodega", "HIP-HOP", 3, { 120.00f, 4.00f, 3500.00f, 6.00f, 11000.00f, 7.00f, 7500.00f,
              1.50f, 40.00f, 8.00f,
              0.25f, 0.10f, 6000.00f, 0.12f, 0.40f, 0.60f, 0.12f,
              { 0.65f, 0.60f, 0.85f, 0.70f, 0.30f, 0.30f, 0.35f },
              1.20f, 0.40f, 0.30f, 6.00f, 0.80f }), // Cardi B: loud, punchy, bright mids, dry
        fpv ("Houston Hottie", "HIP-HOP", 3, { 110.00f, 4.00f, 3000.00f, 6.00f, 11000.00f, 7.00f, 7500.00f,
              1.50f, 45.00f, 8.00f,
              0.25f, 0.10f, 6000.00f, 0.12f, 0.40f, 0.60f, 0.12f,
              { 0.65f, 0.60f, 0.85f, 0.70f, 0.35f, 0.30f, 0.35f },
              1.20f, 0.40f, 0.00f, 6.00f, 0.80f }), // Megan Thee Stallion: thick, punchy, bright and confident
        fpv ("Charlotte Bounce", "HIP-HOP", 5, { 90.00f, 3.00f, 3500.00f, 5.00f, 11000.00f, 6.00f, 6500.00f,
              2.00f, 40.00f, 7.00f,
              0.25f, 0.10f, 5000.00f, 0.10f, 0.35f, 0.60f, 0.10f,
              { 0.65f, 0.50f, 0.85f, 0.55f, 0.35f, 0.20f, 0.20f },
              1.30f, 0.40f, 0.00f, 5.00f, 0.75f }), // DaBaby: dry, fast, punchy, minimal effects
        fpv ("Zone 6 Ice", "HIP-HOP", 4, { 100.00f, 6.00f, 2200.00f, 3.00f, 8500.00f, 2.00f, 5500.00f,
              6.00f, 90.00f, 6.00f,
              0.50f, 0.15f, 2800.00f, 0.15f, 0.45f, 0.75f, 0.15f,
              { 0.70f, 0.45f, 0.80f, 0.40f, 0.55f, 0.30f, 0.35f },
              1.20f, 0.30f, 0.00f, 5.00f, 0.70f }), // 21 Savage: low monotone, dark, dry and close

        // ── Melodic trap ──────────────────────────────────────────────────
        fpv ("Pluto Fuel", "MELODIC TRAP", 1, { 150.00f, 5.00f, 2600.00f, 4.00f, 10000.00f, 5.00f, 6500.00f,
              4.00f, 70.00f, 7.00f,
              0.75f, 0.45f, 3500.00f, 0.40f, 0.82f, 0.60f, 0.40f,
              { 0.65f, 0.50f, 0.80f, 0.65f, 0.50f, 0.45f, 1.00f },
              0.30f, 0.00f, 0.00f, 7.00f, 0.85f }), // Future: heavy autotune, dark, wet and dreamy, saturated
        fpv ("Cactus Rage", "MELODIC TRAP", 1, { 150.00f, 5.00f, 3000.00f, 4.00f, 10500.00f, 6.00f, 6500.00f,
              3.00f, 60.00f, 8.00f,
              0.75f, 0.50f, 4000.00f, 0.45f, 0.95f, 0.50f, 0.55f,
              { 0.65f, 0.50f, 0.75f, 0.70f, 0.55f, 0.85f, 1.00f },
              0.30f, 0.00f, 0.00f, 9.00f, 0.90f }), // Travis Scott: hard tune, huge psychedelic verb and echoes, distorted edge
        fpv ("Lucid Rooms", "MELODIC TRAP", 1, { 150.00f, 5.00f, 3500.00f, 4.00f, 11000.00f, 7.00f, 6500.00f,
              4.00f, 70.00f, 6.00f,
              0.75f, 0.35f, 5000.00f, 0.32f, 0.78f, 0.45f, 0.38f,
              { 0.60f, 0.55f, 0.70f, 0.70f, 0.30f, 0.65f, 0.90f },
              0.55f, 0.10f, 0.00f, 5.00f, 0.75f }), // Juice WRLD: emo melodic, tuned, airy top, medium-big verb
        fpv ("Slime Melody", "MELODIC TRAP", 1, { 150.00f, 5.00f, 4500.00f, 5.00f, 12000.00f, 9.00f, 7500.00f,
              3.00f, 50.00f, 7.00f,
              0.50f, 0.30f, 5000.00f, 0.30f, 0.65f, 0.45f, 0.28f,
              { 0.65f, 0.55f, 0.75f, 0.80f, 0.35f, 0.50f, 1.00f },
              0.40f, 0.00f, 0.00f, 5.00f, 0.75f }), // Young Thug: high, bright, quirky, heavy tune
        fpv ("Casablanca Bronx", "MELODIC TRAP", 1, { 140.00f, 5.00f, 2500.00f, 3.00f, 10000.00f, 5.00f, 6000.00f,
              6.00f, 100.00f, 6.00f,
              1.00f, 0.40f, 3500.00f, 0.40f, 0.78f, 0.60f, 0.35f,
              { 0.65f, 0.50f, 0.75f, 0.55f, 0.50f, 0.60f, 1.00f },
              0.40f, 0.00f, 0.00f, 7.00f, 0.85f }), // French Montana: warm dark mids, buttery hard tune, saturated, long dark 1/4 echoes, wide room
        fpv ("Hollywood Gold", "MELODIC TRAP", 3, { 110.00f, 4.00f, 3200.00f, 4.00f, 11500.00f, 8.00f, 7000.00f,
              6.00f, 100.00f, 5.00f,
              1.00f, 0.38f, 4500.00f, 0.35f, 0.88f, 0.45f, 0.45f,
              { 0.60f, 0.55f, 0.70f, 0.75f, 0.35f, 0.60f, 0.80f },
              0.60f, 0.15f, 0.20f, 5.00f, 0.70f }), // Post Malone: warm, wide, lush tuned vocal with big verb
        fpv ("Atlanta Hustle", "MELODIC TRAP", 1, { 150.00f, 5.00f, 3000.00f, 5.00f, 10500.00f, 6.00f, 6500.00f,
              3.00f, 50.00f, 7.00f,
              0.50f, 0.20f, 5000.00f, 0.20f, 0.50f, 0.60f, 0.20f,
              { 0.65f, 0.50f, 0.80f, 0.65f, 0.40f, 0.35f, 0.85f },
              0.60f, 0.00f, 0.00f, 6.00f, 0.80f }), // Lil Baby: raspy, fast, mid-forward, moderate tune, light space
        fpv ("Drip Season Glide", "MELODIC TRAP", 1, { 150.00f, 5.00f, 2800.00f, 3.00f, 11000.00f, 7.00f, 6500.00f,
              6.00f, 90.00f, 5.00f,
              0.75f, 0.32f, 4500.00f, 0.30f, 0.70f, 0.50f, 0.32f,
              { 0.60f, 0.55f, 0.70f, 0.75f, 0.35f, 0.50f, 0.95f },
              0.45f, 0.10f, 0.00f, 5.00f, 0.75f }), // Gunna: smooth, mellow, laid-back glide tune
        fpv ("Eternal Rockstar", "MELODIC TRAP", 1, { 150.00f, 5.00f, 4000.00f, 5.00f, 12000.00f, 9.00f, 7500.00f,
              2.00f, 45.00f, 7.00f,
              0.50f, 0.32f, 5000.00f, 0.30f, 0.70f, 0.40f, 0.32f,
              { 0.65f, 0.55f, 0.75f, 0.80f, 0.35f, 0.55f, 1.00f },
              0.35f, 0.00f, 0.00f, 6.00f, 0.80f }), // Lil Uzi Vert: bright, energetic, hard tune, wide
        fpv ("Vamp Mode", "MELODIC TRAP", 1, { 160.00f, 5.00f, 2800.00f, 6.00f, 9000.00f, 3.00f, 6500.00f,
              1.00f, 40.00f, 10.00f,
              0.50f, 0.30f, 3000.00f, 0.30f, 0.60f, 0.70f, 0.30f,
              { 0.70f, 0.50f, 0.85f, 0.55f, 0.70f, 0.45f, 1.00f },
              0.40f, 0.00f, 0.00f, 12.00f, 1.00f }), // Playboi Carti: distorted, raw, lo-fi punk grit
        fpv ("Compton Melody", "MELODIC TRAP", 1, { 150.00f, 5.00f, 2800.00f, 5.00f, 10500.00f, 6.00f, 6500.00f,
              3.00f, 55.00f, 7.00f,
              0.75f, 0.30f, 5000.00f, 0.30f, 0.68f, 0.50f, 0.30f,
              { 0.60f, 0.55f, 0.75f, 0.70f, 0.35f, 0.45f, 0.95f },
              0.40f, 0.00f, 0.00f, 5.00f, 0.75f }), // Roddy Ricch: melodic, warm mids, hard tune
        fpv ("Chicago Piano", "MELODIC TRAP", 1, { 150.00f, 5.00f, 3000.00f, 4.00f, 10500.00f, 6.00f, 6500.00f,
              4.00f, 70.00f, 6.00f,
              0.75f, 0.30f, 5000.00f, 0.30f, 0.70f, 0.50f, 0.32f,
              { 0.65f, 0.50f, 0.75f, 0.60f, 0.40f, 0.45f, 0.85f },
              0.50f, 0.00f, 0.00f, 5.00f, 0.75f }), // Polo G: melodic pain, clean smooth tune
        fpv ("Chicago Voice", "MELODIC TRAP", 1, { 150.00f, 5.00f, 2700.00f, 4.00f, 10000.00f, 5.00f, 6500.00f,
              3.00f, 60.00f, 7.00f,
              0.75f, 0.35f, 4000.00f, 0.35f, 0.72f, 0.55f, 0.35f,
              { 0.65f, 0.50f, 0.80f, 0.60f, 0.45f, 0.40f, 1.00f },
              0.25f, 0.00f, 0.00f, 6.00f, 0.80f }), // Lil Durk: very heavy melodic autotune, darker top
        fpv ("Baton Rouge Pain", "MELODIC TRAP", 1, { 150.00f, 5.00f, 2800.00f, 5.00f, 10000.00f, 5.00f, 6500.00f,
              3.00f, 60.00f, 8.00f,
              0.50f, 0.30f, 3800.00f, 0.30f, 0.65f, 0.60f, 0.28f,
              { 0.70f, 0.50f, 0.80f, 0.55f, 0.50f, 0.40f, 0.85f },
              0.45f, 0.00f, 0.00f, 8.00f, 0.90f }), // NBA YoungBoy: raspy, emotional, saturated, tuned
        fpv ("Soul Trap Ballad", "MELODIC TRAP", 2, { 100.00f, 4.00f, 2800.00f, 3.00f, 10500.00f, 6.00f, 6000.00f,
              8.00f, 110.00f, 5.00f,
              1.00f, 0.35f, 4000.00f, 0.35f, 0.85f, 0.55f, 0.42f,
              { 0.60f, 0.55f, 0.70f, 0.65f, 0.45f, 0.55f, 0.90f },
              0.50f, 0.10f, 0.20f, 5.00f, 0.70f }), // Rod Wave: soulful tuned ballad, warm, big verb

        // ── R&B ───────────────────────────────────────────────────────────
        fpv ("A Weekend in Canada", "R&B", 2, { 120.00f, 4.00f, 3000.00f, 3.00f, 12000.00f, 9.00f, 7000.00f,
              8.00f, 120.00f, 4.50f,
              1.50f, 0.40f, 4000.00f, 0.35f, 0.92f, 0.35f, 0.50f,
              { 0.55f, 0.60f, 0.60f, 0.80f, 0.25f, 0.80f, 0.65f },
              1.00f, 0.30f, 0.50f, 5.00f, 0.75f }), // The Weeknd: airy falsetto top, long bright hall, dark dotted-1/4 echoes, smooth comp, glossy tune keeping vibrato
        fpv ("Toronto 6AM", "R&B", 2, { 100.00f, 4.00f, 2200.00f, 2.50f, 9000.00f, 3.00f, 5500.00f,
              10.00f, 150.00f, 4.00f,
              0.50f, 0.20f, 3000.00f, 0.18f, 0.55f, 0.75f, 0.18f,
              { 0.55f, 0.50f, 0.70f, 0.45f, 0.50f, 0.30f, 0.50f },
              1.20f, 0.35f, 0.40f, 4.00f, 0.60f }), // Drake: dark, warm, close and nearly dry, rolled-off top, slow natural tune, subtle dark slap
        fpv ("Ctrl Room", "R&B", 2, { 110.00f, 4.00f, 3200.00f, 3.00f, 12000.00f, 7.00f, 6000.00f,
              10.00f, 130.00f, 4.00f,
              0.75f, 0.30f, 4500.00f, 0.28f, 0.75f, 0.50f, 0.35f,
              { 0.55f, 0.55f, 0.55f, 0.70f, 0.40f, 0.60f, 0.55f },
              1.20f, 0.40f, 0.60f, 5.00f, 0.75f }), // SZA: intimate, natural, airy, light tune that keeps vibrato
        fpv ("Blonde Hours", "R&B", 2, { 90.00f, 4.00f, 2800.00f, 2.00f, 11000.00f, 5.00f, 6000.00f,
              15.00f, 150.00f, 3.50f,
              1.00f, 0.25f, 3500.00f, 0.20f, 0.70f, 0.60f, 0.30f,
              { 0.50f, 0.50f, 0.45f, 0.60f, 0.50f, 0.55f, 0.30f },
              1.60f, 0.60f, 0.80f, 5.00f, 0.75f }), // Frank Ocean: natural, warm, intimate, roomy, barely tuned
        fpv ("Trapsoul Velvet", "R&B", 2, { 110.00f, 4.00f, 2600.00f, 3.00f, 10000.00f, 5.00f, 6000.00f,
              8.00f, 110.00f, 5.00f,
              0.75f, 0.35f, 3500.00f, 0.32f, 0.80f, 0.60f, 0.38f,
              { 0.60f, 0.50f, 0.65f, 0.60f, 0.45f, 0.55f, 0.70f },
              0.70f, 0.20f, 0.20f, 5.00f, 0.75f }), // Bryson Tiller: dark moody trap-soul, smooth tune, dark verb
        fpv ("Brampton Melodies", "R&B", 2, { 120.00f, 4.00f, 3500.00f, 4.00f, 11500.00f, 8.00f, 6800.00f,
              5.00f, 90.00f, 5.00f,
              0.75f, 0.35f, 5000.00f, 0.35f, 0.75f, 0.45f, 0.35f,
              { 0.60f, 0.55f, 0.70f, 0.75f, 0.35f, 0.60f, 0.90f },
              0.50f, 0.10f, 0.10f, 5.00f, 0.75f }), // Tory Lanez: melodic R&B, heavy fluid tune, airy top, dotted-1/8 echoes, medium-big verb
        fpv ("Mississauga 4AM", "R&B", 2, { 110.00f, 4.00f, 2500.00f, 3.00f, 9500.00f, 5.00f, 6000.00f,
              6.00f, 100.00f, 5.00f,
              0.75f, 0.42f, 3200.00f, 0.38f, 0.85f, 0.65f, 0.42f,
              { 0.60f, 0.50f, 0.65f, 0.65f, 0.40f, 0.65f, 0.85f },
              0.55f, 0.10f, 0.10f, 5.00f, 0.75f }), // PartyNextDoor: dark, wet, tuned late-night R&B
        fpv ("Virginia Breeze", "R&B", 2, { 110.00f, 4.00f, 4000.00f, 4.00f, 12500.00f, 9.00f, 7500.00f,
              4.00f, 70.00f, 5.50f,
              0.75f, 0.30f, 4500.00f, 0.28f, 0.72f, 0.40f, 0.35f,
              { 0.60f, 0.60f, 0.70f, 0.80f, 0.30f, 0.55f, 0.75f },
              0.70f, 0.20f, 0.30f, 5.00f, 0.75f }), // Chris Brown: bright polished airy R&B-pop, tight
        fpv ("Atlanta Confessions", "R&B", 2, { 110.00f, 4.00f, 3500.00f, 4.00f, 12000.00f, 8.00f, 6000.00f,
              6.00f, 100.00f, 5.00f,
              0.75f, 0.28f, 4500.00f, 0.25f, 0.72f, 0.45f, 0.35f,
              { 0.60f, 0.60f, 0.65f, 0.75f, 0.30f, 0.50f, 0.55f },
              1.00f, 0.30f, 0.50f, 5.00f, 0.75f }), // Usher: smooth polished R&B, airy, natural vibrato
        fpv ("Over It Silk", "R&B", 2, { 110.00f, 4.00f, 3000.00f, 2.50f, 11500.00f, 7.00f, 6500.00f,
              10.00f, 130.00f, 4.00f,
              0.75f, 0.30f, 4000.00f, 0.28f, 0.78f, 0.55f, 0.35f,
              { 0.55f, 0.55f, 0.55f, 0.70f, 0.45f, 0.55f, 0.60f },
              0.90f, 0.30f, 0.40f, 5.00f, 0.75f }), // Summer Walker: soft, breathy, warm, intimate
        fpv ("Wasteland Mellow", "R&B", 2, { 110.00f, 4.00f, 2400.00f, 2.00f, 9000.00f, 3.00f, 6000.00f,
              12.00f, 150.00f, 4.00f,
              0.75f, 0.32f, 3000.00f, 0.28f, 0.75f, 0.70f, 0.32f,
              { 0.55f, 0.50f, 0.55f, 0.60f, 0.50f, 0.60f, 0.45f },
              1.00f, 0.40f, 0.50f, 6.00f, 0.80f }), // Brent Faiyaz: dark lo-fi warmth, mellow and moody
        fpv ("East Atlanta Blue", "R&B", 2, { 90.00f, 4.00f, 2300.00f, 2.50f, 9000.00f, 3.00f, 6000.00f,
              10.00f, 140.00f, 4.50f,
              0.75f, 0.35f, 3000.00f, 0.30f, 0.80f, 0.70f, 0.35f,
              { 0.55f, 0.50f, 0.60f, 0.55f, 0.50f, 0.55f, 0.60f },
              0.80f, 0.30f, 0.30f, 5.00f, 0.70f }), // 6LACK: low, smoky, dark and moody

        // ── Pop ───────────────────────────────────────────────────────────
        fpv ("Barbados Gold", "POP", 3, { 130.00f, 5.00f, 4500.00f, 5.00f, 12000.00f, 8.00f, 7500.00f,
              3.00f, 60.00f, 6.00f,
              0.75f, 0.28f, 6000.00f, 0.25f, 0.65f, 0.40f, 0.30f,
              { 0.60f, 0.60f, 0.75f, 0.80f, 0.35f, 0.50f, 0.70f },
              0.70f, 0.15f, 0.20f, 6.00f, 0.85f }), // Rihanna: bright, forward 4.5k bite, tight fast comp, island grit, dotted-1/8 delay, crisp plate
        fpv ("Whisper Pop", "POP", 3, { 80.00f, 3.00f, 2500.00f, 2.00f, 10000.00f, 5.00f, 6500.00f,
              2.00f, 60.00f, 8.00f,
              0.50f, 0.12f, 3500.00f, 0.10f, 0.40f, 0.70f, 0.12f,
              { 0.55f, 0.65f, 0.75f, 0.55f, 0.45f, 0.25f, 0.40f },
              1.40f, 0.50f, 0.60f, 5.00f, 0.75f }), // Billie Eilish: whisper-close, heavily compressed, dry, low end kept
        fpv ("Ponytail Gloss", "POP", 3, { 120.00f, 4.00f, 4500.00f, 4.00f, 13000.00f, 10.00f, 8000.00f,
              5.00f, 80.00f, 5.00f,
              0.75f, 0.30f, 6000.00f, 0.28f, 0.80f, 0.35f, 0.42f,
              { 0.60f, 0.65f, 0.65f, 0.85f, 0.20f, 0.65f, 0.75f },
              0.80f, 0.30f, 0.50f, 5.00f, 0.75f }), // Ariana Grande: super airy bright gloss, lush verb
        fpv ("Disco Nostalgia", "POP", 3, { 120.00f, 4.00f, 3500.00f, 5.00f, 12000.00f, 7.00f, 7000.00f,
              3.00f, 60.00f, 6.00f,
              0.50f, 0.25f, 6000.00f, 0.22f, 0.62f, 0.40f, 0.28f,
              { 0.60f, 0.60f, 0.75f, 0.75f, 0.30f, 0.45f, 0.70f },
              0.80f, 0.20f, 0.20f, 5.00f, 0.75f }), // Dua Lipa: punchy mid-forward disco-pop, plate
        fpv ("Houston Queen", "POP", 3, { 120.00f, 4.00f, 4000.00f, 5.00f, 12500.00f, 8.00f, 7500.00f,
              4.00f, 70.00f, 5.50f,
              0.75f, 0.25f, 6000.00f, 0.22f, 0.70f, 0.40f, 0.32f,
              { 0.60f, 0.65f, 0.75f, 0.80f, 0.30f, 0.50f, 0.55f },
              1.10f, 0.40f, 0.60f, 5.00f, 0.75f }), // Beyonce: powerful, bright, polished, natural vibrato
        fpv ("Nashville Diary", "POP", 3, { 120.00f, 4.00f, 3200.00f, 4.00f, 12000.00f, 7.00f, 7000.00f,
              6.00f, 90.00f, 4.50f,
              0.75f, 0.22f, 6000.00f, 0.20f, 0.65f, 0.45f, 0.30f,
              { 0.55f, 0.60f, 0.60f, 0.70f, 0.35f, 0.45f, 0.45f },
              1.20f, 0.40f, 0.50f, 5.00f, 0.75f }), // Taylor Swift: clear storyteller pop, plate, light tune
        fpv ("Stratford Gloss", "POP", 3, { 120.00f, 4.00f, 3500.00f, 4.00f, 12500.00f, 8.00f, 7000.00f,
              5.00f, 80.00f, 5.00f,
              0.75f, 0.30f, 6000.00f, 0.28f, 0.72f, 0.40f, 0.35f,
              { 0.60f, 0.60f, 0.70f, 0.80f, 0.30f, 0.50f, 0.75f },
              0.70f, 0.20f, 0.20f, 5.00f, 0.75f }), // Justin Bieber: smooth airy tuned R&B-pop
        fpv ("Funk Hooligan", "POP", 3, { 120.00f, 4.00f, 3500.00f, 5.00f, 11500.00f, 7.00f, 7000.00f,
              3.00f, 60.00f, 6.00f,
              0.25f, 0.15f, 6000.00f, 0.15f, 0.55f, 0.45f, 0.25f,
              { 0.60f, 0.60f, 0.80f, 0.75f, 0.40f, 0.40f, 0.45f },
              1.30f, 0.50f, 0.60f, 6.00f, 0.80f }), // Bruno Mars: bright punchy retro-funk, short room
        fpv ("Retro Sugar", "POP", 3, { 90.00f, 4.00f, 2800.00f, 3.00f, 10000.00f, 5.00f, 7000.00f,
              10.00f, 120.00f, 4.00f,
              0.50f, 0.20f, 3500.00f, 0.18f, 0.60f, 0.60f, 0.25f,
              { 0.55f, 0.55f, 0.65f, 0.65f, 0.50f, 0.45f, 0.40f },
              1.40f, 0.50f, 0.70f, 6.00f, 0.85f }), // Harry Styles: warm vintage tape-pop, natural
        fpv ("Teen Heartbreak", "POP", 3, { 120.00f, 4.00f, 3500.00f, 4.00f, 12000.00f, 7.00f, 7000.00f,
              5.00f, 80.00f, 5.00f,
              0.75f, 0.22f, 6000.00f, 0.20f, 0.65f, 0.45f, 0.30f,
              { 0.60f, 0.60f, 0.70f, 0.70f, 0.35f, 0.40f, 0.45f },
              1.20f, 0.40f, 0.50f, 5.00f, 0.75f }), // Olivia Rodrigo: clear emotional pop with a little grit
        fpv ("Planet Pink", "POP", 3, { 120.00f, 4.00f, 4000.00f, 5.00f, 12500.00f, 8.00f, 7500.00f,
              3.00f, 60.00f, 6.00f,
              0.50f, 0.25f, 6000.00f, 0.25f, 0.62f, 0.40f, 0.28f,
              { 0.60f, 0.60f, 0.75f, 0.80f, 0.30f, 0.50f, 0.65f },
              0.70f, 0.20f, 0.00f, 5.00f, 0.75f }), // Doja Cat: bright playful polished pop-rap, tuned
        fpv ("Acoustic Loop", "POP", 5, { 90.00f, 3.00f, 3000.00f, 3.00f, 11000.00f, 5.00f, 6500.00f,
              12.00f, 140.00f, 3.50f,
              0.75f, 0.20f, 5000.00f, 0.15f, 0.60f, 0.50f, 0.25f,
              { 0.50f, 0.55f, 0.55f, 0.60f, 0.35f, 0.35f, 0.35f },
              1.50f, 0.60f, 0.80f, 5.00f, 0.75f }), // Ed Sheeran: natural warm close singer-songwriter

        // ── Drill ─────────────────────────────────────────────────────────
        fpv ("Woo Back Baby", "DRILL", 4, { 90.00f, 5.00f, 2000.00f, 5.00f, 8000.00f, 2.00f, 5500.00f,
              1.00f, 40.00f, 8.00f,
              0.50f, 0.15f, 2500.00f, 0.15f, 0.45f, 0.75f, 0.12f,
              { 0.70f, 0.45f, 0.85f, 0.35f, 0.65f, 0.25f, 0.20f },
              1.50f, 0.50f, 0.00f, 9.00f, 0.90f }), // Pop Smoke: deep raspy, dark, gritty and dry
        fpv ("Chiraq Cold", "DRILL", 4, { 160.00f, 6.00f, 2800.00f, 6.00f, 9500.00f, 4.00f, 6000.00f,
              1.50f, 40.00f, 8.00f,
              0.50f, 0.25f, 3200.00f, 0.22f, 0.55f, 0.65f, 0.20f,
              { 0.70f, 0.45f, 0.85f, 0.40f, 0.55f, 0.35f, 0.60f },
              0.50f, 0.00f, 0.00f, 8.00f, 0.90f }), // Chicago drill: raw aggressive, some tune, dark and saturated
        fpv ("Roadman Rhythm", "DRILL", 4, { 160.00f, 6.00f, 3000.00f, 5.00f, 10500.00f, 5.00f, 6000.00f,
              2.00f, 45.00f, 7.00f,
              0.50f, 0.15f, 3500.00f, 0.15f, 0.45f, 0.60f, 0.15f,
              { 0.70f, 0.50f, 0.80f, 0.45f, 0.45f, 0.30f, 0.35f },
              1.20f, 0.40f, 0.00f, 5.00f, 0.75f }), // UK drill: crisp mid-forward, dry, conversational
        fpv ("West London Flow", "DRILL", 4, { 160.00f, 6.00f, 3200.00f, 5.00f, 11000.00f, 6.00f, 6000.00f,
              2.50f, 50.00f, 6.00f,
              0.50f, 0.18f, 3500.00f, 0.18f, 0.50f, 0.55f, 0.18f,
              { 0.70f, 0.50f, 0.80f, 0.50f, 0.40f, 0.30f, 0.30f },
              1.00f, 0.30f, 0.00f, 5.00f, 0.75f }), // Central Cee: smooth clean crisp UK flow
        fpv ("Brooklyn Bop", "DRILL", 4, { 110.00f, 6.00f, 2400.00f, 6.00f, 9000.00f, 3.00f, 6000.00f,
              1.00f, 40.00f, 9.00f,
              0.50f, 0.20f, 3000.00f, 0.20f, 0.50f, 0.70f, 0.15f,
              { 0.70f, 0.45f, 0.85f, 0.40f, 0.55f, 0.30f, 0.30f },
              1.30f, 0.40f, 0.00f, 9.00f, 0.90f }), // Fivio Foreign: loud aggressive gritty Brooklyn drill

        // ── Afro & island ──────────────────────────────────────────────────────────
        fpv ("Lagos Sunset", "AFRO & ISLAND", 0, { 140.00f, 5.00f, 3000.00f, 3.00f, 11000.00f, 7.00f, 6500.00f,
              6.00f, 100.00f, 5.00f,
              0.75f, 0.35f, 4500.00f, 0.32f, 0.78f, 0.45f, 0.38f,
              { 0.55f, 0.55f, 0.60f, 0.70f, 0.35f, 0.55f, 0.65f },
              0.70f, 0.20f, 0.30f, 5.00f, 0.75f }), // Burna Boy / Wizkid: warm smooth laid-back afrobeats, lush
        fpv ("Afrorave Star", "AFRO & ISLAND", 0, { 140.00f, 5.00f, 3800.00f, 4.00f, 12000.00f, 8.00f, 6500.00f,
              4.00f, 70.00f, 6.00f,
              0.75f, 0.35f, 6000.00f, 0.32f, 0.75f, 0.40f, 0.35f,
              { 0.60f, 0.55f, 0.65f, 0.75f, 0.30f, 0.55f, 0.80f },
              0.50f, 0.10f, 0.00f, 5.00f, 0.75f }), // Rema: bright youthful tuned afro-rave, airy and wide
        fpv ("Lagos Velvet", "AFRO & ISLAND", 2, { 90.00f, 4.00f, 2500.00f, 2.50f, 10000.00f, 5.00f, 6000.00f,
              10.00f, 130.00f, 4.00f,
              0.75f, 0.28f, 3500.00f, 0.25f, 0.72f, 0.60f, 0.32f,
              { 0.55f, 0.55f, 0.55f, 0.65f, 0.50f, 0.50f, 0.40f },
              1.30f, 0.50f, 0.70f, 5.00f, 0.70f }), // Tems: deep smoky natural voice, warm room
        fpv ("Kingston Dancehall", "AFRO & ISLAND", 0, { 140.00f, 5.00f, 3500.00f, 6.00f, 11000.00f, 7.00f, 6500.00f,
              2.00f, 45.00f, 7.00f,
              0.50f, 0.30f, 6000.00f, 0.30f, 0.55f, 0.50f, 0.22f,
              { 0.65f, 0.50f, 0.80f, 0.70f, 0.40f, 0.40f, 0.70f },
              0.50f, 0.00f, 0.00f, 7.00f, 0.85f }), // Dancehall: bright punchy tuned, mid-forward with slap echoes

        // ── Utility ───────────────────────────────────────────────────────
        fp ("Live Take",          "UTILITY", 5, { 0.40f, 0.40f, 0.35f, 0.40f, 0.15f, 0.15f, 0.00f }),
        fp ("Spoken Word",        "UTILITY", 5, { 0.55f, 0.55f, 0.60f, 0.35f, 0.20f, 0.00f, 0.00f }),
    };
    return presets;
}
