# Nova Studio — DAW Audit (Nov 23, 2026)

Audited against what a working DAW must do: play, record, edit, mix, save and export reliably.
This is based on reading the engine and editor code and building the app. Its 54 built-in tests pass,
but none of them play or record audio, so they don't catch any of the issues below.

**Bottom line:** Nova Studio has a lot of UI and features started (edit modes, Nova Align
integration, step sequencer, mixer with sends, automation lanes, macros). But the core engine
isn't safe or complete enough yet to make a full song in it. The top problems are crash/dropout
risks on the audio thread, MIDI not playing anything, and export not matching what you hear.

---

## P0 — Breaks sessions or loses work (fix first)

1. **Editing while playing can crash.** The audio thread reads each track's clip list
   (`TrackPlayer::getNextAudioBlock`, `StudioAudioEngine.cpp` ~L120) with no lock, while the editor
   adds, moves and splits clips on the UI thread (`ArrangementModel`). If a clip list grows
   while it's being read, it's a use-after-free.
   *DAW standard:* the audio thread reads an immutable snapshot of the arrangement that the UI swaps in atomically.
2. **Audio thread blocks and allocates.** Every audio block allocates a new buffer
   (`juce::AudioBuffer<float> tempBuffer` at the top of the callback, ~L1646), takes a blocking
   `CriticalSection` (`playerLock`) that the UI thread holds while opening audio files
   (`preloadClipAtPosition` → `loadClip`), and writes log strings to disk during recording (~L2056).
   All three cause clicks and dropouts, especially at low buffer sizes.
3. **Stopping a recording can crash.** `stopRecordingInternal` destroys the writer on the UI thread
   right after clearing `recordingActive`, while the audio thread may still be inside a write to that writer.
4. **Export doesn't match playback.** File → Export uses `ArrangementModel::renderTracksToBuffer`,
   which only applies track volume and clip gain/fades. Exports are missing **pan, every plugin,
   master plugins, sends/aux returns, track EQ and automation**. (The engine's own `exportStereoMix`
   exists but just renders a fixed 10 seconds of the live mixer.)
   *DAW standard:* offline bounce runs the exact same graph as playback, including plugin tails.
5. **Session save loses work.** The session file doesn't store sends and buses, the 6-band track EQ,
   the step-sequencer pattern, piano-roll notes, track input/output routing, or loop/punch ranges.
   There's no autosave or crash recovery.

## P1 — Core DAW features missing or wrong

6. **MIDI doesn't play.** Clips have no note data (`Clip` in `Session.h` has no notes). The piano roll
   only holds notes in its own view (it starts with 4 hard-coded demo notes) and they're never saved or
   played. Every plugin gets an empty `MidiBuffer`, so **instrument plugins make no sound**, and a MIDI
   keyboard is only used for knob-learn, not for playing notes.
7. **Back-to-back clips on one track have gaps.** Each track has one file reader. When playback
   reaches a new clip, the audio thread outputs silence and waits for a UI timer to open the file.
   That leaves a gap at every edit, split or comp boundary. Clip starts are also only accurate to the
   audio block, not the sample.
8. **Wrong signal order on tracks.** Fader and pan are applied before the track's plugins, then EQ
   after them. Every DAW runs inserts → fader → pan. As a result, compressors react to fader moves,
   and mono plugins after the pan get panned audio.
9. **Plugins stop when there's no clip.** If a track has no clip at the playhead, its plugins don't run.
   Reverb and delay tails cut off at clip ends, and you can't monitor input through track plugins.
10. **Recording is single-track and uncompensated.**
    - Only the first armed track gets the take.
    - It always records inputs 1–2; there's no per-track input choice.
    - No recording-latency compensation, so takes land late by the interface's round-trip latency.
    - Punch in/out exists in the transport but isn't applied.
    - No take lanes or comping.
11. **Aux sends depend on track order.** Send buses are cleared, then tracks and aux returns run in
    creation order. Any track created after an aux track sends into a bus that has already been read,
    so its sends are silent.
12. **Tempo is whole-number only and the tempo map isn't used.** `TransportState` stores BPM as an `int`,
    so 92.5 or 140.6 BPM samples can't be matched. The saved tempo map is never read during playback.
    No metronome or count-in.
13. **Automation takes over permanently.** While playing, automation writes over the track's
    volume/pan values and never hands them back to the fader. There are no read/write/touch modes,
    and plugin automation calls `setValue` instead of the host-notifying path.

## P2 — Expected polish

14. No audio/MIDI settings beyond a basic menu item; you set the device in macOS Sound settings.
    A proper device, buffer-size and sample-rate panel (JUCE `AudioDeviceSelectorComponent`) is standard.
15. Master output is hard-clipped at 0 dBFS with no master fader or limiter choice. The clip
    indicator is good, but silently clipping exports isn't.
16. Track meters measure before the plugins and EQ, so they don't show what the track actually outputs.
17. The default build has no build type set (`CMAKE_BUILD_TYPE` empty), so the app is unoptimized. Release builds should be the default.
18. Tests don't cover playback, recording, export or save/load round-trips.

---

## Suggested order

1. **Engine safety:** arrangement snapshots, no locks or allocation in the audio callback, safe recorder stop. (P0 1–3)
2. **One render path:** make export use the playback graph offline, with plugin tails. (P0 4)
3. **Complete save/load,** plus autosave. (P0 5)
4. **Gapless clip playback:** per-clip readers pre-opened ahead of the playhead, sample-accurate starts. (P1 7)
5. **Correct track strip:** inserts → EQ → fader → pan → sends; plugins always run. (P1 8–9, 11)
6. **MIDI:** note data in clips, piano roll saving into clips, MIDI to instruments, live MIDI input. (P1 6)
7. **Recording:** per-track inputs, multi-track, latency compensation, punch, takes. (P1 10)
8. **Tempo, metronome, count-in, automation modes,** then the P2 list.
