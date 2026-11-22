# Nova Meter

A target-based mix/master meter. Set a min/max range per meter, play your mix, and each reading grades itself:
**amber** = below your minimum, **green** = in range, **red** = over your maximum.
Monitoring only: audio passes through untouched. Load it last on the master bus.

## Meters

| Meter | Unit | What it measures |
|---|---|---|
| Peak | dBFS | Highest sample level |
| True Peak | dBTP | 4x oversampled inter-sample peak (BS.1770-4 style) |
| LUFS Integrated | LUFS | Gated programme loudness since the last Reset (BS.1770, absolute -70 / relative -10 LU gates) |
| LUFS Short-Term | LUFS | 3 s sliding window |
| RMS | dBFS | 300 ms unweighted RMS (a full-scale sine reads -3.0) |
| Dynamic Range | PSR | Max true peak over the last 3 s minus short-term loudness |
| Mid Density | dB | RMS of the 250 Hz – 4 kHz band, 400 ms window |

## Features

- **Live View**: ring per meter, coloured by grade, with target ticks and a held value underneath.
- **Set Targets**: drag min/max handles per meter (Shift = fine), or type exact values. Audio keeps updating while you adjust.
- **Held values**: highest (or lowest, per meter) since Reset. They never clear on their own.
- **Reset**: clears held values and the integrated measurement. It never touches targets or the profile.
- **Meter name menu**: click a meter's name to switch its held value between highest and lowest, hide or show it, or reset its range.
- **Reference view**: drop a reference track onto the plugin (or click to browse). It's analysed with the same meters as your mix, and each row shows reference vs. your mix with a plain-language difference ("1.9 LU quieter", "2.2 more open"). **Use as targets** turns the reference into a target profile: integrated ±1 LU, peaks just under the reference's max, and RMS/PSR/mid density across the reference's typical 10–90% range. Intros, fades and silence are excluded from the ranges. Only the reference's numbers are saved with the session, never the audio.
  - Compared on: Peak and True Peak use your held max; LUFS Integrated uses your value so far; LUFS Short-Term uses your loudest 3 s; RMS, PSR and Mid Density use your live reading against the reference's typical value.
  - Formats: WAV, AIFF, FLAC, MP3, OGG (+ M4A/AAC on macOS). Dropped files stream to the plugin in chunks and are analysed from a temp copy that's deleted afterwards.
- **Profiles**: built-in (Mix Bus, Streaming Master, Loud Master, Broadcast EBU R128) plus your own. User profiles are shared by every instance and stored in
  `~/Library/Application Support/Nova Meter/Profiles.json` (macOS) or `%APPDATA%\Nova Meter\Profiles.json` (Windows).
  The session also stores its own copy of the targets, so projects reopen exactly as you left them.

## Build

```bash
cmake -S "Nova Meter" -B "Nova Meter/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "Nova Meter/build" --config Release
```

Formats: VST3 / AU / AAX / Standalone on macOS, VST3 / AAX / Standalone on Windows. AAX is only built when `_shared/AAX-SDK` exists.

### License gate

- Default (`NOVA_METER_REQUIRE_LICENSE=OFF`): opens straight away, for local testing.
- Customer builds: `-DNOVA_METER_REQUIRE_LICENSE=ON` shows the Nova unlock screen until a key is activated (`NovaLicense`, app id `nova-meter`). The GitHub workflows build with it ON.

### Accuracy tests

```bash
cmake --build "Nova Meter/build" --target NovaMeterTests
"Nova Meter/build/NovaMeterTests_artefacts/Release/NovaMeterTests"
```

These run EBU Tech 3341-style signals (1 kHz sine at -23 dBFS → -23.0 LUFS, relative and absolute gating, true-peak of an fs/4 sine at 45°) at 44.1, 48 and 96 kHz.

### UI preview without a DAW

Serve `Source/ui/public` over HTTP and open `index.html`. Without the JUCE backend the UI runs on built-in mock data.
