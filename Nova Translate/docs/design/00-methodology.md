# Nova Translate — Acoustic Modeling Methodology

## The shift

Phase 1/2 built each environment as: **playback environment → DSP parameters → audio**. A person familiar with car audio picked numbers that sounded plausible for "car-ness" and wrote them into `EnvironmentProfile`.

Phase 3 replaces that with: **real playback system → acoustics → physics → DSP → audio**. Every filter, reflection tap, and nonlinearity in a profile must trace back to a physical cause in the real system it represents. If a number in `EnvironmentProfiles.hpp` can't be justified by a physical mechanism cited in that environment's design doc, it's a placeholder and gets flagged as one — not shipped as if it were real.

This document is the shared physics toolkit and the template every environment doc follows. It does not itself change any DSP — per the frozen-architecture instruction, no code changes happen until a doc exists and is reviewed.

## Document structure (per environment)

Every environment is two independent physical systems stacked on top of each other, not one blob of "car-ness" or "phone-ness":

```
Environment (the space)  →  Playback System (the speakers)  →  Listener  →  DSP
```

A cabin's reflections exist whether or not any speaker is playing through it. A speaker's frequency-response limits and distortion exist whether or not it's sitting in a cabin, a room, or held in a hand. Conflating the two is exactly how "arbitrary EQ" happens — a shelf that's secretly trying to represent both "the room is small" and "the woofer is small" at once, matching neither precisely. Keeping them separate is what makes each number in `EnvironmentProfiles.hpp` traceable to one real, specific cause.

1. **Acoustic Model** — the space alone, as if a perfectly neutral omnidirectional source were playing in it: dimensions, materials/absorption, listener geometry relative to the space, and the reflection timing / modal resonances / decay time that emerge purely from the space's physics. A cabin, a room, "no space at all" (Smartphone) — this section exists even when the answer is "there isn't really one."
2. **Playback System Model** — the speakers/amplification alone, as if placed in free space with no room around them: driver count/size, crossover architecture, amplifier power and headroom, manufacturer tuning philosophy (loudness contour, EQ voicing), and the frequency-response limits / distortion / dynamic behavior that emerge purely from the hardware.
3. **DSP Translation** — the explicit mapping from Acoustic Model and Playback System Model items to specific `EnvironmentProfile` fields, each row tagged **[Acoustic]**, **[Playback System]**, or **[Both]** so it's always traceable which real-world layer justifies which number. Anywhere the current implementation doesn't yet match the physics, that's called out as a **gap**, not silently left alone.

Listener position/geometry isn't its own top-level section — it's addressed in both: where the listener sits *relative to the room* (off-center in a cabin, dead-center in a treated studio) belongs to the Acoustic Model; whether the *playback system* corrects for that offset (a premium OEM system's DSP time-alignment vs. a base sedan's uncorrected passive crossover) belongs to the Playback System Model. Splitting it this way is itself a modeling decision worth stating explicitly per environment, not glossed over.

## Physics toolkit (reused across every doc)

**Room/cabin modal resonances** — enclosed air in any box has resonant frequencies determined by its dimensions. The simplest (axial) mode along a dimension of length `L`:

```
f = c / (2L)        c = speed of sound ≈ 343 m/s
```

A small sedan cabin (~2.0–2.2m interior length) resonates around **78–86 Hz**. A larger SUV cabin (~3.0–3.5m) resonates lower, around **49–57 Hz**. This is *why* small cars sound "boomy" at a specific, predictable frequency rather than generically bass-heavy — it's a standing wave, not a tone-control curve. This directly replaces "add a bass shelf" with "add a resonant peak at the cabin's actual modal frequency."

**Reverberation time (Sabine equation)** — for a diffuse field:

```
RT60 = 0.161 · V / A        V = volume (m³), A = Σ(surface area × absorption coefficient)
```

Typical absorption coefficients (mid-band, α, 0=fully reflective, 1=fully absorptive): glass ≈ 0.03–0.05, hard plastic/dashboard ≈ 0.05–0.1, carpet ≈ 0.2–0.3, fabric car seats ≈ 0.3–0.5, open-cell acoustic foam ≈ 0.7–0.9. A car cabin's RT60 is short in absolute time (small V) but the surfaces are mostly hard and reflective (low A), so it's *live for its size* — this is why a cabin can sound bright/flutter-prone despite being "too small to really echo."

**Reflection arrival time** — first reflection delay ≈ (extra path length)/c. A windshield ~0.4–0.6m from a dash-mounted tweeter produces a first reflection arriving in roughly **1–3ms** — far faster than a studio's first reflection off a treated wall (typically 8–15ms). This is why cabin reflections read as "bright glassy slap," not "room ambience."

**Driver low-frequency extension** — a driver's usable low end is set by its physical size, enclosure volume, and mounting; smaller drivers in smaller/leakier enclosures roll off higher and steeper (small sealed drivers approach a 12dB/octave rolloff below resonance, vs. a large ported enclosure's shallower slope). This is why a phone speaker (~11–15mm) and a car door woofer (~5.25–6.5") and a studio monitor woofer (~6.5–8", larger ported/passive-radiator cabinet) have categorically different, physically-determined low-frequency limits — not just "more or less bass."

**Driver breakup / cone nonlinearity** — pushed near or past linear excursion limits, a driver's cone stops moving as a rigid piston and develops resonant breakup modes, heard as added harmonic distortion concentrated around the breakup frequency (often upper-mid/presence range for small drivers). This is the physical cause behind `saturationAmount`, not a generic "warmth" dial — the amount and *frequency focus* of saturation should track the driver's actual size and excursion headroom.

**Amplifier dynamic limiting** — real amplifiers have finite power supply headroom; driven hard they compress or soft-clip. OEM head-unit amps (~15–25W/channel in a base-trim car) hit this limiting far sooner than a professional monitor's internal amp or a premium OEM system's higher-power stage. `compressionAmount` should track real amplifier headroom, not a vague "this system sounds squashed" impression.

## What this means for the frozen architecture

None of the above requires new parameters or new DSP stages — `EnvironmentProfile` already has `lowShelfFreq/Gain`, `midPeakFreq/Gain/Q`, `roomAmount`, `earlyReflectionAmount`, `saturationAmount`, `compressionAmount`. Phase 3's job is retuning *where these numbers point* so each one cites a physical cause, and — where the current architecture genuinely can't represent something real (e.g. a true resonant modal peak vs. a shelf, or frequency-dependent driver directivity) — documenting that as a gap for a future, explicitly-approved DSP change, not smuggling it in.
