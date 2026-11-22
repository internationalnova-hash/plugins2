# Luxury SUV — Design Document

**Status: implemented** (2026-08-07). The original 4 environments cleared their Phase 4 Decision Fidelity gate on 2026-07-30, and this environment was built next per the staged rollout, following this same design target unchanged from the benchmark spec below. Reference class: Mercedes S-Class, BMW X7, Cadillac Escalade, Lexus LX, Lincoln Navigator — flagship OEM audio, not a generic "bigger car."

This is also the clearest illustration of why the Acoustic/Playback System split matters: a Luxury SUV differs from a Sedan in **both** layers independently, and the interesting part of this profile is that the two layers don't just scale together — the Playback System layer actively fights the Acoustic layer's problems in a way Sedan's system doesn't.

## Acoustic Model (the cabin — bigger, and better damped)

- **Dimensions**: substantially larger than a sedan — interior length roughly 1.5–2× (≈3.0–3.5m vs. sedan's ≈2.0–2.2m), often 3-row, taller roofline. Interior air volume roughly 6–9 m³ vs. sedan's ~2.5–3 m³.
- **Materials**: more total glass area (larger windows, frequently a panoramic roof), BUT premium trims commonly spec **acoustic (laminated, sound-dampening) glass** specifically to reduce road/wind noise and harsh reflections — a materially different reflective surface than base-trim tempered glass, not just "more of the same glass."
- **Treatment**: flagship trims invest heavily in noise-vibration-harshness (NVH) treatment (extra sound deadening in doors/floor/firewall) — a real, deliberate engineering difference from a base sedan, independent of the audio system installed.
- **Listener geometry**: multiple rows means multiple genuinely different listening positions relative to the cabin — a 3rd-row passenger sits in a physically different part of the space, not just "further from center" the way a sedan's rear seat is.
- **Modal resonance**: larger cabin → lower resonant frequency, not "more of the same bump." Using the same formula as the Sedan doc, with interior length ≈3.2m:

  ```
  f = 343 / (2 × 3.2) ≈ 54 Hz
  ```

  vs. sedan's ~82Hz — a genuinely different tuning target, purely a consequence of cabin size, before any speaker is even considered.
- **Reflections**: larger cabin means reflections travel farther before returning (later arrival than sedan's 1–3ms), but acoustic glass and heavier NVH treatment damp them more — later AND quieter than a sedan's, a different character rather than simply "less."

## Playback System Model (the factory-premium audio system — actively correcting for the cabin)

- **Speaker count**: flagship systems (Burmester in Mercedes, Bowers & Wilkins in some BMW/Range Rover products, comparable Mark Levinson/Revel-class systems) commonly run **16–24+ channels** — dedicated tweeters, midrange drivers, dedicated subwoofer(s), often a center-channel dash speaker, sometimes overhead/ceiling channels in newer Atmos-capable systems. Categorically more speakers than a sedan's basic 4–6.
- **Crossovers**: active, DSP-managed multi-way crossovers rather than passive networks.
- **Manufacturer tuning — the critical difference**: premium systems actively **time-align and EQ-correct per channel** to compensate for off-center seating. This is the single biggest architectural difference from Sedan: the cabin-geometry asymmetry problem a base sedan's Playback System doesn't correct for is *specifically engineered away* here. Same category of Acoustic-layer problem (listener isn't centered), opposite Playback-System response to it.
- **Amplification**: substantially higher power (hundreds of watts vs. a base sedan's 15–25W/channel), real subwoofer(s) with genuine low-frequency extension (30–40Hz region), operating well within linear headroom at normal listening levels.
- **Nonlinearities**: minimal by design — the most linear, least-colored *mobile* playback system Nova Translate would model, closer in spirit to Nova Main Room's Playback System Model than to Sedan's.

## Perceptual Targets (design targets — success criteria for the eventual build)

1. **Low-end extension** — genuine deep bass (down toward 35–45Hz) rather than Sedan's cabin-reinforced-but-limited low end.
2. **Wide imaging** — a stereo image that's noticeably wider and more precise than Sedan's, specifically because the system corrects for the offset problem rather than because the cabin is bigger.
3. **Smooth vocal presentation** — no crossover-region harshness the way Sedan has; the vocal should sit cleanly rather than fighting hardware artifacts.
4. **Premium playback translation** — the overall impression should read as "expensive, engineered system," not "the same car but louder" — if a listener can't tell this apart from a tuned-up Sedan, the implementation has failed its own benchmark.

## DSP Translation (implemented values)

| Physical cause | Model | Implemented DSP value | Contrast with Sedan |
|---|---|---|---|
| Lower cabin modal frequency (~50–60Hz vs. sedan's ~82Hz) | Acoustic | `lowShelfGain = +3dB @ 55Hz` | Same shelf-not-peak mechanism Sedan currently uses (the shelf→peak fix is still an open gap, not yet made — see `sedan.md`); frequency moved to 55Hz to match the bigger cabin's lower mode, independent of the speakers. |
| Acoustic glass + heavy NVH damping | Acoustic | `roomAmount = 0.35`, `earlyReflectionAmount = 0.35` (vs. Sedan's 0.5/0.6) | Sedan's cabin is bright/live for its size; this cabin is comparatively hushed — an Acoustic-layer difference, independent of the audio system. |
| Active per-channel time-alignment/EQ correction | Playback System | `stereoWidth = 0.9`, `stereoOffset = 0.03`, lateral position bias ±0.05 (vs. Sedan's ±0.12–0.15) | **The key contrast**: the underlying Acoustic-layer offset problem (driver isn't centered) is comparable to Sedan's, but the Playback System actively corrects it here. This is the single most important "not just Sedan+bass" signal, and it's explicitly a Playback System difference, not an Acoustic one — the cabin doesn't get less asymmetric, the system just compensates. |
| High-power, high-headroom amplification | Playback System | `compressionAmount = 0.08` (vs. Sedan's 0.3) | Sedan's amp limits are audible; this system's shouldn't be at normal levels — purely an amplifier-hardware difference. |
| Premium multi-way drivers, larger woofers, real subwoofer | Playback System | `lowCutHz = 40` (vs. Sedan's 90), `highShelfGain = -1.5dB @ 8000Hz` (vs. Sedan's -4dB @ 6000Hz) | Genuine frequency extension from better hardware, not a bass boost layered on Sedan's existing rolloff point. |
| Minimal cone/amp nonlinearity | Playback System | `saturationAmount = 0.05` (vs. Sedan's 0.25) | Flagship components aren't pushed into their nonlinear range at normal levels. |
| Well-managed active crossover (vs. Sedan's passive-network irregularity) | Playback System | `midPeakGain = +0.5dB @ 2800Hz, Q1.0` (vs. Sedan's +2.5dB, Q1.2) | Sedan's crossover-region peak models a real passive-network artifact; an active DSP-managed crossover shouldn't show nearly as much of one. |
| Multiple genuinely different seating rows | Both | 4 positions: Driver(-0.05,0), Front Passenger(0.05,0), 2nd Row(0,0.2 diffuse), 3rd Row(0,0.35 diffuse), `defaultPositionIndex=0` | The *existence* of a 3rd row is Acoustic (cabin size); *which 4 positions get exposed* given the format's slot ceiling (`kMaxPositions`) is a Playback-System-and-UI design decision. |
| Own visual scene | — (UI, not DSP) | Own top-down illustration — larger cabin outline, front/2nd-row bucket seats, 3rd-row bench, 6 speaker markers (Front L/R, Center Channel, 2nd Row L/R, Subwoofer) | Built as its own `SCENES.suv`/`SCENE_MARKERS.suv` module per the frozen UI architecture — not a recolored Sedan scene. |

## Confidence Rating

| Acoustic | Playback System | Overall | Decision Fidelity |
|---|---|---|---|
| 6.5/10 | 7.0/10 | 6.8/10 | Not yet measured — see [`reference-validation-protocol.md`](reference-validation-protocol.md) |

Acoustic score mirrors Sedan's core gap (the modal resonance is a shelf, not a peak — same unresolved issue, just retargeted in frequency), so it can't score higher than Sedan's until that fix lands for both. Playback System scores a bit higher than Sedan's, since "actively corrected, high-headroom, low-distortion" is a lower-risk claim to model than Sedan's specific hardware artifacts. Both scores are design-reasoning confidence, not a listening-test result — this environment has not yet been through even an informal listening pass the way the original 4 have.

**Summary — the actual benchmark**: a listener switching from Sedan to Luxury SUV inside Nova Translate should hear a **lower-pitched cabin resonance** (Acoustic: bigger box), **a wider and more precise stereo image** (Playback System: active correction of a comparable Acoustic-layer offset problem), **a quieter/later room character** (Acoustic: better materials/treatment), and **less audible amplifier strain** (Playback System: more power, more linear). Four differences, cleanly attributable to one layer or the other — not "the same car, louder low end." That the stereo-image improvement comes from the Playback System actively fighting an Acoustic-layer problem Sedan's system leaves uncorrected is exactly the kind of distinction this two-layer methodology exists to surface.
