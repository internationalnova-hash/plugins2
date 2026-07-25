# Mono Cube — Design Document

Reference profile: `EnvironmentLibrary::monoCube()`. Role: balance checker — vocal level, kick/snare relationship, bass masking, mono compatibility. "No hype, no excitement, just brutal honesty."

This environment models a real, specific class of studio tool: a small single-driver **mono reference monitor** — the archetype is the Auratone 5C "Sound Cube" (and its many modern equivalents/clones), a fixture in mastering and mix rooms specifically *because* it's mediocre in a useful, consistent way.

## Acoustic Model (the desk/room — minimal by design of use, not by device)

- **Placement**: desktop/console-top, near-field (~0.5–1m from the listener).
- **Room contribution**: minimal — close near-field placement means direct sound dominates over whatever room the monitor happens to be sitting in. Unlike Smartphone, there IS a room around it, but the intended use case (close monitoring) deliberately minimizes its contribution.
- **Listener geometry**: centered, near-field, single point source — no meaningful off-axis-listener scenario the way a car or multi-speaker system has.

## Playback System Model (the single-driver monitor — where nearly all the character lives)

- **Driver**: single full-range driver, historically ~4.5" (the original Auratone 5C used a 4.5" full-range unit) — small enough to have real, physical bandwidth limits, but larger than a phone speaker, so its limits sit in a different place.
- **Enclosure**: small sealed cabinet, no port.
- **Crossover**: none — a single full-range driver has no crossover-region irregularities, but it does have its own breakup/beaming behavior as its cone approaches and exceeds the wavelength of the frequencies it's asked to reproduce.
- **Manufacturer character**: this class of monitor is deliberately, famously NOT flat — it has a pronounced, boxy midrange coloration that is the actual reason engineers use it. The coloration is the useful feature, not a flaw: if a mix's vocal/kick/snare balance holds up through a small, honky, midrange-forward driver, it'll hold up on far worse consumer systems too.
- **Bandwidth**: full-range but limited at both ends — commonly cited usable range for this monitor class is roughly 200Hz–8kHz-ish, well short of full 20Hz–20kHz extension. No sub-bass, no real air/sparkle — both playback-system limits, not room limits.
- **Mono**: literally, physically mono — one driver, one signal path. The single most important fact in this profile, and the one place in the whole plugin where "the DSP models something real" is unambiguous: `stereoWidth = 0` isn't an approximation of mono, it's an exact match, because `mid = 0.5×(L+R)` IS what a single driver fed a summed signal actually receives.
- **Dynamics/distortion**: modest — this monitor class isn't typically driven hard or fed high-power amplifiers; present but not the star of the show the way it is for Smartphone.

## Perceptual Targets

1. **Midrange balance** — does the vocal/instrument midrange relationship hold up, or does the boxy coloration expose a real balance problem?
2. **Vocal masking** — do other midrange elements crowd the vocal out through this narrow-bandwidth, midrange-forward driver?
3. **Mono compatibility** — does anything phase-cancel or thin out when collapsed to true mono (the single property this environment models exactly, not approximately)?
4. **Instrument separation** — can individual elements still be distinguished despite the limited bandwidth and single-driver summing?

## DSP Translation

| Physical cause | Model | DSP field | Current value | Justification |
|---|---|---|---|---|
| Driver's low-end limit (~200Hz-ish for a small full-range driver, no sub) | Playback System | `lowCutHz = 150`, `lowShelfGain = -3dB @ 200Hz` | Roughly right, possibly conservative | 150Hz is a bit lower than the ~200Hz commonly cited for this monitor class — flagged for ear-verification against a real unit or documented spec. |
| Real cabinet/driver resonance — the defining "boxy" character of this monitor class | Playback System | `midPeakGain = +1.5dB @ 1500Hz, Q0.9` | **Gap — likely too mild** | Real units in this class are known for a *pronounced* boxy midrange; +1.5dB/Q0.9 reads as "neutral with a light touch," which is in tension with "brutal honesty" — the honest character of this monitor class includes its own coloration, not neutrality. Squarely a Playback System number (driver/cabinet resonance), not room-caused — flagged as the most likely under-tuned parameter in the whole plugin. |
| Limited high-frequency extension (small full-range driver beaming/rolling off) | Playback System | `highShelfGain = -2dB @ 8000Hz` | Conservative, plausible | Reasonable but mild; real units often roll off more before 8kHz. |
| Minimal room contribution (near-field use, not an inherently dead room) | Acoustic | `roomAmount = 0.1`, `earlyReflectionAmount = 0.1` | ✅ matches | Correctly low — near-field desktop placement genuinely minimizes room contribution, independent of the driver itself. |
| Literal single-driver mono | Playback System | `stereoWidth = 0` | ✅ matches exactly | The one parameter in this whole plugin that isn't an approximation. |
| Modest, not-heavily-driven amplification | Playback System | `compressionAmount = 0.15`, `saturationAmount = 0.1` | ✅ matches | Correctly modest — lower than Sedan/Smartphone, consistent with this monitor class not being run hard. |
| No listening-position concept (single driver, single point source, near-field) | Playback System | `supportsPosition = false` | ✅ matches | Correct — driven by the *speaker* being a single point source, not by the room being small (the room genuinely could accommodate multiple listening positions; the monitor can't meaningfully differentiate them). |

## Confidence Rating

| Acoustic | Playback System | Overall |
|---|---|---|
| 8.5/10 | 6.0/10 | 7.3/10 |

Acoustic scores high — near-field, minimal room claims, hard to get wrong. Playback System is the second-lowest score of the four environments, driven by one specific, fairly confident "likely wrong" finding (the midrange coloration probably needs to be more pronounced, not just unverified) rather than a vague uncertainty. Both scores reflect design-reasoning confidence, not a listening-test result.

**Summary**: this doc is almost entirely Playback System — the room barely matters here by design of use, and every meaningful character trait (boxiness, bandwidth limit, mono) comes from the single driver itself. The clearest open question stays the same as before the restructure: the midrange coloration is likely under-tuned relative to the real monitor class, and it's now unambiguous that fixing it is a Playback System change, not an Acoustic one — nothing about the room around it should move.
