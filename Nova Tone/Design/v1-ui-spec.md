# Nova Tone — UI Specification v1

## Overview
`Nova Tone` should look like a premium analog EQ in the same design family as `Space By Nova`, but more grounded in classic studio hardware and mastering outboard.

## Window
- **Recommended size:** `980 x 620 px`
- **Framework:** `WebView`
- **Feel:** compact, expensive, hardware-like, and immediately understandable

## Layout Structure

### Header Band
- **Left:** `NOVA TONE` wordmark
- **Subheading:** `MUSICAL TONE EQUALIZER`
- **Right:** small engraved-style descriptor such as `ANALOG CURVES / PREMIUM TONE`

### Main Control Area
Three horizontally aligned EQ sections with one output area:

1. **LOW END**
   - Low Frequency selector (`20 / 30 / 60 / 100 Hz`)
   - `Low Boost`
   - `Low Atten`

2. **HIGH BOOST**
   - High Frequency selector (`3k / 4k / 5k / 8k / 10k / 12k / 16k`)
   - `High Boost`
   - `Bandwidth`

3. **HIGH ATTENUATION**
   - High Atten Frequency selector (`5k / 10k / 20k`)
   - `High Atten`

4. **OUTPUT**
   - `Output Gain`

### Bottom Area
- Large preset buttons:
  - `VOCAL`
  - `BASS`
  - `AIR`
  - `MASTER`
- Subtle divider line above footer copy
- Small footer hints describing what the current mode is optimized for

---

## Control Map

| Parameter | UI Type | Placement | Notes |
|-----------|---------|-----------|-------|
| `low_freq` | segmented selector | Low End section | hardware-style frequency toggle row |
| `low_boost` | rotary knob | Low End section | left knob |
| `low_attenuation` | rotary knob | Low End section | right knob |
| `high_boost_freq` | segmented selector | High Boost section | centered under section title |
| `high_boost` | rotary knob | High Boost section | primary tone lift control |
| `bandwidth` | rotary knob | High Boost section | should read as wide → narrow |
| `high_attenuation_freq` | segmented selector | High Attenuation section | simple 3-way choice |
| `high_attenuation` | rotary knob | High Attenuation section | single large attenuation knob |
| `output_gain` | rotary knob | Output section | output trim for gain matching |
| `mode_preset` | pill buttons | bottom row | applies full musical preset state |

---

## Visual Direction

### Base Color Stack
- **Main faceplate:** `#F2E8DC`
- **Richer alternate:** `#EDE0CF`
- **Inner control panel:** `#E4D6C3`
- **Primary label tone:** `#4A4036`
- **Gold accents:** muted brushed gold, not shiny yellow

### Hardware Details
- very soft top highlight for metal sheen
- gentle bottom shadow for weight
- section dividers should feel etched, not drawn
- knobs should be rounded, tactile, and premium
- family resemblance to `Space By Nova`, but less airy and more classic mastering hardware

---

## Typography
- **Title:** serif or elegant high-contrast face for brand identity
- **Labels:** spaced uppercase, slightly bold, warm dark gray-brown
- **Readouts:** clean and readable, never harsh black

---

## UX Goals
- instantly readable at a glance
- musical and inviting rather than technical
- obvious signal-shaping sections
- feels like a piece of studio gear, not a generic software tool
