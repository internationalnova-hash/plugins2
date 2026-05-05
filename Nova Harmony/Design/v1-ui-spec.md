# Nova Heat — UI Specification v1

## Overview
**Plugin:** `Nova Heat`  
**Subtitle:** `Harmonic Saturation Engine`  
**Theme:** Premium boutique saturation processor with a warm champagne faceplate, richer amber accents than Nova Tone, and a slightly more energetic visual attitude.

## Window
- **Recommended size:** `980 x 620 px`
- **Layout style:** one-screen hardware panel
- **Framework:** `webview`

## Top Bar
### Left
- Large product title: `NOVA HEAT`
- Subtitle beneath: `Harmonic Saturation Engine`

### Right
- Descriptor text: `by International Nova`
- Script-style boutique branding, matching Nova Tone and Nova Space
- Use a slightly toned-down muted copper accent that matches the `HEAT` wordmark

## Main Panel Layout
Use **three major sections** across the center, plus a bottom preset strip.

### 1. Left Section — Input Drive
**Section label:** `INPUT DRIVE`

**Controls:**
- `DRIVE` knob — slightly oversized secondary hero control so the input push feels important immediately
- Readout below: `+0.0 dB`
- Mode buttons at the top of the panel:
  - `SOFT` → clean, polished, safe
  - `MEDIUM` → warm, analog, musical
  - `HARD` → punchy, aggressive, character
- Default mode: `MEDIUM`

**Purpose:**
This section should instantly communicate how hard the signal is being pushed into the saturation engine and what personality the saturation will take on.

### 2. Center Section — Tone Shaping
**Section label:** `TONE SHAPING`

**Controls:**
- `TONE` knob — medium size
- `HEAT` knob — hero control, larger than the others
- Labels clearly separated under each knob

**Behavioral meaning:**
- `TONE` = warmth ↔ brightness tilt
- `HEAT` = harmonic personality and curve intensity

### 3. Right Section — Output
**Section label:** `OUTPUT`

**Controls:**
- `OUTPUT` knob — labeled `MAKEUP GAIN`
- Readout below: `+0.0 dB`
- `MIX` control — compact slider or small supporting knob
- Output meter on the far right with warm gold vertical bars

**Meter behavior:**
- soft gold at normal levels
- brighter amber as intensity rises
- subtle, tasteful, not overly flashy

## Magic Mode Button
- Add a small premium toggle labeled `MAGIC`
- Place it in the **bottom-right of the output section**
- Default state: `OFF`
- Active look: slightly brighter gold with a subtle glow
- Tooltip / helper text:
  > `MAGIC adds smart polish, presence, and control without changing your core settings.`

**Behavior:**
When enabled, it should quietly add a little more dynamic drive, vocal focus, top smoothing, wet stereo enhancement, and auto trim without replacing the user’s current settings.

## Bottom Preset Strip
Use four rounded preset buttons in the same family as Nova Tone:
- `VOCAL` — `Medium`, `Drive 4.2`, `Heat 5.0`, `Tone 5.6`, `Mix 28`, `Output -0.8`
- `DRUMS` — `Hard`, `Drive 5.8`, `Heat 6.5`, `Tone 4.8`, `Mix 35`, `Output -1.5`
- `BASS` — `Medium`, `Drive 5.0`, `Heat 5.8`, `Tone 3.8`, `Mix 32`, `Output -1.2`
- `MASTER` — `Soft`, `Drive 2.8`, `Heat 3.2`, `Tone 5.0`, `Mix 18`, `Output 0.0`

Each preset should recall a complete, gain-balanced musical starting point.

## Control Inventory
| Control | Type | Position | Range | Default |
|--------|------|----------|-------|---------|
| Drive | Large knob | Left panel center | 0.0–10.0 | 2.5 |
| Mode | 3-button group | Left panel top | Soft/Medium/Hard | Medium |
| Tone | Medium knob | Center-left | -5.0–5.0 | 0.0 |
| Heat | Large hero knob | Center-right | 0.0–10.0 | 3.0 |
| Output | Medium knob | Right panel top | -12 to +12 dB | 0.0 dB |
| Mix | Compact slider | Right panel lower area | 0–100% | 18% |
| Meter | Vertical LED-style bars | Right panel edge | visual | n/a |
| Magic | Premium toggle | Centered above presets | Off / On | Off |
| Presets | 4 buttons | Bottom row | Vocal/Drums/Bass/Master | none |

## Color Palette
- **Faceplate:** `#EED9BB`
- **Inner panel:** `#D7BA90`
- **Dark text:** `#4A4036`
- **Soft text:** `#6F6253`
- **Primary gold:** `#C68D1E`
- **Hot amber accent:** `#C27612`
- **Light gold highlight:** `#E6BF67`
- **Signature copper:** `#A85C3B`

## Visual Notes
- Nova Heat should feel **warmer, richer, and slightly more aggressive** than Nova Tone.
- Keep the same Nova family system, but shift the hue about 10–15% warmer and slightly hotter.
- Gold accents should carry more contrast and intensity.
- The `HEAT` word in the title and the `by International Nova` byline should share the same muted copper accent.
- Add a very subtle heated-metal gradient to the `HEAT` wordmark for extra polish.
- The `MAGIC` button should live in the output section’s bottom-right corner with a very soft copper glow.
- Meter peaks and knob highlights should stay in the same copper family for cohesion.
- The `DRIVE` knob should feel slightly larger than the other secondary controls.
- The hero `HEAT` knob should visually anchor the center section.
- The output meter should pop a little more with tasteful glow, not club-style flash.
- The mix percentage should read like a premium pill badge, not a tiny utility label.
- `MAGIC` should look special compared with normal buttons: brighter gold, subtle glow, premium feel.
- The design should remain elegant and premium, not grungy or chaotic.

## UX Notes
- Keep the UI clean and immediate.
- Small control count = fast results.
- Emphasize readouts, mode meaning, and confident hardware styling.
- `SOFT`, `MEDIUM`, and `HARD` must feel like distinct personalities, not generic settings.
- The four bottom presets must sound polished immediately and remain level-balanced when auditioned.
- This should feel like a plugin users reach for when they want a vocal or mix to instantly sound more exciting.
