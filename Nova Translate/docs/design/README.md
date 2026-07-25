# Nova Translate — Environment Design Documents

Phase 3 methodology: every environment's DSP profile must trace back to a real physical mechanism, not an arbitrary EQ curve. Each environment is modeled as two independent, explicitly-separated layers:

```
Acoustic Model (the space)  →  Playback System Model (the speakers)  →  DSP
```

A cabin's reflections exist independent of what's playing through it; a speaker's frequency limits and distortion exist independent of what room it's in. Every DSP Translation row in every doc is tagged **[Acoustic]**, **[Playback System]**, or **[Both]** so it's always traceable which real-world layer justifies which number. Each doc also has a **Perceptual Targets** section (what should the engineer actually notice — the prioritized short-list, not every physical phenomenon modeled) and a **Confidence Rating** (design-reasoning confidence, not a listening-test result). See [`00-methodology.md`](00-methodology.md) for the shared physics toolkit (room-mode formulas, Sabine RT60, absorption coefficients, driver/crossover behavior) and the full document structure.

**Process**: a design doc for an environment must exist and be reviewed before its `EnvironmentProfile` DSP values change. The architecture (`EnvironmentProfile` struct shape, position system, engine signal flow) is frozen separately — these docs drive retuning *numbers within* the existing structure, not structural changes.

## Current milestone: Phase 3 — Reference Validation

**Not** "add another environment." The goal is: make the 4 existing environments convincing enough that, in blind listening, an engineer consistently recognizes the playback scenario and makes the same corrective mix decisions they'd make on the real system. Each environment's **Perceptual Targets** section is the test's success criteria. This is a human listening-test result — nothing in this codebase can produce it; the docs exist to make that test well-defined and easy to score once it happens. Once the 4 environments pass, expanding to Luxury SUV, headphones, Bluetooth speakers, etc. extends a validated framework rather than growing an unfinished one.

## Documents

| Environment | Status | Acoustic | Playback System | Overall | Doc |
|---|---|---|---|---|---|
| Nova Main Room | Implemented — mostly already physically justified | 8.0/10 | 8.5/10 | 8.3/10 | [`nova-main-room.md`](nova-main-room.md) |
| Sedan | Implemented — **clearest gap found**: low-end shelf should be a resonant peak at ~82Hz, not a broad shelf at 150Hz | 6.0/10 | 6.5/10 | 6.3/10 | [`sedan.md`](sedan.md) |
| Smartphone | Implemented — mostly already physically justified | 9.0/10 | 7.5/10 | 8.2/10 | [`smartphone.md`](smartphone.md) |
| Mono Cube | Implemented — possible gap: midrange coloration may be under-tuned relative to the real reference-monitor class it represents | 8.5/10 | 6.0/10 | 7.3/10 | [`mono-cube.md`](mono-cube.md) |
| Luxury SUV | **Not implemented** — benchmark spec only, not built yet | Design Stage | Design Stage | N/A | [`luxury-suv-benchmark.md`](luxury-suv-benchmark.md) |

Confidence scores are self-assessed design-reasoning confidence (how well the current code matches the physics laid out in each doc) — **not** perceptual-validation scores. Sedan is the lowest-confidence, highest-priority environment for Reference Validation to start with.

## Gaps identified across all docs (candidates for future, explicitly-approved DSP work)

1. **[Acoustic] Sedan's low-shelf → resonant peak** — the single clearest "arbitrary EQ vs. models something real" gap in the plugin. Fix is Acoustic-layer only; nothing about the door speakers should change.
2. **[Playback System] Off-axis high-frequency rolloff** (Nova Main Room's Engineer Left/Right, Sedan's Rear seats) — real speaker/driver directivity narrows at high frequencies off-axis; the position system currently only applies lateral bias + generic diffuse treatment, not frequency-dependent loss. Purely a speaker-directivity gap, not a room one.
3. **[Acoustic] `roomAmount`/IR decay not derived from Sabine RT60** — currently a 0–1 blend knob rather than a value traceable to an actual reverberation-time estimate for each space's real volume/materials.
4. **[Playback System] Mono Cube's midrange coloration may be under-tuned** relative to the real Auratone-class monitor it represents.

None of these are being changed yet — they're logged here as ear-testing priorities for when the user runs Reference Validation listening tests.
