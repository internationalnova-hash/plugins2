# Nova Translate — Reference Validation Protocol

Purpose: turn "does the Sedan sound good?" into a repeatable process with the same test material, the same questions, and a comparable result every time a DSP change is made. Every future DSP change to `EnvironmentProfiles.hpp` should be re-validated against this same protocol, not a fresh ad-hoc listen — otherwise "it sounds better to me now" has no baseline to be better *than*.

This document defines the protocol. Running it is a human listening-test activity this session cannot perform (see `00-methodology.md`'s Reference Validation section) — it needs a real listener, real playback systems, and real ears.

## Test material

Four tracks, one per genre, used identically for every environment and every future re-test. Consistency of material matters more than the specific songs chosen — **track selection is the user's call**, not something to be prescribed here: it should be material the user already knows intimately across real playback systems (car, phone, studio monitors, etc.), because that existing familiarity is the ground truth this whole protocol is calibrating Nova Translate against. Once chosen, the same four tracks should be used for every environment and every future re-test — swapping test material invalidates comparison to prior sessions.

| Slot | Genre | Primary listen-for targets |
|---|---|---|
| Song 1 | Modern Pop | Vocal, air, stereo width |
| Song 2 | Hip-Hop | Kick, 808/sub-bass, vocal |
| Song 3 | Rock | Guitars, snare, imaging |
| Song 4 | EDM | Sub-bass, width, brightness |

### Cross-reference: which songs stress-test which environment's Perceptual Targets

Not every song is equally diagnostic for every environment. This matrix connects the test material to each environment's own Perceptual Targets doc, so a session can prioritize the most revealing song per environment rather than treating all four as equally useful everywhere.

| Environment | Most diagnostic song(s) | Why |
|---|---|---|
| Nova Main Room | All four, equally | It's the calibration anchor — every song should sound trustworthy and uncolored here, so none is more diagnostic than another. |
| Sedan | Song 2 (Hip-Hop) and Song 4 (EDM) | Both have prominent sub/808 content that will interact directly with the cabin's ~82Hz modal resonance (Sedan's Perceptual Target #2). Song 3 (Rock) best exercises the crossover-region harshness target (#4) via guitars/snare. |
| Smartphone | Song 1 (Modern Pop) and Song 2 (Hip-Hop) | Vocal-forward and hook-forward material directly tests targets #1/#2 (vocal level, hook clarity); Song 4 (EDM) best exposes target #5 (loss of sub-bass, since EDM leans on sub content Smartphone cannot reproduce at all). |
| Mono Cube | Song 1 (Modern Pop) and Song 3 (Rock) | Both have dense vocal/instrument midrange content that tests masking (target #2) and separation (target #4); any song with hard-panned elements tests mono compatibility (target #3). |
| Luxury SUV (once built) | Song 4 (EDM) and Song 2 (Hip-Hop) | Sub-bass extension (target #1) and wide imaging (target #2) are best exposed by bass-and-width-forward material. |

## Session procedure

1. **Fixed settings**: for each environment under test, use its default position and 100% Environment Amount unless the specific test is about the Position control itself — varying multiple things at once makes a failed test ambiguous about which control caused it.
2. **Play each of the 4 songs** through the environment being tested. For each song, the listener notes their *instinctive* mix reaction — the corrective move they'd instinctively reach for, not a considered technical analysis.
3. **Record against that environment's Decision Fidelity questions** (below) — yes/no or directional answers, not prose, so results are comparable across sessions.
4. **Where possible, A/B against the real system** the environment models (an actual sedan, an actual phone) using the same 4 tracks, and compare the two decision sets. This is the actual ground-truth comparison the whole protocol exists to produce; a session without it is still useful (internal consistency, does the environment produce *a* strong reaction) but can't measure Decision Fidelity against reality, only against the engineer's own prior expectation.
5. **Log results** using the template at the bottom of this doc, dated, so repeat sessions after a DSP change can be compared to the prior baseline rather than relying on memory of "it used to sound different."

## Decision Fidelity

Confidence Ratings (in each environment doc) grade whether the DSP implementation matches the *documented physics*. Decision Fidelity grades something different and, per the philosophy statement, more important: **does the environment cause the same corrective mix decision a real playback system would cause.** A physically-imperfect model with high Decision Fidelity has succeeded at Nova Translate's actual purpose; a physically-elaborate model with low Decision Fidelity has not, regardless of how well-reasoned its `EnvironmentProfiles.hpp` numbers are.

**Scoring**: for each environment, ask the fixed question set below after listening to all 4 songs. Score = the fraction of questions where the engineer's instinctive answer matches what they would do (or have done) on the real system. This is **not yet measured for any environment** — no listening sessions have happened. Each environment doc's Confidence Rating table has been extended with a Decision Fidelity row marked "not yet measured," to be filled in once a real session runs.

### Nova Main Room — Decision Fidelity questions

- Do you trust the tonal balance as printed, with no corrective urge?
- Does the stereo image feel stable and centered?
- Is there any single frequency region you feel pulled to touch? (success = no)

### Sedan — Decision Fidelity questions

- Would you lower the kick?
- Does the bass feel like it's building up around one specific frequency, not just "more bass"?
- Would you push the vocal up to cut through?
- Would you tame something in the upper mids (~2.5–3.5kHz)?
- Does the stereo image feel like it's collapsing toward one side?

### Smartphone — Decision Fidelity questions

- Would you raise the vocal level?
- Is the hook/melody still clearly identifiable?
- Do other midrange elements feel like they're crowding the vocal out?
- Do transients (snare, percussion) still read clearly despite the coloration?
- Does the low end feel genuinely absent, not just quieter?

### Mono Cube — Decision Fidelity questions

- Does the vocal/instrument balance feel accurate, or does something feel masked?
- Does anything noticeably change or disappear when you mentally check mono compatibility?
- Can you still distinguish individual instruments, or does it feel congested?

### Luxury SUV (once built) — Decision Fidelity questions

- Does the low end feel genuinely extended, not just present?
- Does the stereo image feel wide and precise, not just "bigger"?
- Does the vocal sit cleanly without fighting any harsh artifact?
- Does the overall impression read as "premium system," distinct from Sedan?

## Session log template

Copy this table per session; keep every session's row rather than overwriting, so trends across DSP changes are visible.

| Date | Environment | Song(s) used | Decision Fidelity score | Notes | DSP changed since last session? |
|---|---|---|---|---|---|
| | | | _/N | | |
