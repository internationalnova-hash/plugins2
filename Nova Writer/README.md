# Nova Writer — AI Songwriting Companion

A standalone web app for AI-powered songwriting. Generate song blueprints, chord progressions, lyrics, and melody previews using Claude AI.

## Setup

1. Open `app/index.html` in any modern browser (Chrome, Firefox, Safari, Edge).
2. When prompted, enter your [Anthropic API key](https://console.anthropic.com/) (starts with `sk-ant-`).
3. Your key is saved in `localStorage` — never sent anywhere except Anthropic's API.

No build step, no server, no dependencies. It runs entirely in the browser.

## Features

- **Song Blueprint Generator** — AI-generated structure, chord progressions, hook concepts, and titles
- **Lyrics Writer** — Full song lyrics (Verse, Pre-Chorus, Hook, Bridge) in any language
- **AI Tools** — One-click rewrites: Make Hook Stronger, More Emotional, More Latin, Add Spanglish, and more
- **Language Control** — English, Spanish, Spanglish, Bilingual, with dialect and Spanglish amount settings
- **Genre & Mood** — Latin Pop, Reggaeton, R&B, Pop, Trap, Afrobeats, Gospel, Drill, Cinematic
- **Chord Grid** — Visual chord progression display for all song sections
- **Melody Preview** — Piano roll visualization of your melody
- **Arrangement View** — Timeline and track layer overview
- **MIDI Export** — Download chord and melody MIDI files
- **Lyrics PDF** — Print or export lyrics to PDF
- **Transport Bar** — Playback controls with progress bar and BPM/key display

## File Structure

```
Nova Writer/
  app/
    index.html   — Complete self-contained web app
    n_logo.png   — Nova logo
  README.md
```

## Requirements

- Modern browser with ES6+ support
- Anthropic API key (Claude Sonnet access)
