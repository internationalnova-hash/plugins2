# Sedan — Design Document

Reference profile: `EnvironmentLibrary::sedan()`. Role: "primary translation check" — must expose vocal balance, bass buildup, kick translation, and upper-mid harshness by ear, immediately.

## Acoustic Model (the cabin, independent of the audio system)

- **Dimensions**: small, roughly 2.5–3 m³ interior air volume. Interior length (~firewall to rear seatback) ≈ 2.0–2.2m.
- **Materials**: large windshield + side windows, highly reflective (α ≈ 0.03–0.05) — a bigger contributor to early reflections, per unit area, than almost anything else in the cabin. Dashboard: hard plastic, also low absorption (α ≈ 0.05–0.1), sits directly in front of dash-mounted tweeters — a near-field reflective surface, not a distant wall. Seats: fabric or leather, moderate absorption (α ≈ 0.3–0.5 mid/high) — the main damping surface in the cabin, concentrated at ear height.
- **Listener geometry**: the driver is NOT centered between the L/R speakers — asymmetric to the physical cabin geometry itself, independent of whether the audio system tries to correct for it.
- **Reflections**: very fast. Windshield/dashboard proximity (~0.4–0.6m extra path) puts first reflections at roughly **1–3ms** — physically incapable of reading as "room ambience"; it reads as a bright, glassy slap right on top of the direct sound.
- **Modal resonance**: the defining low-frequency behavior of the space itself, independent of what speakers are playing into it. Cabin length ≈ 2.0–2.2m gives a first axial mode at:

  ```
  f = 343 / (2 × 2.1) ≈ 82 Hz
  ```

  This is *why* sedans sound bass-heavy at a specific frequency rather than generically boomy — it's a standing wave in the cabin's air volume, and it would exist even with a hypothetically perfect, full-range speaker system installed.

## Playback System Model (the factory audio system, independent of the cabin)

- **Speaker count/placement**: door woofers, 5.25"–6.5" typical, mounted low in the doors (below ear height) — a real driver/ear height mismatch. Tweeters small (~1" dome), A-pillar or dash-mounted, narrow high-frequency dispersion (beaming).
- **Crossover**: passive, 2-way, typical woofer/tweeter handoff ≈ 2.5–3.5kHz. Passive crossovers commonly show a small response irregularity (dip or bump) right at the handoff frequency rather than a clean transition — this is a hardware characteristic that exists regardless of cabin size.
- **Amplifier**: base-trim OEM head-unit power is modest, roughly 15–25W/channel — meaningfully lower headroom than a studio monitor or premium system, so it compresses/soft-limits sooner at real listening levels. This is a property of the amplifier, not the cabin.
- **Extension**: door woofers alone typically -3dB around 80–100Hz without a subwoofer — a driver-size limit that happens to land almost exactly on the cabin's own modal frequency (82Hz, above), which is *why the two effects reinforce each other* rather than one masking the other. Most base trims lack a dedicated subwoofer.
- **Manufacturer tuning**: base-trim OEM systems are generally NOT actively DSP-corrected — no time-alignment for the off-center driver seat, no per-channel EQ correction. Unlike a premium system (see `luxury-suv-benchmark.md`), the cabin's physical asymmetry passes straight through uncorrected.
- **Dynamics**: amplifier headroom limiting is a distinct mechanism from the cabin's road-noise masking (a separate Acoustic-adjacent phenomenon this plugin already models via `noiseAmount`) — both push perceived dynamics down, but for different physical reasons.

## Perceptual Targets

The primary translation check — what a mix engineer specifically needs this environment to expose:

1. **Kick drum balance** — does the kick sit correctly against the cabin's ~82Hz modal buildup, or get swallowed/exaggerated by it?
2. **Bass resonance** — is the cabin-mode bass buildup audible and roughly frequency-accurate, distinct from a generic "boosted low end"?
3. **Vocal intelligibility** — does the vocal stay legible through the crossover-region coloration and road-noise masking?
4. **Snare bite/upper-mid harshness** — does the crossover-region character (2.5–3.5kHz) expose harsh snare/upper-mid content the way a real door-speaker crossover would?
5. **Stereo collapse toward the driver** — does the image narrow and pull toward the driver's seat, reflecting the real uncorrected cabin-offset problem?

## DSP Translation

| Physical cause | Model | DSP field | Current value | Justification |
|---|---|---|---|---|
| Door woofer low-end limit (~80–100Hz -3dB) | Playback System | `lowCutHz = 90` | ✅ matches | Right in the physically correct range for a 5.25–6.5" door woofer without a sub. |
| Cabin modal resonance (~82Hz, calculated above) | Acoustic | `lowShelfGain = +3dB @ 150Hz` | **Gap — wrong shape and wrong frequency** | This should be a resonant **peak** near 80–85Hz (modeling the cabin standing wave), not a broad shelf centered at 150Hz. The clearest case in the whole plugin of an Acoustic-Model phenomenon currently being represented with the wrong filter shape. |
| Passive crossover irregularity + tweeter beaming onset (~2.5–3.5kHz) | Playback System | `midPeakGain = +2.5dB @ 2500Hz, Q1.2` | Right region, wrong intent | Previously tuned as a generic "presence boost" — should represent the crossover-region hardware irregularity instead. This is unambiguously Playback System (crossover network), not Acoustic — worth re-deriving once unfrozen with that framing rather than "make it sound present." |
| Tweeter's HF extension + windshield's absorption of very top end | Both | `highShelfGain = -4dB @ 6000Hz` | Partially right, wrong physical story | Currently *reduces* the content the brief wants exposed (see Phase 2 tuning report). Two real, separable causes are collapsed into one shelf here: the tweeter's own extension limit (Playback System) and any HF loss from windshield absorption (Acoustic, though glass is actually low-absorption so this contributes less than intuition suggests). The harshness the brief wants belongs in the 2.5–3.5kHz crossover region above, not this shelf. |
| Fast (~1–3ms) glass/dashboard reflections | Acoustic | `earlyReflectionAmount = 0.6`, tap spacing in `generateSyntheticIR` | Directionally correct | High `earlyReflectionAmount` correctly biases toward dense/early taps; **gap** — the synthetic IR generator isn't currently told "these taps must land inside ~1–3ms," it just gets more of them scaled by this parameter. |
| OEM amplifier headroom (~15–25W/channel, sooner-limiting) | Playback System | `compressionAmount = 0.3` | Directionally correct | Second-highest of the 4 profiles — correct ordering, since a car amp has more headroom than a phone speaker but far less than a monitor or premium system. Purely a Playback System number; nothing about the cabin should affect this value. |
| Road noise floor (cabin/ambient, not the audio system) | Acoustic-adjacent | `noiseAmount = 0.15` | Correctly modeled as separate | Already treated as distinct from amp compression, matching the physics — road noise is an environment property, not a playback-system one, even though both reduce perceived dynamic range. |
| Driver seat offset (cabin geometry) + no DSP correction (system doesn't compensate) | Both | `stereoWidth = 0.6`, `positions[]` lateral bias | ✅ matches | This row is the clearest example of "Both" in this doc: the offset itself is Acoustic (cabin geometry), but the fact that it's audible and uncorrected is a Playback System property (base trims lack time-alignment DSP) — a premium system with the identical cabin geometry would correct this away, which is exactly the contrast `luxury-suv-benchmark.md` is built around. |
| Tweeter beaming (off-axis HF loss, position-dependent) | Playback System | *(not modeled)* | — | **Gap**: Rear Left/Right (further off-axis to the dash tweeters) should lose real high-frequency energy from driver directivity, not just get the generic `diffuseAmount` treatment. Playback System cause, not Acoustic. |

## Confidence Rating

| Acoustic | Playback System | Overall |
|---|---|---|
| 6.0/10 | 6.5/10 | 6.3/10 |

Lowest overall confidence of the 4 implemented environments, and honestly so — this is the environment with the most consequential known gap (shelf-vs-peak on the low end is a shape error, not just an unverified corner frequency) and the one carrying the most Perceptual Targets to satisfy. Both scores reflect design-reasoning confidence, not a listening-test result. This is the clearest candidate for where Reference Validation should start.

**Summary**: the two-layer split resolves an ambiguity the earlier single-list version glossed over — the low end is Acoustic (fix: shelf → peak at 82Hz), the crossover-region harshness is Playback System (fix: reframe the 2.5–3.5kHz peak as a hardware artifact, not a presence boost), and the driver-offset/imaging problem is explicitly **Both**, which is exactly why Luxury SUV — same kind of offset, different (DSP-corrected) Playback System — should sound different from Sedan without needing a different cabin.
