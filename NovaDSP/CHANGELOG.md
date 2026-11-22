# NovaDSP CHANGELOG

---

## [1.1.0] — 2026-07-17  *(in progress — Phase 4D)*

### Added

- `Console/NovaConsoleParameters` — 55-field POD parameter struct covering all
  Nova Console stages: global, input/output, preamp, filter, EQ, compressor,
  gate, analog engine, and intelligence controls.

- `Console/NovaConsoleDSP` — Phase 4C: Input/Output gain, Preamp, Gate stages added.
  Phase 4B content: Filter + EQ stages.
  - 8 `StateVariableTPTFilter` per channel (HPF + LPF, 1/2/4 cascaded stages)
  - 5 `IIR::Filter` per channel (low shelf, low-mid, high-mid, high shelf, air)
  - 17 SmoothedValues for filter/EQ parameters (block-rate coefficient smoothing)
  - ModeProfile blend (35 ms morph SmoothedValue) included — required by EQ gain scaling
  - EQ coefficient dirty-check cache (avoids redundant IIR allocations on static params)
  - Phase 4C adds: inputSmoothed, outputSmoothed (30 ms); driveSmoothed, colorSmoothed,
    trimSmoothed (30 ms); 5 gate smoothers (30–45 ms); preampPrevInput[2] history;
    gateEnv[2], gateHoldCounter[2], gateDetectorHpf/LpfState[4][2]
  - `processPreamp()` — manual oversampling (1×/2×/4×), linear interpolation,
    odd+even harmonic saturation (tanh), color tilt, transient retention
  - `processGate()` — dual-mode (expand/hard), hysteresis, hold counter,
    per-channel independent envelope, optional detector sidechain filter
  - `process(main, sidechain*)` — full chain: input gain → preamp → filter → EQ →
    compressor → gate → modeTrim + clip(±1.35) → output gain
  - Remaining stages (MixAssist, SmartGain, AnalogEngine) added in 4E–4G

- Phase 4D adds: Compressor stage + `gainReductionMeter` atomic.
  - 7 compressor SmoothedValues (threshold 45ms, ratio 50ms, attack 15ms, release 45ms,
    mix 30ms, makeup 30ms, punch 30ms)
  - Linked stereo detector: RMS/peak blend (66%/34%), crest-factor auto-release
    ([35ms, 450ms] mapped via jmap on clamped crest [1.0, 10.0])
  - 4-dB soft knee, quadratic gain-computer in knee region
  - Separate attack/release ballistics on gain state (gainAttack = jmax(0.25, attackMs×0.45))
  - Punch processing: 1-pole LP (0.92 coeff), transient = signal − LP,
    driven = signal + transient × punch × 0.45 × modePunch
  - Detector HPF/LPF: same 4-stage state array design as Gate
  - Thickening: `thickBlend = 0.012 + 0.01×evenDrive`; dry blended with tanh saturation
  - Parallel compression: `compMix` 0–100 (smoother at /100), wet/dry blend per sample
  - `gainReductionMeter`: atomic float; 0.84 decay when active, 0.92 decay when bypassed;
    normalized to [0,1] via jlimit(0,1, maxReduction/18)
  - `getGainReductionDb() const noexcept` — UI-thread-safe accessor
  - qualityTightness: quality==2 → 1.03, quality==0 → 0.92, else 1.0

- `Console/NovaConsoleRegressionTest` — expanded to 125 scenarios (Phase 4D: +45 net).
  All deterministic (no RNG). All pass `peakAbsDiff == 0.0f`.
  Phase 4D audit corrected 6 parameter-range errors: all min/default/max scenarios now
  use exact APVTS `NormalisableRange` bounds from `PluginProcessor.cpp`. Specifically:
  threshold min −40 dB (was −60), ratio max 10:1 (was 20:1), attack min 0.5 ms (was 0.1),
  attack max 60 ms (was 150), release max 500 ms (was 1000), makeup 0/+24 dB (was −6/+6).
  Added external sidechain runner (`runScenarioWithSidechain`): identical sidechain buffer
  supplied to both reference and engine. 4 new external-sidechain scenarios (122–125):
  stereo main + stereo sidechain, nullptr fallback, mono main + mono sidechain,
  stereo main + stereo sidechain with HPF applied to external detector.
  Phase 4D additions cover: comp off, silence, impulse, loud sine; threshold/ratio/attack/
  release/punch/makeup/mix at APVTS min-default-max; quality eco/master; all five modes × comp;
  mono, SR 48000/96000, block sizes 64/2048; filter+comp, EQ+comp, gate+comp chains;
  full chain all stages; 4 external-sidechain scenarios.

### Technical Debt

- **TD-003** NovaConsoleDSP: 44 SmoothedValue instances owned individually (not
  consolidated into a ConsoleSmoothers aggregate). Matches original PluginProcessor
  layout. Consolidation deferred to NovaDSP v2 after full extraction.

### Behavioral Differences from Original Plugin (Phase 4B–4C)

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
