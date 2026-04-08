# Nova Tone User Manual

**Current V1 Build**  
**Musical Tone Shaping Equalizer**

---

## Introduction

`Nova Tone` is a premium musical EQ designed for broad, confident shaping rather than surgical correction. It is especially good for adding weight, clarity, air, and gentle polish.

The plugin uses a Pultec-style workflow: wide tonal moves, frequency selectors, and a smooth output stage that stays musical even when pushed.

---

## Quick Start

1. Start in `NEUTRAL`
2. Choose the **low frequency** you want to shape
3. Add **LOW BOOST** for weight or **LOW ATTENUATION** for control
4. Choose a **high boost frequency** and add presence or air
5. Adjust **BANDWIDTH** to make the top-end boost broader or more focused
6. Use **HIGH ATTENUATION** to smooth harshness
7. Match loudness with **OUTPUT GAIN**

---

## Interface Overview

### Low Section
- `LOW FREQ`
- `LOW BOOST`
- `LOW ATTENUATION`

### High Section
- `HIGH BOOST FREQ`
- `HIGH BOOST`
- `BANDWIDTH`
- `HIGH ATTENUATION FREQ`
- `HIGH ATTENUATION`

### Output / Workflow
- `OUTPUT GAIN`
- Preset buttons: `NEUTRAL`, `VOCAL`, `BASS`, `AIR`, `MASTER`

---

## Control Reference

| Control | What you hear | What it changes in the DSP |
|---|---|---|
| `LOW FREQ` | Chooses which part of the low end gets shaped | Selects the turnover frequency for the low shelf network: `20`, `30`, `60`, or `100 Hz` |
| `LOW BOOST` | Adds weight, warmth, and body | Increases a **broad low-shelf boost** at the selected low frequency |
| `LOW ATTENUATION` | Tightens mud or bloom without making the signal feel thin | Applies a **broad low-shelf cut** slightly above the selected low frequency |
| `HIGH BOOST FREQ` | Chooses where the top-end enhancement lives | Selects the center frequency for the high boost: `3k` to `16k` |
| `HIGH BOOST` | Adds presence, bite, sheen, or air | Increases a **peak filter boost** at the selected high frequency |
| `BANDWIDTH` | Changes how broad or focused the high boost feels | Adjusts the **Q** of the high-boost filter; higher settings make the boost more focused |
| `HIGH ATTENUATION FREQ` | Chooses where top-end softening begins | Selects the frequency for the high attenuation shelf: `5k`, `10k`, or `20k` |
| `HIGH ATTENUATION` | Smooths harshness and glare | Applies a **high-shelf cut** above the selected attenuation frequency |
| `OUTPUT GAIN` | Matches level after EQ moves | Applies final output trim from `-10 dB` to `+10 dB` |

---

## Preset Buttons

The preset buttons are **recall buttons**, not hidden DSP modes.

When you click a preset, it **sets the actual controls** to a useful musical starting point:

- `NEUTRAL` — flat starting point
- `VOCAL` — presence and air without harshness
- `BASS` — classic low-end trick for weight + control
- `AIR` — open, expensive top-end lift
- `MASTER` — broad overall polish

> Important: these buttons do **not** add secret processing behind the scenes. They simply move the EQ settings to preset values.

---

## What Happens Under the Hood

`Nova Tone` combines four main filter stages:

1. **Low shelf boost**
2. **Low shelf attenuation**
3. **High peak boost**
4. **High shelf attenuation**

It also adds a **small amount of soft analog-style saturation** after the EQ stage. That means bigger tone moves can sound a little richer and smoother instead of brittle.

---

## Practical Tips

### For vocals
- Use `10k` or `12k` on the high boost side
- Add a little `HIGH BOOST`
- Use a touch of `HIGH ATTENUATION` if sibilance builds up

### For bass
- Set `LOW FREQ` to `60`
- Use **both** `LOW BOOST` and `LOW ATTENUATION`
- This gives the classic “bigger but tighter” low-end feel

### For mix bus polish
- Keep moves small
- Use `MASTER` as a starting point
- Match output carefully with `OUTPUT GAIN`

---

## Summary

`Nova Tone` is for broad, expensive-sounding EQ moves:
- **Low section** = weight and control
- **High section** = clarity and air
- **Preset buttons** = fast starting points
- **Output Gain** = level match after shaping

