# Nova Master User Manual

**Current V1 Build**  
**Final Mix Processor**

---

## Introduction

`Nova Master` is a finishing/mastering plugin focused on speed, polish, and reliable decision-making.  
Its interface is compact by design: quick tonal shaping, controlled loudness behavior, and finish-ready output feedback.

---

## Current UI Overview

- Top bar: `Presets` dropdown + preset previous/next buttons
- Main shaping controls: `TONE`, `GLUE`, `WEIGHT`, `AIR`, `WIDTH`
- Utility controls: `MIX`, `OUTPUT GAIN`, `FINISH`
- Analysis/Metering: waveform, loudness, dynamic range, stereo image, output meter
- Mode row: `MASTER BUS`, `MUSIC`, `STREAMING`, `BROADCAST`, `REFERENCE`

---

## Parameter Ranges

| Parameter | Range | Notes |
|---|---|---|
| `TONE` | `0.0` to `10.0` | Broad dark-to-bright tilt behavior |
| `GLUE` | `0.0` to `10.0` | Cohesion/density control |
| `WEIGHT` | `0.0` to `10.0` | Low-end body/foundation |
| `AIR` | `0.0` to `10.0` | High-end openness/polish |
| `WIDTH` | `0.0` to `10.0` | Stereo spread and side-energy behavior |
| `MIX` | `0` to `100%` | Wet/dry blend of finishing behavior |
| `OUTPUT GAIN` | `-12 dB` to `+12 dB` | Final output trim |

---

## Main Controls (What You Hear)

| Control | What you hear | Behavioral impact |
|---|---|---|
| `TONE` | Darker left, brighter right | Rebalances overall tonal contour |
| `GLUE` | More cohesion and compactness | Tightens dynamic behavior and mix-bus feel |
| `WEIGHT` | More low-end mass | Increases perceived body/foundation |
| `AIR` | More top-end openness | Adds high-end clarity/sheen behavior |
| `WIDTH` | More stereo spread | Expands side field and decorrelation risk zone at high values |
| `MIX` | More/less processed feel | Globally blends finishing behavior against dry feel |
| `OUTPUT GAIN` | Level match | Trims final output stage |

---

## Mode Buttons

`MASTER BUS`, `MUSIC`, `STREAMING`, `BROADCAST`, and `REFERENCE` are active voicing modes.

| Mode | Typical use |
|---|---|
| `MASTER BUS` | Balanced default finishing on a stereo bus |
| `MUSIC` | Polished musical enhancement |
| `STREAMING` | Streaming-forward loudness/clarity behavior |
| `BROADCAST` | Tighter, controlled, stable dynamics |
| `REFERENCE` | More restrained/transparent finishing |

---

## Preset Browser

The top-right `Presets` control opens a compact preset browser with search and categories:

- `CORE`
- `GENRE`
- `FIXES`
- `NOVA`

Behavior:

- Click preset row to apply all mapped parameters + mode
- Active row is gold highlighted
- Hover row uses subtle purple/gold glow
- Previous/next arrows cycle presets quickly

---

## FINISH Button

`FINISH` engages the final enhancement stage with stronger polish behavior.

Current interaction behavior includes:

- quick glow ramp on click
- subtle temporary UI dim
- one-shot meter pulse
- brief stereo tighten-then-release effect

---

## Meter Views

- `OUT`: output level behavior view
- `LU`: loudness-style view

Use both while matching level and evaluating final impact.

---

## Output Status States

The status line above `FINISH` updates in real time:

- `Headroom Safe`  
  Default safe state
- `Streaming Safe`  
  True peak at or below `-1.0 dBTP`
- `Optimized`  
  Integrated loudness in target window while true peak remains safe
- `Watch Ceiling`  
  Ceiling-risk warning when true peak rises above roughly `-0.2 dBTP`

These are guidance states for workflow speed, not a replacement for final listening and metering checks.

---

## Practical Workflow

1. Choose a preset closest to your target finish.
2. Fine-tune `TONE`, `GLUE`, `WEIGHT`, `AIR`, `WIDTH`.
3. Use `MIX` to dial processing depth.
4. Level-match with `OUTPUT GAIN`.
5. Engage `FINISH` for final pass.
6. Confirm `OUT`/`LU` readings and output status.

---

## Summary

`Nova Master` is built for fast, premium finishing:

- musical macro controls
- tight preset workflow
- state-aware metering feedback
- compact UI built for real mastering decisions
