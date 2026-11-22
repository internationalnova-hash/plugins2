# Nova 7even — Architecture Plan (pre-implementation)

Status: **planning only**. No source files, CMakeLists, or scaffolding have been created. This document is the deliverable for the spec's own section 79 checklist ("inspect repo, propose architecture, define frequencies/gain-staging/modes/oversampling/parameter IDs, build a test harness — THEN build v0.1"). It mirrors the Nova Bass planning pass done before that plugin's code was written.

---

## 1. Context

Nova 7even is a vintage-1970s-British-console-style Class-A preamp + 3-band EQ + HPF, with Clean/Classic/Hot character modes, a real-ballistics VU meter, and a **locked, pixel-accurate** photorealistic hardware UI (wood cheeks, aged charcoal/blue faceplate, red/burgundy knobs, cream legends). The full ~80-section spec is treated as fixed requirements; this document is engineering planning for **how** to build it against this codebase's existing patterns — not a re-litigation of **what** to build.

Catalog root: `/Users/internationalnova/Nova/plugins2/` (~26 JUCE 8.0.8 plugins, WebView UIs, FetchContent-based builds).

---

## 2. Scope

**In (v0.1, per spec section 75):**
Input Trim, Preamp Gain, input nonlinear (transformer) stage, Class-A coloration stage, HPF, Low EQ, Mid EQ, fixed 12k high shelf, Output stage, Clean/Classic/Hot, Mix, Out Trim, 2x oversampling (architected for 4x), VU meter, Bypass, Phase. AU + VST3. AAX SDK path wired but not built.

**Out (deferred):**
AAX build, preset browser beyond the factory list, NovaLicense gating (will be added later, same as Nova Sub), any UI asset generation (flagged in §7 below), 4x oversampling as a *user-facing* control.

---

## 3. Reusable infrastructure — findings

Everything below was confirmed by reading actual files in this repo, not assumed.

### 3.1 Build system
`/Users/internationalnova/Nova/plugins2/Nova Doubler/CMakeLists.txt` is the cleanest current reference (more recent than Nova Keys): FetchContent JUCE 8.0.8 pinned, `juce_set_aax_sdk_path` wired for APPLE/WIN32 but AAX deliberately excluded from `FORMATS`, `juce_add_binary_data` for the WebUI bundle, `juce_add_plugin(FORMATS VST3 AU Standalone)`, and one `juce_add_console_app` validation-harness target per DSP subsystem (`NovaDoublerModulatorValidation`, `NovaDoublerShiftLineValidation`, …), each linking only `juce::juce_core` + the relevant `_shared/NovaDoublerCore` headers so harnesses build fast and headless. Nova 7even's CMakeLists should copy this shape exactly.

### 3.2 Processor/parameter skeleton
`Nova Heat/Source/PluginProcessor.h` + `.cpp` and `Nova Tone/Source/PluginProcessor.cpp` are the closest DSP-shape precedents (single-band-ish coloration + shelf/peak EQ + output trim). Standard skeleton confirmed across the catalog: `juce::AudioProcessorValueTreeState apvts` member, `createParameterLayout()` building `AudioParameterFloat`/`Choice`/`Bool`, `ScopedNoDenormals` at the top of `processBlock`, XML-based `getStateInformation`/`setStateInformation` via `apvts.copyState()`/`replaceState()`. `ParameterIDs.hpp` uses `inline constexpr char id[] = "snake_case_id";` per parameter (Nova Heat's file is 7 lines, trivial to copy the pattern from).

### 3.3 Nova Heat's saturation — evaluated, **not a direct fit**
`Nova Heat/Source/PluginProcessor.cpp:54-63` (`applySaturation`) is a single `tanh`-based asymmetric-cubic-skew shaper with a 3-mode profile table (`ModeProfile` struct: driveMultiplier, asymmetry, transientSoftening, etc., lines 9-29). It's a good **pattern reference** (mode-profile-as-struct, per-mode deltas, dynamic envelope-driven drive) but its actual curve is tuned for a "warmth/character" harmonic exciter, not a two-stage transformer+Class-A preamp model, and it isn't oversampled. Nova 7even needs its own curve (§5.2-5.3) — Nova Heat's *structure* is reused, its *shape* is not.

### 3.4 Oversampling — one real precedent, directly reusable as a pattern
`_shared/NovaAIMasterCore/DSP/NovaSaturation.h` + `.cpp` wraps `juce::dsp::Oversampling<float>` with `filterHalfBandFIREquiripple`, factor `1` (i.e. 2x), latency-compensated (`true, false` ctor args), `initProcessing(maximumBlockSize)` in `prepare`, `processSamplesUp`/`processSamplesDown` bracketing the nonlinearity in `process()`. This is the only oversampling-wrapped saturation stage found in the whole catalog and is architecturally exactly what section 30 asks for (oversample only the nonlinear stage, not the EQ). `Nova Apex/Source/PluginProcessor.h:130-132` also carries a `std::unique_ptr<juce::dsp::Oversampling<float>> oversampler` member for its drive stage but with a *selectable* factor exposed via an `AudioParameterChoice` ("Off","2x","4x") — useful precedent for the *architecture* (factor-selectable), even though v0.1 won't expose it as a user control (§5.6).

### 3.5 EQ / filter precedents
No standalone reusable "EQ band" class exists in `_shared/`. Two live patterns in-catalog:
- **Nova Console** (`Source/PluginProcessor.cpp:170-225`) — continuous-knob 4-band EQ (Low/Low-Mid/High-Mid/Air), each with independent Freq/Q/Shelf-or-Bell mode, built directly from `juce::dsp::IIR::Coefficients<float>::makeLowShelf/makeHighShelf/makePeakFilter` computed per-block from raw parameter values (lines 711-726). Continuous, not stepped — not what Nova 7even needs, but confirms the coefficient functions to use.
- **Nova Tone** (`Source/PluginProcessor.cpp:36-40, 60-64, 224-247`) — **this is the exact pattern Nova 7even needs**: `AudioParameterChoice` with human-readable `StringArray` labels ("20","30","60","100" / "3k","4k","5k","8k","10k","12k","16k"), a `static constexpr std::array<float, N>` lookup table of actual Hz values in the same order, and `getChoiceValue(table, (int)apvts.getRawParameterValue(id)->load())` to resolve the selected index to a real frequency before feeding `makeLowShelf`/`makePeakFilter`. Nova 7even's stepped HPF/Low/Mid selectors (§5.4) will use this pattern verbatim.

`_shared/NovaAIMasterCore/DSP/NovaCorrectiveEQ.h` (hand-rolled RBJ-cookbook biquads, 4-band) and `_shared/NovaBassCore/Filter/NovaBassFilter.h` (thin `StateVariableTPTFilter` wrapper for HPF/LPF) were also read; both are viable *coefficient-math* references but are single-plugin-scoped classes, not shared EQ infrastructure — no benefit to depending on either cross-plugin.

### 3.6 Metering
No true VU-ballistics class exists anywhere in the catalog. `Nova Vox/Source/ui/VUMeter.h` is a native `juce::Component` LED-segment meter but its ballistics (`attackCoef=0.5f, releaseCoef=0.07f`, VUMeter.h:32-35) are fast-attack/slow-release — that's peak/PPM-style behavior mislabeled "VU," not the symmetric ~300ms-integration ballistics a real VU meter has. It is **not reusable** for Nova 7even's DSP-side ballistics, and its `paint()` is irrelevant anyway since Nova 7even's UI is WebView-based (an SVG needle, not a native Component). Only its dB-segment-mapping math is worth a glance, nothing more. `Nova Apex/Source/PluginProcessor.h:47-51` confirms the catalog convention for exposing meter data: `std::atomic<float>` members on the processor (never APVTS parameters), sampled by the editor's timer.

### 3.7 A/B snapshot system
**None exists.** `grep -rliE "a.?b.?system|snapshotA|snapshotB|abSnapshot|compareState"` across all of `plugins2` (excluding `build*`) returned zero real hits — only false positives inside JUCE's own bundled `examples/GUI/CameraDemo.h` (unrelated `abSomething` token collisions). Nova 7even's A/B system is new (§5.7), built from scratch using the `apvts.copyState()`/`replaceState()` primitives already used for plugin state save/load.

### 3.8 WebView UI bridge
Identical pattern across nearly every plugin; read `Nova Doubler/Source/PluginEditor.cpp` in full as the reference (most recently built, cleanest example — user's memory confirms it was completed/installed/auval-passed 2026-08-15). Confirmed pattern:
- `SinglePageBrowser` (a `juce::WebBrowserComponent` subclass) constructed with `Options` from `withNativeIntegrationEnabled()`, `withResourceProvider(getResource)`, and `withNativeFunction("setParam", ...)` that dispatches into `apvts.getParameter(id)->setValueNotifyingHost(...)`.
- `getResource(url)` matches by **substring on the raw URL**, lower-cased, in a fixed if-chain (`index.html`, `js/index.js`, `juce/index.js`, `juce/check_native_interop.js`) — explicitly *not* stripping `getResourceProviderRoot()`, which the code comments say silently breaks JS loading. Copy this exactly.
- Live telemetry (VU meter data, in Nova 7even's case) goes out via a `startTimerHz(30)` `timerCallback()` in the editor that reads `std::atomic<float>` members off the processor and calls `webView->evaluateJavascript("window.updateX(...)")` — see `PluginEditor.cpp:40-65` for the exact shape (Doubler's stereo-field telemetry). Nova 7even's VU meter needle angle will be pushed the same way.

### 3.9 Licensing
`_shared/NovaLicense/NovaLicense.h` — seat-limited activation module, already proven on Nova Sub. Not wired for v0.1 per scope, but the integration point (editor construction calls `isUnlocked()`, shows unlock screen via a native function if false) is a known, cheap addition for later — no action needed now.

### 3.10 Validation-harness build pattern
`Nova Bass/Source/StereoValidation.cpp` (and its siblings `VoiceValidation.cpp`, `ColorValidation.cpp`, `PresetValidation.cpp`, etc.) is the reference for section 63's DSP test harness requirement: a plain `int main()` console app, `check(condition, description)` printing PASS/FAIL, synthetic-signal generators (sine, decorrelated stereo, etc.), asserting measured properties against expected thresholds. Wired into `CMakeLists.txt` via `juce_add_console_app(...)` + `target_link_libraries(... juce::juce_core juce::juce_recommended_config_flags)` only — no plugin-client dependency, so it builds and runs fast, headless, in seconds. Nova 7even will have one such harness per DSP subsystem plus one comprehensive chain-level harness (§8).

---

## 4. Directory layout

```
Nova 7even/
  CMakeLists.txt
  Source/
    PluginProcessor.h / .cpp
    PluginEditor.h / .cpp
    ParameterIDs.hpp
    ABSnapshot.h                      # new — §5.7
    TransformerStageValidation.cpp    # console-app harness
    ClassAAmpValidation.cpp           # console-app harness
    EQValidation.cpp                  # console-app harness (section 64 freq check lives here)
    VUMeterValidation.cpp             # console-app harness
    DSPHarness.cpp                    # section 63 comprehensive chain harness
    ui/public/
      index.html
      js/index.js
      js/juce/index.js
      js/juce/check_native_interop.js
      img/                           # raster texture assets — see §7.3, sourcing flagged
        wood-grain.png (@1x/@2x/@3x)
        faceplate-texture.png (@1x/@2x/@3x)
        knob-cap-red.png (@1x/@2x/@3x)   # only if CSS gradients can't sell it — §7.2
_shared/NovaSevenCore/
  DSP/
    NovaTransformerStage.h / .cpp
    NovaClassAAmp.h / .cpp
    NovaSevenEQ.h / .cpp
    NovaVUBallistics.h / .cpp
```

This mirrors `_shared/NovaBassCore/Filter/` and `_shared/NovaAIMasterCore/DSP/` — one `_shared/Nova<X>Core/` directory per plugin, subsystem-named subfolders, header+cpp pairs. `ABSnapshot.h` stays plugin-local (Source/) rather than `_shared/` since it's genuinely new and unproven; promote to `_shared/` later only if a second plugin wants it.

---

## 5. Core DSP design decisions

### 5.1 Signal flow (locked)

```
In → Input Trim → [Input Transformer Stage] → [Class-A Amp Stage, mode-dependent]
   → HPF → Low EQ → Mid EQ → 12k High Shelf
   → Output Stage → Mix (dry/wet blend of everything above) → Out Trim
   → Phase Invert → Bypass (hard A/B of entire chain) → Out
```
VU meter taps the final output (post Out Trim, pre-bypass-passthrough when bypassed shows input instead — standard channel-strip convention). Oversampling (§5.6) wraps only the two bracketed nonlinear stages.

### 5.2 Input Transformer stage

Models core saturation, which in real transformers is **frequency-dependent** (flux saturates more readily at low frequencies) and mildly asymmetric. Concrete design:
1. Pre-emphasis: `makeLowShelf(sampleRate, 150Hz, Q=0.6, gain=+3dB)` applied *only to the signal feeding the nonlinearity*, not the audible path — biases the shaper toward saturating bass content first, the way iron core actually behaves.
2. Nonlinearity: `y = tanh(x·k) + 0.04·x²·sign(x)` — dominant odd-harmonic tanh core plus a small even-harmonic term (the `x²` component) for the slight 2nd-harmonic "iron" coloration real transformers add. `k` ranges roughly 1.0–1.6 driven jointly by Input Trim level and a fixed small always-on amount (this stage is *not* separately user-controllable per spec's v0.1 scope — it's an always-on character tied to signal level, matching how a real transformer has no "amount" knob).
3. Post: subtract the pre-emphasis (inverse low-shelf) so the *audible* tonal balance is unaffected — only the harmonic content added during saturation carries the low-frequency bias, not the fundamental's frequency response.

This is intentionally understated: the transformer stage should be felt as "glue," not heard as distortion.

### 5.3 Class-A Amp stage

The main coloration stage, driven by the **Preamp Gain** knob and the **Clean/Classic/Hot** mode. Distinct curve from §5.2 — stronger, mode-shaped, with a dynamic "sag" component:
```
driven = x · driveGain(preampGain, mode)
sagEnv = 0.995·sagEnv + 0.005·|driven|          // ~15ms-ish envelope follower
k = mode.baseCurve + sagEnv · mode.sagAmount     // dynamic knee softening under sustained level — Class-A "bias sag"
positive: y = tanh((driven + mode.asymmetry·0.3·driven³) · k)
negative: y = tanh((driven + mode.asymmetry·0.3·driven³) · max(0.2, k − mode.asymmetry·0.4))
```
Same two-piece asymmetric-tanh shape as Nova Heat's `applySaturation` (structure reused, coefficients are new — see §3.3), plus the sag envelope for the "breathing under drive" quality real Class-A stages have that Nova Heat's doesn't model.

### 5.4 Clean / Classic / Hot — concrete mode table

| Mode | driveMultiplier | asymmetry | sagAmount | evenHarmonic (transformer bleed) | transient rounding (envelope ms) |
|---|---|---|---|---|---|
| Clean | 0.55 | 0.05 | 0.10 | 0.02 | 2 ms (fast, minimal rounding) |
| Classic | 1.00 | 0.18 | 0.35 | 0.05 | 8 ms |
| Hot | 1.70 | 0.32 | 0.65 | 0.09 | 18 ms |

"Transient rounding" = the time-constant of the sag envelope follower in §5.3 — larger = slower-reacting = more audible transient softening/compression-like rounding. Clean is intentionally close to transparent (this is a *character* preamp, not a clean-boost plugin, but Clean mode must still sound meaningfully different from Hot in an A/B).

### 5.5 Stepped EQ / HPF frequencies (locked, confirmed against the mockup's suggested values)

The spec explicitly flags the mockup's printed labels as unverified — final values below were evaluated for musical sense (they land on a recognizable British-console-style stepped-EQ layout, e.g. Neve-style low/mid step spacing) and are adopted as-is; §8.2 defines the runtime verification that must re-confirm each one against the actual filter math before shipping.

| Band | Steps (Hz) | Filter type | Q |
|---|---|---|---|
| HPF | Off, 20, 50, 80, 160, 300 | `StateVariableTPTFilter` highpass (matches `NovaBassFilter.h` pattern, §3.5) | fixed ~0.707 (Butterworth-ish, no resonant bump) |
| Low | 35, 60, 110, 220, 360, 700 | Shelf (`makeLowShelf`) | 0.6 (broad, musical) |
| Mid | 110, 220, 360, 700, 1600, 3200 | Bell (`makePeakFilter`) | 0.8 (broad, musical — not surgical) |
| High | fixed 12,000 (no selector) | Shelf (`makeHighShelf`) | 0.6 |

Implementation follows Nova Tone's exact pattern (§3.5): `AudioParameterChoice` with string labels, `static constexpr std::array<float,N>` value table, `getChoiceValue()` lookup feeding the IIR coefficient functions.

### 5.6 Oversampling architecture

- Wraps **only** §5.2 + §5.3 (the two nonlinear stages), processed back-to-back inside one oversampled block — not the EQ, not the meter tap. Matches `NovaSaturation.h`'s proven wrapper shape exactly: `juce::dsp::Oversampling<float>(numChannels, factor, filterHalfBandFIREquiripple, /*isMaxQuality*/true, /*integerLatency*/false)`.
- v0.1 ships **hardcoded 2x** (`factor = 1` in JUCE's oversampling-stage-count convention) — no user-facing oversampling selector in v0.1, unlike Nova Console's exposed Off/2x/4x choice (§3.4). "Architect for 4x" means: the wrapper class takes `factor` as a constructor/`prepare()` argument so bumping to 4x later is a one-line change plus re-`prepare()`, not a redesign.
- Latency: report `oversampler.getLatencyInSamples()` via `setLatencySamples()` on the processor so hosts compensate correctly — required for any oversampled plugin, easy to forget.
- Must be correct across the full 44.1–192kHz range the spec requires (§ sample-rate support); the halfband FIR filter's cutoff is relative to Nyquist so this is automatic, but `prepare()` must be called on every `prepareToPlay` with the current `sampleRate`, not cached from construction.

### 5.7 VU meter ballistics

True VU behavior (not Nova Vox's fast/slow peak-style meter, §3.6): symmetric attack and release, ~300ms integration time to reach ~99% of a step input (per the ANSI C16.5 mechanical VU standard). Implementation: a single one-pole smoother applied identically in both directions —
```
coeff = 1 − exp(−1 / (0.3 · sampleRate))   // 300ms time constant
displayLevel += (rectifiedInputRMS − displayLevel) · coeff
```
computed on the audio thread from the post-Out-Trim signal (§5.1), written to an `std::atomic<float>` each block, and pushed to the WebView every 30Hz via the timer→`evaluateJavascript` pattern (§3.8) to animate an SVG needle. Reference level: **0 VU = −18 dBFS**, i.e. the displayed needle position is `20·log10(displayLevel) + 18` mapped onto the meter's dB scale — matches the spec's fixed reference and gives ~18dB of headroom above 0VU before digital clipping, standard practice.

### 5.8 Gain staging model

The Input Trim knob is **labeled 0–80** on the UI (an intentional vintage-console convention — real preamps like this print a 0–80dB-style scale even though the pot doesn't apply 80dB of actual gain) but must not literally apply up to 80dB internally (would clip instantly). Mapping:
- `input_trim` parameter: `AudioParameterFloat`, UI-displayed range 0–80 (unitless "console" label, matching the mockup), internally mapped via a skewed `NormalisableRange` to **−20dB to +24dB** actual applied gain, with the skew weighting most of the knob's rotation into the musically useful −6dB…+12dB zone (mirrors how a real trim pot's printed dB scale is deliberately nonlinear against actual gain-before-clip). This directly resolves the spec's explicit "how does 0–80 map without becoming absurd" question.
- `preamp_gain`: separate float 0–10 (unitless "amount into the Class-A stage," not a dB value) — controls §5.3's `driveGain(preampGain, mode)`, auto-compensated internally (similar to Nova Heat's `autoTrimDb` pattern, `PluginProcessor.cpp:354`) so cranking drive doesn't also blow up output level; the user hears more saturation, not more loudness.
- `output_trim`: `AudioParameterFloat`, ±15dB, post-EQ, pre-mix-blend-with-dry. This is the "Output stage" level control from the spec's scope list.
- `mix`: 0–100%, linear crossfade between the fully-dry input (tapped pre-everything) and the fully-processed signal — same `jmap` dry/wet technique as Nova Heat (`PluginProcessor.cpp:345-352`) and Nova Tone.

### 5.9 A/B snapshot system (new — no precedent to reuse, §3.7)

`Source/ABSnapshot.h`, header-only, plugin-local:
```cpp
class ABSnapshot {
public:
    void captureA(juce::AudioProcessorValueTreeState& apvts) { stateA = apvts.copyState(); }
    void captureB(juce::AudioProcessorValueTreeState& apvts) { stateB = apvts.copyState(); }
    void recallA(juce::AudioProcessorValueTreeState& apvts) { if (stateA.isValid()) apvts.replaceState(stateA); }
    void recallB(juce::AudioProcessorValueTreeState& apvts) { if (stateB.isValid()) apvts.replaceState(stateB); }
    void copyActiveToOther(...);
    // serialize both ValueTrees as child elements under the plugin's saved
    // XML state so A/B survives session save/reload, not just in-session.
};
```
Both snapshots are embedded as extra child `ValueTree`s inside `getStateInformation`'s XML, alongside the live `apvts.state`, so DAW session recall restores both A and B, not just whichever is currently active. This is intentionally simple (no undo stack, no >2 slots) — matches spec scope.

### 5.10 Parameter IDs (locked — 13 parameters, stable once shipped per spec section 60)

| ID | Type | Range / Choices | Default |
|---|---|---|---|
| `input_trim` | Float | 0–80 (UI) → −20…+24dB (internal) | 40 (≈ 0dB internal) |
| `preamp_gain` | Float | 0–10 | 3.0 |
| `character_mode` | Choice | Clean, Classic, Hot | Classic (1) |
| `hpf_freq` | Choice | Off, 20, 50, 80, 160, 300 | Off (0) |
| `low_freq` | Choice | 35, 60, 110, 220, 360, 700 | 110 (index 2) |
| `low_gain` | Float | ±15 dB | 0 |
| `mid_freq` | Choice | 110, 220, 360, 700, 1600, 3200 | 700 (index 3) |
| `mid_gain` | Float | ±15 dB | 0 |
| `high_shelf_gain` | Float | ±15 dB (freq fixed 12k) | 0 |
| `mix` | Float | 0–100% | 100 |
| `output_trim` | Float | ±15 dB | 0 |
| `phase_invert` | Bool | — | false |
| `bypass` | Bool | — | false |

VU meter level and A/B active-slot state are **not** APVTS parameters (matches `Nova Apex`/`Nova Heat` convention of `std::atomic<float>` telemetry members, §3.6) — meters aren't automatable and shouldn't show up in a host's parameter list.

---

## 6. Character-mode / gain-staging cross-check

Worth flagging explicitly: `preamp_gain` (0-10) and `character_mode` are multiplicative in §5.4's table (`driveMultiplier` scales the same knob range differently per mode), meaning the *same* preamp_gain=5 setting sounds meaningfully different across Clean/Classic/Hot — that's intentional (matches real console-strip behavior where a "character" switch changes the whole gain stage's response, not just adds fixed coloration on top). This should be called out in the functional acceptance walkthrough (§8.3) as a thing to specifically listen for, not just measure.

---

## 7. UI asset strategy

The spec bans "one giant flattened screenshot" and requires vector-for-functional + raster-for-material. Catalog precedent (§7.1) shows this project needs to go further than anything built so far.

### 7.1 What the catalog currently does (confirmed by reading `Nova Heat/Source/ui/public/index.html`)
Every knob in the catalog (Nova Heat's `.knob` CSS class, lines 303-423) is built from **pure CSS**: a `conic-gradient(from 218deg, ...)` sweep for the fill arc, layered `radial-gradient`s for shading/bevel, and a single inline data-URI SVG `feTurbulence` filter (line 64) for a subtle noise texture applied as a `background-image`. **No raster/photographic knob images exist anywhere in this catalog's WebViews.** This achieves a clean "soft-analog" modern look, which is good precedent for the vector-primary approach the spec wants — but it does not, by itself, reach "photorealistic vintage hardware" fidelity (real wood grain, aged/worn charcoal-blue metal, red/burgundy knob-cap material). That's a materially higher visual bar than anything else in the catalog has attempted.

### 7.2 Recommended hybrid split
- **SVG (inline, vector, crisp at any Retina zoom):** all text/legends (cream lettering — either SVG `<text>` or a bundled vintage-style webfont, never baked into a raster, so it stays sharp and the labels stay correctable if §8.2's frequency-verification pass needs to fix a mislabeled value), the VU needle + arc + scale ticks (driven by `transform: rotate()` from the §5.7 telemetry push, same mechanism as Doubler's stereo-field visualization), switch/button shapes, screw-head glyphs (one `<symbol>` instanced via `<use>` at each screw position — cheap, consistent, resolution-independent), and each knob's pointer/indicator line.
- **CSS gradients:** general bevel/shading/lighting layers, reusing the catalog's existing conic/radial-gradient knob technique as the base layer under everything else.
- **Raster (PNG, exported @1x/@2x/@3x for Retina, tileable/stretchable — not one flattened panel):** only for the things CSS gradients cannot convincingly fake — a wood-grain texture strip for the side-cheeks, an aged/worn charcoal-blue faceplate texture (brushed-metal-with-wear look) tiled across the panel background, and *possibly* a photographic/rendered red-burgundy knob-cap texture layered under the SVG indicator line if pure CSS gradients can't sell the vintage photorealism (try CSS-only first, since it's cheaper and infinitely scalable; fall back to a raster knob-cap only if a visual comparison against the locked mockup shows it's needed). These composite as `background-image` layers *within* the vector-built knob/panel markup — never as a single flattened screenshot of the whole UI, satisfying the spec's explicit requirement.

### 7.3 Flagged follow-up (does not block DSP/architecture work)
Producing the actual wood-grain, aged-metal, and (if needed) knob-cap texture assets is an image-generation or asset-sourcing task, not something this research pass can or should attempt. Recommend resolving this with either (a) a dedicated image-generation step to produce tileable, PBR-style texture swatches matching the mockup's exact aesthetic, or (b) the user sourcing/licensing real texture photography, **before UI implementation begins** — DSP/build work (§8) can proceed independently and isn't blocked by this.

---

## 8. Build order and verification

Headless-validate-then-integrate, matching the catalog's established discipline (Nova Doubler/Nova Bass build order, §3.10).

### 8.1 Build order
1. `NovaTransformerStage` (§5.2) — standalone, `TransformerStageValidation.cpp`: silence→silence, DC input stays bounded/safe, near-linear at low level, harmonic content measurably increases with input level, no NaN/Inf at extreme drive, left/right channels processed independently (no cross-talk).
2. `NovaClassAAmp` (§5.3) — standalone, `ClassAAmpValidation.cpp`: Clean vs Classic vs Hot produce measurably distinct THD and asymmetry (2nd-harmonic content) at matched input level, per §5.4's table; sag envelope only engages with sustained level, not on transients; bounded output at max drive.
3. Oversampling wrapper (§5.6) integrated around 1+2 — validate: `getLatencyInSamples()` reports correctly, aliasing content above Nyquist/2 measurably reduced vs. non-oversampled, correct behavior at 44.1/48/88.2/96/192kHz.
4. `NovaSevenEQ` (§5.4/5.5) — standalone, `EQValidation.cpp` — **this is where section 64's requirement lives**: for every stepped HPF/Low/Mid choice, measure the actual filter's −3dB corner (HPF) or shelf/peak center frequency via a swept-sine or impulse+FFT measurement, and assert it matches the table in §5.5 within a defined tolerance (e.g. ±5%). Any mismatch found here must correct the implementation (or, if the mockup's *printed* label turns out to be the thing that's wrong, flag that back for a mockup correction) — never ship a UI label that doesn't match the measured frequency.
5. `NovaVUBallistics` (§5.7) — standalone, `VUMeterValidation.cpp`: step-response settles to ~99% at 300ms ±10%, symmetric attack/release time constants, 0VU reading corresponds to a calibrated −18dBFS input sine within a small tolerance.
6. Full-chain integration into `PluginProcessor` — `DSPHarness.cpp` runs the section 63 suite (silence, DC, sine sweep, impulse, white/pink noise, transient burst) through the *complete* signal path, measuring frequency response, THD, and aliasing at each stage boundary, not just per-subsystem.
7. `ABSnapshot` (§5.9) — round-trip test: capture A, edit params, capture B, recall A restores exact original values, state survives a save/reload cycle (serialize→deserialize the wrapping XML, confirm both snapshots intact).
8. Parameter layout finalized against §5.10's locked table — confirmed IDs, ranges, defaults match exactly before any UI wiring begins (spec section 60: IDs must be stable once shipped).
9. WebView UI wiring — `SinglePageBrowser` + native-function RPC + substring `getResource` (§3.8), CSS/SVG/raster hybrid (§7), knobs/switches bound to `setParam`, VU needle bound to the 30Hz telemetry push. UI asset sourcing (§7.3) should be resolved before this step starts in earnest.
10. `CMakeLists.txt` assembled per §3.1's Nova Doubler template — `FORMATS VST3 AU Standalone`, AAX path wired/excluded, one `juce_add_console_app` per harness above.
11. Build + install to `~/Library/Audio/Plug-Ins` (per the established cmake-build-then-install workflow), AU validated via `auval`.
12. Functional acceptance walkthrough (spec section 76) and visual acceptance test against the locked mockup (spec section 77) — pixel-level comparison, Retina/@2x check, and specifically confirming **immediate knob tracking with no visible lag** (a named spec requirement — test by comparing pointer position to visual knob position during a fast drag, not just at rest).

### 8.2 Section 64 EQ-frequency-verification — explicit method
For each of the 6 HPF steps, 6 Low steps, and 6 Mid steps: generate a swept-sine or use an impulse response, take an FFT, locate the actual −3dB point (HPF) or the actual peak/shelf-midpoint gain frequency (Low/Mid), and assert against the table in §5.5. This must run at at least 44.1kHz and 96kHz (bilinear-transform-derived coefficients shift slightly with sample rate at fixed Hz targets — worth confirming the shift stays within tolerance across the spec's 44.1–192kHz range, not just at one rate).

### 8.3 Functional acceptance walkthrough (section 76) — checklist shape
- Every one of the 13 parameters in §5.10 audibly/measurably does what its label says, in isolation.
- Clean/Classic/Hot produce a clearly audible difference at matched preamp_gain (ties to §6).
- A/B recall is bit-exact (or perceptually exact) on every parameter.
- Bypass is truly transparent (no residual gain/EQ/saturation).
- Phase invert flips polarity exactly, nothing else changes.
- VU meter tracks program material the way a real VU does — noticeably slower/smoother than a peak meter, not twitchy.
- No NaN/Inf/denormal-driven CPU spikes across silence→full-level→silence transitions, all supported sample rates.

### 8.4 Visual acceptance test (section 77)
Direct pixel/layout comparison against the locked mockup at 1x and 2x (Retina), confirming wood/faceplate/knob-cap rendering reads as intended once §7.3's texture assets exist, confirming the vector layer (text, VU needle, screws) stays crisp when the window is resized/zoomed, and confirming knob drag tracking has no visible lag (§8.1 step 12).

---

## 9. Open items for human review before implementation starts

1. **UI texture assets (§7.3)** — need an image-generation pass or user-sourced photography for wood-grain/faceplate/possibly-knob-cap textures before pixel-accurate UI work can start. DSP work is not blocked by this.
2. **EQ frequency table (§5.5)** — adopted the mockup's suggested values as musically sound; §8.2's measurement pass is the real gate, and could still surface a needed correction to either the code or the mockup's printed label.
3. Confirm this is genuinely a fresh `_shared/NovaSevenCore/` (no existing plugin named similarly) — no naming collision found during this research pass.
