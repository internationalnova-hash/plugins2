# NovaDSP Shared Engine — Migration Checklist

One authoritative DSP implementation per algorithm.
Each standalone plugin is a thin APVTS translator. Nova Vox calls the same engines.

## Status

| Engine         | Shared class exists | Standalone updated | Nova Vox integrated |
|----------------|--------------------|--------------------|---------------------|
| Nova Level     | ✅ Done             | ✅ Done             | ⬜ Pending           |
| Nova Motion FX | ⬜ Pending          | ⬜ Pending          | ⬜ Pending           |
| Space By Nova  | ⬜ Pending          | ⬜ Pending          | ⬜ Pending           |
| Nova Console   | ⬜ Pending          | ⬜ Pending          | ⬜ Pending           |
| Nova Curve     | ⬜ Pending          | ⬜ Pending          | ⬜ Pending           |

## Rules for every shared DSP class

1. No APVTS. No ValueTree. No UI includes.
2. No memory allocation inside process().
3. No calls to getRawParameterValue() or load() inside DSP code.
4. Expose: `prepare(ProcessSpec)`, `reset()`, `setParameters(ParamStruct)`, `process(AudioBuffer<float>&)`.
5. Meter data exposed via atomics written by audio thread, read by UI thread.
6. Parameter structs are plain POD — no juce types, no shared_ptr.

## Phase 2 — Nova Motion FX (next)

Effort: Low
- Move LFO + delay-line modulation out of processBlock
- Create NovaMotionParameters { amount, mode, rate, depth, width, feedback, mix }
- Create NovaMotionDSP with DelayLine members
- Update Nova Motion FX standalone PluginProcessor to translate and call

## Phase 3 — Space By Nova

Effort: Medium
- SmoothedValues in prepareToPlay currently read from APVTS — move smoothing into DSP class
- Pass raw target values via NovaSpaceParameters each block
- DSP class owns the SmoothedValues internally

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

## Preset / automation compatibility guarantee

Changing where the DSP code lives does NOT change:
- APVTS parameter IDs
- Parameter ranges  
- Parameter default values
- Processing algorithm

Presets, automation lanes, and DAW state are entirely determined by APVTS.
The shared DSP engine has no knowledge of them.
