# Nova Studio — First Vocal Session Test Plan
### Alpha Build | UA Interface | Mac

---

## Before You Start

**Build the app on your Mac:**
```
cd "Nova Studio"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target NovaStudio
open build/NovaStudio_artefacts/Release/NovaStudio.app
```

**Files are saved to:**
- Sessions: `~/Documents/NovaStudio/Sessions/` (`.novastudio` + `.plugins.json` sidecar)
- Recordings: `~/Documents/NovaStudio/Recordings/`
  - All three folders are auto-created on first launch
  - Recorded files are named `NovaStudioRecording_<date>_<time>.wav`
  - After stopping a recording, the status bar shows the full file path

---

## Step 1 — First Launch

- [ ] App opens, title bar shows **Nova Studio**
- [ ] Default workspace is **Edit** mode
- [ ] Transport bar visible near the top (Play / Stop / Rec / Loop / Arm / Monitor buttons)
- [ ] Status bar at bottom-right shows: `Nova Studio Lite prototype ready`
- [ ] Two default tracks visible in track panel: `Audio 1` and `MIDI 1`
- [ ] No crash, no assertion dialogs

**Watch for:** Any JUCE assertion popups. On Mac with a UA interface, audio should initialize automatically using Core Audio. If you see "Audio initialization failed" in the status bar, check System Preferences → Security & Privacy → Microphone access for Nova Studio.

---

## Step 2 — Audio Device Setup

JUCE picks your default Core Audio device automatically via `initialiseWithDefaultDevices(2 inputs, 2 outputs)`.

**To verify the UA interface is active:**
- [ ] Play something through your UA interface — if you hear it, Core Audio has it selected
- [ ] Nova Studio has no audio settings panel yet (Alpha gap) — change input/output using **macOS System Preferences → Sound** before launching, or use the UA Console to set your UA as the system default

**Expected behavior:** On a UA interface, `initialize()` will succeed silently and the status bar will show the prototype ready message (not an error). If it fails, the status bar will read `Audio initialization failed.`

---

## Step 3 — Arm a Track

- [ ] Switch to **Edit** workspace (toolbar, top-left button row: Edit)
- [ ] In the track panel (left side), find **Audio 1**
- [ ] Click the **Arm** button in the transport bar (orange when armed)
  - This arms the global record-arm state — track-level arm is also set via the Mixer
- [ ] Status bar shows: `Record armed.`

**Alternatively, arm from Mixer:**
- [ ] Click **Mixer** workspace button
- [ ] Click the **A** (Arm) button on the Audio 1 channel strip — it turns red

---

## Step 4 — Enable Input Monitoring

- [ ] Click the **Monitor** button in the transport bar (it highlights when on)
- [ ] Status bar shows: `Input monitoring enabled.`
- [ ] You should now hear your microphone/vocal through your UA outputs in real time
- [ ] Monitoring uses 0ms additional latency compensation — you may hear UA Console's hardware monitoring at the same time; disable hardware monitoring in UA Console if double-monitoring

**Watch for:** If you hear nothing, check: UA interface selected as system default, input channel connected, input level not zero in macOS Sound settings.

---

## Step 5 — Record a Vocal Take

- [ ] Confirm track is armed (Arm button lit)
- [ ] Confirm monitoring is on (you hear yourself)
- [ ] Click **Rec** in the transport bar
  - Transport starts playing automatically
  - Rec button lights up
  - Status bar shows: `Recording active.`
- [ ] Sing/speak your vocal
- [ ] Click **Stop** when done
  - Status bar shows: `Playback stopped.`
  - Recording stops, WAV file written to disk
  - Clip automatically added to Audio 1 track in the arrangement

**Watch for:**
- If Rec button does nothing: check that Arm is engaged first
- Recording writes a 32-bit float WAV — compatible with all DAWs
- Only the first armed track receives the clip (multi-track simultaneous record is a known Alpha gap)

---

## Step 6 — Play Back the Recording

- [ ] Click **Play** in the transport bar
- [ ] You should hear the recorded vocal through your UA outputs
- [ ] Status bar shows: `Playback engaged.`
- [ ] Click **Stop** to stop
- [ ] The clip appears in the arrangement view (Edit workspace)

**Watch for:** Clip starts at the exact sample position where record began. If playback sounds stuttery, the audio buffer may be too small — this is a system/driver setting, not configurable in Nova Studio Alpha.

---

## Step 7 — Load Nova Console / Nova Level / Nova Space

- [ ] Switch to **Mixer** workspace (toolbar)
- [ ] On the **Audio 1** channel strip, find the **insert slots** (labeled `-- empty --`)
- [ ] Click the first `-- empty --` slot
  - The **Plugin Browser** window opens
- [ ] Click **Scan Plugins**
  - Nova Studio scans: `/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/VST3`
  - Nova plugins should appear in the list (Nova Console, Nova Level, Nova Space)
- [ ] Type "Nova" in the search box to filter
- [ ] Select **Nova Console** (or Nova Level / Nova Space) from the list
- [ ] Click **Load to Insert** (or double-click)
  - Status line in the browser shows: `Loaded: Nova Console → Track 1 Slot 1`
  - The insert slot on the channel strip now shows **Nova Console** with a blue border
- [ ] Close the Plugin Browser window

**If plugins don't appear after scan:**
- [ ] Click **Browse File...** in the Plugin Browser
- [ ] Navigate to `/Library/Audio/Plug-Ins/VST3/Nova Console.vst3` (or your Nova install path)
- [ ] Select the `.vst3` bundle — it will be added directly

**Watch for:** UA plugins (Apollo Console, Luna) may also appear — don't load those, they require UA hardware routing.

---

## Step 8 — Open the Plugin Editor

- [ ] In the Mixer, click the insert slot that now shows **Nova Console**
  - (Since it now has a plugin loaded, clicking opens the editor instead of the browser)
- [ ] Nova Console GUI window opens as a floating panel
- [ ] Adjust EQ/compression/saturation settings as desired
- [ ] Close the editor window (X button) — settings are retained in memory

**Watch for:** Some plugins open their editor behind the main window — check the Dock or use Mission Control if the editor doesn't appear on top.

---

## Step 9 — Save the Session

- [ ] Click **Save** button (top-right of the toolbar, green text)
- [ ] A file save dialog opens, defaulting to `~/Documents/NovaStudio/`
- [ ] Name your session (e.g. `vocal_test_01`) and click Save
- [ ] Status bar shows: `Session saved: vocal_test_01.novastudio`
- [ ] Two files are created:
  - `vocal_test_01.novastudio` — tracks, clips, tempo, markers
  - `vocal_test_01.plugins.json` — plugin chain + all parameter states (base64)

**Note:** The recorded WAV file is **not** embedded in the session file — it stays in `~/Documents/NovaStudio/Recordings/`. After stopping, the status bar shows the full path. Keep both the session and recordings folders for a complete project.

---

## Step 10 — Reopen the Session

- [ ] Quit Nova Studio
- [ ] Relaunch Nova Studio
- [ ] Click **Load** button (top-right of toolbar, blue text)
- [ ] Navigate to your `vocal_test_01.novastudio` file and open it
- [ ] Status bar shows: `Session loaded: vocal_test_01.novastudio`
- [ ] Verify:
  - [ ] Tracks restored
  - [ ] Clips visible in arrangement (Edit workspace)
  - [ ] Switch to Mixer — plugin chain restored (Nova Console shown in insert slot)
  - [ ] Click the insert slot — plugin editor opens with saved parameters

---

## Known Alpha Gaps (Do Not File as Bugs)

| Gap | Workaround |
|---|---|
| No audio device settings panel | Set UA as macOS system default before launch |
| Only first armed track records | Arm only one track at a time |
| No latency compensation | Manually shift clip in arrangement if needed |
| No MIDI recording | Alpha — audio only |
| Mixer meters always dark | Will animate when audio is playing (requires hardware) |
| Rack mode (mode 4) has no content | Use Edit/Mixer/Beat modes |
| No waveform visible on recorded clip immediately | Waveform cache builds async; appears on next repaint |
| Session does not store WAV file location automatically | Keep recordings folder alongside session folder |

---

## Bugs to Watch For and Report

| Symptom | Likely Cause |
|---|---|
| App crashes on launch | Audio device init — check Console.app for crash log |
| `Audio initialization failed` in status | UA not set as system default audio device |
| Record button does nothing | Track not armed — click Arm first |
| No sound during playback | Clip path is absolute — if session moved, clip won't load |
| Plugin browser shows 0 plugins after scan | VST3 path differs — use Browse File button |
| Plugin editor doesn't open | Plugin may require AU format (scan finds VST3 only on Mac by default) |
| Session save dialog doesn't appear | macOS sandbox — grant file access if prompted |
| Crash when opening plugin editor | Plugin has a GUI bug — note the plugin name and report |

---

## Quick Reference — Transport Controls

| Button | Action |
|---|---|
| **Play** | Start playback |
| **Stop** | Stop playback and recording |
| **Rec** | Toggle recording (auto-arms if needed, auto-plays) |
| **Arm** | Toggle global record arm |
| **Monitor** | Toggle live input through outputs |
| **Loop** | Toggle loop playback |
| **Save** | Save session to `.novastudio` file |
| **Load** | Open existing `.novastudio` session |

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Cmd+Z` | Undo clip edit |
| `Cmd+Shift+Z` | Redo clip edit |
| `Cmd+Shift+P` | Toggle preview/original audio (when Nova Align preview exists) |

---

*Nova Studio Alpha — Branch: `claude/epic-goodall-2v7A7`*
