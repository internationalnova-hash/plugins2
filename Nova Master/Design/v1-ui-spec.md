# Nova Master — UI Specification v1

## Overview
**Plugin name:** `Nova Master`  
**Display title:** `NOVA MASTER`  
**Subtitle:** `Final Mix Processor`

Nova Master should feel like the **flagship finishing stage** of the Nova suite:
- more precise
- more refined
- more spacious
- less warm and “colored” than `Nova Heat`
- more elevated than the other plugins

## Design Philosophy
> **Everything is precise… except the magic.**

That means:
- the overall UI should feel calm, controlled, and mastering-room ready
- the base should stay in the Nova family via champagne materials
- the accents should shift toward **platinum / brushed silver**
- the `FINISH` button should be the one warm gold contrast moment

## Core Layout

```text
┌──────────────────────────────────────────────────────────────┐
│ NOVA MASTER                         by International Nova    │
│ Final Mix Processor                                        │
│                                                              │
│   TONE   GLUE   WEIGHT   AIR   WIDTH        METER / OUTPUT   │
│                                                              │
│        [ CLEAN ] [ WARM ] [ WIDE ] [ LOUD ]                 │
│                                                    [FINISH]  │
└──────────────────────────────────────────────────────────────┘
```

## Section Breakdown

### Top Bar
**Left**
- `NOVA MASTER`
- subtitle: `Final Mix Processor`
- compact silver status chip such as `MASTER BUS FINISHER`

**Right**
- `by International Nova`
- script treatment matching the Nova family branding
- restrained refinement line to reinforce the flagship identity

### Main Control Row
Five primary knobs across the center:
1. `TONE` — `Balance`
2. `GLUE` — `Control`
3. `WEIGHT` — `Low End`
4. `AIR` — `Top End`
5. `WIDTH` — `Stereo`

Each should have:
- equal visual size
- no single hero knob
- premium metallic silver body
- restrained ring graphics
- small, precise value display underneath
- a subtle individual control bay so the row feels more authoritative and mastering-grade

### Why no hero knob?
Because Nova Master is a **balancing plugin**, not a one-action tool. The row should feel even, measured, and mastering-grade.

### Precision Rail
On the right side or slightly offset right:
- vertical output meter
- small `OUT` / `LU` or `OUT` / `CEILING` micro-display
- compact finishing area

This section should feel more technical and “mastering-grade” than the rest of the suite, but still elegant.

### Finish Button
`FINISH` should sit slightly apart from the main knobs so it reads as the premium enhancement stage.

When OFF:
- silver / neutral
- subtle and refined

When ON:
- warm gold fill or glow
- slight inner highlight
- clear contrast against the cooler silver UI

### Mode Row
Bottom row buttons:
- `CLEAN`
- `WARM`
- `WIDE`
- `LOUD`

These should be:
- compact
- even in width
- premium, slightly thinner than previous Nova buttons
- clearly active when selected

## Visual Style Notes

### Material system
- champagne faceplate base
- brushed platinum / silver accents
- cool-white labels
- very light blue-white meter energy
- minimal glow except where musically meaningful

### Knob treatment
Keep the family knob design, but refine it for mastering:
- slightly thinner outer ring
- sharper indicator line
- less dramatic glow
- cooler metallic reflections
- more “precision instrument” than “tone box”

### Meter treatment
This is where the silver identity becomes strongest:
- cool white / soft blue tint
- precise, clean movement
- restrained brightness
- no hot orange/red saturation vibe unless truly clipping

## UX Intent
A user should open Nova Master and immediately think:
> “This is the final stage. Keep it elegant.”

The UI should reinforce:
- restraint
- precision
- polish
- confidence

## V1 Lock
For V1, keep the layout simple:
- 5 main knobs only
- one `FINISH` button
- one mode row
- one clean meter/output area

Do **not** let it become a crowded mastering suite.
