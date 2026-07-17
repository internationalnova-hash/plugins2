# NovaDSP CHANGELOG

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
