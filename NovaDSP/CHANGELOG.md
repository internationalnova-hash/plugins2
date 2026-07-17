# NovaDSP CHANGELOG

---

## [1.1.0] — 2026-07-17  *(in progress — Phase 4B only)*

### Added

- `Console/NovaConsoleParameters` — 55-field POD parameter struct covering all
  Nova Console stages: global, input/output, preamp, filter, EQ, compressor,
  gate, analog engine, and intelligence controls.

- `Console/NovaConsoleDSP` — Phase 4B: Filter + EQ stages extracted.
  - 8 `StateVariableTPTFilter` per channel (HPF + LPF, 1/2/4 cascaded stages)
  - 5 `IIR::Filter` per channel (low shelf, low-mid, high-mid, high shelf, air)
  - 17 SmoothedValues for filter/EQ parameters (block-rate coefficient smoothing)
  - ModeProfile blend (35 ms morph SmoothedValue) included — required by EQ gain scaling
  - EQ coefficient dirty-check cache (avoids redundant IIR allocations on static params)
  - `process(main, sidechain*)` — sidechain reserved for Compressor/Gate phases
  - Remaining 27 SmoothedValues and all other stages added in 4C–4H

- `Console/NovaConsoleRegressionTest` — 30 scenarios (Phase 4B).
  All deterministic (no RNG). All pass `peakAbsDiff == 0.0f`.
  Covers: silence, impulse, sine, noise; HPF/LPF at all three slopes; EQ all
  bands (shelf+bell, boost+cut, min/max); all five modes; mode morph transition;
  combined filter+EQ chain; block sizes 64/128/512/1024/2048; SR 44100/48000/96000;
  mono and stereo.

### Technical Debt

- **TD-003** NovaConsoleDSP: 44 SmoothedValue instances owned individually (not
  consolidated into a ConsoleSmoothers aggregate). Matches original PluginProcessor
  layout. Consolidation deferred to NovaDSP v2 after full extraction.

### Behavioral Differences from Original Plugin (Phase 4B)

- **Smoother seeding on prepare:** Original `prepareToPlay()` left all filter/EQ
  smoothers at 0 after `reset(sr, time)`, causing a ramp-from-zero on the first
  block. `NovaConsoleDSP::prepare(spec, initial)` calls `setCurrentAndTargetValue()`
  for every smoother, eliminating the first-block ramp. This is the correct NovaDSP
  v1.0.0 contract (same fix applied to NovaSpaceDSP in Phase 3).

---

All notable changes to the NovaDSP internal SDK are documented here.

Format: [VERSION] — YYYY-MM-DD  
Sections: Added / Changed / Fixed / Technical Debt

---

## [1.0.0] — 2026-07-17

Initial SDK release. Establishes the shared DSP engine architecture.

### Added

- `Dynamics/NovaLevelDSP` — compressor/limiter engine extracted from Nova Level
  - Linked peak detector, attack/release envelope, magic (tanh) saturation
  - Meters: `getGainReductionDb()`, `getOutputPeak()`, `isOutputHot()`

- `Modulation/NovaMotionDSP` — multi-FX engine extracted from Nova Motion FX
  - StateVariableTPTFilter + DelayLine + juce::Reverb chain
  - Stage gating with active→inactive transition reset
  - Meters: `getPeakL()`, `getPeakR()`

- `Spatial/NovaSpaceDSP` — premium reverb engine extracted from Space By Nova
  - Pre-delay, 6-tap early reflections with diffusion, decorrelation delay
  - 8 StateVariableTPTFilters, motion/halo LFOs, vocal ducking, wet glue saturator
  - 5 LinearSmoothedValues advanced per block via skip()

- `Dynamics/NovaLevelRegressionTest` — 72 scenario regression suite
- `Modulation/NovaMotionRegressionTest` — 20 scenario regression suite
- `Spatial/NovaSpaceRegressionTest` — 29 static + 2 automation scenario regression suite

- `MIGRATION_CHECKLIST.md` — tracks extraction status across all five engines
- `VERSION` — this file

### Common API (all engines)

```cpp
// Consistent lifecycle across all NovaDSP engines:
void prepare(const juce::dsp::ProcessSpec& spec,
             const XxxParameters& initial = {});  // seeds state at init time
void reset() noexcept;                             // clears runtime DSP state
void setParameters(const XxxParameters& p) noexcept; // call once per block
void process(juce::AudioBuffer<float>& buffer) noexcept; // in-place, no alloc
```

### Directory structure

```
NovaDSP/
  Core/           (reserved — shared utilities future)
  Dynamics/       NovaLevel compressor
  EQ/             (reserved — Nova Curve future)
  Modulation/     NovaMotion filter+delay+reverb
  Saturation/     (reserved — Nova Console future)
  Spatial/        NovaSpace reverb
```

### Technical Debt

- **TD-001** NovaMotionDSP: 10 SmoothedValues initialised but not advanced in
  process(). Matches original behaviour. Fix scheduled for v2.
- **TD-002** NovaSpaceDSP: smoother seeding was previously a separate public
  `seedSmoothedValues()` call (two-step contract). Resolved in v1.0.0 by folding
  into `prepare(spec, initial)`.

---

## Versioning policy

NovaDSP uses **semantic versioning**:

- **PATCH** (1.0.x) — bug fixes, documentation. No API change. No algorithm change.
- **MINOR** (1.x.0) — new engine added, new meter accessor, new non-breaking overload. Existing callers unaffected.
- **MAJOR** (x.0.0) — breaking API change, algorithm behavioural change, or parameter range change that would alter saved presets.

**Rule:** A change that alters the output of any regression test at the sample level is always a MAJOR version bump, regardless of apparent scope.
