# Luxury SUV — Design Document (BENCHMARK SPEC, NOT YET IMPLEMENTED)

**Status: design target only.** No `EnvironmentProfile` exists for this yet — per the user's staged rollout, this environment is not built until the current 4 are finalized by ear. This document exists now because the user asked for it to set the *standard* the eventual implementation is held to: explicitly **not** "Sedan plus more bass." Reference class: Mercedes S-Class, BMW X7, Cadillac Escalade, Lexus LX, Lincoln Navigator — flagship OEM audio, not a generic "bigger car."

## 1. Physical Characteristics

- **Cabin volume**: substantially larger than a sedan — roughly 1.5–2× the interior length (≈3.0–3.5m vs. sedan's ≈2.0–2.2m), often 3-row, taller roofline. Interior air volume roughly 6–9 m³ vs. sedan's ~2.5–3 m³.
- **Glass**: more total glass area (larger windows, frequently a panoramic roof), BUT premium trims commonly spec **acoustic (laminated, sound-dampening) glass** specifically to reduce road/wind noise and harsh reflections — a materially different reflective surface than base-trim tempered glass.
- **Insulation/NVH treatment**: flagship trims invest heavily in noise-vibration-harshness treatment (extra sound deadening in doors/floor/firewall) — this is a real, deliberate engineering difference from a base sedan, not just "a nicer car."
- **Listener geometry**: multiple rows means multiple genuinely different listening positions relative to the speaker array — a 3rd-row passenger is not just "further from center," they're outside the design sweet spot of a system tuned primarily for the front seats.

## 2. Playback System

- **Speaker count**: flagship systems (Burmester in Mercedes, Bowers & Wilkins in some BMW/Range Rover products, comparable Mark Levinson/Revel-class systems) commonly run **16–24+ channels** — dedicated tweeters, midrange drivers, dedicated subwoofer(s), often a center-channel dash speaker, sometimes overhead/ceiling channels in newer Atmos-capable systems. This is categorically more speakers than a sedan's basic 4–6.
- **Crossovers**: active, DSP-managed multi-way crossovers rather than passive networks — critically, **premium systems actively time-align and EQ-correct per channel** to compensate for the driver's off-center seating position. This is the single biggest architectural difference from a sedan: the asymmetry problem a base sedan doesn't correct for is *specifically engineered away* in a flagship system.
- **Amplification**: substantially higher power (hundreds of watts vs. a base sedan's 15–25W/channel), real subwoofer(s) with genuine low-frequency extension (30–40Hz region), operating well within linear headroom at normal listening levels.
- **Nonlinearities**: minimal by design — this is the most linear, least-colored *mobile* playback system Nova Translate would model, closer in spirit to Nova Main Room than to Sedan.

## 3. Acoustic Behavior

- **Modal resonance**: larger cabin → lower resonant frequency, not "more of the same bump." Using the same formula as the Sedan doc, with interior length ≈3.2m:

  ```
  f = 343 / (2 × 3.2) ≈ 54 Hz
  ```

  vs. sedan's ~82Hz. This is a genuinely different tuning target, not a bigger version of the same one.
- **Reflections**: larger cabin means reflections travel farther before returning (later arrival than sedan's 1–3ms), but acoustic glass and heavier NVH treatment damp them more — net effect is likely reflections that are both *later* and *quieter* than a sedan's, a different character rather than simply "less."
- **Stereo image**: should be the most accurate, widest, best-corrected image of any "in-vehicle" environment Nova Translate models — active DSP time-alignment specifically targets this. This is the clearest place where "premium ≠ just louder/bassier" matters: a well-implemented SUV profile should sound *more precise*, not just *bigger*.
- **Dynamics/distortion**: minimal — high headroom, professional-grade components run conservatively.

## 4. DSP Translation (design targets — not implemented)

| Physical cause | Proposed DSP direction | Contrast with Sedan |
|---|---|---|
| Lower cabin modal frequency (~50–60Hz vs. sedan's ~82Hz) | Resonant peak near 50–60Hz (once the shelf→peak fix from the Sedan doc exists to reuse) | Different frequency, not more gain at the same frequency. |
| Active per-channel time-alignment/EQ correction | Wider `stereoWidth`, smaller/near-zero lateral position bias | The opposite direction from Sedan's narrowed, offset-biased image — this is the single most important "not just Sedan+bass" signal. |
| Acoustic glass + heavy NVH damping | Lower `roomAmount`/gentler `earlyReflectionAmount`, later reflection timing | Sedan's cabin is bright/live for its size; a flagship SUV cabin is comparatively hushed. |
| High-power, high-headroom amplification | Lower `compressionAmount` than Sedan | Sedan's amp limits are audible; a flagship system's shouldn't be, at normal levels. |
| Premium multi-way drivers, larger woofers, real subwoofer | Extended `lowCutHz` well below Sedan's 90Hz (target ~35–45Hz), smoother/gentler high-shelf than Sedan's | Genuine frequency extension, not just a bass boost layered on Sedan's existing rolloff point. |
| Minimal cone/amp nonlinearity | Lower `saturationAmount` than Sedan | Flagship components aren't being pushed into their nonlinear range at normal levels. |
| Multiple genuinely different seating rows | 4 position slots (kMaxPositions ceiling applies here too) — likely Driver, Front Passenger, 2nd Row, 3rd Row | A materially different position *set* than Sedan's 2-row Driver/Passenger/Rear-Left/Rear-Right, reflecting the 3-row reality, within the same 4-slot architectural constraint. |
| Own visual scene | Own top-down illustration (larger cabin outline, more speaker markers, third row) | Already required by the frozen UI architecture — SUV gets its own `SCENES`/`SCENE_MARKERS` module, never a recolored Sedan scene. |

**Summary — the actual benchmark**: a listener switching from Sedan to Luxury SUV inside Nova Translate should hear a **lower-pitched cabin resonance, a wider and more precise stereo image, a quieter/later room character, and less audible amplifier strain** — not simply "the same car, louder low end." Every one of those four differences traces to a real, documented engineering difference between a base-trim sedan audio system and a flagship OEM system, which is exactly the standard this whole Phase 3 methodology exists to enforce.
