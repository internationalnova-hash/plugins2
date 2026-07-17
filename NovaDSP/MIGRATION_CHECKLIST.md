# NovaDSP Shared Engine — Migration Checklist

One authoritative DSP implementation per algorithm.
Each standalone plugin is a thin APVTS translator. Nova Vox calls the same engines.

## Directory Structure

```
NovaDSP/
  Core/          — future: shared utilities, DC block, etc.
  Dynamics/      — compressors, limiters
    NovaLevelParameters.h
    NovaLevelDSP.h
    NovaLevelDSP.cpp
    NovaLevelRegressionTest.h
  EQ/            — equalizers
  Modulation/    — LFO, delay, reverb, motion FX
    NovaMotionParameters.h
    NovaMotionDSP.h
    NovaMotionDSP.cpp
    NovaMotionRegressionTest.h
  Saturation/    — console saturation, tape, etc.
  Spatial/       — reverb, stereo width, space algorithms
    NovaSpaceParameters.h
    NovaSpaceDSP.h
    NovaSpaceDSP.cpp
    NovaSpaceRegressionTest.h
```

## Status

| Engine         | Shared class exists | Standalone updated | Nova Vox integrated |
|----------------|--------------------|--------------------|---------------------|
| Nova Level     | ✅ Done             | ✅ Done             | ⬜ Pending           |
| Nova Motion FX | ✅ Done             | ✅ Done             | ⬜ Pending           |
| Space By Nova  | ✅ Done             | ✅ Done             | ⬜ Pending           |
| Nova Console   | ⬜ Pending          | ⬜ Pending          | ⬜ Pending           |
| Nova Curve     | ⬜ Pending          | ⬜ Pending          | ⬜ Pending           |

## Rules for every shared DSP class

1. No APVTS. No ValueTree. No UI includes.
2. No memory allocation inside process().
3. No calls to getRawParameterValue() or load() inside DSP code.
4. Expose: `prepare(ProcessSpec)`, `reset()`, `setParameters(ParamStruct)`, `process(AudioBuffer<float>&)`.
5. Meter data exposed via atomics written by audio thread, read by UI thread.
6. Parameter structs are plain POD — no juce types, no shared_ptr.

## Phase 4 — Nova Console (next)

Effort: High
- 30+ smoothed values — move all into NovaConsoleDSP

## Phase 4 — Nova Console

Effort: High
- 30+ smoothed values — move all into NovaConsoleDSP
- Mode blending via ProfileMorph stays inside DSP class
- ModeProfile struct becomes part of NovaConsoleDSP.h
- No behaviour changes; only restructured ownership

## Phase 5 — Nova Curve

Effort: High
- Band states as plain struct array (not atomics owned by processor)
- FFT analysis stays in processor (UI concern) or moves to a separate analyzer class
- Coefficient update logic extracted into NovaCurveDSP

## Technical Debt

### TD-001 — NovaMotionDSP: SmoothedValues initialized but never advanced
**Severity:** Low  
**Discovered:** Phase 2 extraction (Nova Motion FX)  
**Description:** `NovaMotionDSP` initialises 10 `SmoothedValue<float>` members
(`smCutoff`, `smResonance`, `smDrive`, `smFeedback`, `smDelayMix`, `smSize`,
`smReverbMix`, `smInput`, `smOutput`, `smMix`) with a 20ms ramp time in
`prepare()`, but `process()` never calls `getNextValue()` on any of them.
The raw `params` struct values are used directly instead.  
**Effect:** Parameter changes take effect immediately with no interpolation ramp,
which may produce zipper noise when parameters are swept quickly.  
**Status:** Preserved intentionally — the original `processBlock` had the same
behaviour. Algorithm-compatibility takes priority over correctness.  
**Resolution:** Implement proper per-sample smoothing in NovaMotionDSP v2 after
the full NovaDSP migration is complete and all products are verified identical.
Do NOT fix this during the v1 extraction phase.

### TD-002 — NovaSpaceDSP: SmoothedValue seeding requires post-prepare call from processor
**Severity:** Low  
**Discovered:** Phase 3 extraction (Space By Nova)  
**Description:** The original `prepareToPlay()` seeds the five `LinearSmoothedValue` members by
reading current APVTS values directly (`setCurrentAndTargetValue(apvts.getRawParameterValue(...))`).
The shared engine has no APVTS access, so `prepare()` seeds at parameter defaults. To exactly
match original behavior the standalone `prepareToPlay()` immediately calls `spaceDSP.seedSmoothedValues(init)`
after `spaceDSP.prepare()`, passing a struct populated from APVTS. This is a structural requirement
of the seeding contract — callers MUST call `seedSmoothedValues()` after every `prepare()` call.  
**Effect:** If a host calls `prepareToPlay()` while the plugin is loaded at non-default settings
and the caller fails to call `seedSmoothedValues()`, the smoothers will start from defaults and
ramp over 200ms before reaching the correct values. In practice this is inaudible since
`prepareToPlay` is called before audio starts.  
**Resolution:** Acceptable as-is for v1. A future option is for `prepare()` to accept an initial
`NovaSpaceParameters` struct so that seeding happens atomically in a single call.

---

## Preset / automation compatibility guarantee

Changing where the DSP code lives does NOT change:
- APVTS parameter IDs
- Parameter ranges  
- Parameter default values
- Processing algorithm

Presets, automation lanes, and DAW state are entirely determined by APVTS.
The shared DSP engine has no knowledge of them.
