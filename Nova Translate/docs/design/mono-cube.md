# Mono Cube — Design Document

Reference profile: `EnvironmentLibrary::monoCube()`. Role: balance checker — vocal level, kick/snare relationship, bass masking, mono compatibility. "No hype, no excitement, just brutal honesty."

## 1. Physical Characteristics

This environment models a real, specific class of studio tool: a small single-driver **mono reference monitor** — the archetype is the Auratone 5C "Sound Cube" (and its many modern equivalents/clones), a fixture in mastering and mix rooms specifically *because* it's mediocre in a useful, consistent way.

- **Placement**: desktop/console-top, near-field (~0.5–1m from the listener).
- **Enclosure**: small sealed cabinet, no port.
- **Room contribution**: minimal — close near-field placement means direct sound dominates over the room.

## 2. Playback System

- **Driver**: single full-range driver, historically ~4.5" (the original Auratone 5C used a 4.5" full-range unit) — small enough to have real, physical bandwidth limits, but far larger than a phone speaker, so its limits sit in a different place.
- **Crossover**: none — that's the point. A single full-range driver has no crossover-region irregularities to worry about, but it does have its own breakup/beaming behavior as its cone approaches and exceeds the wavelength of the frequencies it's reproducing.
- **Character**: this class of monitor is deliberately, famously NOT flat — it has a pronounced, boxy midrange coloration that is the actual reason engineers use it. The coloration itself is the useful feature: if a mix's vocal/kick/snare balance holds up through a small, honky, midrange-forward driver, it'll hold up on far worse consumer systems too.

## 3. Acoustic Behavior

- **Bandwidth**: full-range but limited at both ends — commonly cited usable range for this monitor class is roughly 200Hz–8kHz-ish, well short of full 20Hz–20kHz extension. No sub-bass, no real air/sparkle.
- **Midrange character**: a genuine, audible cabinet/driver resonance in the low-mid to mid range gives this monitor class its distinctive "boxy"/"honky" signature — NOT a subtle EQ nudge, a real and fairly pronounced coloration.
- **Mono**: literally, physically mono — one driver, one signal path. This is the single most important physical fact in this profile, and the one place where "the DSP models something real" is unambiguous: `stereoWidth = 0` isn't an approximation of mono, it's an exact match, because `mid = 0.5×(L+R)` IS what a single driver fed a summed signal actually receives.
- **Distortion/dynamics**: modest — these monitors aren't typically driven hard or fed high-power amplifiers; distortion and compression are present but not the star of the show the way they are for Smartphone.

## 4. DSP Translation

| Physical cause | DSP field | Current value | Justification |
|---|---|---|---|
| Driver's low-end limit (~200Hz-ish for a small full-range driver, no sub) | `lowCutHz = 150`, `lowShelfGain = -3dB @ 200Hz` | Roughly right, possibly conservative | 150Hz is a bit lower than the ~200Hz commonly cited for this monitor class — worth ear-verifying against a real unit (or a documented spec) rather than assuming; flagged, not changed. |
| Real cabinet/driver resonance — the defining "boxy" character of this monitor class | `midPeakGain = +1.5dB @ 1500Hz, Q0.9` | **Gap — likely too mild** | This is the one place this doc pushes back hardest on the current tuning: real units in this class are known for a *pronounced* boxy midrange, and +1.5dB/Q0.9 is a fairly gentle nudge by comparison. The current profile reads as "neutral with a light touch," which partly conflicts with "no hype, brutal honesty" — brutal honesty from this monitor class actually includes its own honest, characterful coloration, not neutrality. Needs ear comparison against a real Auratone-class unit before changing the number, but flagged as the most likely under-tuned parameter in this profile. |
| Limited high-frequency extension (small full-range driver beaming/rolling off) | `highShelfGain = -2dB @ 8000Hz` | Conservative, plausible | Reasonable but mild; real units often roll off more before 8kHz. Flagged for verification, not changed. |
| Minimal room contribution (close near-field) | `roomAmount = 0.1`, `earlyReflectionAmount = 0.1` | ✅ matches | Correctly low — near-field desktop placement genuinely minimizes room contribution. |
| Literal single-driver mono | `stereoWidth = 0` | ✅ matches exactly | The one parameter in this whole plugin that isn't an approximation — a single driver fed `mid = 0.5×(L+R)` is not a model of mono, it IS mono. |
| Modest, not-heavily-driven amplification | `compressionAmount = 0.15`, `saturationAmount = 0.1` | ✅ matches | Correctly modest — lower than Sedan/Smartphone, consistent with this monitor class not being run hard. |
| No listening-position concept (single driver, single point source, near-field) | `supportsPosition = false` | ✅ matches | Correct — with one driver and near-field placement, "position" isn't a meaningful stereo-image variable the way it is for a multi-speaker system. |

**Summary**: the mono fold-down is the one place in the entire plugin where the DSP doesn't approximate reality, it reproduces it exactly. The main open question is whether the midrange coloration is currently UNDER-tuned relative to the real monitor class this profile represents — flagged for ear-testing, since "how boxy is boxy enough" is a judgment call this doc can identify but not settle without listening.
