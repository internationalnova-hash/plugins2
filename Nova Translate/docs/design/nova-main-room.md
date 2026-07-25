# Nova Main Room — Design Document

Reference profile: `EnvironmentLibrary::novaMainRoom()`. Role: "trusted reference" — if a mix sounds good here, trust it. Minimal coloration is the entire point; this doc exists to confirm that intent is *earned*, not just declared.

## 1. Physical Characteristics

- **Room**: professional control room, non-domestic proportions, roughly 5m × 4m × 2.7m (L×W×H, ≈ 54 m³) — representative of a mid-size commercial control room, not a converted bedroom.
- **Treatment**: broadband absorption at first-reflection points (side walls, ceiling above the console), bass trapping in corners, some diffusion on the rear wall to avoid a totally "dead" front-to-back slap. This is a *treated* room, not an anechoic chamber — some liveness remains, deliberately controlled.
- **Listener position**: symmetric sweet spot, equilateral-triangle placement between the two main monitors, seated at the console.

## 2. Playback System

- **Main monitors**: large-format, often soffit-mounted (flush in the front wall) to eliminate front-baffle diffraction and remove the wall behind them as a reflection source. Full-range extension, roughly 30Hz–20kHz.
- **Nearfield monitors**: smaller 2-way, ~5–8" woofer + tweeter, placed close to the listener (1–1.5m). Rolls off lower end higher than the mains (commonly ~45Hz limit) since they're not relying on room gain the way soffit mains can.
- **Subwoofer**: extends below the nearfields' limit, typically crossed ~80–120Hz.
- **Amplification**: active monitors, internal amps engineered specifically for flat, low-distortion response — professional monitors are the one system in this whole plugin where "accurate" is the literal design goal, not a compromise.

## 3. Acoustic Behavior

- **Early reflections**: present but heavily damped by absorption at the first-reflection points — audible as "room," not as slap or flutter.
- **RT60**: short. Sabine estimate with ~54 m³ volume and a mostly-absorptive surface set (treated walls α≈0.7–0.9 at first-reflection zones, harder floor/console/glass elsewhere) lands in the **~0.25–0.35s** range for a well-treated room — short enough that reflections read as subtle depth, not audible reverb.
- **Modal resonances**: still mathematically present (axial mode for a 5m dimension ≈ 34Hz, for 4m ≈ 43Hz) but damped by bass trapping specifically because those low frequencies are hardest to absorb — the room shouldn't have an audibly boomy resonance the way an untreated room would.
- **Stereo image**: precise. Symmetric placement + controlled reflections preserve phantom-center accuracy — this is the one environment where imaging should NOT be degraded.

## 4. DSP Translation

| Physical cause | DSP field | Current value | Justification |
|---|---|---|---|
| Professionally calibrated monitors, flat by design | `lowShelfGain/midPeakGain/highShelfGain = 0dB` | ✅ matches | The one environment where "no coloration" is the physically correct answer, not a cop-out. |
| Short, damped RT60 (~0.25–0.35s) | `roomAmount = 0.15` | Directionally correct | **Gap**: `roomAmount` is a blend knob (0–1), not seconds. It isn't currently derived from the Sabine estimate above — it should be, so the *decay time baked into the synthetic IR* (`generateSyntheticIR`'s `decaySeconds` formula) actually targets ~0.3s rather than whatever the current formula happens to produce for `roomAmount=0.15`. Worth checking against the formula's real output once DSP changes are unfrozen. |
| Absorption at first-reflection points | `earlyReflectionAmount = 0.15` | Directionally correct | Sparse early taps = correct qualitative shape (a treated room has few, controlled early reflections, not dense flutter). |
| Damped low-frequency modes (bass trapping) | *(none currently)* | — | **Gap**: there's no explicit modal-resonance modeling anywhere in the engine (see methodology doc) — for this room specifically that's fine, since the whole point is that trapping suppresses the mode. Nothing to add here; flagging it as "intentionally absent," not overlooked. |
| Symmetric imaging, full-range mains + nearfields | `stereoWidth = 1.0` | ✅ matches | Unchanged stereo width is the physically correct choice — nothing in this system should be narrowing the image. |
| Sweet-spot vs. off-axis listening | `positions[]`: Sweet Spot (neutral), Engineer Left/Right (±0.2 lateral bias), Client Sofa (0.5 diffuse) | Directionally correct, incomplete | **Gap**: real monitor directivity means off-axis listening loses high-frequency energy (polar response narrows with frequency), not just stereo balance. Engineer Left/Right currently only shifts lateral bias — a physically fuller model would also roll off a little high-frequency content off-axis. Not done yet; flagged for a future DSP change once unfrozen. |
| Minimal driver/amp nonlinearity (professional gear, not pushed hard) | `compressionAmount = 0.05`, `saturationAmount = 0.02` | ✅ matches | Lowest of all four profiles, correctly — this is the one system that shouldn't be coloring anything. |

**Summary**: Nova Main Room is the environment closest to "physically justified" already, because its defining physical property (accurate reference monitoring) IS the near-flat/near-clean numbers already in the profile. The two real gaps are (1) tying `roomAmount`/IR decay to an actual Sabine-estimated RT60 rather than an arbitrary 0–1 value, and (2) off-axis high-frequency rolloff for Engineer Left/Right, which the current lateral-bias-only model doesn't capture.
