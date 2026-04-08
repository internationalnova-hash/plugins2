# Style Guide v1 — Space By Nova

## Visual Intent
Space By Nova should feel like a **modern premium studio device**: elegant, calm, and expensive. The look should support pop, R&B, and Latin vocal workflows without becoming busy or intimidating.

## Core Mood Words
- Luxurious
- Smooth
- Boutique
- Calm
- Premium
- Modern

## Color System

### Primary Palette
- **Matte Ivory Background:** `#F4F1EB`
- **Soft White Surface:** `#FBF9F5`
- **Champagne Gold:** `#C8A45D`
- **Deep Gold Accent:** `#9E7A34`
- **Warm Stone Text:** `#2F2A24`
- **Muted Secondary Text:** `#7D7467`
- **Border / Divider:** `#E4DDD2`

### Support Colors
- **Soft Glow Highlight:** `rgba(200, 164, 93, 0.22)`
- **Panel Shadow:** `rgba(92, 72, 32, 0.16)`
- **Success / Meter Green:** `#84B28A`
- **Meter Warm Peak:** `#D4A94A`

## Typography
- **Font stack:** `Inter, "Segoe UI", Roboto, Helvetica, Arial, sans-serif`
- **Plugin Title:** 22px / 700 / letter-spacing `0.18em`
- **Section Labels:** 10px / 700 / uppercase / letter-spacing `0.18em`
- **Control Labels:** 11px / 700 / uppercase / letter-spacing `0.14em`
- **Value Readouts:** 12–13px / 600
- **Support Text:** 12px / 500

## Spacing Rules
- Outer padding: `20px`
- Panel radius: `20px`
- Control card radius: `18px`
- Internal control gap: `18–24px`
- Standard section padding: `16px`
- Bottom mode strip padding: `12px 14px`

## Control Styles

### Main Knobs
- Circular gold rim with soft inset highlight
- Cream/ivory inner cap with thin metallic pointer
- Subtle inner shadow to simulate hardware depth
- Active/hover state adds restrained gold glow

### Hero Width Knob
- Larger than the other controls
- Slight halo ring or spotlight effect
- Used as the emotional centerpiece of the interface

### Mode Buttons
- Rounded segmented buttons
- Inactive state: matte ivory / muted text
- Active state: champagne gold fill with dark text and slight glow
- Must animate smoothly when switched

### Advanced Panel Controls
- Smaller, quieter, more technical
- Use mini cards or slim sliders to avoid competing with the main macros

## Motion Rules
- **Knob and mode transitions:** `220ms cubic-bezier(0.22, 1, 0.36, 1)`
- **Drawer open/close:** `180ms ease`
- **Do not use springy or bouncy motion**; motion should feel confident and expensive
- `Nova Mode` changes should visually align with the requested **100–300 ms** DSP smoothing window

## Material Treatment
- Prefer layered surfaces over heavy gradients
- Use subtle shadows instead of strong contrast
- Keep metallic gold tasteful and slightly desaturated
- No glossy “gaming UI” look

## Branding Notes
- Title should read `SPACE BY NOVA`
- Subtitle can read `Luxury Vocal Reverb`
- Present the current `Nova Mode` as a refined badge or pill in the header

## Metering Guidance
- Stereo meter should be minimal and elegant
- Use low-motion ambient pulsing rather than aggressive activity
- Meter should support the luxury aesthetic, not dominate it

## Accessibility / Readability
- Ensure text contrast remains readable on the ivory background
- Numeric values should be easy to parse quickly during mixing
- Important states should be indicated by more than color alone
