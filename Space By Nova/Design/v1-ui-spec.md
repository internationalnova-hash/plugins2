# UI Specification v1 — Space By Nova

## Overview
`Space By Nova` should present itself as a **luxury vocal reverb** with a boutique-hardware feel: fast to understand, visually premium, and intentionally uncluttered.

## Layout
- **Window:** `840 x 520 px`
- **Theme:** Light luxury / matte ivory base with champagne-gold controls
- **Sections:**
  1. **Header bar** — plugin title, subtitle, current mode badge
  2. **Primary macro row** — `Space`, `Air`, `Depth`
  3. **Hero center zone** — larger `Width` control with branded emphasis
  4. **Right utility zone** — `Mix` knob and subtle stereo level meter
  5. **Mode strip** — `Studio`, `Arena`, `Dream`, `Vintage`
  6. **Advanced drawer** — `Pre-Delay`, `Decay`, `Damping`, `Early Reflections`

## Grid Structure
- 12-column visual rhythm with generous horizontal breathing room
- Top row emphasizes the three core tone-shaping macros
- `Width` sits centrally as the hero control because it is the emotional “size” control
- `Mix` is isolated on the right for quick balancing during vocal work
- `Nova Mode` buttons occupy the bottom center as the signature feature

## Controls

| Parameter | Type | Position | Range | Default |
|-----------|------|----------|-------|---------|
| `space` | Rotary knob | Top-left | 0.0 - 10.0 | 5.0 |
| `air` | Rotary knob | Top-center | 0.0 - 10.0 | 5.0 |
| `depth` | Rotary knob | Top-right | 0.0 - 10.0 | 4.5 |
| `width` | Large rotary knob | Center stage | 0.0 - 10.0 | 7.0 |
| `mix` | Rotary knob | Right-side utility section | 0 - 100% | 24% |
| `nova_mode` | Segmented buttons | Bottom center | Studio / Arena / Dream / Vintage | Studio |
| `pre_delay_ms` | Mini slider / mini knob | Advanced drawer | 0 - 150 ms | 28 ms |
| `decay` | Mini slider / mini knob | Advanced drawer | 0.3 - 8.0 s | 2.2 s |
| `damping` | Mini slider / mini knob | Advanced drawer | 0.0 - 1.0 | 0.45 |
| `early_reflections` | Mini slider / mini knob | Advanced drawer | 0.0 - 1.0 | 0.35 |

## Interaction Notes
- `Nova Mode` switching should visibly update the five main controls.
- Motion should feel **premium** rather than abrupt.
- Use **100–300 ms** eased transitions for both knob movement and value readouts.
- The `Advanced` section should be hidden by default to preserve simplicity.

## Default Visual State
- Active mode: `Studio`
- Width knob highlighted with a subtle halo/glow
- Meter animation is understated and premium, not flashy
- Values shown as elegant numeric readouts beneath controls

## Color Palette
- **Background:** `#F4F1EB`
- **Panel Surface:** `#FBF9F5`
- **Primary Gold:** `#C8A45D`
- **Deep Gold Accent:** `#9E7A34`
- **Warm Shadow:** `rgba(92, 72, 32, 0.16)`
- **Text Primary:** `#2F2A24`
- **Text Secondary:** `#7D7467`
- **Hairline Border:** `#E4DDD2`

## Style Notes
- Avoid a sterile “flat web app” look; keep subtle depth and hardware-inspired material cues.
- Knobs should feel like polished champagne anodized metal.
- Labels should be clear, restrained, and studio-professional.
- The interface should read well inside DAWs with both dark and light surroundings.

## Preview / Implementation Guidance
- `Design/v1-test.html` should demonstrate the layout and `Nova Mode` transitions.
- The eventual WebView production UI should closely match this visual structure.
