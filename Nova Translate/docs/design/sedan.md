# Sedan — Design Document

Reference profile: `EnvironmentLibrary::sedan()`. Role: "primary translation check" — must expose vocal balance, bass buildup, kick translation, and upper-mid harshness by ear, immediately.

## 1. Physical Characteristics

- **Cabin volume**: small, roughly 2.5–3 m³ interior air volume. Interior length (~firewall to rear seatback) ≈ 2.0–2.2m.
- **Glass**: large windshield + side windows, highly reflective (α ≈ 0.03–0.05) — a bigger contributor to early reflections, per unit area, than almost anything else in the cabin.
- **Dashboard**: hard plastic, also low absorption (α ≈ 0.05–0.1), sits directly in front of dash-mounted tweeters — a near-field reflective surface, not a distant wall.
- **Seats**: fabric or leather, moderate absorption (α ≈ 0.3–0.5 mid/high) — the main damping surface in the cabin, concentrated at ear height.
- **Listener offset**: the driver is NOT centered between the L/R speakers — asymmetric to both the physical geometry and (in base-trim systems) the crossover/EQ tuning, unlike a studio's deliberately symmetric setup.

## 2. Playback System

- **Door woofers**: 5.25"–6.5" typical, mounted low in the doors (below ear height) — creates a real path-length/height mismatch between driver and ear that a flat in-room monitor doesn't have.
- **Tweeters**: small (~1" dome), A-pillar or dash-mounted, narrow high-frequency dispersion (beaming) — off-axis seats lose top end for real physical reasons, not just "far away."
- **Crossover**: passive, 2-way, typical woofer/tweeter handoff ≈ 2.5–3.5kHz. Passive crossovers commonly show a small response irregularity (dip or bump) right at the handoff frequency rather than a clean transition.
- **Amplifier**: base-trim OEM head-unit power is modest, roughly 15–25W/channel — meaningfully lower headroom than a studio monitor or premium system, so it compresses/soft-limits sooner at real listening levels.
- **Subwoofer**: often absent or small in base trims — most of the "bass" a listener hears in an ordinary sedan is cabin resonance reinforcing the door woofers' limited low end, not genuine sub-bass extension.

## 3. Acoustic Behavior

- **Early reflections**: very fast. Windshield/dashboard proximity (~0.4–0.6m extra path) puts first reflections at roughly **1–3ms** — physically incapable of reading as "room ambience"; it reads as a bright, glassy slap right on top of the direct sound.
- **Modal resonance**: the defining low-frequency behavior. Cabin length ≈ 2.0–2.2m gives a first axial mode at:

  ```
  f = 343 / (2 × 2.1) ≈ 82 Hz
  ```

  This is *why* sedans sound bass-heavy at a specific frequency rather than generically boomy — it's a standing wave reinforced by the door woofers' own natural rolloff sitting right in the same region.
- **Frequency response**: door woofers alone typically -3dB around 80–100Hz without a subwoofer; the crossover region (~2.5–3.5kHz) is where passive-crossover irregularities and the tweeter's beaming onset both live — which is also uncomfortably close to the ear's own most sensitive range, a large part of why cheap car audio reads as "harsh."
- **Dynamics**: amplifier headroom + cabin road noise floor together push perceived dynamic range down — both a real compression mechanism (amp limiting) and a masking mechanism (noise floor), which are physically distinct even though they're both loosely "the car makes things sound more compressed."
- **Stereo image**: compromised by driver offset, asymmetric path lengths, and glass reflections — narrower and less precise than a studio, not by taste but by physical asymmetry.

## 4. DSP Translation

| Physical cause | DSP field | Current value | Justification |
|---|---|---|---|
| Door woofer low-end limit (~80–100Hz -3dB) | `lowCutHz = 90` | ✅ matches | Right in the physically correct range for a 5.25–6.5" door woofer without a sub. |
| Cabin modal resonance (~82Hz, calculated above) | `lowShelfGain = +3dB @ 150Hz` | **Gap — wrong shape and wrong frequency** | This should be a resonant **peak** near 80–85Hz (modeling the standing wave), not a broad shelf centered at 150Hz. A shelf boosts everything above 150Hz; the real phenomenon is a narrow buildup at one specific modal frequency sitting almost exactly at the highpass corner. Flagged as the clearest DSP-shape gap in the whole plugin — needs a peaking filter, not a shelf, once unfrozen. |
| Passive crossover irregularity + tweeter beaming onset (~2.5–3.5kHz) | `midPeakGain = +2.5dB @ 2500Hz, Q1.2` | Right region, wrong intent | This was previously tuned as a generic "presence boost." Reframed correctly: it should represent the crossover-region irregularity, which is typically NOT a clean boost — real passive crossovers just as often show a small suck-out right at handoff as a peak. Needs ear-verification against a real car before deciding boost-vs-dip is more accurate here. |
| High-frequency rolloff from tweeter beaming + windshield absorption of very top end | `highShelfGain = -4dB @ 6000Hz` | Partially right, wrong physical story | This currently *reduces* the very content (upper-mid harshness) the brief wants exposed — see the tuning-report finding from Phase 2. The physically correct read: tweeter beaming reduces top-end **off-axis**, but doesn't remove the crossover-region harshness on-axis. The −4dB@6kHz shelf should probably shrink or move higher (genuine HF rolloff is more like 8–10kHz+ for a reasonable tweeter), while the 2.5–3.5kHz region (above) should carry the harshness the brief is asking for. |
| Fast (~1–3ms) glass/dashboard reflections | `earlyReflectionAmount = 0.6`, tap spacing in `generateSyntheticIR` | Directionally correct | High `earlyReflectionAmount` correctly biases toward dense/early taps; **gap** — the synthetic IR generator isn't currently told "these taps must land inside ~1–3ms," it just gets more of them scaled by this parameter. Worth checking the actual tap timing against this number once unfrozen. |
| OEM amplifier headroom (~15–25W/channel, sooner-limiting) | `compressionAmount = 0.3` | Directionally correct | Second-highest of the 4 profiles (behind Smartphone) — correct ordering, since a car amp has more headroom than a phone speaker but far less than a monitor or premium system. |
| Road noise floor (separate mechanism from amp limiting) | `noiseAmount = 0.15` | Correctly modeled as separate | Good — this session already treats road noise (ambience bed) and amp compression as two distinct mechanisms rather than conflating them, matching the physics. |
| Driver seat offset, asymmetric path lengths | `stereoWidth = 0.6`, `positions[]` lateral bias | ✅ matches | Narrowed width + per-seat lateral bias correctly represent physical asymmetry, not a stylistic choice. |
| Tweeter beaming (off-axis HF loss, position-dependent) | *(not modeled)* | — | **Gap**, same category as Nova Main Room's off-axis gap: Rear Left/Right (further from and more off-axis to the dash tweeters) should lose real high-frequency energy, not just get the generic "diffuse" treatment `diffuseAmount` currently applies. |

**Summary**: the single biggest fix this doc surfaces is **the low-end shelf should be a resonant peak at the cabin's actual modal frequency (~82Hz)**, not a broad shelf at 150Hz — that's the clearest case in this environment of "arbitrary EQ" vs. "models something real." The harshness/high-shelf tension flagged in the Phase 2 tuning report is now explained physically: the −4dB@6kHz shelf is fighting the wrong frequency region for what "harsh upper mids" actually means in a real car (crossover-region irregularity around 2.5–3.5kHz, not the 6kHz+ region a shelf targets).
