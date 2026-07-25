# Nova Main Room — Design Document

Reference profile: `EnvironmentLibrary::novaMainRoom()`. Role: "trusted reference" — if a mix sounds good here, trust it. Minimal coloration is the entire point; this doc exists to confirm that intent is *earned*, not just declared.

## Acoustic Model (the room, independent of what's playing in it)

- **Dimensions**: professional control room, roughly 5m × 4m × 2.7m (L×W×H, ≈ 54 m³) — representative of a mid-size commercial control room, not a converted bedroom.
- **Materials/treatment**: broadband absorption at first-reflection points (side walls, ceiling above the console), bass trapping in corners, some diffusion on the rear wall to avoid a totally "dead" front-to-back slap. This is a *treated* room, not an anechoic chamber — some liveness remains, deliberately controlled.
- **Listener geometry**: symmetric sweet spot, equilateral-triangle placement, seated at the console — centered relative to the room, unlike every other environment this plugin models.
- **Reflections**: present but heavily damped by absorption at the first-reflection points — audible as "room," not as slap or flutter.
- **RT60**: short. Sabine estimate (`RT60 = 0.161·V/A`) with ~54 m³ volume and a mostly-absorptive surface set (treated walls α≈0.7–0.9 at first-reflection zones, harder floor/console/glass elsewhere) lands in the **~0.25–0.35s** range for a well-treated room — short enough that reflections read as subtle depth, not audible reverb.
- **Modal resonances**: still mathematically present (axial mode for a 5m dimension ≈ 34Hz, for 4m ≈ 43Hz) but damped by bass trapping specifically because those low frequencies are hardest to absorb — the room shouldn't have an audibly boomy resonance the way an untreated room would.

## Playback System Model (the monitors, independent of the room)

- **Main monitors**: large-format, often soffit-mounted (flush in the front wall) to eliminate front-baffle diffraction and remove the wall behind them as a reflection source. Full-range extension, roughly 30Hz–20kHz.
- **Nearfield monitors**: smaller 2-way, ~5–8" woofer + tweeter, placed close to the listener (1–1.5m). Rolls off lower end higher than the mains (commonly ~45Hz limit) since they're not relying on room gain the way soffit mains can.
- **Subwoofer**: extends below the nearfields' limit, typically crossed ~80–120Hz.
- **Amplification**: active monitors, internal amps engineered specifically for flat, low-distortion response — professional monitors are the one system in this whole plugin where "accurate" is the literal design goal, not a compromise.
- **Manufacturer tuning**: none, by design — no loudness contour, no house voicing; the entire product category exists to NOT have a "sound."
- **Directivity/imaging**: symmetric placement + controlled, well-behaved polar response preserve phantom-center accuracy — this is the one environment where imaging should not be degraded by the playback system itself.

## Perceptual Targets

Nova Main Room's target is inverted relative to the other three: success is the engineer noticing *nothing wrong*, not a specific problem being exposed. It's the calibration anchor — if a translation issue shows up here too, it's a mix problem, not an environment-modeling problem.

1. **Tonal balance trustworthiness** — the mix should sound like what was actually printed, no surprises, no single frequency region calling attention to itself.
2. **Stereo image precision** — phantom center should be stable and precise; nothing should collapse or wander.
3. **Low-end resolution/control** — the engineer should be able to trust bass/kick level decisions made here.
4. **Absence of coloration as a positive result** — "boring" and "uneventful" are the success condition, not a failure to differentiate.

## DSP Translation

| Physical cause | Model | DSP field | Current value | Justification |
|---|---|---|---|---|
| Professionally calibrated monitors, flat by design, no house voicing | Playback System | `lowShelfGain/midPeakGain/highShelfGain = 0dB` | ✅ matches | The one environment where "no coloration" is the physically correct answer, not a cop-out. |
| Short, damped RT60 (~0.25–0.35s) | Acoustic | `roomAmount = 0.15` | Directionally correct | **Gap**: `roomAmount` is a blend knob (0–1), not seconds. It isn't currently derived from the Sabine estimate above — it should be, so the *decay time baked into the synthetic IR* (`generateSyntheticIR`'s `decaySeconds` formula) actually targets ~0.3s rather than whatever the current formula happens to produce for `roomAmount=0.15`. |
| Absorption at first-reflection points | Acoustic | `earlyReflectionAmount = 0.15` | Directionally correct | Sparse early taps = correct qualitative shape (a treated room has few, controlled early reflections, not dense flutter). |
| Damped low-frequency modes (bass trapping) | Acoustic | *(none currently)* | — | **Gap, intentionally**: there's no explicit modal-resonance modeling anywhere in the engine — for this room specifically that's fine, since the whole point is that trapping suppresses the mode. Flagging as "correctly absent," not overlooked. |
| Symmetric imaging, full-range mains + nearfields, controlled directivity | Playback System | `stereoWidth = 1.0` | ✅ matches | Unchanged stereo width is the physically correct choice — nothing in this system should be narrowing the image. |
| Sweet-spot vs. off-axis seating (room-geometry side of the split) | Acoustic | `positions[]` base points | ✅ matches | Sweet Spot / Engineer Left / Engineer Right / Client Sofa correctly describe *where in the room* the listener sits. |
| Whether the monitors' own directivity penalizes off-axis listening (playback-system side of the split) | Playback System | `positions[]` bias values (lateral only, no HF rolloff) | Incomplete | **Gap**: real monitor polar response narrows with frequency, so Engineer Left/Right should lose some top end off-axis, not just shift stereo balance. This is specifically a Playback System gap, not an Acoustic one — the room doesn't cause it, the monitors' directivity does. |
| Minimal driver/amp nonlinearity (professional gear, not pushed hard) | Playback System | `compressionAmount = 0.05`, `saturationAmount = 0.02` | ✅ matches | Lowest of all four profiles, correctly — this is the one system that shouldn't be coloring anything. |

## Confidence Rating

| Acoustic | Playback System | Overall |
|---|---|---|
| 8.0/10 | 8.5/10 | 8.3/10 |

Highest overall confidence of the 4 implemented environments — deliberately, since "flat and uncolored" is a low-risk target to hit. The Acoustic score is held back by the RT60-derivation gap; the Playback score by the missing off-axis HF rolloff. Both scores reflect design-reasoning confidence, not a listening-test result — no blind listening has happened yet (see Reference Validation, `00-methodology.md`).

**Summary**: Nova Main Room is the cleanest case of the two-layer split doing real work — almost everything here is Playback System (accurate monitors, no voicing), with the Acoustic Model contributing only a light, damped room signature. The two gaps are cleanly separable now too: the RT60-derivation gap is purely Acoustic; the off-axis HF-rolloff gap is purely Playback System (monitor directivity, not room behavior).
