# Nova Heat User Manual

**Current V1 Build**  
**Harmonic Saturation & Energy Processor**

---

## Introduction

`Nova Heat` is a premium saturation plugin built to add **warmth, density, bite, glue, and excitement** without turning into harsh distortion. It is designed to feel fast and musical: a few controls, a lot of result.

---

## Quick Start

1. Start in `MEDIUM`
2. Raise `DRIVE` until the source wakes up
3. Use `HEAT` to decide how intense and harmonically rich the saturation feels
4. Shape the tone with `TONE`
5. Blend with `MIX`
6. Level-match with `OUTPUT`
7. Turn on `MAGIC` for extra polish if needed

---

## Main Controls

| Control | What you hear | What it changes in the DSP |
|---|---|---|
| `DRIVE` | More push, density, edge, and forwardness | Increases the signal hitting the saturator. In the backend it raises the **input drive amount** and dynamic push into the nonlinear stage |
| `TONE` | Left = warmer/darker, Right = brighter/more present | Applies a **tilt-style tonal balance** using low and high shelves before/after saturation, and changes the focus region around the upper mids |
| `HEAT` | More harmonic density, thickness, and intensity | Increases the **curve amount**, transient softening, asymmetry, and saturation blend so the harmonics feel stronger rather than simply louder |
| `MIX` | Blends clean and processed signal | Controls the final **dry/wet mix** |
| `OUTPUT` | Matches loudness after saturation | Applies final output trim from `-12 dB` to `+12 dB` |

---

## Mode Buttons

`SOFT`, `MEDIUM`, and `HARD` are real DSP modes.

| Mode | Feel | What changes in the DSP |
|---|---|---|
| `SOFT` | Smooth, warm, forgiving | Lower drive multiplier, lower heat intensity, gentler transient softening, smaller stereo spread |
| `MEDIUM` | Balanced, polished, versatile | Middle-ground settings for density, focus, smoothing, and character |
| `HARD` | Bolder, more aggressive, more obvious | Higher drive multiplier, stronger curve shaping, more transient control, more asymmetry, more width and intensity |

These buttons do **not** just change the label — they change the actual saturation profile in the backend.

---

## MAGIC Button

`MAGIC` is a dedicated **finishing layer**, not a hidden preset.

When `MAGIC` is ON, the DSP adds:
- extra upper-mid focus
- smoother top-end control
- a bit more dynamic sensitivity
- slightly richer saturation curve behavior
- a touch more stereo spread and polish

### Important note
`MAGIC` changes the **backend processing**, but it does **not** automatically move the main knobs by itself.

---

## Preset Buttons

The preset buttons recall complete starting points for common sources:

- `VOCAL`
- `DRUMS`
- `BASS`
- `MASTER`

When you click a preset, it changes the actual controls and may also turn `MAGIC` on or off depending on the preset.

---

## What Happens Under the Hood

`Nova Heat` combines several processes at once:

1. **Drive stage** to push signal into saturation
2. **Tone shelves** to shift warmth vs presence
3. **Dynamic harmonic shaping** so louder moments react differently
4. **Top smoothing and focus filters** to keep the result polished
5. **Stereo width enhancement** on stereo material
6. **Dry/wet blending** for parallel saturation

That is why `Nova Heat` feels more like a finished tone tool than a simple distortion box.

---

## Practical Tips

### Vocals
- Use `MEDIUM`
- Keep `DRIVE` moderate
- Use `MAGIC` for polish and forwardness

### Drums
- Try `HARD`
- Push `HEAT` more than `DRIVE` if you want size without too much fuzz

### Bass
- Keep `TONE` a little lower for weight
- Blend with `MIX` instead of going fully wet

### Master bus
- Use `SOFT`
- Keep settings low
- Think “glue and glow,” not obvious distortion

---

## Summary

`Nova Heat` gives you:
- `DRIVE` = how hard you hit the saturator
- `HEAT` = how rich and intense the harmonics become
- `TONE` = brightness vs warmth balance
- `MIX` = parallel blend
- `MAGIC` = extra polish layer
- `SOFT / MEDIUM / HARD` = real backend character changes

