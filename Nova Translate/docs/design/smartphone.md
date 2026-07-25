# Smartphone — Design Document

Reference profile: `EnvironmentLibrary::smartphone()`. Role: exposes vocal intelligibility, snare presence, melody clarity, hook translation — "does the mix disappear on a phone."

## Acoustic Model (the space — deliberately almost nothing)

- **No cabin/room**: there is no enclosed air volume to speak of beyond the phone's own tiny internal speaker chamber. This is the one environment where the Acoustic Model layer is honestly near-empty — the whole character comes from the Playback System layer instead, and the design doc should say that plainly rather than inventing room behavior that doesn't exist.
- **Listening distance**: typically close — arm's length to face distance (~30–50cm) for handheld playback, sometimes closer. Close enough that direct sound totally dominates any incidental surface reflections (a table the phone might be resting on, etc. — a real but secondary case, not modeled here).
- **Net effect**: essentially free-field. `roomAmount`/`earlyReflectionAmount` should be near-zero because there's genuinely nothing acoustic happening, not because "phones sound dry" as a vibe.

## Playback System Model (the phone speaker + its own onboard DSP — where all the character lives)

- **Driver**: a single tiny driver, typically **11–15mm** — one of the smallest transducers in any consumer playback path. Physical cone area this small cannot move enough air to reproduce genuine bass regardless of amplifier power.
- **Enclosure**: the phone body itself acts as a small, mostly-sealed (sometimes ported) enclosure — its size directly sets the achievable low-frequency extension.
- **Low-frequency extension**: commonly **-3dB around 700Hz–1.2kHz** for phone speakers of this size — a hard physical limit, not a stylistic "thin" sound.
- **Crossover**: none — single full-range driver.
- **Amplifier/protection**: small Class-D amp; the real limiting factor is thermal/mechanical — cone excursion limits and driver heating force aggressive dynamic-range compression/limiting at even moderate output, well before the amplifier itself runs out of electrical headroom. Thermal protection circuits will actively pull level down if the driver is pushed too hard for too long.
- **Manufacturer DSP**: real phones apply their own aggressive onboard processing before the driver ever sees the signal — loudness/EQ enhancement, sometimes synthetic bass via harmonic generation to psychoacoustically imply low end the driver can't physically reproduce. Nova Translate's Smartphone profile approximates the *net perceptual result* of phone-DSP + driver together, not a bare driver in isolation — worth stating explicitly since it affects how literally the low-end cutoff should be read.
- **Driver resonance**: small drivers commonly resonate/break up in the 2–4kHz region, not coincidentally close to the ear's own most sensitive range — part of why phone audio reads as thin-but-present rather than just quiet.
- **Channels**: many phones are effectively mono, or have two drivers spaced so closely that meaningful stereo separation doesn't survive to any realistic listening distance.

## Perceptual Targets

1. **Lead vocal level** — does the vocal maintain a competitive, forward level despite the driver's limited range?
2. **Hook/melody clarity** — does the melodic hook stay identifiable and prominent?
3. **Midrange masking** — do busy midrange elements (guitars, synths, backing vocals) start to obscure the lead vocal, the way they would on a real small driver?
4. **Transient intelligibility** — do snare/percussion transients stay articulate despite the driver's breakup-region coloration and heavy limiting?
5. **Loss of sub-bass** — does the mix's low end genuinely disappear (not just "get quieter"), forcing the engineer to judge the mix without it?

## DSP Translation

| Physical cause | Model | DSP field | Current value | Justification |
|---|---|---|---|---|
| Driver's hard physical low-end limit (~700Hz–1.2kHz -3dB) | Playback System | `lowCutHz = 400`, `lowShelfGain = -6dB @ 300Hz` | Directionally correct, worth re-checking the corner | 400Hz sits somewhat below the typical -3dB point cited above — plausible given phone-DSP bass compensation partially fills it back in, but flagged for ear-verification against a real device rather than assumed. |
| Driver resonance / breakup-region presence peak (~2–4kHz) | Playback System | `midPeakGain = +5dB @ 3000Hz, Q1.5` | ✅ matches | Strongest peak of any profile, correctly — models an actual driver resonance, not a taste EQ. |
| Driver's HF extension limit | Playback System | `highShelfGain = -3dB @ 7000Hz` | Directionally correct | Reasonable; exact frequency varies by device generation — flagged for ear-verification. |
| No meaningful room (near-field, no reflective geometry worth modeling) | Acoustic | `roomAmount = 0.05`, `earlyReflectionAmount = 0.05` | ✅ matches | Correctly near-zero — nothing acoustic is really happening here, and these are already the lowest values of the four profiles. |
| Driver spacing too close for stereo separation to survive | Playback System | `stereoWidth = 0.05` | ✅ matches | Correctly near-mono — physically justified by driver geometry, not a style choice. |
| Thermal/excursion-driven amplifier + protection limiting | Playback System | `compressionAmount = 0.5` | ✅ matches | Highest of all four profiles, correctly — this system hits its physical/thermal limits soonest of anything modeled. |
| Cone breakup distortion at the resonance region | Playback System | `saturationAmount = 0.4` | ✅ matches | Second-highest of the four — correctly the most colored/distorted system modeled. |
| No listening-position concept at arm's-length distance with negligible stereo spacing | Playback System | `supportsPosition = false` | ✅ matches | Correct — driven by the playback system's own geometry (driver spacing), not by any acoustic-space reasoning, since there's no space to speak of. |

## Confidence Rating

| Acoustic | Playback System | Overall |
|---|---|---|
| 9.0/10 | 7.5/10 | 8.2/10 |

Acoustic scores highest of any environment — "there is essentially no room" is a low-risk, easy-to-get-right claim. Playback System is held back by two flagged-for-verification corner frequencies (low-cut point, high-shelf point) rather than any structural shape error — a smaller gap than Sedan's. Both scores reflect design-reasoning confidence, not a listening-test result.

**Summary**: Smartphone is the environment where the two-layer split matters most conceptually even though it changes almost nothing in the table — nearly every number here is Playback System, and the Acoustic Model section exists mainly to explicitly confirm "there is no room" rather than let that go unstated. The two open items are both "verify the exact corner frequencies against a real device," both squarely Playback System questions.
