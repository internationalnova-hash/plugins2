# Nova Writer

**AI Songwriting Companion** — Generate song blueprints, lyrics, chord progressions, and melody previews powered by Claude AI.

## Install (macOS Desktop App)

1. Download the latest `Nova Writer.dmg` from the [Releases](../../releases) page or CI artifacts.
2. Open the DMG and drag **Nova Writer** into your Applications folder.
3. Double-click to launch.

## Run locally

```bash
npm install && npm start
```

On first launch, enter your Anthropic API key (`sk-ant-...`). Get one at [console.anthropic.com](https://console.anthropic.com). Your key is saved to localStorage only — it never leaves your machine.

## Build distributables

```bash
npm run build:mac    # macOS DMG + ZIP (arm64 + x64)
npm run build:win    # Windows NSIS installer
npm run build:linux  # Linux AppImage
```

## Setup (browser mode)

1. Open `app/index.html` in any modern browser (Chrome, Firefox, Safari, Edge).
2. When prompted, enter your Anthropic API key. Your key is saved to browser localStorage — you only need to enter it once.

## Usage

1. **Write your concept** — Describe the song you want to write in the text area.
2. **Set parameters** — Choose language, genre, mood, BPM, key, and energy level.
3. **Generate Blueprint** — Click ✦ Generate Song Blueprint to get song structure, chord progressions, hook ideas, and a melody preview.
4. **Generate Lyrics** — In the Writer Room (right panel), click ✦ Generate Lyrics to write full lyrics for all sections.
5. **Refine with AI Tools** — Use the AI tool buttons to make hooks stronger, add more Latin flavor, simplify, add Spanglish, and more.
6. **Export** — Export chords as MIDI, melody as MIDI, or lyrics as a printable PDF.

## Features

- 3-column layout: sidebar, main workspace, writer room
- Song Blueprint with section tabs, chord grids, hook ideas, and piano roll preview
- Full lyrics generation with per-section editing
- 8 AI rewrite tools
- Language control: English / Spanish / Spanglish / Bilingual / Latin Crossover
- Dialect flavor: Neutral Latin / Puerto Rican / Dominican / Mexican / Colombian / Spain
- Reference vocal panel with waveform display
- Transport bar with playback controls
- MIDI export for chords and melody
- PDF export for lyrics
- No external dependencies — pure HTML/CSS/JS
- Artist DNA & Reference Vocal — Phase 2
