# Luxury SUV — Design Document (BENCHMARK SPEC, NOT YET IMPLEMENTED)

**Status: design target only.** No `EnvironmentProfile` exists for this yet — per the user's staged rollout, this environment is not built until the current 4 are finalized by ear. This document exists now because the user asked for it to set the *standard* the eventual implementation is held to: explicitly **not** "Sedan plus more bass." Reference class: Mercedes S-Class, BMW X7, Cadillac Escalade, Lexus LX, Lincoln Navigator — flagship OEM audio, not a generic "bigger car."

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

## DSP Translation (design targets — not implemented)

| Physical cause | Model | Proposed DSP direction | Contrast with Sedan |
|---|---|---|---|
| Lower cabin modal frequency (~50–60Hz vs. sedan's ~82Hz) | Acoustic | Resonant peak near 50–60Hz (reusing the shelf→peak fix flagged in `sedan.md` once it exists) | Different frequency because the cabin is bigger — nothing to do with the speakers. |
| Acoustic glass + heavy NVH damping | Acoustic | Lower `roomAmount`/gentler `earlyReflectionAmount`, later reflection timing | Sedan's cabin is bright/live for its size; a flagship SUV cabin is comparatively hushed — an Acoustic-layer difference, independent of the audio system. |
| Active per-channel time-alignment/EQ correction | Playback System | Wider `stereoWidth`, smaller/near-zero lateral position bias | **The key contrast**: the underlying Acoustic-layer offset problem (driver isn't centered) is comparable to Sedan's, but the Playback System actively corrects it here. This is the single most important "not just Sedan+bass" signal, and it's explicitly a Playback System difference, not an Acoustic one — the cabin doesn't get less asymmetric, the system just compensates. |
| High-power, high-headroom amplification | Playback System | Lower `compressionAmount` than Sedan | Sedan's amp limits are audible; a flagship system's shouldn't be at normal levels — purely an amplifier-hardware difference. |
| Premium multi-way drivers, larger woofers, real subwoofer | Playback System | Extended `lowCutHz` well below Sedan's 90Hz (target ~35–45Hz), smoother/gentler high-shelf than Sedan's | Genuine frequency extension from better hardware, not a bass boost layered on Sedan's existing rolloff point. |
| Minimal cone/amp nonlinearity | Playback System | Lower `saturationAmount` than Sedan | Flagship components aren't pushed into their nonlinear range at normal levels. |
| Multiple genuinely different seating rows | Both | 4 position slots (kMaxPositions ceiling applies here too) — likely Driver, Front Passenger, 2nd Row, 3rd Row | The *existence* of a 3rd row is Acoustic (cabin size); *which 4 positions get exposed* given the format's slot ceiling is a Playback-System-and-UI design decision. |
| Own visual scene | — (UI, not DSP) | Own top-down illustration (larger cabin outline, more speaker markers, third row) | Already required by the frozen UI architecture — SUV gets its own `SCENES`/`SCENE_MARKERS` module, never a recolored Sedan scene. |

## Confidence Rating

| Acoustic | Playback System | Overall |
|---|---|---|
| Design Stage | Design Stage | N/A |

No `EnvironmentProfile` exists yet, so there's nothing to rate against — this row exists only so the summary table in `README.md` doesn't silently omit this environment.

**Summary — the actual benchmark**: a listener switching from Sedan to Luxury SUV inside Nova Translate should hear a **lower-pitched cabin resonance** (Acoustic: bigger box), **a wider and more precise stereo image** (Playback System: active correction of a comparable Acoustic-layer offset problem), **a quieter/later room character** (Acoustic: better materials/treatment), and **less audible amplifier strain** (Playback System: more power, more linear). Four differences, cleanly attributable to one layer or the other — not "the same car, louder low end." That the stereo-image improvement comes from the Playback System actively fighting an Acoustic-layer problem Sedan's system leaves uncorrected is exactly the kind of distinction this two-layer methodology exists to surface.
