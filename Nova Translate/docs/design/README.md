# Nova Translate — Environment Design Documents

## Philosophy

**Nova Translate does not try to physically recreate another playback system — it tries to recreate the mix decisions that playback system causes an engineer to make.** Every section below — the physics modeling, the Perceptual Targets, the Confidence Ratings, the Decision Fidelity protocol — exists in service of that one sentence. See [`00-methodology.md`](00-methodology.md) for the full statement.

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| 1 | Plugin architecture | ✅ Done |
| 2 | UI and framework | ✅ Done |
| 3 | Acoustic modeling documentation | ✅ Done |
| 4 | Reference Validation (initial pass, all 4 environments) | ✅ Done |
| 5 | Environment library expansion | ← **Current phase** |
| 6 | Beta testing with mix engineers | Not started |
| 7 | Commercial release | Not started |

All 4 original environments passed an initial informal Decision Fidelity listening test on 2026-07-30 — the gate for expansion set in the Phase 4 kickoff has been met. Luxury SUV was built first on 2026-08-07 following the one-at-a-time discipline (design target already existed in `luxury-suv-benchmark.md`). The user then explicitly asked to finish the remaining 7 environments in one pass rather than one at a time, so **Nearfield Studio, Mastering Suite, Bedroom Studio, Laptop, Bluetooth Speaker, Boombox, and Club PA** were all implemented together on 2026-08-08 — a deliberate, explicit exception to the one-at-a-time rule, not a quiet abandonment of it. All 12 environments in the original roster are now implemented and automated-test-clean; none of the 8 built since Reference Validation (Luxury SUV + the 7 latest) have been through a listening check yet.

## Methodology

Every environment's DSP profile must trace back to a real physical mechanism, not an arbitrary EQ curve. Each environment is modeled as two independent, explicitly-separated layers:

```
Acoustic Model (the space)  →  Playback System Model (the speakers)  →  DSP
```

A cabin's reflections exist independent of what's playing through it; a speaker's frequency limits and distortion exist independent of what room it's in. Every DSP Translation row in every doc is tagged **[Acoustic]**, **[Playback System]**, or **[Both]** so it's always traceable which real-world layer justifies which number. Each doc also has a **Perceptual Targets** section (what should the engineer actually notice — the prioritized short-list, not every physical phenomenon modeled) and a **Confidence Rating** (design-reasoning confidence, not a listening-test result). See [`00-methodology.md`](00-methodology.md) for the shared physics toolkit (room-mode formulas, Sabine RT60, absorption coefficients, driver/crossover behavior) and the full document structure.

**Process**: a design doc for an environment must exist and be reviewed before its `EnvironmentProfile` DSP values change. The architecture (`EnvironmentProfile` struct shape, position system, engine signal flow) is frozen separately — these docs drive retuning *numbers within* the existing structure, not structural changes.

## Current milestone: Phase 5 — Environment Library Expansion

Phase 4's gate has been cleared: all 4 existing environments were checked in an informal listening session on 2026-07-30 against their **Perceptual Targets**, and all 4 passed by the user's own ear (Sedan: "sounds real good"; Nova Main Room: "immaculate"; Smartphone: "sounds legit like a phone"; Mono Cube: "sounds like what it should"). This was a single informal pass, not yet the full fixed-material protocol in [`reference-validation-protocol.md`](reference-validation-protocol.md) — that protocol remains available for a deeper, repeatable session later, especially if any environment is revisited for the gaps logged below.

Expansion proceeds one environment at a time, in the same order of operations used to get the first four this far: design doc → implement → validate (automated tests, `auval`, install) → listening check → move on. Luxury SUV was built this way on 2026-08-07 (72/72 automated tests passing, `auval` clean, installed). The remaining 7 were then built together on 2026-08-08 per explicit user request — same validation bar (152/152 automated tests passing, `auval` clean, installed), but no individual per-environment design docs were written for this batch, and none have had a listening check yet. Full design docs (Acoustic/Playback System split, Perceptual Targets, physics grounding) can still be written for any of the 7 before they're revised, per the frozen process — they just weren't a precondition for this initial build-out, since the user asked to move directly to testing.

## Documents

| Environment | Status | Acoustic | Playback System | Overall | Decision Fidelity | Doc |
|---|---|---|---|---|---|---|
| Nova Main Room | Implemented — mostly already physically justified | 8.0/10 | 8.5/10 | 8.3/10 | **PASS** (2026-07-30) | [`nova-main-room.md`](nova-main-room.md) |
| Nearfield Studio | Implemented (2026-08-08) — semi-treated project studio, real nearfield monitors, no separate design doc yet | — | — | — | Not yet measured | *(none yet)* |
| Mastering Suite | Implemented (2026-08-08) — the most controlled/linear profile in the plugin by design, no separate design doc yet | — | — | — | Not yet measured | *(none yet)* |
| Bedroom Studio | Implemented (2026-08-08) — untreated-room worst-case check, boosted low shelf models real room-mode buildup, no separate design doc yet | — | — | — | Not yet measured | *(none yet)* |
| Sedan | Implemented — **clearest gap found**: low-end shelf should be a resonant peak at ~82Hz, not a broad shelf at 150Hz | 6.0/10 | 6.5/10 | 6.3/10 | **PASS** (2026-07-30) | [`sedan.md`](sedan.md) |
| Luxury SUV | Implemented (2026-08-07) — inherits Sedan's shelf-vs-peak gap at a different frequency; otherwise the cleanest "Both layers differ independently" case in the plugin | 6.5/10 | 7.0/10 | 6.8/10 | Not yet measured | [`luxury-suv-benchmark.md`](luxury-suv-benchmark.md) |
| Smartphone | Implemented — mostly already physically justified | 9.0/10 | 7.5/10 | 8.2/10 | **PASS** (2026-07-30) | [`smartphone.md`](smartphone.md) |
| Laptop | Implemented (2026-08-08) — narrower version of Smartphone's constraints with real (if narrow) stereo width, no separate design doc yet | — | — | — | Not yet measured | *(none yet)* |
| Bluetooth Speaker | Implemented (2026-08-08) — first profile with a boosted low shelf + negative mid peak (V-shaped consumer tuning), no separate design doc yet | — | — | — | Not yet measured | *(none yet)* |
| Boombox | Implemented (2026-08-08) — wider stereo and honky (not scooped) midrange vs. Bluetooth Speaker, ported-cabinet bass character, no separate design doc yet | — | — | — | Not yet measured | *(none yet)* |
| Club PA | Implemented (2026-08-08) — deepest bass extension and heaviest compression of any profile, new VENUES category, distance/coverage-based positions rather than seating, no separate design doc yet | — | — | — | Not yet measured | *(none yet)* |
| Mono Cube | Implemented — possible gap: midrange coloration may be under-tuned relative to the real reference-monitor class it represents | 8.5/10 | 6.0/10 | 7.3/10 | **PASS** (2026-07-30) | [`mono-cube.md`](mono-cube.md) |

Confidence scores are self-assessed design-reasoning confidence (how well the current code matches the physics laid out in each doc) — **not** perceptual-validation scores; the 7 newest environments don't have one yet because they don't have a design doc yet, not because the DSP values are arbitrary (the reasoning behind each is documented inline as comments in `EnvironmentProfiles.hpp`). All 4 original environments have a passing Decision Fidelity result, including Sedan, the lowest design-confidence environment — direct evidence the two metrics are genuinely separate axes, per the philosophy statement above. The known gaps below remain real and worth fixing but are not currently blocking anything.

## Gaps identified across all docs (candidates for future, explicitly-approved DSP work)

1. **[Acoustic] Sedan's low-shelf → resonant peak** — the single clearest "arbitrary EQ vs. models something real" gap in the plugin. Fix is Acoustic-layer only; nothing about the door speakers should change.
2. **[Acoustic] Luxury SUV's low-shelf → resonant peak** — the same gap as Sedan's, at ~55Hz instead of ~82Hz, since the SUV profile reuses the same shelf mechanism until the underlying fix lands (see `sedan.md`'s note that the fix should apply to both once made).
3. **[Acoustic] Bedroom Studio and Club PA also use the shelf-not-peak mechanism** for their own room-mode/sub-resonance bumps (70Hz and 60Hz respectively) — same class of gap as Sedan/Luxury SUV, now present in 4 of the 12 profiles.
4. **[Playback System] Off-axis high-frequency rolloff** (Nova Main Room's Engineer Left/Right, Sedan's Rear seats) — real speaker/driver directivity narrows at high frequencies off-axis; the position system currently only applies lateral bias + generic diffuse treatment, not frequency-dependent loss. Purely a speaker-directivity gap, not a room one.
5. **[Acoustic] `roomAmount`/IR decay not derived from Sabine RT60** — currently a 0–1 blend knob rather than a value traceable to an actual reverberation-time estimate for each space's real volume/materials.
6. **[Playback System] Mono Cube's midrange coloration may be under-tuned** relative to the real Auratone-class monitor it represents.
7. **No design docs yet for the 7 newest environments** — the physical reasoning exists (inline comments in `EnvironmentProfiles.hpp`), but hasn't been written up in the full Acoustic/Playback System Model format the first 5 environments have. Worth doing before any of the 7 get their first DSP revision.

None of the DSP values above are being changed yet — per the "One Failure = One Fix" workflow, DSP changes wait for a failed listening test, not theory alone. They're logged here as ear-testing priorities for when the user runs Reference Validation listening sessions.
