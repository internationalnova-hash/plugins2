# Smartphone — Design Document

Reference profile: `EnvironmentLibrary::smartphone()`. Role: exposes vocal intelligibility, snare presence, melody clarity, hook translation — "does the mix disappear on a phone."

## 1. Physical Characteristics

- **No cabin/room** — there is no enclosed air volume to speak of beyond the phone's own tiny internal speaker chamber. Acoustically this environment is closer to free-field than any of the other three.
- **Listening distance**: typically close — arm's length to face distance (~30–50cm) for handheld playback, sometimes closer.
- **Enclosure**: the phone body itself acts as a small, mostly-sealed (sometimes ported) enclosure for the driver — its size directly sets the achievable low-frequency extension (see §2).

## 2. Playback System

- **Driver**: a single tiny driver, typically **11–15mm** — one of the smallest transducers in any consumer playback path. Physical cone area this small cannot move enough air to reproduce genuine bass regardless of amplifier power.
- **Low-frequency extension**: commonly **-3dB around 700Hz–1.2kHz** for phone speakers of this size — not a stylistic "thin" sound, a hard physical limit of a driver this small in an enclosure this size.
- **Crossover**: none — single full-range driver, no crossover network.
- **Amplifier**: small Class-D amp, but the real limiting factor is thermal/mechanical — cone excursion limits and driver heating force aggressive dynamic-range compression/limiting at even moderate output, well before the amplifier itself runs out of electrical headroom.
- **Channels**: many phones are effectively mono, or have two drivers spaced so closely (bottom-firing driver + earpiece pressed into stereo duty) that meaningful stereo separation doesn't survive to any realistic listening distance.
- **Note**: real phones already apply their own aggressive DSP (loudness/EQ enhancement, sometimes synthetic bass via harmonic generation) before the driver even sees the signal — Nova Translate's Smartphone profile approximates the *net perceptual result* of phone + phone's own DSP + driver, not a "bare driver" in isolation. Worth stating explicitly since it affects how literally the low-end cutoff should be interpreted.

## 3. Acoustic Behavior

- **Room contribution**: essentially none at typical listening distance — this is the one environment where "room" isn't really a meaningful concept.
- **Frequency response**: sharply high-pass filtered by the driver's physical limits, with a driver-resonance-driven presence peak (small drivers commonly resonate/breakup in the 2–4kHz region — not coincidentally close to the ear's own most sensitive range, part of why phone audio reads as thin-but-present rather than just quiet).
- **Distortion**: cone breakup and thermal/excursion limiting both become audible at realistic listening volumes far sooner than any other environment in this plugin — this is the most heavily colored/limited playback system Nova Translate models.
- **Stereo**: not meaningfully present; treat as mono.

## 4. DSP Translation

| Physical cause | DSP field | Current value | Justification |
|---|---|---|---|
| Driver's hard physical low-end limit (~700Hz–1.2kHz -3dB for an 11–15mm driver) | `lowCutHz = 400`, `lowShelfGain = -6dB @ 300Hz` | Directionally correct, worth re-checking the corner | 400Hz is somewhat below the typical -3dB point cited above; not necessarily wrong (phones vary, and the phone's own internal loudness DSP can partially compensate), but worth ear-verifying against a real device rather than assuming — flagged, not changed. |
| Driver resonance / breakup-region presence peak (~2–4kHz) | `midPeakGain = +5dB @ 3000Hz, Q1.5` | ✅ matches | Strongest peak of any profile, correctly — this is the most physically justified "big boost" in the whole plugin, since it's modeling an actual driver resonance, not a taste EQ. |
| Driver's HF extension limit | `highShelfGain = -3dB @ 7000Hz` | Directionally correct | Reasonable; small drivers do roll off top end, though the exact frequency varies by device generation — flagged for ear-verification, not changed. |
| No meaningful room | `roomAmount = 0.05`, `earlyReflectionAmount = 0.05` | ✅ matches | Correctly near-zero — there's no room to model here, and these are already the lowest values of the four profiles. |
| Mono/negligible stereo separation | `stereoWidth = 0.05` | ✅ matches | Correctly near-mono — physically justified by driver spacing, not a style choice. |
| Thermal/excursion-driven amplifier limiting | `compressionAmount = 0.5` | ✅ matches | Highest of all four profiles, correctly — this system hits its physical limits soonest of anything modeled. |
| Cone breakup distortion at the resonance region | `saturationAmount = 0.4` | ✅ matches | Second-highest of the four (behind nothing — actually tied for highest with compression's ranking) — correctly the most colored/distorted system modeled. |
| No listening-position concept at this distance/driver spacing | `supportsPosition = false` | ✅ matches | Correct — "position" isn't a meaningful physical variable for a device held at arm's length with negligible stereo separation. |

**Summary**: Smartphone is the second-most physically justified profile after Nova Main Room — nearly every number already has a real mechanism behind it. The two open items are both "verify the exact corner frequencies against a real device" rather than "wrong shape," which is a much smaller gap than Sedan's shelf-vs-peak issue.
