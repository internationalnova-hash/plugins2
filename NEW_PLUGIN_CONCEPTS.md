# New Plugin Concepts — Working Doc

*Discussion draft, not a spec. Not wired into any build system or the storefront.*

## Catalog map (as of this pass)

Reviewed all ~30 top-level dirs (skipping `_shared/`, `NovaDSP/`, `PT Converter`, `Nova Writer` [AI songwriting desktop app, not a plugin], `Nova Studio` [DAW app], `Nova Bass Lab` [internal preset-authoring tool for Nova Bass], `Nova AI Master Standalone` [preview harness]) plus `nova-store/data/plugins.ts` for the commercial category taxonomy.

- **Dynamics (4):** Nova Master (mix-bus glue/finishing), Nova One (4-dimension finishing macro), Nova Apex (Clean/Punch/Smooth/Loud transient-and-level macro), Nova Level (auto-leveling/riding). — *crowded, and all four are deliberately macro-driven, not exposed-knob compressors.*
- **Distortion (3):** Nova Sub (808/bass saturation, 4 circuit modes), Nova Heat (harmonic drive, 3 characters), Nova Clip (clipping/loudness). — *crowded.*
- **EQ/Filter (4):** Nova Voice (vocal tone modes + Morph), Nova Curve (filter/EQ), Nova Tone (tonal EQ), Nova Silk (resonance suppression). — *crowded.*
- **Instrument (3):** Nova Chord (self-playing diatonic chord engine + MIDI export), Nova Keys (4-layer sampler/synth hybrid), Nova Bass (Sound Vault-driven bass instrument, sampler+synth hybrid). — *all three are melodic/harmonic. No rhythm-section instrument exists at all.*
- **Modulation (2):** Nova Motion FX (one macro morphs filter→delay→reverb), Nova Doubler (2-4 voice vocal doubler). Also `Juice Gang` (unreleased prototype, same filter+delay+reverb+LFO territory as Motion FX, just full-knob instead of macro).
- **Spatial/Reverb (2):** Nova Aura (width/depth), Nova Space (reverb).
- **Pitch (2):** Nova Harmony (harmony generation), Nova Autotune (unreleased correction engine — Nova Pitch is dead, don't touch this lane).
- **Delay (1):** Nova Delay.
- **Restoration (1):** Nova Clean (spectral denoise + click/pop repair).
- **Channel Strip (2):** Nova Console, Nova Vox (5-macro vocal strip built from other plugins' engines).
- **Mix Tools (2):** Nova Translate (auditions the mix through 12 playback environments), Nova AI Master (analyzes the mix, builds and applies a mastering chain automatically). — *both are "smart," decision-making tools. Neither is a plain, transparent meter/analyzer — there is no pure monitoring utility anywhere in the catalog, despite the analysis DSP for one already existing in `_shared/NovaAIMasterCore/Analysis/`.*

House style, confirmed from source comments (Nova AI Master, Nova Bass, Nova Keys, Nova Doubler headers): a prescriptive per-plugin spec, strict "v0.1 then stop" scope discipline, a shared engine class driven identically by the plugin and any internal tooling, honest inline notes about deliberate v0.1 simplifications, WebView UI with a per-plugin dark/colored theme (`Design/v1-style-guide.md`), and per-plugin `_shared/NovaXCore/` engine modules reused across the plugin + its variants, sitting alongside a genuinely generic `NovaDSP/` toolkit (Dynamics, EQ, Saturation, Spatial, Modulation, Console).

## Proposals

### 1. Nova Scope — mix-bus analysis & metering utility
**Pitch:** A zero-processing, always-transparent metering suite — integrated loudness (LUFS-S/I), true-peak, a real-time spectrum/spectrogram, and a phase-correlation goniometer — for mixing/mastering engineers who currently have to run three different free plugins to see what Nova AI Master already computes invisibly for its own decisions. Sits anywhere in the chain, changes nothing, just tells the truth about the signal.

**Why it's a gap:** Mix Tools currently has two "smart" plugins (Translate simulates playback environments, AI Master automates a mastering chain) but nothing that just shows the engineer the numbers. Every other plugin in the catalog processes; this one only observes. It's also the natural companion to Translate and Master in the Nova Analog Bundle workflow — the tool you'd open right before committing to either.

**DSP concept:** No original DSP work needed for v0.1 — this is a UI/aggregation product wrapping four analyzer engines that already exist and are already validated in production use: `NovaLoudnessAnalyzer`, `NovaSpectralAnalyzer`, `NovaStereoAnalyzer`, and `NovaDynamicsAnalyzer` from `_shared/NovaAIMasterCore/Analysis/`. The only new work is a shared read-only wrapper, a lock-free snapshot handoff to the UI thread (same SpinLock-guarded-POD pattern Nova AI Master already uses for its `NovaMasteringPlan`), and the WebView visualization layer.

**Shared infra reuse:** Directly reuses `_shared/NovaAIMasterCore/Analysis/*` wholesale — this is close to the cheapest possible v0.1 in the whole catalog, since the hard DSP is done and validated.

### 2. Nova Drum — trap/hip-hop drum & 808 instrument
**Pitch:** A MIDI drum-and-sub instrument built for trap/hip-hop production — kick, snare/clap, hats/rolls, and a glide-capable 808 sub-bass voice, each with its own character controls, laid out as a compact kit rather than a full multi-zone sampler. The instrument the producer reaches for to *build* the 808 pattern that Nova Sub then colors and Nova Master then glues.

**Why it's a gap:** Instrument has three entries and all three are melodic/harmonic (Chord generates progressions, Keys is a general sampler/synth hybrid, Bass is a melodic bass voice). None of them is rhythm-section-shaped, and nothing in the catalog *generates* drum or 808 sound — Nova Sub only processes 808s that already exist. Given the catalog already leans hard into trap/bass production (Sub, Bass, the "Neon/Galaxy" saturation naming), a drum companion is the obvious missing piece of that story, not a new direction.

**DSP concept:** Kick/snare/hats as short synthesized one-shots (sine-sweep kick with pitch-envelope punch, noise-burst snare/hat through a resonant filter) rather than sample playback, keeping the plugin small and royalty-free; the 808 voice reuses a pitch-glide sub oscillator conceptually identical to what Nova Bass already ships. Per-voice saturation stage borrows `NovaDSP/Saturation` directly instead of re-deriving Nova Sub's four circuit modes.

**Shared infra reuse:** `_shared/NovaBassCore/PitchEnvelope/NovaPitchEnvelope.h` and `.../Synth/NovaBassOscillator.h` are close to exactly the glide/sub-oscillator behavior an 808 voice needs. Kit/pad-layer management can borrow structurally from `_shared/NovaKeysCore/Layer/` and `Sampler/` even if v0.1 ships synthesized-only. `NovaDSP/Saturation` covers per-voice color without inventing a fifth saturation engine.

### 3. Nova Grain — granular texture & glitch effect
**Pitch:** A creative sound-design effect for reversed swells, stutter/glitch rolls, pitched grain clouds, and time-frozen textures — the "hit the FREEZE button and it turns into an ambient pad" tool for transitions, risers, and vocal chops. One character knob (Grain Size/Density macro) plus TEXTURE, PITCH, and FREEZE, in the same "one big idea, few knobs" spirit as Motion FX.

**Why it's a gap:** Modulation/creative-FX currently means Motion FX (continuous macro-morph between standard filter/delay/reverb — never touches grain structure) and Doubler (subtle, not extreme, doubling). Nothing granular or glitch-based exists anywhere in the catalog, and it's one of the most requested categories in modern trap/EDM/cinematic production that a catalog this size is visibly missing.

**DSP concept:** A circular buffer feeding overlapping, windowed grains (variable size/overlap/pitch-per-grain) with a FREEZE mode that stops writing and loops the captured buffer indefinitely; TEXTURE control blends grain-cloud density from tight/glitchy to smooth/pad-like. The grain pitch-shift path can reuse the same micro-shift-line technique Doubler already validated rather than writing a new pitch engine from scratch.

**Shared infra reuse:** `_shared/NovaDoublerCore/PitchShift/NovaMicroShiftLine.h` for per-grain pitch shifting, `NovaDSP/Modulation` (`NovaMotionDSP`) for the LFO/macro-morph layer, and `NovaDSP/Spatial` (`NovaSpaceDSP`) if a built-in send-verb tail is wanted on frozen textures.

## Recommendation

If only one gets built: **Nova Scope.** It's the cheapest possible v0.1 in this list (wraps already-validated, already-shared analysis DSP instead of writing new DSP), fills a category that is currently empty by definition rather than merely thin, and it makes every other plugin in the catalog — especially Master, AI Master, and Translate — more credible by giving the engineer a way to verify what they did.
